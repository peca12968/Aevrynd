"""Codex handoff, worker aggregation and full ingress tests."""

from __future__ import annotations

import http.client
import json
import socket
import tempfile
import threading
import time
import time
import unittest
from pathlib import Path
from unittest import mock

from tools.line_gateway.codex_handoff import (
    CodexHandoff,
    CodexHandoffError,
    CodexReplayError,
)
from tools.line_gateway.config import GatewayConfig
from tools.line_gateway.host_tasks import HostTasks
from tools.line_gateway.qwen_client import QwenReply
from tools.line_gateway.queue_store import QueueStore
from tools.line_gateway.routing import Route, route_event
from tools.line_gateway.server import create_server
from tools.line_gateway.signature import compute_signature
from tools.line_gateway.worker import Worker


class FakeLineClient:
    def __init__(self) -> None:
        self.pushes: list[tuple[str, str, str]] = []

    def push(self, destination: str, message: str, retry_key: str) -> None:
        self.pushes.append((destination, message, retry_key))


class SlowReplyLineClient(FakeLineClient):
    def __init__(self) -> None:
        super().__init__()
        self.reply_calls = 0

    def reply(self, _reply_token: str, _message: str) -> None:
        self.reply_calls += 1
        time.sleep(2)


class FakeQwenClient:
    def __init__(self, text: str = "Qwen result") -> None:
        self.text = text
        self.prompts: list[str] = []

    def chat(self, prompt: str) -> QwenReply:
        self.prompts.append(prompt)
        return QwenReply(self.text, "fake-model")


def _write_response(handoff: CodexHandoff, job_id: str, digest: str, result: str) -> None:
    path = handoff.root / "responses" / f"{job_id}.json"
    path.write_text(
        json.dumps(
            {
                "schemaVersion": 1,
                "jobId": job_id,
                "requestHash": digest,
                "status": "completed",
                "result": result,
                "errorCode": "",
            },
            separators=(",", ":"),
        ),
        encoding="utf-8",
    )


class CodexHandoffTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.handoff = CodexHandoff(Path(self.temporary.name) / "handoff", allow_test_root=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_path_escape_and_noncanonical_ids_are_rejected(self) -> None:
        for job_id in ("../escape", "not-a-uuid", "{12345678-1234-1234-1234-123456789abc}"):
            with self.subTest(job_id=job_id), self.assertRaises(CodexHandoffError):
                self.handoff.submit(job_id, "request")

    def test_hash_identity_and_replay_are_enforced(self) -> None:
        import uuid

        job_id = str(uuid.uuid4())
        request = self.handoff.submit(job_id, "request")
        reconciled = self.handoff.submit(job_id, "request")
        self.assertEqual(request.request_hash, reconciled.request_hash)
        with self.assertRaises(CodexReplayError):
            self.handoff.submit(job_id, "changed request")
        _write_response(self.handoff, job_id, "0" * 64, "wrong")
        with self.assertRaises(CodexHandoffError):
            self.handoff.poll(job_id, request.request_hash)
        (self.handoff.root / "responses" / f"{job_id}.json").unlink()
        _write_response(self.handoff, job_id, request.request_hash, "done")
        self.assertEqual("done", self.handoff.poll(job_id, request.request_hash).result_text)
        with self.assertRaises(CodexReplayError):
            self.handoff.poll(job_id, request.request_hash)
        with self.assertRaises(CodexReplayError):
            self.handoff.submit(job_id, "request")

    def test_reparse_root_is_rejected(self) -> None:
        with mock.patch("tools.line_gateway.codex_handoff._is_link_or_reparse", return_value=True):
            with self.assertRaises(CodexHandoffError):
                CodexHandoff(Path(self.temporary.name) / "reparse", allow_test_root=True)


class WorkerAndEndToEndTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.store = QueueStore(root / "queue.sqlite3")
        self.handoff = CodexHandoff(root / "handoff", allow_test_root=True)
        self.line = FakeLineClient()
        self.qwen = FakeQwenClient()
        self.worker = Worker(
            self.store,
            self.line,
            HostTasks(self.store, health_provider=lambda: {"test": True}),
            self.handoff,
            qwen_client=self.qwen,
            worker_id="test-worker",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_all_route_aggregates_and_waits_for_codex(self) -> None:
        queued = self.store.enqueue(
            webhook_event_id="event-all",
            route=Route.ALL.value,
            source_id="group-allowed",
            requester_id="user-allowed",
            request_text="analyze this",
        )
        self.assertTrue(self.worker.process_once())
        awaiting = self.store.list_awaiting_codex()
        self.assertEqual(1, len(awaiting))
        self.assertEqual("awaiting_codex", self.store.status(queued.job_id)["status"])
        self.assertEqual([], self.line.pushes)
        self.assertIn("[主機]", awaiting[0].result_text)
        self.assertIn("[Qwen]", awaiting[0].result_text)

        _write_response(self.handoff, queued.job_id, awaiting[0].handoff_hash, "Codex result")
        self.assertTrue(self.worker.process_once())
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        self.assertEqual(1, len(self.line.pushes))
        delivered = self.line.pushes[0][1]
        self.assertIn("[主機]", delivered)
        self.assertEqual(1, delivered.count("[Qwen]"))
        self.assertIn("[Codex]\nCodex result", delivered)

    def test_crash_after_handoff_publish_reconciles_at_attempt_cap(self) -> None:
        base = time.time()
        queued = self.store.enqueue(
            webhook_event_id="event-crash-reconcile",
            route=Route.CODEX.value,
            source_id="group-allowed",
            requester_id="user-allowed",
            request_text="recover this request",
            ttl_seconds=600,
            max_attempts=1,
            now=base - 10,
        )
        claimed = self.store.claim_next(
            "crashed-worker", lease_seconds=1, now=base - 10
        )
        request = self.handoff.submit(claimed.job_id, claimed.request_text)
        self.assertEqual("running", self.store.status(queued.job_id)["status"])

        self.assertTrue(self.worker.process_once())
        awaiting = self.store.list_awaiting_codex()
        self.assertEqual(1, len(awaiting))
        self.assertEqual(request.request_hash, awaiting[0].handoff_hash)
        self.assertEqual(1, awaiting[0].attempt_count)

        _write_response(self.handoff, queued.job_id, request.request_hash, "recovered")
        self.assertTrue(self.worker.process_once())
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        self.assertEqual(1, len(self.line.pushes))
        self.assertIn("recovered", self.line.pushes[0][1])

    def test_crash_after_response_consumption_recovers_idempotently(self) -> None:
        queued = self.store.enqueue(
            webhook_event_id="event-consumed-crash",
            route=Route.CODEX.value,
            source_id="group-allowed",
            requester_id="user-allowed",
            request_text="consume safely",
        )
        claimed = self.store.claim_next("crashed-worker")
        request = self.handoff.submit(claimed.job_id, claimed.request_text)
        self.store.mark_awaiting_codex(queued.job_id, request.request_hash)
        _write_response(self.handoff, queued.job_id, request.request_hash, "durable result")

        consumed = self.handoff.poll(queued.job_id, request.request_hash)
        self.assertEqual("durable result", consumed.result_text)
        self.assertEqual("awaiting_codex", self.store.status(queued.job_id)["status"])

        self.assertTrue(self.worker.process_once())
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        self.assertEqual(1, len(self.line.pushes))
        self.assertIn("durable result", self.line.pushes[0][1])

    def test_permanent_qwen_failure_delivers_only_fixed_safe_text(self) -> None:
        prompt = "sensitive-user-text-must-not-leak"
        worker = Worker(
            self.store,
            self.line,
            HostTasks(self.store),
            self.handoff,
            qwen_client=None,
            worker_id="failure-worker",
        )
        queued = self.store.enqueue(
            webhook_event_id="event-permanent-failure",
            route=Route.QWEN.value,
            source_id="group-allowed",
            requester_id="user-allowed",
            request_text=prompt,
        )
        self.assertTrue(worker.process_once())
        self.assertEqual("failed", self.store.status(queued.job_id)["status"])
        self.assertEqual(1, len(self.line.pushes))
        self.assertNotIn(prompt, self.line.pushes[0][1])
        self.assertEqual("工作處理失敗，請稍後查詢工作狀態。", self.line.pushes[0][1])

    def test_http_to_queue_to_worker_with_fake_transport(self) -> None:
        config = GatewayConfig(
            channel_secret="test-channel-secret-value",
            channel_access_token="test-access-token-value",
            allowed_user_ids=frozenset({"user-allowed"}),
            allowed_group_ids=frozenset({"group-allowed"}),
            port=self._free_port(),
        )

        def sink(event: dict[str, object]) -> bool:
            return self.worker.accept_event(event, route_event(event, config))

        server = create_server(config, sink)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            event = {
                "webhookEventId": "event-e2e",
                "deliveryContext": {"isRedelivery": False},
                "replyToken": "ephemeral-reply-token",
                "source": {
                    "type": "group",
                    "userId": "user-allowed",
                    "groupId": "group-allowed",
                },
                "message": {"type": "text", "text": "@Qwen test end to end"},
            }
            body = json.dumps({"events": [event]}, separators=(",", ":")).encode()
            connection = http.client.HTTPConnection("127.0.0.1", config.port, timeout=5)
            connection.request(
                "POST",
                "/webhook",
                body=body,
                headers={
                    "Content-Type": "application/json",
                    "X-Line-Signature": compute_signature(body, config.channel_secret),
                },
            )
            response = connection.getresponse()
            self.assertEqual(200, response.status)
            response.read()
            connection.close()
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)

        self.assertEqual(1, self.store.counts()["queued"])
        self.assertTrue(self.worker.process_once())
        self.assertEqual(["test end to end"], self.qwen.prompts)
        self.assertEqual(1, len(self.line.pushes))
        self.assertIn("Qwen result", self.line.pushes[0][1])

    def test_webhook_does_not_wait_for_slow_line_reply(self) -> None:
        slow_line = SlowReplyLineClient()
        worker = Worker(
            self.store,
            slow_line,
            HostTasks(self.store, health_provider=lambda: {"test": True}),
            self.handoff,
            qwen_client=self.qwen,
            worker_id="no-sync-reply-worker",
        )
        config = GatewayConfig(
            channel_secret="test-channel-secret-value",
            channel_access_token="test-access-token-value",
            allowed_user_ids=frozenset({"user-allowed"}),
            allowed_group_ids=frozenset({"group-allowed"}),
            port=self._free_port(),
        )

        def sink(event: dict[str, object]) -> bool:
            return worker.accept_event(event, route_event(event, config))

        server = create_server(config, sink)
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            event = {
                "webhookEventId": "event-no-sync-reply",
                "deliveryContext": {"isRedelivery": False},
                "replyToken": "ephemeral-only",
                "source": {
                    "type": "group",
                    "userId": "user-allowed",
                    "groupId": "group-allowed",
                },
                "message": {"type": "text", "text": "@Qwen test"},
            }
            body = json.dumps({"events": [event]}, separators=(",", ":")).encode()
            started = time.monotonic()
            connection = http.client.HTTPConnection(
                "127.0.0.1", config.port, timeout=5
            )
            connection.request(
                "POST",
                "/webhook",
                body=body,
                headers={
                    "Content-Type": "application/json",
                    "X-Line-Signature": compute_signature(body, config.channel_secret),
                },
            )
            response = connection.getresponse()
            self.assertEqual(200, response.status)
            response.read()
            connection.close()
            self.assertLess(time.monotonic() - started, 0.5)
            self.assertEqual(0, slow_line.reply_calls)
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)

    @staticmethod
    def _free_port() -> int:
        with socket.socket() as probe:
            probe.bind(("127.0.0.1", 0))
            return int(probe.getsockname()[1])


if __name__ == "__main__":
    unittest.main()
