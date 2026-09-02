"""Redirect-proof LINE notification delivery owned only by the main hub."""

from __future__ import annotations

import json
import socket
import time
import uuid
from datetime import datetime, timezone
from typing import Any, Callable
from urllib import error as urlerror, request
from urllib.parse import urlsplit

from tools.line_gateway.line_client import PermanentLineError, RetryableLineError

from .coordinator import HubCoordinator

LINE_PUSH_URL = "https://api.line.me/v2/bot/message/push"


class _NoRedirectHandler(request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[no-untyped-def]
        return None


class NoRedirectLineClient:
    """Bounded text-only LINE push client whose transport can never follow 3xx."""

    def __init__(self, access_token: str, *, timeout: float = 10.0, max_bytes: int = 65_536, opener: Callable[..., Any] | None = None, api_root: str = LINE_PUSH_URL, allow_test_root: bool = False) -> None:
        if not isinstance(access_token, str) or not access_token or any(c in access_token for c in "\r\n\x00"):
            raise ValueError("LINE access token is invalid")
        if not 0.5 <= timeout <= 30.0 or not 1024 <= max_bytes <= 1_048_576:
            raise ValueError("LINE transport bounds are invalid")
        if api_root != LINE_PUSH_URL:
            parsed = urlsplit(api_root)
            if not allow_test_root or parsed.scheme != "http" or parsed.hostname != "127.0.0.1" or parsed.username or parsed.password or parsed.query or parsed.fragment:
                raise ValueError("LINE API root is not allowlisted")
        if opener is not None and not allow_test_root:
            raise ValueError("custom LINE transport is test-only")
        self._token = access_token
        self._timeout = timeout
        self._max_bytes = max_bytes
        self._api_root = api_root
        self._opener = request.build_opener(_NoRedirectHandler()).open if opener is None else opener

    def push(self, destination_id: str, message: str, retry_key: str) -> None:
        if not destination_id or len(destination_id) > 256 or not message or len(message) > 5000:
            raise ValueError("LINE push content is invalid")
        try:
            canonical_retry_key = str(uuid.UUID(retry_key))
        except (ValueError, TypeError, AttributeError) as exc:
            raise ValueError("LINE retry key is invalid") from exc
        if canonical_retry_key != retry_key.lower():
            raise ValueError("LINE retry key is not canonical")
        body = json.dumps({"to": destination_id, "messages": [{"type": "text", "text": message}]}, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
        if len(body) > self._max_bytes:
            raise ValueError("LINE request is too large")
        req = request.Request(self._api_root, data=body, method="POST", headers={
            "Authorization": f"Bearer {self._token}", "Content-Type": "application/json",
            "User-Agent": "Aevrynd-GameModeController/1", "X-Line-Retry-Key": canonical_retry_key,
        })
        try:
            with self._opener(req, timeout=self._timeout) as response:
                status = int(response.getcode())
                response_body = response.read(self._max_bytes + 1)
        except urlerror.HTTPError as exc:
            status = int(exc.code)
            response_body = b""
            exc.close()
        except (urlerror.URLError, TimeoutError, socket.timeout, OSError) as exc:
            raise RetryableLineError("LINE request failed before a response") from exc
        if 200 <= status < 300 or status == 409:
            if len(response_body) > self._max_bytes:
                raise RetryableLineError("LINE response is too large")
            return
        if 300 <= status < 400:
            raise PermanentLineError("LINE redirect is permanently forbidden")
        if 400 <= status < 500:
            raise PermanentLineError(f"LINE request was rejected with HTTP {status}")
        raise RetryableLineError(f"LINE service returned HTTP {status}")


class LineNotificationDrainer:
    def __init__(self, coordinator: HubCoordinator, line_client: NoRedirectLineClient, destination_id: str, *, clock: Callable[[], float] = time.time, maximum_pending_seconds: int = 86_400) -> None:
        if not isinstance(line_client, NoRedirectLineClient):
            raise TypeError("redirect-proof LINE client is required")
        if not destination_id or len(destination_id) > 256:
            raise ValueError("LINE destination is invalid")
        if maximum_pending_seconds != 86_400:
            raise ValueError("LINE pending expiry is fixed at 24 hours")
        self.coordinator = coordinator
        self.line_client = line_client
        self.destination_id = destination_id
        self.clock = clock
        self.maximum_pending_seconds = maximum_pending_seconds

    def drain_one(self) -> bool:
        pending = self.coordinator.pending_notifications()
        if not pending:
            return False
        item = pending[0]
        created = datetime.fromisoformat(item.created_utc.replace("Z", "+00:00"))
        age = self.clock() - created.astimezone(timezone.utc).timestamp()
        if age > self.maximum_pending_seconds:
            # 過期事件需人工處理；不可再使用原 retry key 傳送可能已失去時效的通知。
            self.coordinator.mark_notification_dead_letter(item.notification_id, "line_delivery_expired")
            return True
        try:
            self.line_client.push(self.destination_id, item.text, item.notification_id)
        except PermanentLineError:
            self.coordinator.mark_notification_dead_letter(item.notification_id, "line_permanent_rejection")
            return True
        self.coordinator.mark_notification_delivered(item.notification_id)
        return True
