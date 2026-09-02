"""Small fail-closed LINE Messaging API client."""

from __future__ import annotations

import json
import socket
import uuid
from dataclasses import dataclass
from typing import Any, Callable, Mapping, Sequence
from urllib import error, request


LINE_API_ROOT = "https://api.line.me/v2/bot/message"
MAX_TEXT_CHARS = 5_000


class LineClientError(RuntimeError):
    retryable = False


class RetryableLineError(LineClientError):
    retryable = True


class PermanentLineError(LineClientError):
    retryable = False


@dataclass(frozen=True, slots=True)
class LineResponse:
    status_code: int
    duplicate: bool = False


class LineClient:
    def __init__(
        self,
        access_token: str,
        timeout: float = 10.0,
        max_bytes: int = 65_536,
        *,
        opener: Callable[..., Any] | None = None,
    ) -> None:
        if not isinstance(access_token, str) or not access_token:
            raise ValueError("access_token must not be empty")
        if timeout <= 0 or timeout > 60:
            raise ValueError("timeout is outside the allowed range")
        if max_bytes < 1_024 or max_bytes > 1_048_576:
            raise ValueError("max_bytes is outside the allowed range")
        self._access_token = access_token
        self._timeout = float(timeout)
        self._max_bytes = int(max_bytes)
        self._opener = request.urlopen if opener is None else opener

    def push(
        self,
        destination_id: str,
        message: str | Sequence[Mapping[str, object]],
        retry_key: str,
    ) -> LineResponse:
        if not destination_id or len(destination_id) > 256:
            raise ValueError("destination_id is invalid")
        try:
            canonical_retry_key = str(uuid.UUID(retry_key))
        except (ValueError, TypeError, AttributeError) as exc:
            raise ValueError("retry_key must be a UUID") from exc
        if canonical_retry_key != retry_key.lower():
            raise ValueError("retry_key must use canonical UUID form")
        payload = {"to": destination_id, "messages": self._messages(message)}
        return self._send("push", payload, retry_key=canonical_retry_key)

    @staticmethod
    def _messages(message: str | Sequence[Mapping[str, object]]) -> list[dict[str, object]]:
        if isinstance(message, str):
            if not message or len(message) > MAX_TEXT_CHARS:
                raise ValueError("message text is outside the allowed range")
            return [{"type": "text", "text": message}]
        if not isinstance(message, Sequence) or isinstance(message, (bytes, bytearray)):
            raise ValueError("messages must be text or a sequence")
        if not 1 <= len(message) <= 5:
            raise ValueError("messages count is outside the allowed range")
        normalized: list[dict[str, object]] = []
        for item in message:
            if not isinstance(item, Mapping):
                raise ValueError("message item must be an object")
            copied = dict(item)
            if copied.get("type") != "text" or not isinstance(copied.get("text"), str):
                raise ValueError("only bounded text messages are supported")
            if not copied["text"] or len(str(copied["text"])) > MAX_TEXT_CHARS:
                raise ValueError("message text is outside the allowed range")
            normalized.append({"type": "text", "text": copied["text"]})
        return normalized

    def _send(
        self, endpoint: str, payload: Mapping[str, object], *, retry_key: str | None
    ) -> LineResponse:
        body = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if len(body) > self._max_bytes:
            raise ValueError("request body exceeds max_bytes")
        headers = {
            "Authorization": f"Bearer {self._access_token}",
            "Content-Type": "application/json",
            "User-Agent": "Aevrynd-Line-Gateway/1",
        }
        if retry_key is not None:
            headers["X-Line-Retry-Key"] = retry_key
        api_request = request.Request(
            f"{LINE_API_ROOT}/{endpoint}", data=body, headers=headers, method="POST"
        )
        try:
            with self._opener(api_request, timeout=self._timeout) as response:
                status = int(response.getcode())
                response_body = response.read(self._max_bytes + 1)
        except error.HTTPError as exc:
            status = int(exc.code)
        except (error.URLError, TimeoutError, socket.timeout, OSError) as exc:
            # 錯誤訊息刻意不包含 request，避免 Authorization header 洩漏。
            raise RetryableLineError("LINE request failed before a response") from exc
        if 200 <= status < 300:
            if len(response_body) > self._max_bytes:
                raise LineClientError("LINE response exceeds max_bytes")
            return LineResponse(status)
        if status == 409:
            return LineResponse(status, duplicate=True)
        if 400 <= status < 500:
            raise PermanentLineError(f"LINE request was rejected with HTTP {status}")
        raise RetryableLineError(f"LINE service returned HTTP {status}")
