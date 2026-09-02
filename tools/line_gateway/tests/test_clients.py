"""Hostile LINE and Qwen transport tests."""

from __future__ import annotations

import io
import json
import unittest
import uuid
from urllib import error

from tools.line_gateway.line_client import (
    LineClient,
    PermanentLineError,
    RetryableLineError,
)
from tools.line_gateway.qwen_client import (
    InvalidQwenResponse,
    QwenClient,
    QwenClientError,
    RetryableQwenError,
)
from tools.line_gateway.tests.support import FakeResponse, RecordingOpener


class LineClientTests(unittest.TestCase):
    def _http_error(self, status: int):
        return error.HTTPError("https://example.invalid", status, "error", {}, io.BytesIO())

    def test_2xx_and_409_are_success(self) -> None:
        retry_key = str(uuid.uuid4())
        success = LineClient("fake-token", opener=lambda *_args, **_kwargs: FakeResponse(202))
        self.assertEqual(202, success.push("destination", "hello", retry_key).status_code)

        def duplicate(*_args, **_kwargs):
            raise self._http_error(409)

        response = LineClient("fake-token", opener=duplicate).push(
            "destination", "hello", retry_key
        )
        self.assertTrue(response.duplicate)

    def test_client_exposes_only_durable_push_delivery(self) -> None:
        # Gateway 不保留 replyToken，因此 client 不提供同步 reply 捷徑。
        self.assertFalse(hasattr(LineClient("fake-token"), "reply"))

    def test_4xx_is_permanent_and_5xx_is_retryable(self) -> None:
        for status, expected in ((400, PermanentLineError), (429, PermanentLineError), (500, RetryableLineError)):
            def failing(*_args, _status=status, **_kwargs):
                raise self._http_error(_status)

            with self.subTest(status=status), self.assertRaises(expected):
                LineClient("fake-token", opener=failing).push(
                    "destination", "hello", str(uuid.uuid4())
                )


class QwenClientTests(unittest.TestCase):
    def _client(self, opener, **kwargs):
        return QwenClient(
            "http://192.168.50.98:8080/v1",
            "fake-api-key",
            "test-model.gguf",
            opener=opener,
            **kwargs,
        )

    def test_base_url_is_strictly_allowlisted(self) -> None:
        hostile_urls = (
            "https://192.168.50.98:8080/v1",
            "http://127.0.0.1:8080/v1",
            "http://192.168.50.98:8081/v1",
            "http://user@192.168.50.98:8080/v1",
            "http://192.168.50.98:8080/v1?target=evil",
            "http://192.168.50.98.evil.invalid:8080/v1",
        )
        for url in hostile_urls:
            with self.subTest(url=url), self.assertRaises(ValueError):
                QwenClient(url, "fake-api-key", "model")

    def test_chat_uses_fixed_url_model_and_bounded_payload(self) -> None:
        response_body = json.dumps(
            {
                "model": "returned-model",
                "choices": [{"message": {"content": "QWEN_OK"}}],
            }
        ).encode()
        opener = RecordingOpener(lambda _request: FakeResponse(200, response_body))
        reply = self._client(opener).chat("hello", system_prompt="safe system")
        self.assertEqual("QWEN_OK", reply.text)
        outgoing = opener.requests[0]
        self.assertEqual(
            "http://192.168.50.98:8080/v1/chat/completions", outgoing.full_url
        )
        payload = json.loads(outgoing.data)
        self.assertEqual("test-model.gguf", payload["model"])
        self.assertEqual(["system", "user"], [item["role"] for item in payload["messages"]])

    def test_hostile_chat_responses_fail_closed(self) -> None:
        cases = (
            b"not-json",
            b"[]",
            b'{"choices":[]}',
            b'{"choices":[{"message":{"content":""}}]}',
            b'{"choices":[{"message":{"content":7}}]}',
        )
        for body in cases:
            with self.subTest(body=body), self.assertRaises(InvalidQwenResponse):
                self._client(lambda *_args, **_kwargs: FakeResponse(200, body)).chat("hello")

    def test_429_and_5xx_retry_but_4xx_is_permanent(self) -> None:
        for status, expected in ((400, QwenClientError), (429, RetryableQwenError), (503, RetryableQwenError)):
            def failing(*_args, _status=status, **_kwargs):
                raise error.HTTPError("http://qwen.invalid", _status, "error", {}, io.BytesIO())

            with self.subTest(status=status), self.assertRaises(expected):
                self._client(failing).chat("hello")

    def test_oversized_response_and_input_are_rejected(self) -> None:
        body = b"{" + b"x" * 2048
        with self.assertRaises(InvalidQwenResponse):
            self._client(lambda *_args, **_kwargs: FakeResponse(200, body), max_response_bytes=1024).chat("hello")
        with self.assertRaises(ValueError):
            self._client(lambda *_args, **_kwargs: FakeResponse(200), max_input_chars=256).chat("x" * 257)


if __name__ == "__main__":
    unittest.main()
