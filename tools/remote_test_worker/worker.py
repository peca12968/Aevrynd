"""Restricted worker orchestration with an injectable executor."""

from __future__ import annotations

import hashlib
import os
import re
import time
from typing import Any, Callable, Protocol

from .policy import CLASSIFICATIONS, MAX_LOG_BYTES, PRESETS, SCHEMA_VERSION, SUMMARY_LOG_BYTES
from .protocol import ProtocolError, ReplayLedger, sign, validate_job, verify_envelope


class Executor(Protocol):
    def run(self, commit_sha: str, preset: str, timeout_seconds: int) -> dict[str, Any]: ...


class SingleJobLock:
    """跨程序排他檔案；同一節點同時只允許一個工作。"""

    def __init__(self, path):
        self.path = path
        self.fd = None

    def __enter__(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        try:
            self.fd = os.open(self.path, os.O_CREAT | os.O_EXCL | os.O_WRONLY)
        except FileExistsError as exc:
            raise RuntimeError("worker_busy") from exc
        os.write(self.fd, str(os.getpid()).encode("ascii"))
        return self

    def __exit__(self, *_):
        if self.fd is not None:
            os.close(self.fd)
            self.fd = None
        try:
            self.path.unlink()
        except FileNotFoundError:
            pass


_SECRET = re.compile(
    r"(?i)\b(?:authorization\s*[:=]\s*(?:bearer\s+)?|bearer\s+|"
    r"(?:token|password|secret|api[_-]?key)\s*[:=]\s*)\S+"
)
_PATH = re.compile(r"(?i)(?:[a-z]:\\|\\\\|/(?:home|Users|tmp|var|etc)/)[^\s\"']+")


def redact_log(raw: str) -> str:
    bounded = raw.encode("utf-8", "replace")[:MAX_LOG_BYTES].decode("utf-8", "replace")
    return _PATH.sub("[PATH]", _SECRET.sub("[REDACTED]", bounded))


def process_job(envelope: dict[str, Any], node_id: str, key: bytes, ledger: ReplayLedger,
                executor: Executor, classifier: Callable[[str], str] | None = None,
                now: int | None = None) -> dict[str, Any]:
    started = int(time.time()) if now is None else now
    payload = validate_job(verify_envelope(envelope, key), node_id, started)
    ledger.claim("job", payload["nonce"], started)
    result = executor.run(payload["commit_sha"], payload["preset"], payload["timeout_seconds"])
    if not isinstance(result, dict) or set(result) != {"status", "log", "toolchain", "tests", "durations_ms"}:
        raise ProtocolError("executor_result_schema")
    safe_log = redact_log(str(result.get("log", "")))
    default_class = {
        "passed": "clean", "test_failed": "test_failure", "build_failed": "build_failure",
        "timeout": "timeout", "infrastructure_error": "infrastructure_error",
    }[result["status"]]
    classification = default_class
    if classifier is not None:
        # Qwen 只能看到截斷且去敏摘要，輸出也只能是固定 enum，不參與任何命令或成功判定。
        candidate = classifier(safe_log[-SUMMARY_LOG_BYTES:])
        if candidate in CLASSIFICATIONS:
            classification = candidate
    receipt = {
        "schema_version": SCHEMA_VERSION,
        "job_id": payload["job_id"], "node_id": node_id,
        "commit_sha": payload["commit_sha"], "preset": payload["preset"],
        "test_label": PRESETS[payload["preset"]].test_label,
        "status": result["status"], "toolchain": result["toolchain"],
        "tests": result["tests"], "durations_ms": result["durations_ms"],
        "log_sha256": hashlib.sha256(safe_log.encode()).hexdigest(),
        "classification": classification,
        "created_at": started + int(result["durations_ms"]["total"] / 1000),
        "nonce": payload["nonce"],
    }
    from .protocol import validate_receipt
    validate_receipt(receipt, payload)
    return sign(receipt, key)
