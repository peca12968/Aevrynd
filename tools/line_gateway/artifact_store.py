"""Bounded, tokenized artifact publication and download storage."""

from __future__ import annotations

import argparse
import hashlib
import mimetypes
import os
import re
import secrets
import sqlite3
import stat
import tempfile
import time
import uuid
from contextlib import closing
from dataclasses import dataclass
from pathlib import Path

ALLOWED_EXTENSIONS = frozenset({".pptx", ".docx", ".pdf", ".zip", ".png", ".jpg", ".jpeg"})
MAX_ARTIFACT_BYTES = 25 * 1024 * 1024
MAX_TOTAL_BYTES = 256 * 1024 * 1024
MAX_ARTIFACTS = 128
MIN_TTL_SECONDS = 60
MAX_TTL_SECONDS = 24 * 60 * 60
MAX_DOWNLOADS = 5
TOKEN_PATTERN = re.compile(r"^[A-Za-z0-9_-]{43}$")


class ArtifactError(RuntimeError):
    pass


class ArtifactNotFound(ArtifactError):
    pass


@dataclass(frozen=True, slots=True)
class PublishedArtifact:
    relative_url: str
    expires_at: int
    max_downloads: int
    size: int
    sha256: str


@dataclass(frozen=True, slots=True)
class ArtifactDownload:
    body: bytes
    content_type: str
    download_name: str
    sha256: str
    reservation_id: str


