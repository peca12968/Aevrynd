"""Bounded HTTP ingress for the Aevrynd LINE gateway."""

from __future__ import annotations

import json
import socket
import time
from collections.abc import Callable, Mapping
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

from .config import GatewayConfig
from .signature import verify_signature


EventSink = Callable[[Mapping[str, object]], bool]
HealthProvider = Callable[[], Mapping[str, object]]


class _DuplicateJsonKey(ValueError):
    pass


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise _DuplicateJsonKey("Duplicate JSON key.")
        result[key] = value
    return result


def _reject_json_constant(_value: str) -> None:
    raise ValueError("Non-finite JSON number.")


class GatewayHttpServer(ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = False

    def __init__(
        self,
        server_address: tuple[str, int],
        config: GatewayConfig,
        event_sink: EventSink,
        health_provider: HealthProvider | None = None,
    ) -> None:
        self.gateway_config = config
        self.event_sink = event_sink
        self.health_provider = health_provider
        super().__init__(server_address, GatewayRequestHandler)

    def handle_error(self, _request: object, _client_address: object) -> None:
        # 核心入口不把例外或請求內容輸出到 stderr；上層監控只記固定狀態碼。
        return


class GatewayRequestHandler(BaseHTTPRequestHandler):
    server: GatewayHttpServer
    protocol_version = "HTTP/1.1"
    server_version = "AevryndLineGateway/1"
    sys_version = ""

    def setup(self) -> None:
        super().setup()
        self.connection.settimeout(self.server.gateway_config.request_timeout_seconds)

    def log_message(self, _format: str, *_arguments: object) -> None:
        # 請求路徑、header 與 body 可能帶有敏感資訊，核心層預設完全不記錄。
        return

    def version_string(self) -> str:
        return self.server_version

    def _write_json(self, status: HTTPStatus, payload: Mapping[str, object]) -> None:
        body = json.dumps(payload, separators=(",", ":"), ensure_ascii=True).encode(
            "ascii"
        )
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def _method_not_allowed(self) -> None:
        self.close_connection = True
        self.send_response(HTTPStatus.METHOD_NOT_ALLOWED)
        self.send_header("Allow", "GET, POST")
        self.send_header("Content-Length", "0")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Connection", "close")
        self.end_headers()

    def do_GET(self) -> None:
        # 一律關閉 HTTP/1.1 連線，避免帶 body 的 GET 或 pipeline 造成 request desync。
        self.close_connection = True
        if self.path != "/health":
            self._write_json(HTTPStatus.NOT_FOUND, {"status": "not-found"})
            return
        health = {"status": "ok"}
        if self.server.health_provider is not None:
            try:
                supplied = self.server.health_provider()
            except Exception:
                self._write_json(HTTPStatus.SERVICE_UNAVAILABLE, {"status": "degraded"})
                return
            if not isinstance(supplied, Mapping):
                self._write_json(HTTPStatus.SERVICE_UNAVAILABLE, {"status": "degraded"})
                return
            health.update(supplied)
        status = HTTPStatus.OK if health.get("status") == "ok" else HTTPStatus.SERVICE_UNAVAILABLE
        self._write_json(status, health)

    def do_POST(self) -> None:
        # 早期拒絕不會讀完整 body；關閉連線可防止殘留 bytes 被解析成下一個 request。
        self.close_connection = True
        if self.path != "/webhook":
            self._write_json(HTTPStatus.NOT_FOUND, {"status": "not-found"})
            return
        self._handle_webhook()

    def do_HEAD(self) -> None:
        self._method_not_allowed()

    def do_OPTIONS(self) -> None:
        self._method_not_allowed()

    def do_PUT(self) -> None:
        self._method_not_allowed()

    def do_PATCH(self) -> None:
        self._method_not_allowed()

    def do_DELETE(self) -> None:
        self._method_not_allowed()

    def _handle_webhook(self) -> None:
        config = self.server.gateway_config
        if self.headers.get_all("Transfer-Encoding"):
            self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-request"})
            return
        if self.headers.get_content_type().lower() != "application/json":
            self._write_json(
                HTTPStatus.UNSUPPORTED_MEDIA_TYPE, {"status": "invalid-content-type"}
            )
            return
        charset = self.headers.get_content_charset()
        if charset is not None and charset.lower().replace("_", "-") != "utf-8":
            self._write_json(
                HTTPStatus.UNSUPPORTED_MEDIA_TYPE, {"status": "invalid-content-type"}
            )
            return
        content_length_headers = self.headers.get_all("Content-Length", [])
        if len(content_length_headers) != 1:
            self._write_json(HTTPStatus.LENGTH_REQUIRED, {"status": "length-required"})
            return
        content_length_value = content_length_headers[0]
        if (
            len(content_length_value) > 10
            or not content_length_value.isascii()
            or not content_length_value.isdecimal()
        ):
            self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-request"})
            return
        content_length = int(content_length_value)
        if content_length < 1 or content_length > config.max_body_bytes:
            self._write_json(
                HTTPStatus.REQUEST_ENTITY_TOO_LARGE, {"status": "body-too-large"}
            )
            return
        try:
            raw_body = self.rfile.read(content_length)
        except (OSError, socket.timeout):
            self.close_connection = True
            self._write_json(HTTPStatus.REQUEST_TIMEOUT, {"status": "request-timeout"})
            return
        if len(raw_body) != content_length:
            self.close_connection = True
            self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-request"})
            return
        if not verify_signature(
            raw_body,
            self.headers.get("x-line-signature"),
            config.channel_secret,
        ):
            self._write_json(HTTPStatus.UNAUTHORIZED, {"status": "invalid-signature"})
            return

        try:
            payload = json.loads(
                raw_body.decode("utf-8"),
                object_pairs_hook=_reject_duplicate_keys,
                parse_constant=_reject_json_constant,
            )
        except (UnicodeDecodeError, json.JSONDecodeError, ValueError):
            self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-json"})
            return
        if not isinstance(payload, dict) or set(payload) - {"destination", "events"}:
            self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-payload"})
            return
        events = payload.get("events")
        if not isinstance(events, list) or len(events) > config.max_events_per_webhook:
            self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-events"})
            return
        if not events:
            self._write_json(HTTPStatus.OK, {"status": "ok"})
            return
        for event in events:
            if not isinstance(event, dict):
                self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-event"})
                return
            event_id = event.get("webhookEventId")
            delivery_context = event.get("deliveryContext")
            if (
                not isinstance(event_id, str)
                or not event_id
                or len(event_id) > 256
                or not isinstance(delivery_context, dict)
                or not isinstance(delivery_context.get("isRedelivery"), bool)
            ):
                self._write_json(HTTPStatus.BAD_REQUEST, {"status": "invalid-event"})
                return
        enqueue_deadline = time.monotonic() + config.request_timeout_seconds
        try:
            for event in events:
                if time.monotonic() >= enqueue_deadline:
                    self._write_json(
                        HTTPStatus.SERVICE_UNAVAILABLE,
                        {"status": "enqueue-timeout"},
                    )
                    return
                if self.server.event_sink(event) is not True:
                    self._write_json(
                        HTTPStatus.SERVICE_UNAVAILABLE, {"status": "enqueue-failed"}
                    )
                    return
                if time.monotonic() >= enqueue_deadline:
                    self._write_json(
                        HTTPStatus.SERVICE_UNAVAILABLE,
                        {"status": "enqueue-timeout"},
                    )
                    return
        except Exception:
            self._write_json(
                HTTPStatus.SERVICE_UNAVAILABLE, {"status": "enqueue-failed"}
            )
            return
        self._write_json(HTTPStatus.OK, {"status": "accepted"})


def create_server(
    config: GatewayConfig,
    event_sink: EventSink,
    health_provider: HealthProvider | None = None,
) -> GatewayHttpServer:
    """Create a loopback-only server; caller controls its worker and lifetime."""

    if not callable(event_sink):
        raise TypeError("event_sink must be callable")
    return GatewayHttpServer(
        (config.bind_host, config.port), config, event_sink, health_provider
    )
