"""File-based, schema-checked handoff between the gateway and Codex."""

from __future__ import annotations

import hashlib
import json
import os
import stat
import tempfile
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any


SCHEMA_VERSION = 1
MAX_REQUEST_CHARS = 16_384
MAX_RESULT_CHARS = 32_768
MAX_DOCUMENT_BYTES = 131_072


class CodexHandoffError(RuntimeError):
    pass


class CodexReplayError(CodexHandoffError):
    pass


@dataclass(frozen=True, slots=True)
class HandoffRequest:
    job_id: str
    request_hash: str
    path: Path


@dataclass(frozen=True, slots=True)
class HandoffResult:
    job_id: str
    request_hash: str
    status: str
    result_text: str
    error_code: str


def _canonical_job_id(value: str) -> str:
    try:
        parsed = uuid.UUID(value)
    except (ValueError, TypeError, AttributeError) as exc:
        raise CodexHandoffError("job_id must be a UUID") from exc
    canonical = str(parsed)
    if value.lower() != canonical:
        raise CodexHandoffError("job_id must use canonical UUID form")
    return canonical


def _is_link_or_reparse(path: Path) -> bool:
    try:
        metadata = path.lstat()
    except FileNotFoundError:
        return False
    attributes = getattr(metadata, "st_file_attributes", 0)
    reparse_flag = getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400)
    return stat.S_ISLNK(metadata.st_mode) or bool(attributes & reparse_flag)


