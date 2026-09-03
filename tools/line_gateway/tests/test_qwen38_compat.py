"""Qwen3.8 非思考回應相容性測試。"""

from __future__ import annotations

import json
import unittest

from tools.line_gateway.qwen_client import InvalidQwenResponse, QwenClient
from tools.line_gateway.tests.support import FakeResponse, RecordingOpener


class Qwen38CompatibilityTests(unittest.TestCase):
    def _client(self, opener: object) -> QwenClient:
        return QwenClient(
            "http://192.168.50.98:8080/v1",
            "fake-api-key",
            "test-model.gguf",
            opener=opener,
        )

    @staticmethod
    def _response(*, content: object = "QWEN_OK", reasoning: object = None, finish: str = "stop") -> bytes:
        message = {"content": content}
        if reasoning is not None:
            message["reasoning_content"] = reasoning
        return json.dumps(
            {"model": "test-model.gguf", "choices": [{"finish_reason": finish, "message": message}]}
        ).encode("utf-8")

    def test_payload_disables_thinking_with_template_kwargs(self) -> None:
        opener = RecordingOpener(
            lambda _request: FakeResponse(200, self._response())
        )

        reply = self._client(opener).chat("hello")

        self.assertEqual("QWEN_OK", reply.text)
        payload = json.loads(opener.requests[0].data)
        self.assertEqual({"enable_thinking": False}, payload["chat_template_kwargs"])
        self.assertEqual("/no_think\nhello", payload["messages"][-1]["content"])

    def test_length_with_empty_content_fails_as_token_limit(self) -> None:
        client = self._client(
            lambda *_args, **_kwargs: FakeResponse(
                200, self._response(content="", finish="length")
            )
        )

        with self.assertRaisesRegex(InvalidQwenResponse, "token limit"):
            client.chat("hello")

    def test_reasoning_only_response_fails_closed(self) -> None:
        client = self._client(
            lambda *_args, **_kwargs: FakeResponse(
                200, self._response(content="", reasoning="private chain")
            )
        )

        with self.assertRaisesRegex(InvalidQwenResponse, "content"):
            client.chat("hello")

    def test_content_is_returned_without_reasoning_content(self) -> None:
        client = self._client(
            lambda *_args, **_kwargs: FakeResponse(
                200,
                self._response(
                    content="PUBLIC_ANSWER",
                    reasoning="PRIVATE_REASONING_MUST_NOT_ESCAPE",
                ),
            )
        )

        reply = client.chat("hello")

        self.assertEqual("PUBLIC_ANSWER", reply.text)
        self.assertNotIn("PRIVATE_REASONING", reply.text)


if __name__ == "__main__":
    unittest.main()