def _is_reparse(path: Path) -> bool:
    try:
        metadata = path.lstat()
    except FileNotFoundError:
        return False
    attributes = getattr(metadata, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return stat.S_ISLNK(metadata.st_mode) or bool(attributes & reparse_flag)


def _assert_safe_chain(path: Path, stop: Path) -> None:
    cursor = path
    while True:
        if cursor.exists() and _is_reparse(cursor):
            raise ArtifactError("artifact_root_reparse")
        if os.path.normcase(str(cursor)) == os.path.normcase(str(stop)):
            return
        if cursor == cursor.parent:
            raise ArtifactError("artifact_root_outside_program_data")
        cursor = cursor.parent


class ArtifactStore:
    def __init__(self, root: str | Path | None = None, *, allow_test_root: bool = False,
                 max_total_bytes: int = MAX_TOTAL_BYTES, max_artifacts: int = MAX_ARTIFACTS,
                 recover_startup_reservations: bool = False) -> None:
        program_data = Path(os.path.abspath(os.environ.get("ProgramData", r"C:\ProgramData")))
        fixed_root = program_data / "Aevrynd" / "LineGateway" / "artifacts"
        selected = fixed_root if root is None else Path(os.path.abspath(root))
        if not allow_test_root:
            if os.path.normcase(str(selected)) != os.path.normcase(str(fixed_root)):
                raise ArtifactError("artifact_root_not_fixed")
            _assert_safe_chain(selected, program_data)
        self._program_data = program_data
        self._allow_test_root = allow_test_root
        if not allow_test_root and (max_total_bytes != MAX_TOTAL_BYTES or max_artifacts != MAX_ARTIFACTS):
            raise ArtifactError("quota_not_fixed")
        if max_total_bytes < 1 or max_total_bytes > MAX_TOTAL_BYTES or max_artifacts < 1 or max_artifacts > MAX_ARTIFACTS:
            raise ArtifactError("quota_denied")
        self.max_total_bytes = max_total_bytes
        self.max_artifacts = max_artifacts
        self.root = selected
        self.files = selected / "files"
        self.database = selected / "metadata.sqlite3"
        for directory in (self.root, self.files):
            if directory.exists() and _is_reparse(directory):
                raise ArtifactError("artifact_root_reparse")
            directory.mkdir(parents=True, exist_ok=True)
            if _is_reparse(directory):
                raise ArtifactError("artifact_root_reparse")
        if self.database.exists() and _is_reparse(self.database):
            raise ArtifactError("artifact_database_reparse")
        with closing(sqlite3.connect(self.database)) as connection:
            connection.execute(
                "CREATE TABLE IF NOT EXISTS artifacts ("
                "token_hash TEXT PRIMARY KEY, artifact_id TEXT UNIQUE NOT NULL, extension TEXT NOT NULL, "
                "size INTEGER NOT NULL, sha256 TEXT NOT NULL, expires_at INTEGER NOT NULL, "
                "max_downloads INTEGER NOT NULL, downloads INTEGER NOT NULL DEFAULT 0)"
            )
            connection.execute(
                "CREATE TABLE IF NOT EXISTS artifact_reservations ("
                "reservation_id TEXT PRIMARY KEY, token_hash TEXT NOT NULL, expires_at INTEGER NOT NULL)"
            )
            connection.commit()
        if _is_reparse(self.database):
            raise ArtifactError("artifact_database_reparse")
        self.garbage_collect()
        if recover_startup_reservations:
            # Gateway 尚未接流量時，清除上一個已停止 process 遺留的占位；publisher 不得清除。
            with closing(sqlite3.connect(self.database, timeout=10)) as connection:
                connection.execute("DELETE FROM artifact_reservations")
                connection.commit()

    def _validate_storage(self) -> None:
        if not self._allow_test_root:
            _assert_safe_chain(self.root, self._program_data)
        if _is_reparse(self.root) or _is_reparse(self.files) or _is_reparse(self.database):
            raise ArtifactError("artifact_storage_reparse")

    def garbage_collect(self, *, now: int | None = None) -> tuple[int, int]:
        """先原子選定並移除 stale metadata，再只清除同一批固定 ID 檔案。"""
        self._validate_storage()
        current = int(time.time()) if now is None else now
        with closing(sqlite3.connect(self.database, timeout=10, isolation_level=None)) as connection:
            connection.execute("BEGIN IMMEDIATE")
            connection.execute("DELETE FROM artifact_reservations WHERE expires_at <= ?", (current,))
            stale = connection.execute(
                "SELECT artifact_id,extension,token_hash FROM artifacts a "
                "WHERE (expires_at <= ? OR downloads >= max_downloads) AND NOT EXISTS "
                "(SELECT 1 FROM artifact_reservations r WHERE r.token_hash=a.token_hash)", (current,),
            ).fetchall()
            connection.executemany("DELETE FROM artifacts WHERE token_hash = ?", ((row[2],) for row in stale))
            connection.commit()
        removed_files = 0
        # 只刪除本交易明確認定 stale 的固定 ID；未知 orphan 保留並計入 quota。
        for artifact_id, extension, _token_hash in stale:
            candidate = self.files / f"{artifact_id}{extension}"
            try:
                if candidate.is_file() and not _is_reparse(candidate):
                    candidate.unlink()
                    removed_files += 1
            except OSError:
                pass
        return len(stale), removed_files

    def publish(self, source: str | Path, *, ttl_seconds: int = 3600,
                max_downloads: int = 3, now: int | None = None) -> PublishedArtifact:
        if not isinstance(ttl_seconds, int) or not MIN_TTL_SECONDS <= ttl_seconds <= MAX_TTL_SECONDS:
            raise ArtifactError("ttl_denied")
        if not isinstance(max_downloads, int) or not 1 <= max_downloads <= MAX_DOWNLOADS:
            raise ArtifactError("download_limit_denied")
        source_path = Path(source)
        if not source_path.is_absolute() or not source_path.is_file() or _is_reparse(source_path):
            raise ArtifactError("source_denied")
        extension = source_path.suffix.lower()
        if extension not in ALLOWED_EXTENSIONS:
            raise ArtifactError("extension_denied")
        if source_path.stat().st_size < 1 or source_path.stat().st_size > MAX_ARTIFACT_BYTES:
            raise ArtifactError("size_denied")
        created = int(time.time()) if now is None else now
        self.garbage_collect(now=created)
        artifact_id = uuid.uuid4().hex
        destination = self.files / f"{artifact_id}{extension}"
        descriptor, temporary_name = tempfile.mkstemp(prefix="publish-", suffix=".tmp", dir=self.files)
        temporary = Path(temporary_name)
        digest = hashlib.sha256()
        size = 0
        try:
            with source_path.open("rb") as source_stream, os.fdopen(descriptor, "wb") as output:
                while chunk := source_stream.read(1024 * 1024):
                    size += len(chunk)
                    if size > MAX_ARTIFACT_BYTES:
                        raise ArtifactError("size_denied")
                    digest.update(chunk)
                    output.write(chunk)
                output.flush()
                os.fsync(output.fileno())
            if size < 1 or _is_reparse(source_path):
                raise ArtifactError("source_changed")
        except Exception:
            temporary.unlink(missing_ok=True)
            raise
        token = secrets.token_urlsafe(32)
        if not TOKEN_PATTERN.fullmatch(token):
            destination.unlink(missing_ok=True)
            raise ArtifactError("token_generation_failed")
        token_hash = hashlib.sha256(token.encode("ascii")).hexdigest()
        try:
            with closing(sqlite3.connect(self.database, timeout=10, isolation_level=None)) as connection:
                connection.execute("BEGIN IMMEDIATE")
                count, total = connection.execute("SELECT COUNT(*),COALESCE(SUM(size),0) FROM artifacts").fetchone()
                physical_files = [entry for entry in self.files.iterdir() if entry.is_file() and not _is_reparse(entry)]
                physical_size = sum(entry.stat().st_size for entry in physical_files)
                if (int(count) + 1 > self.max_artifacts or len(physical_files) > self.max_artifacts or
                        max(int(total) + size, physical_size) > self.max_total_bytes):
                    connection.rollback()
                    raise ArtifactError("quota_exceeded")
                os.replace(temporary, destination)
                connection.execute(
                    "INSERT INTO artifacts VALUES (?, ?, ?, ?, ?, ?, ?, 0)",
                    (token_hash, artifact_id, extension, size, digest.hexdigest(),
                     created + ttl_seconds, max_downloads),
                )
                connection.commit()
        except Exception:
            temporary.unlink(missing_ok=True)
            destination.unlink(missing_ok=True)
            raise
        return PublishedArtifact(f"/artifacts/{token}", created + ttl_seconds,
                                 max_downloads, size, digest.hexdigest())

    def inspect(self, token: str, *, now: int | None = None) -> PublishedArtifact:
        """Landing page 查詢不扣下載額度；讀取內容僅用於完整性驗證。"""
        if not isinstance(token, str) or not TOKEN_PATTERN.fullmatch(token):
            raise ArtifactNotFound("not_found")
        self._validate_storage()
        token_hash = hashlib.sha256(token.encode("ascii")).hexdigest()
        current = int(time.time()) if now is None else now
        self._recover_reservations(current)
        with closing(sqlite3.connect(self.database, timeout=10)) as connection:
            row = connection.execute(
                "SELECT artifact_id,extension,size,sha256,expires_at,max_downloads,downloads "
                "FROM artifacts WHERE token_hash = ?", (token_hash,),
            ).fetchone()
        if row is None or current >= int(row[4]) or int(row[6]) >= int(row[5]):
            raise ArtifactNotFound("not_found")
        path = self.files / f"{row[0]}{row[1]}"
        if not path.is_file() or _is_reparse(path) or path.stat().st_size != int(row[2]):
            raise ArtifactNotFound("not_found")
        with path.open("rb") as stream:
            body = stream.read(MAX_ARTIFACT_BYTES + 1)
        if len(body) != int(row[2]) or hashlib.sha256(body).hexdigest() != str(row[3]) or _is_reparse(path):
            raise ArtifactNotFound("not_found")
        return PublishedArtifact(f"/artifacts/{token}", int(row[4]), int(row[5]), int(row[2]), str(row[3]))

    def _recover_reservations(self, now: int) -> None:
        with closing(sqlite3.connect(self.database, timeout=10)) as connection:
            connection.execute("DELETE FROM artifact_reservations WHERE expires_at <= ?", (now,))
            connection.commit()

    def prepare_download(self, token: str, *, now: int | None = None) -> ArtifactDownload:
        if not isinstance(token, str) or not TOKEN_PATTERN.fullmatch(token):
            raise ArtifactNotFound("not_found")
        token_hash = hashlib.sha256(token.encode("ascii")).hexdigest()
        current = int(time.time()) if now is None else now
        try:
            self._validate_storage()
        except ArtifactError as exc:
            raise ArtifactNotFound("not_found")
        self._recover_reservations(current)
        # 大檔讀取與 SHA-256 驗證在交易外完成；交易內只做短 CAS。
        with closing(sqlite3.connect(self.database, timeout=10)) as connection:
            connection.row_factory = sqlite3.Row
            row = connection.execute("SELECT * FROM artifacts WHERE token_hash = ?", (token_hash,)).fetchone()
        if row is None or current >= int(row["expires_at"]) or int(row["downloads"]) >= int(row["max_downloads"]):
            raise ArtifactNotFound("not_found")
        artifact_id, extension = str(row["artifact_id"]), str(row["extension"])
        if not re.fullmatch(r"[0-9a-f]{32}", artifact_id) or extension not in ALLOWED_EXTENSIONS:
            raise ArtifactNotFound("not_found")
        path = self.files / f"{artifact_id}{extension}"
        if not path.is_file() or _is_reparse(path):
            raise ArtifactNotFound("not_found")
        with path.open("rb") as stream:
            body = stream.read(MAX_ARTIFACT_BYTES + 1)
        if (len(body) != int(row["size"]) or len(body) > MAX_ARTIFACT_BYTES or
                hashlib.sha256(body).hexdigest() != row["sha256"] or _is_reparse(path)):
            raise ArtifactNotFound("not_found")
        reservation_id = uuid.uuid4().hex
        with closing(sqlite3.connect(self.database, timeout=10, isolation_level=None)) as connection:
            connection.execute("BEGIN IMMEDIATE")
            connection.execute("DELETE FROM artifact_reservations WHERE expires_at <= ?", (current,))
            latest = connection.execute(
                "SELECT artifact_id,extension,size,sha256,expires_at,max_downloads,downloads FROM artifacts "
                "WHERE token_hash = ?", (token_hash,),
            ).fetchone()
            reserved = connection.execute(
                "SELECT COUNT(*) FROM artifact_reservations WHERE token_hash = ?", (token_hash,),
            ).fetchone()[0]
            if (latest is None or current >= int(latest[4]) or int(latest[6]) + int(reserved) >= int(latest[5]) or
                    tuple(latest[:4]) != (artifact_id, extension, len(body), hashlib.sha256(body).hexdigest())):
                connection.rollback()
                raise ArtifactNotFound("not_found")
            connection.execute("INSERT INTO artifact_reservations VALUES (?, ?, ?)",
                               (reservation_id, token_hash, int(latest[4])))
            connection.commit()
        content_type = mimetypes.types_map.get(extension, "application/octet-stream")
        return ArtifactDownload(body, content_type, f"download{extension}", str(row["sha256"]), reservation_id)

    def finalize_download(self, reservation_id: str, *, now: int | None = None) -> None:
        if not re.fullmatch(r"[0-9a-f]{32}", reservation_id):
            raise ArtifactNotFound("not_found")
        self._validate_storage()
        current = int(time.time()) if now is None else now
        stale_path = None
        with closing(sqlite3.connect(self.database, timeout=10, isolation_level=None)) as connection:
            connection.execute("BEGIN IMMEDIATE")
            connection.execute("DELETE FROM artifact_reservations WHERE expires_at <= ?", (current,))
            row = connection.execute(
                "SELECT a.token_hash,a.artifact_id,a.extension,a.downloads,a.max_downloads "
                "FROM artifact_reservations r JOIN artifacts a ON a.token_hash=r.token_hash "
                "WHERE r.reservation_id = ?", (reservation_id,),
            ).fetchone()
            if row is None:
                connection.rollback()
                raise ArtifactNotFound("not_found")
            connection.execute("DELETE FROM artifact_reservations WHERE reservation_id = ?", (reservation_id,))
            downloads = int(row[3]) + 1
            if downloads >= int(row[4]):
                connection.execute("DELETE FROM artifact_reservations WHERE token_hash = ?", (row[0],))
                connection.execute("DELETE FROM artifacts WHERE token_hash = ?", (row[0],))
                stale_path = self.files / f"{row[1]}{row[2]}"
            else:
                connection.execute("UPDATE artifacts SET downloads = ? WHERE token_hash = ?", (downloads, row[0]))
            connection.commit()
        if stale_path is not None:
            try:
                if stale_path.is_file() and not _is_reparse(stale_path):
                    stale_path.unlink()
            except OSError:
                pass

    def release_download(self, reservation_id: str, *, now: int | None = None) -> None:
        if not re.fullmatch(r"[0-9a-f]{32}", reservation_id):
            return
        self._validate_storage()
        current = int(time.time()) if now is None else now
        with closing(sqlite3.connect(self.database, timeout=10)) as connection:
            connection.execute("DELETE FROM artifact_reservations WHERE expires_at <= ?", (current,))
            connection.execute("DELETE FROM artifact_reservations WHERE reservation_id = ?", (reservation_id,))
            connection.commit()

    def consume(self, token: str, *, now: int | None = None) -> ArtifactDownload:
        """Non-HTTP convenience API; a successful return is finalized immediately."""
        result = self.prepare_download(token, now=now)
        self.finalize_download(result.reservation_id, now=now)
        return result


def _main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("publish", choices=("publish",))
    parser.add_argument("--source", required=True)
    parser.add_argument("--ttl-seconds", type=int, default=3600)
    parser.add_argument("--max-downloads", type=int, default=3)
    args = parser.parse_args()
    try:
        result = ArtifactStore().publish(args.source, ttl_seconds=args.ttl_seconds,
                                         max_downloads=args.max_downloads)
    except (ArtifactError, OSError, sqlite3.Error):
        # CLI 不輸出來源路徑、例外內容或本機配置。
        print("artifact_publication_failed", file=__import__("sys").stderr)
        return 2
    import json
    print(json.dumps({"relativeUrl": result.relative_url, "expiresAt": result.expires_at,
                      "maxDownloads": result.max_downloads, "size": result.size,
                      "sha256": result.sha256}, separators=(",", ":")))
    return 0


if __name__ == "__main__":
    raise SystemExit(_main())
