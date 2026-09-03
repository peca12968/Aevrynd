"""Canonical signed job and receipt protocol."""

from __future__ import annotations

import hashlib
import hmac
import json
import re
import sqlite3
import time
import uuid
from contextlib import closing
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Mapping

from .policy import (
    ALLOWED_NODES, ALLOWED_TIMEOUTS, CLASSIFICATIONS, MAX_CLOCK_SKEW_SECONDS,
    MAX_TTL_SECONDS, PRESETS, RECEIPT_STATUSES, SCHEMA_VERSION,
)

_SHA = re.compile(r"^[0-9a-f]{40}$")
_NONCE = re.compile(r"^[A-Za-z0-9_-]{22,64}$")
JOB_FIELDS = frozenset(
    {"schema_version", "job_id", "node_id", "commit_sha", "preset", "test_label",
     "timeout_seconds", "issued_at", "expires_at", "nonce"}
)
RECEIPT_FIELDS = frozenset(
    {"schema_version", "job_id", "node_id", "commit_sha", "preset", "test_label", "status",
     "toolchain", "tests", "durations_ms", "log_sha256", "classification", "created_at", "nonce"}
)


class ProtocolError(ValueError):
    pass


def canonical_bytes(payload: Mapping[str, Any]) -> bytes:
    try:
        return json.dumps(payload, sort_keys=True, separators=(",", ":"), ensure_ascii=True,
                          allow_nan=False).encode("ascii")
    except (TypeError, ValueError) as exc:
        raise ProtocolError("noncanonical_json") from exc


def sign(payload: Mapping[str, Any], key: bytes) -> dict[str, Any]:
    if len(key) < 32:
        raise ProtocolError("key_too_short")
    return {"payload": dict(payload), "signature": hmac.new(key, canonical_bytes(payload), hashlib.sha256).hexdigest()}


def verify_envelope(envelope: Mapping[str, Any], key: bytes) -> dict[str, Any]:
    if (set(envelope) != {"payload", "signature"} or not isinstance(envelope["payload"], dict)
            or not isinstance(envelope["signature"], str)
            or not re.fullmatch(r"[0-9a-f]{64}", envelope["signature"])):
        raise ProtocolError("invalid_envelope")
    expected = hmac.new(key, canonical_bytes(envelope["payload"]), hashlib.sha256).hexdigest()
    if not hmac.compare_digest(expected, envelope["signature"]):
        raise ProtocolError("bad_signature")
    return dict(envelope["payload"])


def validate_job(payload: Mapping[str, Any], expected_node: str, now: int | None = None) -> dict[str, Any]:
    now = int(time.time()) if now is None else now
    if set(payload) != JOB_FIELDS:
        raise ProtocolError("job_schema")
    try:
        parsed_job_id = uuid.UUID(payload["job_id"]) if isinstance(payload["job_id"], str) else None
    except ValueError as exc:
        raise ProtocolError("job_id") from exc
    if parsed_job_id is None or str(parsed_job_id) != payload["job_id"]:
        raise ProtocolError("job_id")
    if type(payload["schema_version"]) is not int or payload["schema_version"] != SCHEMA_VERSION \
            or not isinstance(payload["node_id"], str) or payload["node_id"] != expected_node:
        raise ProtocolError("job_identity")
    if expected_node not in ALLOWED_NODES or not isinstance(payload["commit_sha"], str) \
            or not _SHA.fullmatch(payload["commit_sha"]):
        raise ProtocolError("job_value")
    preset = PRESETS.get(str(payload["preset"]))
    if preset is None or payload["test_label"] != preset.test_label:
        raise ProtocolError("policy_denied")
    if type(payload["timeout_seconds"]) is not int or payload["timeout_seconds"] not in ALLOWED_TIMEOUTS:
        raise ProtocolError("timeout_denied")
    if type(payload["issued_at"]) is not int or type(payload["expires_at"]) is not int:
        raise ProtocolError("ttl_denied")
    issued, expires = payload["issued_at"], payload["expires_at"]
    if expires <= issued or expires - issued > MAX_TTL_SECONDS:
        raise ProtocolError("ttl_denied")
    if now < issued - MAX_CLOCK_SKEW_SECONDS or now > expires:
        raise ProtocolError("expired")
    if not isinstance(payload["nonce"], str) or not _NONCE.fullmatch(payload["nonce"]):
        raise ProtocolError("nonce")
    return dict(payload)