class CodexHandoff:
    def __init__(self, runtime_root: str | Path | None = None, *, allow_test_root: bool = False) -> None:
        program_data = Path(os.environ.get("ProgramData", r"C:\ProgramData")).resolve()
        selected = (
            program_data / "Aevrynd" / "LineGateway" / "handoff"
            if runtime_root is None
            else Path(runtime_root).resolve()
        )
        if not allow_test_root:
            try:
                selected.relative_to(program_data)
            except ValueError as exc:
                raise CodexHandoffError("runtime root must be below ProgramData") from exc
        self._root = selected
        self._requests = selected / "requests"
        self._responses = selected / "responses"
        self._consumed = selected / "consumed"
        self._prepare_directories()

    @property
    def root(self) -> Path:
        return self._root

    def _prepare_directories(self) -> None:
        # 每一層在建立前後都拒絕 symlink/junction，避免路徑逸出 runtime root。
        for path in (self._root, self._requests, self._responses, self._consumed):
            if path.exists() and _is_link_or_reparse(path):
                raise CodexHandoffError("handoff path must not be a link or reparse point")
            path.mkdir(parents=True, exist_ok=True)
            if _is_link_or_reparse(path):
                raise CodexHandoffError("handoff path became a link or reparse point")

    def _path(self, directory: Path, job_id: str) -> Path:
        canonical = _canonical_job_id(job_id)
        candidate = (directory / f"{canonical}.json").resolve(strict=False)
        try:
            candidate.relative_to(directory.resolve())
        except ValueError as exc:
            raise CodexHandoffError("handoff path escaped its directory") from exc
        return candidate

    @staticmethod
    def _request_hash(job_id: str, request_text: str) -> str:
        canonical = json.dumps(
            {"schemaVersion": SCHEMA_VERSION, "jobId": job_id, "request": request_text},
            ensure_ascii=False,
            sort_keys=True,
            separators=(",", ":"),
        ).encode("utf-8")
        return hashlib.sha256(canonical).hexdigest()

    def submit(self, job_id: str, request_text: str) -> HandoffRequest:
        canonical_id = _canonical_job_id(job_id)
        if not isinstance(request_text, str) or not request_text or len(request_text) > MAX_REQUEST_CHARS:
            raise CodexHandoffError("request_text is outside the allowed range")
        request_path = self._path(self._requests, canonical_id)
        response_path = self._path(self._responses, canonical_id)
        consumed_path = self._path(self._consumed, canonical_id)
        digest = self._request_hash(canonical_id, request_text)
        if consumed_path.exists():
            raise CodexReplayError("handoff job was already consumed")
        if request_path.exists():
            return self._reconcile_request(request_path, canonical_id, request_text, digest)
        if response_path.exists():
            raise CodexReplayError("handoff response exists without its request")
        document = {
            "schemaVersion": SCHEMA_VERSION,
            "jobId": canonical_id,
            "requestHash": digest,
            "request": request_text,
            "createdAt": int(time.time()),
        }
        try:
            self._atomic_write(request_path, document, replace=False)
        except CodexReplayError:
            # 另一個恢復 worker 可能剛發布相同 request；只接受完全相同內容。
            return self._reconcile_request(request_path, canonical_id, request_text, digest)
        return HandoffRequest(canonical_id, digest, request_path)

    def _reconcile_request(
        self, request_path: Path, job_id: str, request_text: str, request_hash: str
    ) -> HandoffRequest:
        if _is_link_or_reparse(request_path) or not request_path.is_file():
            raise CodexHandoffError("handoff request must be a regular file")
        raw = request_path.read_bytes()
        if len(raw) > MAX_DOCUMENT_BYTES:
            raise CodexHandoffError("handoff request is oversized")
        if _is_link_or_reparse(request_path):
            raise CodexHandoffError("handoff request changed during validation")
        try:
            document = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise CodexHandoffError("handoff request is invalid JSON") from exc
        required = {"schemaVersion", "jobId", "requestHash", "request", "createdAt"}
        if not isinstance(document, dict) or set(document) != required:
            raise CodexReplayError("handoff request schema does not match")
        if (
            document["schemaVersion"] != SCHEMA_VERSION
            or document["jobId"] != job_id
            or document["requestHash"] != request_hash
            or document["request"] != request_text
            or not isinstance(document["createdAt"], int)
        ):
            raise CodexReplayError("handoff request identity does not match")
        return HandoffRequest(job_id, request_hash, request_path)

    def poll(
        self,
        job_id: str,
        expected_request_hash: str,
        *,
        recover_consumed: bool = False,
    ) -> HandoffResult | None:
        canonical_id = _canonical_job_id(job_id)
        if (
            not isinstance(expected_request_hash, str)
            or len(expected_request_hash) != 64
            or any(character not in "0123456789abcdef" for character in expected_request_hash)
        ):
            raise CodexHandoffError("expected_request_hash must be a SHA-256 digest")
        response_path = self._path(self._responses, canonical_id)
        consumed_path = self._path(self._consumed, canonical_id)
        if consumed_path.exists():
            if not recover_consumed:
                raise CodexReplayError("handoff response was already consumed")
            return self._read_response(
                consumed_path, canonical_id, expected_request_hash, "consumed response"
            )
        if not response_path.exists():
            return None
        result = self._read_response(
            response_path, canonical_id, expected_request_hash, "response"
        )
        if consumed_path.exists():
            raise CodexReplayError("handoff response replay was detected")
        try:
            os.replace(response_path, consumed_path)
        except FileNotFoundError as exc:
            if recover_consumed and consumed_path.exists():
                return self._read_response(
                    consumed_path, canonical_id, expected_request_hash, "consumed response"
                )
            raise CodexReplayError("handoff response was consumed concurrently") from exc
        return result

    def _read_response(
        self, path: Path, job_id: str, expected_request_hash: str, label: str
    ) -> HandoffResult:
        if _is_link_or_reparse(path) or not path.is_file():
            raise CodexHandoffError(f"handoff {label} must be a regular file")
        raw = path.read_bytes()
        if len(raw) > MAX_DOCUMENT_BYTES:
            raise CodexHandoffError("handoff response is oversized")
        if _is_link_or_reparse(path):
            raise CodexHandoffError("handoff response changed during validation")
        try:
            document = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise CodexHandoffError("handoff response is invalid JSON") from exc
        return self._validate_response(document, job_id, expected_request_hash)

    @staticmethod
    def _validate_response(
        document: object, job_id: str, expected_request_hash: str
    ) -> HandoffResult:
        if not isinstance(document, dict):
            raise CodexHandoffError("handoff response root must be an object")
        required = {"schemaVersion", "jobId", "requestHash", "status", "result", "errorCode"}
        if set(document) != required:
            raise CodexHandoffError("handoff response fields do not match the schema")
        if document["schemaVersion"] != SCHEMA_VERSION:
            raise CodexHandoffError("handoff response schema version is unsupported")
        if document["jobId"] != job_id or document["requestHash"] != expected_request_hash:
            raise CodexHandoffError("handoff response identity does not match the request")
        status_value = document["status"]
        result_value = document["result"]
        error_value = document["errorCode"]
        if status_value not in {"completed", "failed"}:
            raise CodexHandoffError("handoff response status is invalid")
        if not isinstance(result_value, str) or len(result_value) > MAX_RESULT_CHARS:
            raise CodexHandoffError("handoff response result is invalid")
        if not isinstance(error_value, str) or len(error_value) > 128:
            raise CodexHandoffError("handoff response errorCode is invalid")
        if status_value == "completed" and (not result_value or error_value):
            raise CodexHandoffError("completed handoff response is inconsistent")
        if status_value == "failed" and (result_value or not error_value):
            raise CodexHandoffError("failed handoff response is inconsistent")
        return HandoffResult(job_id, expected_request_hash, status_value, result_value, error_value)

    @staticmethod
    def _atomic_write(path: Path, document: dict[str, Any], *, replace: bool) -> None:
        encoded = json.dumps(
            document, ensure_ascii=False, sort_keys=True, separators=(",", ":")
        ).encode("utf-8")
        if len(encoded) > MAX_DOCUMENT_BYTES:
            raise CodexHandoffError("handoff document is oversized")
        descriptor, temporary_name = tempfile.mkstemp(prefix=path.name + ".", suffix=".tmp", dir=path.parent)
        temporary = Path(temporary_name)
        try:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(encoded)
                stream.flush()
                os.fsync(stream.fileno())
            if replace:
                os.replace(temporary, path)
            else:
                try:
                    # 同磁碟 hard link 會以 fail-if-exists 原子發布完整暫存檔。
                    os.link(temporary, path)
                except FileExistsError as exc:
                    raise CodexReplayError("handoff path already exists") from exc
        finally:
            temporary.unlink(missing_ok=True)
