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
        if command_digest(self.argv) != self.command_sha256:
            raise ValueError("runtime command hash is invalid")
        model = canonical_path(self.model_path)
        if not any(canonical_path(argument) == model for argument in self.argv):
            raise ValueError("model path is absent from exact command")

    @classmethod
    def from_dict(cls, value: Mapping[str, object]) -> "RuntimeManifest":
        expected = {
            "node_id", "profile", "owner", "executable_path", "executable_sha256",
            "argv", "command_sha256", "model_path", "model_sha256",
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
        )

    def verify_files(self) -> None:
        try:
            executable_hash = file_sha256(self.executable_path)
            model_hash = file_sha256(self.model_path)
        except OSError as error:
            raise ProcessIdentityError("Qwen runtime file is unavailable") from error
        if executable_hash != self.executable_sha256 or model_hash != self.model_sha256:
            raise ProcessIdentityError("Qwen runtime file hash mismatch")


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
        observed.owner == manifest.owner,
        canonical_path(observed.executable_path) == canonical_path(manifest.executable_path),
        observed.executable_sha256 == manifest.executable_sha256,
        command_digest(observed.argv) == manifest.command_sha256,
        tuple(observed.argv) == manifest.argv,
    )
    if not all(checks):
        raise ProcessIdentityError("process identity validation failed")
