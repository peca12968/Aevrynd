"""Closed-manifest, crash-recoverable installation transaction."""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import sys
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Mapping

from .atomic_store import AtomicJsonStore, replace_with_retry

COMPONENTS = ("app", "scripts", "manifests")


class InstallError(RuntimeError):
    pass


@dataclass(frozen=True, slots=True)
class SourceEntry:
    source: str
    destination: str
    size: int
    sha256: str


def _hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _safe_relative(value: object) -> str:
    if not isinstance(value, str) or not value or "\\" in value:
        raise InstallError("manifest path is invalid")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts or any(not part for part in path.parts):
        raise InstallError("manifest path escaped its root")
    return value


def load_source_manifest(source_root: Path, manifest_path: Path, expected_manifest_sha256: str) -> tuple[SourceEntry, ...]:
    source_root = source_root.resolve()
    manifest_path = manifest_path.resolve()
    try:
        manifest_bytes = manifest_path.read_bytes()
    except OSError as error:
        raise InstallError("source manifest is unavailable") from error
    if len(manifest_bytes) > 262_144 or len(expected_manifest_sha256) != 64 or hashlib.sha256(manifest_bytes).hexdigest() != expected_manifest_sha256:
        raise InstallError("source manifest hash validation failed")
    try:
        # hash 與 parse 使用同一份 immutable bytes，避免 manifest path 的 check/use 雙讀競爭。
        document = json.loads(manifest_bytes.decode("utf-8"), object_pairs_hook=_unique_object)
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        raise InstallError("source manifest is invalid") from error
    if not isinstance(document, dict) or set(document) != {"schema_version", "files"} or document["schema_version"] != 1 or not isinstance(document["files"], list):
        raise InstallError("source manifest schema is invalid")
    entries: list[SourceEntry] = []
    sources: set[str] = set()
    destinations: set[str] = set()
    for raw in document["files"]:
        if not isinstance(raw, dict) or set(raw) != {"source", "destination", "size", "sha256"}:
            raise InstallError("source manifest entry is invalid")
        source = _safe_relative(raw["source"])
        destination = _safe_relative(raw["destination"])
        if destination.split("/", 1)[0] not in COMPONENTS or source in sources or destination in destinations:
            raise InstallError("source manifest is not a closed unique set")
        size = raw["size"]
        sha256 = raw["sha256"]
        if type(size) is not int or size < 0 or not isinstance(sha256, str) or len(sha256) != 64 or any(c not in "0123456789abcdef" for c in sha256):
            raise InstallError("source manifest metadata is invalid")
        path = source_root / source
        if path.is_symlink() or not path.is_file() or path.stat().st_size != size or _hash(path) != sha256:
            raise InstallError("source file validation failed")
        sources.add(source)
        destinations.add(destination)
        entries.append(SourceEntry(source, destination, size, sha256))
    if not entries or not all(any(entry.destination.startswith(component + "/") for entry in entries) for component in ("app", "scripts")):
        raise InstallError("source manifest omits a required component")
    return tuple(entries)


