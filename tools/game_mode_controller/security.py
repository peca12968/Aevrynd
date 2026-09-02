"""Mutually authenticated request signing and durable replay protection."""

from __future__ import annotations

import base64
import binascii
import hashlib
import hmac
import re
import sqlite3
import threading
import time
from contextlib import closing
from dataclasses import dataclass
from pathlib import Path
from typing import Callable, Mapping

from .inventory import NODE_IDS

_NONCE = re.compile(r"^[A-Za-z0-9_-]{22,64}$")
_PATHS = frozenset({"/v1/status", "/v1/command", "/v1/poll", "/v1/ack"})
_METHODS = frozenset({"GET", "POST"})
_HEADER_KEYS = frozenset({
    "x-aevrynd-node", "x-aevrynd-timestamp", "x-aevrynd-nonce", "x-aevrynd-signature"
})


class AuthenticationError(RuntimeError):
    pass


@dataclass(frozen=True, slots=True)
class AuthenticatedRequest:
    node_id: str
    timestamp: int
    nonce: str


def canonical_message(node_id: str, method: str, path: str, timestamp: int, nonce: str, body: bytes) -> bytes:
    if node_id not in NODE_IDS or method not in _METHODS or path not in _PATHS:
        raise ValueError("request target is not allowlisted")
    if type(timestamp) is not int or not _NONCE.fullmatch(nonce):
        raise ValueError("request metadata is invalid")
    if not isinstance(body, bytes) or len(body) > 262_144:
        raise ValueError("request body is invalid")
    body_hash = hashlib.sha256(body).hexdigest()
    return f"aevrynd-game-mode-v1\n{node_id}\n{method}\n{path}\n{timestamp}\n{nonce}\n{body_hash}".encode("ascii")


def sign_request(secret: str, node_id: str, method: str, path: str, timestamp: int, nonce: str, body: bytes) -> dict[str, str]:
    _validate_secret(secret)
    digest = hmac.new(secret.encode("utf-8"), canonical_message(node_id, method, path, timestamp, nonce, body), hashlib.sha256).digest()
    return {
        "X-Aevrynd-Node": node_id,
        "X-Aevrynd-Timestamp": str(timestamp),
        "X-Aevrynd-Nonce": nonce,
        "X-Aevrynd-Signature": base64.b64encode(digest).decode("ascii"),
    }


def sign_response(secret: str, node_id: str, request_nonce: str, body: bytes) -> str:
    _validate_secret(secret)
    if node_id not in NODE_IDS or not _NONCE.fullmatch(request_nonce) or not isinstance(body, bytes) or len(body) > 262_144:
        raise ValueError("response metadata is invalid")
    message = b"aevrynd-game-mode-response-v1\n" + node_id.encode("ascii") + b"\n" + request_nonce.encode("ascii") + b"\n" + hashlib.sha256(body).hexdigest().encode("ascii")
    return base64.b64encode(hmac.new(secret.encode("utf-8"), message, hashlib.sha256).digest()).decode("ascii")


def verify_response(secret: str, node_id: str, request_nonce: str, body: bytes, signature: str | None) -> bool:
    try:
        expected = sign_response(secret, node_id, request_nonce, body)
        if not isinstance(signature, str):
            return False
        received = base64.b64decode(signature.encode("ascii"), validate=True)
        expected_raw = base64.b64decode(expected.encode("ascii"), validate=True)
        return len(received) == len(expected_raw) and hmac.compare_digest(received, expected_raw)
    except (ValueError, UnicodeError, binascii.Error):
        return False


class ReplayLedger:
    """SQLite unique key makes nonce consumption atomic across local processes."""

    def __init__(self, path: str | Path, *, retention_seconds: int = 600) -> None:
        if not 120 <= retention_seconds <= 86_400:
            raise ValueError("retention_seconds is outside the allowed range")
        self.path = Path(path).resolve()
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.retention_seconds = retention_seconds
        self._lock = threading.Lock()
        with closing(self._connect()) as connection:
            connection.execute("PRAGMA journal_mode=WAL")
            connection.execute("CREATE TABLE IF NOT EXISTS nonce (node_id TEXT NOT NULL, value TEXT NOT NULL, seen_at INTEGER NOT NULL, PRIMARY KEY(node_id,value))")

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.path, timeout=5.0, isolation_level=None)
        connection.execute("PRAGMA busy_timeout=5000")
        return connection

    def consume(self, node_id: str, nonce: str, now: int) -> bool:
        with self._lock, closing(self._connect()) as connection:
            connection.execute("BEGIN IMMEDIATE")
            try:
                connection.execute("DELETE FROM nonce WHERE seen_at < ?", (now - self.retention_seconds,))
                connection.execute("INSERT INTO nonce(node_id,value,seen_at) VALUES(?,?,?)", (node_id, nonce, now))
                connection.execute("COMMIT")
                return True
            except sqlite3.IntegrityError:
                connection.execute("ROLLBACK")
                return False


class RequestAuthenticator:
    def __init__(self, secret_reader: Callable[[str], str | None], ledger: ReplayLedger, *, maximum_skew_seconds: int = 60, clock: Callable[[], float] = time.time) -> None:
        if not 10 <= maximum_skew_seconds <= 300:
            raise ValueError("maximum_skew_seconds is outside the allowed range")
        self._secret_reader = secret_reader
        self._ledger = ledger
        self._skew = maximum_skew_seconds
        self._clock = clock

    def verify(self, method: str, path: str, headers: Mapping[str, str], body: bytes) -> AuthenticatedRequest:
        lowered = {str(key).lower(): str(value) for key, value in headers.items()}
        if not _HEADER_KEYS.issubset(lowered):
            raise AuthenticationError("authentication headers are incomplete")
        node_id = lowered["x-aevrynd-node"]
        nonce = lowered["x-aevrynd-nonce"]
        if node_id not in NODE_IDS or not _NONCE.fullmatch(nonce):
            raise AuthenticationError("request identity is invalid")
        try:
            timestamp = int(lowered["x-aevrynd-timestamp"])
        except ValueError as error:
            raise AuthenticationError("request timestamp is invalid") from error
        now = int(self._clock())
        if abs(now - timestamp) > self._skew:
            raise AuthenticationError("request timestamp is outside the allowed window")
        secret = self._secret_reader(node_id)
        try:
            _validate_secret(secret)
            received = base64.b64decode(lowered["x-aevrynd-signature"].encode("ascii"), validate=True)
        except (ValueError, UnicodeError, binascii.Error) as error:
            raise AuthenticationError("request signature is invalid") from error
        expected = hmac.new(secret.encode("utf-8"), canonical_message(node_id, method, path, timestamp, nonce, body), hashlib.sha256).digest()  # type: ignore[union-attr]
        if len(received) != len(expected) or not hmac.compare_digest(received, expected):
            raise AuthenticationError("request signature is invalid")
        if not self._ledger.consume(node_id, nonce, now):
            raise AuthenticationError("request replay was detected")
        return AuthenticatedRequest(node_id, timestamp, nonce)


def _validate_secret(secret: str | None) -> None:
    if not isinstance(secret, str) or not 32 <= len(secret) <= 16_384 or any(character in secret for character in "\r\n\x00"):
        raise ValueError("node credential is unavailable or invalid")
