"""Power-loss recoverable transaction spanning core and dynamic post-install assets."""

from __future__ import annotations

import hashlib
import os
import sys
import uuid
from pathlib import Path
from typing import Callable

from .atomic_store import AtomicJsonStore, replace_with_retry
from .installer_transaction import InstallError, InstallTransaction, load_source_manifest


class DeploymentTransaction:
    def __init__(self, source_root: str | Path, install_root: str | Path, source_manifest_sha256: str, shortcut_destination: str | Path) -> None:
        if _is_link(Path(install_root)):
            raise InstallError("installation root must not be a reparse point")
        self.source_root = Path(source_root).resolve()
        self.install_root = Path(install_root).resolve()
        self.shortcut_destination = Path(shortcut_destination).resolve()
        if self.shortcut_destination.name != "Aevrynd Game Mode.lnk":
            raise InstallError("desktop shortcut destination is invalid")
        entries = load_source_manifest(self.source_root, self.source_root / "source-manifest.json", source_manifest_sha256)
        self.core = InstallTransaction(self.source_root, self.install_root, entries)
        self.journal = AtomicJsonStore(self.install_root / "transactions" / "deployment-transaction.json", maximum_bytes=262_144)
        self.mutex = AtomicJsonStore(self.install_root / "transactions" / "deployment-wide", maximum_bytes=1024)

    def recover(self) -> None:
        self.install_root.mkdir(parents=True, exist_ok=True)
        with self.mutex.exclusive_lock():
            self._recover_unlocked()

    def prepare(self, node_source: str | Path, runtime_source: str | Path, shortcut_source: str | Path, shortcut_destination: str | Path, *, failure_hook: Callable[[str], None] | None = None) -> None:
        self.install_root.mkdir(parents=True, exist_ok=True)
        with self.mutex.exclusive_lock():
            self._recover_unlocked()
            assets = self._asset_plan(node_source, runtime_source, shortcut_source, shortcut_destination)
            transaction_id = uuid.uuid4().hex
            records = []
            for name, source, destination in assets:
                raw = source.read_bytes()
                records.append({
                    "name": name, "destination": str(destination), "backup": str(destination.with_name(f".{destination.name}.aevrynd-{transaction_id}.bak")),
                    "had_destination": destination.exists(), "phase": "pending", "sha256": hashlib.sha256(raw).hexdigest(),
                })
            document: dict[str, object] = {"schema_version": 1, "transaction_id": transaction_id, "status": "preparing", "assets": records}
            self.journal.write(document)
            try:
                self.core.install(defer_commit=True)
                for (name, source, destination), record in zip(assets, records, strict=True):
                    raw = source.read_bytes()
                    if hashlib.sha256(raw).hexdigest() != record["sha256"]:
                        raise InstallError("post-install source changed during transaction")
                    destination.parent.mkdir(parents=True, exist_ok=True)
                    if _is_link(destination.parent) or _is_link(destination):
                        raise InstallError("post-install destination must not be a reparse point")
                    record["phase"] = "backing_up"; self.journal.write(document)
                    backup = Path(str(record["backup"]))
                    if destination.exists():
                        replace_with_retry(destination, backup)
                    record["phase"] = "installing"; self.journal.write(document)
                    temporary = destination.with_name(f".{destination.name}.aevrynd-{transaction_id}.tmp")
                    with temporary.open("xb") as stream:
                        stream.write(raw); stream.flush(); os.fsync(stream.fileno())
                    replace_with_retry(temporary, destination)
                    record["phase"] = "switched"; self.journal.write(document)
                    if failure_hook is not None:
                        failure_hook(name)
                document["status"] = "prepared"; self.journal.write(document)
            except BaseException:
                self._recover_unlocked()
                raise

    def commit(self) -> None:
        with self.mutex.exclusive_lock():
            document = self.journal.read()
            if document is None or document.get("status") != "prepared":
                raise InstallError("no prepared deployment is available")
            self._validate(document)
            document["status"] = "committing"; self.journal.write(document)
            self._finish_commit(document)

    def rollback(self) -> None:
        with self.mutex.exclusive_lock():
            document = self.journal.read()
            if document is not None and document.get("status") == "committing":
                raise InstallError("committing deployment must finish forward")
            self._recover_unlocked()

    def _recover_unlocked(self) -> None:
        document = self.journal.read()
        if document is None:
            self.core.recover()
            return
        self._validate(document)
        if document["status"] == "committing":
            self._finish_commit(document)
            return
        for record in reversed(document["assets"]):  # type: ignore[index]
            destination = Path(str(record["destination"])); backup = Path(str(record["backup"]))
            if backup.exists():
                destination.unlink(missing_ok=True); replace_with_retry(backup, destination)
            elif record["had_destination"] is False and record["phase"] in {"installing", "switched"}:
                destination.unlink(missing_ok=True)
        self.core.rollback()
        self.journal.path.unlink(missing_ok=True)

    def _finish_commit(self, document: dict[str, object]) -> None:
        core_journal = self.core.journal_store.read()
        if core_journal is not None:
            if core_journal.get("status") == "awaiting_finalize":
                self.core.finalize()
            elif core_journal.get("status") == "committed":
                self.core.recover()
            else:
                raise InstallError("core transaction cannot commit forward")
        for record in document["assets"]:  # type: ignore[index]
            destination = Path(str(record["destination"]))
            if not destination.is_file() or destination.is_symlink() or _hash(destination) != record["sha256"]:
                raise InstallError("committed post-install asset validation failed")
            Path(str(record["backup"])).unlink(missing_ok=True)
        self.journal.path.unlink(missing_ok=True)

    def _asset_plan(self, node_source: str | Path, runtime_source: str | Path, shortcut_source: str | Path, shortcut_destination: str | Path):  # type: ignore[no-untyped-def]
        supplied_sources = (Path(node_source), Path(runtime_source), Path(shortcut_source))
        if any(_is_link(source) for source in supplied_sources):
            raise InstallError("post-install source must not be a reparse point")
        node, runtime, shortcut = (source.resolve() for source in supplied_sources)
        desktop = Path(shortcut_destination).resolve()
        expected = (
            ("node", node, self.install_root / "config" / "node.json"),
            ("runtime", runtime, self.install_root / "manifests" / "runtime-manifest.json"),
            ("shortcut", shortcut, desktop),
        )
        if desktop != self.shortcut_destination or any(source.is_symlink() or not source.is_file() for _, source, _ in expected):
            raise InstallError("post-install asset plan is invalid")
        return expected

    def _validate(self, value: dict[str, object]) -> None:
        if set(value) != {"schema_version", "transaction_id", "status", "assets"} or value.get("schema_version") != 1 or value.get("status") not in {"preparing", "prepared", "committing"}:
            raise InstallError("deployment journal is invalid")
        records = value.get("assets")
        transaction_id = value.get("transaction_id")
        if not isinstance(transaction_id, str) or len(transaction_id) != 32 or any(character not in "0123456789abcdef" for character in transaction_id):
            raise InstallError("deployment transaction identity is invalid")
        if not isinstance(records, list) or [item.get("name") for item in records if isinstance(item, dict)] != ["node", "runtime", "shortcut"]:
            raise InstallError("deployment journal assets are invalid")
        for item in records:
            if not isinstance(item, dict) or set(item) != {"name", "destination", "backup", "had_destination", "phase", "sha256"} or type(item["had_destination"]) is not bool or item["phase"] not in {"pending", "backing_up", "installing", "switched"}:
                raise InstallError("deployment journal asset record is invalid")
            destination = Path(str(item["destination"])).resolve()
            allowed = {self.install_root / "config" / "node.json", self.install_root / "manifests" / "runtime-manifest.json", self.shortcut_destination}
            expected_backup = destination.with_name(f".{destination.name}.aevrynd-{transaction_id}.bak")
            if destination not in {path.resolve() for path in allowed} or Path(str(item["backup"])).resolve() != expected_backup.resolve() or not isinstance(item["sha256"], str) or len(item["sha256"]) != 64 or any(character not in "0123456789abcdef" for character in item["sha256"]):
                raise InstallError("deployment journal destination escaped allowlist")


def _hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _is_link(path: Path) -> bool:
    return path.is_symlink() or bool(getattr(path, "is_junction", lambda: False)())


def main(argv: list[str]) -> int:
    if len(argv) < 6 or argv[1] not in {"recover", "prepare", "commit", "rollback"}:
        raise InstallError("deployment transaction arguments are invalid")
    shortcut_destination = argv[8] if argv[1] == "prepare" and len(argv) == 9 else argv[5]
    transaction = DeploymentTransaction(argv[2], argv[3], argv[4], shortcut_destination)
    if argv[1] == "recover": transaction.recover()
    elif argv[1] == "commit": transaction.commit()
    elif argv[1] == "rollback": transaction.rollback()
    elif len(argv) == 9: transaction.prepare(argv[5], argv[6], argv[7], argv[8])
    else: raise InstallError("deployment prepare arguments are invalid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