class InstallTransaction:
    def __init__(self, source_root: str | Path, install_root: str | Path, entries: tuple[SourceEntry, ...]) -> None:
        self.source_root = Path(source_root).resolve()
        self.install_root = Path(install_root).resolve()
        self.entries = entries
        self.staging_root = self.install_root / "staging"
        self.journal_store = AtomicJsonStore(self.install_root / "state" / "install-transaction.json", maximum_bytes=262_144)
        self.install_mutex = AtomicJsonStore(self.install_root / "state" / "install-wide", maximum_bytes=1024, lock_timeout_seconds=5.0)

    def recover(self) -> None:
        with self.install_mutex.exclusive_lock():
            self._recover_unlocked()

    def _recover_unlocked(self) -> None:
        journal = self.journal_store.read()
        if journal is None:
            return
        self._validate_journal(journal)
        transaction_root = self._transaction_path(str(journal["transaction_id"]))
        if journal["status"] == "committed":
            self._safe_remove_tree(transaction_root)
            self.journal_store.path.unlink(missing_ok=True)
            return
        for component in reversed(COMPONENTS):
            record = journal["components"][component]  # type: ignore[index]
            destination = self.install_root / component
            backup = transaction_root / "backup" / component
            if backup.exists():
                self._safe_remove_tree(destination)
                replace_with_retry(backup, destination)
            elif record["had_destination"] is False and record["phase"] in {"installing", "switched"}:
                self._safe_remove_tree(destination)
        self._safe_remove_tree(transaction_root)
        self.journal_store.path.unlink(missing_ok=True)

    def install(self, *, failure_hook: Callable[[str], None] | None = None, defer_commit: bool = False) -> None:
        self.install_root.mkdir(parents=True, exist_ok=True)
        with self.install_mutex.exclusive_lock():
            self._install_unlocked(failure_hook=failure_hook, defer_commit=defer_commit)

    def _install_unlocked(self, *, failure_hook: Callable[[str], None] | None = None, defer_commit: bool = False) -> None:
        self._recover_unlocked()
        transaction_id = uuid.uuid4().hex
        transaction_root = self._transaction_path(transaction_id)
        stage = transaction_root / "stage"
        backup = transaction_root / "backup"
        stage.mkdir(parents=True)
        backup.mkdir()
        if self.install_root.stat().st_dev != stage.stat().st_dev:
            raise InstallError("staging directory is not on the installation volume")
        try:
            for entry in self.entries:
                source = self.source_root / entry.source
                destination = stage / entry.destination
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, destination)
            self._verify_stage(stage)
            installed_manifest = {
                "schema_version": 1,
                "files": [
                    {"path": entry.destination, "size": entry.size, "sha256": entry.sha256}
                    for entry in self.entries if not entry.destination.startswith("manifests/")
                ],
            }
            manifest_file = stage / "manifests" / "installation-manifest.json"
            manifest_file.parent.mkdir(parents=True, exist_ok=True)
            manifest_file.write_text(json.dumps(installed_manifest, sort_keys=True, separators=(",", ":")), encoding="utf-8")
            (stage / "manifests" / "installation-manifest.sha256").write_text(_hash(manifest_file), encoding="ascii")
        except BaseException:
            self._safe_remove_tree(transaction_root)
            raise
        journal: dict[str, object] = {
            "schema_version": 1, "transaction_id": transaction_id, "status": "switching",
            "components": {
                component: {"had_destination": (self.install_root / component).exists(), "phase": "pending"}
                for component in COMPONENTS
            },
        }
        self.journal_store.write(journal)
        try:
            for component in COMPONENTS:
                record = journal["components"][component]  # type: ignore[index]
                record["phase"] = "backing_up"
                self.journal_store.write(journal)
                destination = self.install_root / component
                if destination.exists():
                    replace_with_retry(destination, backup / component)
                record["phase"] = "installing"
                self.journal_store.write(journal)
                replace_with_retry(stage / component, destination)
                record["phase"] = "switched"
                self.journal_store.write(journal)
                if failure_hook is not None:
                    failure_hook(component)
            journal["status"] = "awaiting_finalize" if defer_commit else "committed"
            self.journal_store.write(journal)
        except BaseException:
            self._recover_unlocked()
            raise
        if not defer_commit:
            self._recover_unlocked()

    def finalize(self) -> None:
        with self.install_mutex.exclusive_lock():
            journal = self.journal_store.read()
            if journal is None or journal.get("status") != "awaiting_finalize":
                raise InstallError("no prepared installation is available")
            self._validate_journal(journal)
            journal["status"] = "committed"
            self.journal_store.write(journal)
            self._recover_unlocked()

    def rollback(self) -> None:
        with self.install_mutex.exclusive_lock():
            journal = self.journal_store.read()
            if journal is None:
                return
            self._validate_journal(journal)
            if journal["status"] == "committed":
                raise InstallError("committed installation cannot be rolled back")
            journal["status"] = "switching"
            self.journal_store.write(journal)
            self._recover_unlocked()

    def _verify_stage(self, stage: Path) -> None:
        expected = {entry.destination for entry in self.entries}
        actual = {path.relative_to(stage).as_posix() for path in stage.rglob("*") if path.is_file()}
        if actual != expected:
            raise InstallError("staged file set differs from closed manifest")
        for entry in self.entries:
            path = stage / entry.destination
            if path.is_symlink() or path.stat().st_size != entry.size or _hash(path) != entry.sha256:
                raise InstallError("staged file validation failed")

    def _transaction_path(self, transaction_id: str) -> Path:
        if len(transaction_id) != 32 or any(c not in "0123456789abcdef" for c in transaction_id):
            raise InstallError("transaction identity is invalid")
        path = (self.staging_root / transaction_id).resolve()
        if path.parent != self.staging_root.resolve():
            raise InstallError("transaction path escaped staging root")
        return path

    def _safe_remove_tree(self, path: Path) -> None:
        resolved = path.resolve()
        allowed = {self._transaction_path(resolved.name)} if resolved.parent == self.staging_root.resolve() else set()
        allowed.update((self.install_root / component).resolve() for component in COMPONENTS)
        if resolved not in allowed:
            raise InstallError("cleanup path is outside the exact allowlist")
        if resolved.exists():
            if resolved.is_symlink():
                raise InstallError("cleanup target must not be a link")
            shutil.rmtree(resolved)

    @staticmethod
    def _validate_journal(value: Mapping[str, object]) -> None:
        if set(value) != {"schema_version", "transaction_id", "status", "components"} or value.get("schema_version") != 1 or value.get("status") not in {"switching", "awaiting_finalize", "committed"}:
            raise InstallError("install journal is invalid")
        components = value.get("components")
        if not isinstance(components, dict) or set(components) != set(COMPONENTS):
            raise InstallError("install journal components are invalid")
        for record in components.values():
            if not isinstance(record, dict) or set(record) != {"had_destination", "phase"} or type(record["had_destination"]) is not bool or record["phase"] not in {"pending", "backing_up", "installing", "switched"}:
                raise InstallError("install journal component is invalid")


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def main(argv: list[str]) -> int:
    if len(argv) != 5 or argv[1] not in {"install", "prepare", "finalize", "rollback"}:
        raise InstallError("installer arguments are invalid")
    source_root, install_root, expected_hash = argv[2], argv[3], argv[4]
    root = Path(source_root).resolve()
    entries = load_source_manifest(root, root / "source-manifest.json", expected_hash)
    transaction = InstallTransaction(root, install_root, entries)
    if argv[1] == "install":
        transaction.install()
    elif argv[1] == "prepare":
        transaction.install(defer_commit=True)
    elif argv[1] == "finalize":
        transaction.finalize()
    else:
        transaction.rollback()
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