def validate_receipt(payload: Mapping[str, Any], expected_job: Mapping[str, Any]) -> dict[str, Any]:
    if set(payload) != RECEIPT_FIELDS:
        raise ProtocolError("receipt_schema")
    for field in ("schema_version", "job_id", "node_id", "commit_sha", "preset", "test_label", "nonce"):
        if payload[field] != expected_job[field]:
            raise ProtocolError("receipt_mismatch")
    if type(payload["schema_version"]) is not int:
        raise ProtocolError("receipt_mismatch")
    if payload["status"] not in RECEIPT_STATUSES or payload["classification"] not in CLASSIFICATIONS:
        raise ProtocolError("receipt_value")
    if not re.fullmatch(r"[0-9a-f]{64}", str(payload["log_sha256"])):
        raise ProtocolError("receipt_hash")
    if set(payload["tests"]) != {"total", "passed", "failed", "skipped"}:
        raise ProtocolError("receipt_tests")
    if any(type(v) is not int or v < 0 for v in payload["tests"].values()):
        raise ProtocolError("receipt_counts")
    tests = payload["tests"]
    if tests["passed"] + tests["failed"] + tests["skipped"] != tests["total"]:
        raise ProtocolError("receipt_counts")
    if payload["status"] == "passed" and tests["failed"] != 0:
        raise ProtocolError("receipt_counts")
    if set(payload["durations_ms"]) != {"fetch", "configure", "build", "test", "total"}:
        raise ProtocolError("receipt_durations")
    if any(type(v) is not int or v < 0 for v in payload["durations_ms"].values()):
        raise ProtocolError("receipt_duration_values")
    durations = payload["durations_ms"]
    if durations["total"] < sum(durations[k] for k in ("fetch", "configure", "build", "test")):
        raise ProtocolError("receipt_duration_values")
    if set(payload["toolchain"]) != {"git", "cmake", "ctest"}:
        raise ProtocolError("receipt_toolchain")
    # 僅接受版本字串，禁止把路徑、URL 或診斷內容塞入 receipt。
    if any(not isinstance(v, str) or not re.fullmatch(r"[A-Za-z0-9.+_-]{1,32}", v)
           for v in payload["toolchain"].values()):
        raise ProtocolError("receipt_toolchain_value")
    created = payload["created_at"]
    if type(created) is not int or created < expected_job["issued_at"] or \
            created > expected_job["expires_at"] + expected_job["timeout_seconds"] + 300:
        raise ProtocolError("receipt_time")
    return dict(payload)


class ReplayLedger:
    """SQLite 唯一鍵確保跨程序、重啟後仍拒絕 replay。"""

    def __init__(self, path: Path):
        self.path = path
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with closing(sqlite3.connect(path)) as db:
            db.execute("CREATE TABLE IF NOT EXISTS seen (kind TEXT, nonce TEXT, seen_at INTEGER, PRIMARY KEY(kind, nonce))")
            db.commit()

    def claim(self, kind: str, nonce: str, now: int | None = None) -> None:
        if kind not in {"job", "receipt"} or not isinstance(nonce, str) or not _NONCE.fullmatch(nonce):
            raise ProtocolError("ledger_value")
        try:
            with closing(sqlite3.connect(self.path, timeout=10)) as db:
                db.execute("INSERT INTO seen VALUES (?, ?, ?)", (kind, nonce, int(time.time()) if now is None else now))
                db.commit()
        except sqlite3.IntegrityError as exc:
            raise ProtocolError("replay") from exc
