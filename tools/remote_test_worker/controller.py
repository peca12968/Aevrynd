"""Controller-side job creation and receipt verification."""

import secrets
import time
import uuid
from typing import Any

from .policy import MAX_TTL_SECONDS, PRESETS, SCHEMA_VERSION
from .protocol import ReplayLedger, sign, validate_receipt, verify_envelope


def create_job(node_id: str, commit_sha: str, preset: str, timeout_seconds: int, key: bytes,
               now: int | None = None, ttl_seconds: int = MAX_TTL_SECONDS) -> dict[str, Any]:
    issued = int(time.time()) if now is None else now
    definition = PRESETS[preset]
    payload = {
        "schema_version": SCHEMA_VERSION,
        "job_id": str(uuid.uuid4()),
        "node_id": node_id,
        "commit_sha": commit_sha,
        "preset": preset,
        "test_label": definition.test_label,
        "timeout_seconds": timeout_seconds,
        "issued_at": issued,
        "expires_at": issued + ttl_seconds,
        "nonce": secrets.token_urlsafe(24),
    }
    # 產生端也走相同限制，避免簽出 worker 必然拒絕的工作。
    from .protocol import validate_job
    validate_job(payload, node_id, issued)
    return sign(payload, key)


def verify_receipt(envelope: dict[str, Any], expected_job: dict[str, Any], key: bytes,
                   ledger: ReplayLedger) -> dict[str, Any]:
    payload = verify_envelope(envelope, key)
    checked = validate_receipt(payload, expected_job["payload"])
    ledger.claim("receipt", checked["nonce"])
    return checked
