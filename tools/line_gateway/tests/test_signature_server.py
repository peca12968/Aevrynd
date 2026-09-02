"""Hostile signature and HTTP ingress tests."""

from __future__ import annotations

import http.client
import json
import socket
import threading
import time
import unittest

from tools.line_gateway.config import GatewayConfig
from tools.line_gateway.server import create_server
from tools.line_gateway.signature import compute_signature, verify_signature


SECRET = "test-channel-secret-value"


def _free_port() -> int:
    with socket.socket() as probe:
        probe.bind(("127.0.0.1", 0))
        return int(probe.getsockname()[1])


def _config(*, port: int, max_body_bytes: int = 262_144) -> GatewayConfig:
    return GatewayConfig(
        channel_secret=SECRET,
        channel_access_token="test-access-token-value",
        allowed_user_ids=frozenset({"user-allowed"}),
        allowed_group_ids=frozenset({"group-allowed"}),
        port=port,
        max_body_bytes=max_body_bytes,
    )


class SignatureTests(unittest.TestCase):
    def test_raw_body_signature_and_mutation(self) -> None:
        raw = b'{"events": [], "spacing": true}'
        signature = compute_signature(raw, SECRET)
        self.assertTrue(verify_signature(raw, signature, SECRET))
        self.assertFalse(verify_signature(raw.replace(b" ", b""), signature, SECRET))
        self.assertFalse(verify_signature(raw + b"\n", signature, SECRET))

    def test_bad_base64_and_wrong_digest_sizes_fail_closed(self) -> None:
        for hostile in (None, "", "%%%", "YWJj", "A" * 43, "密碼"):
            with self.subTest(hostile=hostile):
                self.assertFalse(verify_signature(b"{}", hostile, SECRET))


class ServerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.events: list[dict[str, object]] = []
        self.port = _free_port()
        self.server = create_server(_config(port=self.port), self._sink)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()

    def tearDown(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)

    def _sink(self, event: dict[str, object]) -> bool:
        self.events.append(dict(event))
        return True

    def _post(
        self,
        body: bytes,
        *,
        signature: str | None = None,
        content_type: str = "application/json",
    ) -> tuple[int, bytes]:
        connection = http.client.HTTPConnection("127.0.0.1", self.port, timeout=5)
        headers = {"Content-Type": content_type}
        if signature is not None:
            headers["X-Line-Signature"] = signature
        connection.request("POST", "/webhook", body=body, headers=headers)
        response = connection.getresponse()
        result = (response.status, response.read())
        connection.close()
        return result

    def _raw_exchange(self, request_bytes: bytes) -> bytes:
        with socket.create_connection(("127.0.0.1", self.port), timeout=5) as connection:
            connection.settimeout(5)
            connection.sendall(request_bytes)
            response = bytearray()
            while True:
                chunk = connection.recv(4096)
                if not chunk:
                    return bytes(response)
                response.extend(chunk)

    def test_empty_events_are_accepted_without_enqueue(self) -> None:
        body = b'{"events":[]}'
        status, _ = self._post(body, signature=compute_signature(body, SECRET))
        self.assertEqual(200, status)
        self.assertEqual([], self.events)

    def test_signature_is_checked_before_json_and_body_mutation_fails(self) -> None:
        valid = b'{"events":[]}'
        mutated = b'{ "events": [] }'
        status, _ = self._post(mutated, signature=compute_signature(valid, SECRET))
        self.assertEqual(401, status)

        invalid_json = b"not-json"
        status, _ = self._post(
            invalid_json, signature=compute_signature(invalid_json, SECRET)
        )
        self.assertEqual(400, status)

    def test_content_type_and_charset_are_strict(self) -> None:
        body = b'{"events":[]}'
        signature = compute_signature(body, SECRET)
        for content_type in ("text/plain", "application/json; charset=utf-16"):
            with self.subTest(content_type=content_type):
                status, _ = self._post(
                    body, signature=signature, content_type=content_type
                )
                self.assertEqual(415, status)

    def test_body_limit_rejects_before_event_sink(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        self.port = _free_port()
        self.server = create_server(_config(port=self.port, max_body_bytes=1024), self._sink)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        body = b"x" * 1025
        status, _ = self._post(body, signature=compute_signature(body, SECRET))
        self.assertEqual(413, status)
        self.assertEqual([], self.events)

    def test_early_rejection_closes_connection_before_pipelined_request(self) -> None:
        hostile_requests = (
            b"POST /wrong HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\nJUNK"
            b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n",
            b"POST /webhook HTTP/1.1\r\nHost: localhost\r\nContent-Type: text/plain\r\n"
            b"Content-Length: 4\r\n\r\nJUNK"
            b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n",
            b"POST /webhook HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
            b"Content-Length: 999999\r\n\r\n"
            b"GET /health HTTP/1.1\r\nHost: localhost\r\n\r\n",
        )
        for raw_request in hostile_requests:
            with self.subTest(raw_request=raw_request[:40]):
                response = self._raw_exchange(raw_request)
                self.assertEqual(1, response.count(b"HTTP/1.1"))
                self.assertIn(b"Connection: close", response)

    def test_hundred_event_batch_has_bounded_enqueue_time(self) -> None:
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=5)
        self.port = _free_port()
        config = GatewayConfig(
            channel_secret=SECRET,
            channel_access_token="test-access-token-value",
            allowed_user_ids=frozenset({"user-allowed"}),
            allowed_group_ids=frozenset({"group-allowed"}),
            port=self.port,
            request_timeout_seconds=0.5,
        )
        calls = 0

        def bounded_slow_sink(_event: dict[str, object]) -> bool:
            nonlocal calls
            calls += 1
            time.sleep(0.01)
            return True

        self.server = create_server(config, bounded_slow_sink)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        events = [
            {
                "webhookEventId": f"event-{index}",
                "deliveryContext": {"isRedelivery": False},
            }
            for index in range(100)
        ]
        body = json.dumps({"events": events}, separators=(",", ":")).encode()
        started = time.monotonic()
        status, response_body = self._post(
            body, signature=compute_signature(body, SECRET)
        )
        elapsed = time.monotonic() - started
        self.assertEqual(503, status)
        self.assertEqual("enqueue-timeout", json.loads(response_body)["status"])
        self.assertLess(elapsed, 1.0)
        self.assertLess(calls, 100)

    def test_duplicate_json_keys_and_hostile_constants_are_rejected(self) -> None:
        for body in (
            b'{"events":[],"events":[]}',
            b'{"events":[],"destination":NaN}',
        ):
            with self.subTest(body=body):
                status, _ = self._post(body, signature=compute_signature(body, SECRET))
                self.assertEqual(400, status)

    def test_valid_event_is_forwarded_verbatim_as_mapping(self) -> None:
        event = {
            "webhookEventId": "event-1",
            "deliveryContext": {"isRedelivery": False},
            "source": {"type": "user", "userId": "user-allowed"},
            "message": {"type": "text", "text": "hello"},
        }
        body = json.dumps({"events": [event]}, separators=(",", ":")).encode()
        status, _ = self._post(body, signature=compute_signature(body, SECRET))
        self.assertEqual(200, status)
        self.assertEqual([event], self.events)


if __name__ == "__main__":
    unittest.main()
