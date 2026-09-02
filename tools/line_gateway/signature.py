"""LINE webhook signature generation and verification."""

from __future__ import annotations

import base64
import binascii
import hashlib
import hmac


def compute_signature(raw_body: bytes, channel_secret: str) -> str:
    if not isinstance(raw_body, bytes):
        raise TypeError("raw_body must be bytes")
    if not isinstance(channel_secret, str) or not channel_secret:
        raise ValueError("channel_secret is unavailable")
    digest = hmac.new(channel_secret.encode("utf-8"), raw_body, hashlib.sha256).digest()
    return base64.b64encode(digest).decode("ascii")


def verify_signature(
    raw_body: bytes, received_signature: str | None, channel_secret: str
) -> bool:
    """Verify strict Base64 HMAC-SHA256 without modifying the request body."""

    if (
        not isinstance(raw_body, bytes)
        or not isinstance(received_signature, str)
        or not received_signature
        or not isinstance(channel_secret, str)
        or not channel_secret
    ):
        return False
    try:
        received_digest = base64.b64decode(
            received_signature.encode("ascii"), validate=True
        )
    except (UnicodeEncodeError, binascii.Error, ValueError):
        return False
    if len(received_digest) != hashlib.sha256().digest_size:
        return False
    expected_digest = hmac.new(
        channel_secret.encode("utf-8"), raw_body, hashlib.sha256
    ).digest()
    return hmac.compare_digest(received_digest, expected_digest)
