"""Exact Qwen runtime identity validation before start or stop."""

from __future__ import annotations

import hashlib
import json
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping, Sequence

from .inventory import get_node

_SHA256 = re.compile(r"^[0-9a-f]{64}$")
_RUNTIME_MANIFEST_MAXIMUM_BYTES = 65_536


class ProcessIdentityError(RuntimeError):
    pass


def command_digest(argv: Sequence[str]) -> str:
    if not argv or any(not isinstance(item, str) or not item or "\x00" in item for item in argv):
        raise ValueError("command arguments are invalid")
    canonical = json.dumps(list(argv), ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def file_sha256(path: str | Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def canonical_path(value: str | Path) -> str:
    return os.path.normcase(str(Path(value).resolve()))


@dataclass(frozen=True, slots=True)
class RuntimeManifest:
    node_id: str
    profile: str
    owner: str
    executable_path: str
    executable_sha256: str
    argv: tuple[str, ...]
    command_sha256: str
    model_path: str
    model_sha256: str
    model_id: str = ""

    def __post_init__(self) -> None:
        spec = get_node(self.node_id)
        if self.profile != spec.qwen_profile:
            raise ValueError("runtime profile differs from fixed node inventory")
        if not isinstance(self.owner, str) or not 2 <= len(self.owner) <= 256 or any(c in self.owner for c in "\r\n\x00"):
            raise ValueError("runtime owner is invalid")
        if canonical_path(self.executable_path) != canonical_path(self.argv[0]):
            raise ValueError("argv executable differs from manifest")
        if not _SHA256.fullmatch(self.executable_sha256) or not _SHA256.fullmatch(self.model_sha256):
            raise ValueError("runtime file hash is invalid")
        if self.model_id and (len(self.model_id) > 512 or any(character in self.model_id for character in "\r\n\x00")):
            raise ValueError("runtime model ID is invalid")
        if command_digest(self.argv) != self.command_sha256:
            raise ValueError("runtime command hash is invalid")
        model = canonical_path(self.model_path)
        if not any(canonical_path(argument) == model for argument in self.argv):
            raise ValueError("model path is absent from exact command")
        if not Path(self.executable_path).is_absolute() or not Path(self.model_path).is_absolute():
            raise ValueError("runtime paths must be absolute")

    def _validate_network_contract(self) -> None:
        """Keep the managed main runtime on its one approved loopback endpoint."""

        if self.node_id != "main":
            return
        values: dict[str, list[str]] = {}
        index = 1
        while index < len(self.argv):
            argument = self.argv[index]
            if argument.startswith("--") and index + 1 < len(self.argv):
                values.setdefault(argument, []).append(self.argv[index + 1])
                index += 2
            else:
                index += 1
        required = {"--model": self.model_path, "--host": "127.0.0.1", "--port": "18080", "--parallel": "1"}
        if any(values.get(option) != [expected] for option, expected in required.items()):
            raise ValueError("main runtime endpoint contract is invalid")

    @classmethod
    def from_dict(cls, value: Mapping[str, object]) -> "RuntimeManifest":
        expected = {
            "node_id", "profile", "owner", "executable_path", "executable_sha256",
            "argv", "command_sha256", "model_path", "model_sha256",
            "model_id",
        }
        if (
            set(value) != expected
            or not isinstance(value.get("argv"), list)
            or not value["argv"]
            or any(not isinstance(item, str) for item in value["argv"])
        ):
            raise ValueError("runtime manifest schema is invalid")
        return cls(
            node_id=str(value["node_id"]), profile=str(value["profile"]),
            owner=str(value["owner"]), executable_path=str(value["executable_path"]),
            executable_sha256=str(value["executable_sha256"]),
            argv=tuple(value["argv"]),  # type: ignore[arg-type]
            command_sha256=str(value["command_sha256"]), model_path=str(value["model_path"]),
            model_sha256=str(value["model_sha256"]),
            model_id=str(value["model_id"]),
        )

    def verify_files(self) -> None:
        for value in (self.executable_path, self.model_path):
            path = Path(value)
            if path.is_symlink() or not path.is_file():
                raise ProcessIdentityError("Qwen runtime file is unavailable")
        try:
            executable_hash = file_sha256(self.executable_path)
            model_hash = file_sha256(self.model_path)
        except OSError as error:
            raise ProcessIdentityError("Qwen runtime file is unavailable") from error
        if executable_hash != self.executable_sha256 or model_hash != self.model_sha256:
            raise ProcessIdentityError("Qwen runtime file hash mismatch")


def load_runtime_manifest(path: str | Path, *, expected_node_id: str, expected_sha256: str) -> RuntimeManifest:
    """Load one immutable, pinned manifest read and verify its closed schema and files."""

    supplied_path = Path(path)
    manifest_path = supplied_path.resolve()
    if (
        not manifest_path.is_absolute()
        or not _SHA256.fullmatch(expected_sha256)
        or supplied_path.is_symlink()
    ):
        raise ProcessIdentityError("runtime manifest identity is invalid")
    try:
        raw = manifest_path.read_bytes()
    except OSError as error:
        raise ProcessIdentityError("runtime manifest is unavailable") from error
    if not 2 <= len(raw) <= _RUNTIME_MANIFEST_MAXIMUM_BYTES or hashlib.sha256(raw).hexdigest() != expected_sha256:
        raise ProcessIdentityError("runtime manifest hash mismatch")
    try:
        document = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object)
    except (UnicodeError, json.JSONDecodeError, ValueError) as error:
        raise ProcessIdentityError("runtime manifest JSON is invalid") from error
    if not isinstance(document, dict) or set(document) != {"schema_version", "runtime"} or document.get("schema_version") != 1 or not isinstance(document.get("runtime"), dict):
        raise ProcessIdentityError("runtime manifest schema is invalid")
    try:
        manifest = RuntimeManifest.from_dict(document["runtime"])
    except (TypeError, ValueError) as error:
        raise ProcessIdentityError("runtime manifest content is invalid") from error
    if manifest.node_id != expected_node_id:
        raise ProcessIdentityError("runtime manifest node identity mismatch")
    if not manifest.model_id:
        raise ProcessIdentityError("runtime manifest model ID is missing")
    try:
        manifest._validate_network_contract()
    except ValueError as error:
        raise ProcessIdentityError("runtime manifest endpoint contract is invalid") from error
    manifest.verify_files()
    return manifest


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


@dataclass(frozen=True, slots=True)
class ObservedProcess:
    pid: int
    owner: str
    executable_path: str
    argv: tuple[str, ...]
    executable_sha256: str


def validate_stop_eligibility(*, persisted_pid: int, observed: ObservedProcess, manifest: RuntimeManifest) -> None:
    """Single mandatory stop gate: persisted PID, files and observed identity must all match."""

    if type(persisted_pid) is not int or persisted_pid <= 0 or type(observed.pid) is not int or observed.pid <= 0:
        raise ProcessIdentityError("Qwen PID is invalid")
    if observed.pid != persisted_pid:
        raise ProcessIdentityError("Qwen PID differs from persisted owner record")
    manifest.verify_files()
    checks = (
        observed.owner.casefold() == manifest.owner.casefold(),
        canonical_path(observed.executable_path) == canonical_path(manifest.executable_path),
        observed.executable_sha256 == manifest.executable_sha256,
        command_digest(observed.argv) == manifest.command_sha256,
        tuple(observed.argv) == manifest.argv,
    )
    if not all(checks):
        raise ProcessIdentityError("process identity validation failed")
