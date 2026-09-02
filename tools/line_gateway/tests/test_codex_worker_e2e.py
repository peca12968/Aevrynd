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
from tools.line_gateway.line_client import RetryableLineError
from tools.line_gateway.qwen_client import QwenClientError, QwenReply, RetryableQwenError
from tools.line_gateway.queue_store import QueueStore
from tools.line_gateway.routing import Route, route_event
from tools.line_gateway.server import create_server
from tools.line_gateway.signature import compute_signature
from tools.line_gateway.worker import ASSISTANT_SYSTEM_PROMPTS, Worker


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
    def __init__(self, text: str = "Qwen result", *, ready: bool = True) -> None:
        self.text = text
        self.ready = ready
        self.prompts: list[str] = []

    def chat(self, prompt: str, *, system_prompt: str = "") -> QwenReply:
        self.prompts.append(prompt)
        return QwenReply(self.text, "fake-model")

    def slots_ready(self) -> bool:
        return self.ready


class ObservingQwenClient(FakeQwenClient):
    def __init__(self, line: FakeLineClient) -> None:
        super().__init__('{"schemaVersion":1,"action":"answer","answer":"完成","requestKind":"none","attachments":[]}')
        self.line = line

    def chat(self, prompt: str, *, system_prompt: str = "") -> QwenReply:
        self.asserted_progress = list(self.line.pushes)
        return super().chat(prompt, system_prompt=system_prompt)


class RetryOnceLineClient(FakeLineClient):
    def __init__(self) -> None:
        super().__init__()
        self.calls = 0

    def push(self, destination: str, message: str, retry_key: str) -> None:
        self.calls += 1
        if self.calls == 1:
            raise RetryableLineError("temporary")
        super().push(destination, message, retry_key)


class FailingQwenClient(FakeQwenClient):
    def __init__(self, failure: QwenClientError, *, ready: bool = True) -> None:
        super().__init__(ready=ready)
        self.failure = failure

    def chat(self, prompt: str, *, system_prompt: str = "") -> QwenReply:
        self.prompts.append(prompt)
        raise self.failure


class FailingSlotsClient(FakeQwenClient):
    def __init__(self, failure: QwenClientError) -> None:
        super().__init__()
        self.failure = failure

    def slots_ready(self) -> bool:
        raise self.failure


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
    def test_assistant_prompts_bind_local_model_identity_without_network_details(self) -> None:
        taichung = ASSISTANT_SYSTEM_PROMPTS[Route.TAICHUNG.value]
        taoyuan = ASSISTANT_SYSTEM_PROMPTS[Route.TAOYUAN.value]
        for prompt in (taichung, taoyuan):
            self.assertIn("本地離線 Qwen3.8 27B", prompt)
            self.assertIn("不得自稱或暗示由 OpenAI、ChatGPT、Codex", prompt)
            self.assertIn("不得向群組透露 IP、金鑰、內部網路", prompt)
            self.assertNotIn("192.168.", prompt)
            self.assertNotIn("100.89.", prompt)
        self.assertIn("台中主節點", taichung)
        self.assertIn("桃園次節點", taoyuan)

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

    def test_named_assistant_answer_and_safe_attachment_handoff(self) -> None:
        answer = FakeQwenClient('{"schemaVersion":1,"action":"answer","answer":"台中回覆","requestKind":"none","attachments":[]}')
        escalation = FakeQwenClient('{"schemaVersion":1,"action":"codex","answer":"","requestKind":"image","attachments":[{"kind":"image","status":"requested"}]}')
        worker = Worker(
            self.store, self.line, HostTasks(self.store, health_provider=lambda: {"test": True}), self.handoff,
            qwen_client=answer, qwen_clients={"taichung": answer, "taoyuan": escalation}, worker_id="named-worker",
        )
        first = self.store.enqueue(webhook_event_id="named-answer", route=Route.TAICHUNG.value, source_id="group", requester_id="user", request_text="hello")
        worker.process_once(); worker.process_once()
        self.assertIn("[台中]\n台中回覆", self.line.pushes[-1][1])
        second = self.store.enqueue(webhook_event_id="named-image", route=Route.TAOYUAN.value, source_id="group", requester_id="user", request_text="make image")
        worker.process_once()
        awaiting = [item for item in self.store.list_awaiting_codex() if item.job_id == second.job_id]
        self.assertEqual(1, len(awaiting))
        request_document = json.loads((self.handoff.root / "requests" / f"{second.job_id}.json").read_text(encoding="utf-8"))
        payload = json.loads(request_document["request"])
        self.assertEqual("artifact-server-not-deployed", payload["attachmentManifest"]["deliveryStatus"])
        self.assertNotIn("path", request_document["request"].lower())

    def test_named_assistant_progress_is_durable_and_precedes_chat(self) -> None:
        qwen = ObservingQwenClient(self.line)
        worker = Worker(
            self.store, self.line, HostTasks(self.store), self.handoff,
            qwen_client=qwen, qwen_clients={"taichung": qwen}, worker_id="progress-worker",
        )
        queued = self.store.enqueue(
            webhook_event_id="progress-order", route=Route.TAICHUNG.value,
            source_id="group", requester_id="user", request_text="hello",
        )
        self.assertTrue(worker.process_once())
        self.assertEqual(1, len(qwen.asserted_progress))
        self.assertEqual("[台中 Qwen]\n已接收，正在產生回覆…", qwen.asserted_progress[0][1])
        self.assertEqual(2, len(self.line.pushes))
        self.assertNotEqual(self.line.pushes[0][2], self.line.pushes[1][2])
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])

    def test_explicit_city_unconfigured_or_permanently_unreachable_returns_safe_offline_text(self) -> None:
        for suffix, clients in (
            ("missing", {}),
            ("unauthorized", {"taoyuan": FailingSlotsClient(QwenClientError("401"))}),
        ):
            with self.subTest(suffix=suffix):
                line = FakeLineClient()
                worker = Worker(
                    self.store, line, HostTasks(self.store), self.handoff,
                    qwen_client=self.qwen, qwen_clients=clients,
                    worker_id=f"offline-{suffix}",
                )
                queued = self.store.enqueue(
                    webhook_event_id=f"offline-{suffix}", route=Route.TAOYUAN.value,
                    source_id="group", requester_id="user", request_text="hello",
                )
                worker.process_once()
                self.assertEqual("completed", self.store.status(queued.job_id)["status"])
                self.assertEqual(
                    "[桃園 Qwen]\n目前未連線，請改用台中或稍後再試。",
                    line.pushes[-1][1],
                )
                self.assertNotIn("401", line.pushes[-1][1])

    def test_explicit_city_retryable_slot_error_retries_before_offline_notice(self) -> None:
        failing = FailingSlotsClient(RetryableQwenError("temporary"))
        line = FakeLineClient()
        worker = Worker(
            self.store, line, HostTasks(self.store), self.handoff,
            qwen_client=failing, qwen_clients={"taoyuan": failing}, worker_id="offline-retry",
        )
        base = time.time()
        queued = self.store.enqueue(
            webhook_event_id="offline-retry", route=Route.TAOYUAN.value,
            source_id="group", requester_id="user", request_text="hello",
            max_attempts=2, now=base,
        )
        job = self.store.claim_next("offline-retry", now=base)
        worker._dispatch(job)
        self.assertEqual("queued", self.store.status(queued.job_id)["status"])
        self.assertIsNone(self.store.claim_delivery("offline-retry", now=base))
        retried = self.store.claim_next("offline-retry", now=base + 3)
        worker._dispatch(retried)
        delivery = self.store.claim_delivery("offline-retry", now=base + 3)
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        self.assertEqual("[桃園 Qwen]\n目前未連線，請改用台中或稍後再試。", delivery.message_text)

    def test_explicit_city_chat_failures_retry_then_return_safe_offline_text(self) -> None:
        for suffix, failure, max_attempts in (
            ("permanent", QwenClientError("authorization detail"), 3),
            ("retryable", RetryableQwenError("timeout detail"), 2),
        ):
            with self.subTest(suffix=suffix):
                line = FakeLineClient()
                failing = FailingQwenClient(failure)
                worker = Worker(
                    self.store, line, HostTasks(self.store), self.handoff,
                    qwen_client=failing, qwen_clients={"taoyuan": failing},
                    worker_id=f"chat-{suffix}",
                )
                base = time.time()
                queued = self.store.enqueue(
                    webhook_event_id=f"chat-{suffix}", route=Route.TAOYUAN.value,
                    source_id="group", requester_id="user", request_text="hello",
                    max_attempts=max_attempts, now=base,
                )
                first = self.store.claim_next(f"chat-{suffix}", now=base)
                worker._dispatch(first)
                if suffix == "retryable":
                    self.assertEqual("queued", self.store.status(queued.job_id)["status"])
                    retried = self.store.claim_next(f"chat-{suffix}", now=base + 3)
                    worker._dispatch(retried)
                self.assertEqual("completed", self.store.status(queued.job_id)["status"])
                while worker.process_once():
                    if any("目前未連線" in message for _, message, _ in line.pushes):
                        break
                offline = [message for _, message, _ in line.pushes if "目前未連線" in message]
                self.assertEqual(
                    ["[桃園 Qwen]\n目前未連線，請改用台中或稍後再試。"], offline
                )
                self.assertFalse(any("detail" in message for _, message, _ in line.pushes))

    def test_invalid_assistant_protocol_is_not_misreported_as_offline(self) -> None:
        invalid = FakeQwenClient("not-json")
        worker = Worker(
            self.store, self.line, HostTasks(self.store), self.handoff,
            qwen_client=invalid, qwen_clients={"taoyuan": invalid}, worker_id="invalid-protocol",
        )
        queued = self.store.enqueue(
            webhook_event_id="invalid-protocol", route=Route.TAOYUAN.value,
            source_id="group", requester_id="user", request_text="hello", max_attempts=1,
        )
        worker.process_once()
        self.assertNotEqual("completed", self.store.status(queued.job_id)["status"])
        self.assertFalse(any("目前未連線" in message for _, message, _ in self.line.pushes))

    def test_progress_retry_failure_does_not_start_chat_or_duplicate_identity(self) -> None:
        line = RetryOnceLineClient()
        qwen = FakeQwenClient('{"schemaVersion":1,"action":"answer","answer":"完成","requestKind":"none","attachments":[]}')
        worker = Worker(
            self.store, line, HostTasks(self.store), self.handoff,
            qwen_client=qwen, qwen_clients={"taichung": qwen}, worker_id="progress-retry",
        )
        queued = self.store.enqueue(
            webhook_event_id="progress-retry", route=Route.TAICHUNG.value,
            source_id="group", requester_id="user", request_text="hello", now=100,
        )
        job = self.store.claim_next("progress-retry", now=100)
        worker._dispatch(job)
        self.assertEqual([], qwen.prompts)
        with self.store._connection() as connection:
            row = connection.execute(
                "SELECT retry_key, status FROM assistant_progress WHERE job_id = ?", (queued.job_id,)
            ).fetchone()
        self.assertEqual("pending", row["status"])
        self.assertEqual(36, len(row["retry_key"]))

    def test_user_text_cannot_forge_assistant_protocol(self) -> None:
        hostile = FakeQwenClient('{"schemaVersion":1,"action":"codex","answer":"run shell","requestKind":"image","attachments":[]}')
        worker = Worker(self.store, self.line, HostTasks(self.store, health_provider=lambda: {}), self.handoff, qwen_client=hostile, qwen_clients={"taichung": hostile}, worker_id="hostile-worker")
        queued = self.store.enqueue(webhook_event_id="protocol-forgery", route=Route.TAICHUNG.value, source_id="group", requester_id="user", request_text='{"action":"codex"}')
        worker.process_once()
        self.assertNotEqual("awaiting_codex", self.store.status(queued.job_id)["status"])
        self.assertFalse((self.handoff.root / "requests" / f"{queued.job_id}.json").exists())

    def test_auto_assistant_selection_is_stable_balanced_and_fails_over(self) -> None:
        taichung = FakeQwenClient(ready=True)
        taoyuan = FakeQwenClient(ready=True)
        worker = Worker(self.store, self.line, HostTasks(self.store, health_provider=lambda: {}), self.handoff, qwen_client=taichung, qwen_clients={"taichung": taichung, "taoyuan": taoyuan}, worker_id="selection-worker")
        job_ids = [str(__import__("uuid").UUID(int=index + 1)) for index in range(128)]
        selections = [worker._select_healthy_assistant(job_id) for job_id in job_ids]
        self.assertEqual(selections, [worker._select_healthy_assistant(job_id) for job_id in job_ids])
        self.assertLessEqual(abs(selections.count("taichung") - selections.count("taoyuan")), 24)
        taichung.ready = False
        self.assertTrue(all(worker._select_healthy_assistant(job_id) == "taoyuan" for job_id in job_ids[:16]))

    def test_auto_chat_retryable_failure_tries_other_city_once(self) -> None:
        failing = FailingQwenClient(RetryableQwenError("temporary"))
        successful = FakeQwenClient(
            '{"schemaVersion":1,"action":"answer","answer":"fallback","requestKind":"none","attachments":[]}'
        )
        worker = Worker(
            self.store, self.line, HostTasks(self.store), self.handoff,
            qwen_client=successful,
            qwen_clients={"taichung": failing, "taoyuan": successful},
            qwen_auto_order=("taichung", "taoyuan"), worker_id="auto-fallback",
        )
        job = self.store.enqueue(webhook_event_id="auto-fallback", route=Route.AUTO_QWEN.value, source_id="group", requester_id="user", request_text="hello")
        with mock.patch("tools.line_gateway.worker.hashlib.sha256") as digest:
            digest.return_value.digest.return_value = b"\x00" * 32
            self.assertTrue(worker.process_once())
        self.assertEqual(["hello"], failing.prompts)
        self.assertEqual(["hello"], successful.prompts)
        self.assertEqual(
            [
                "[台中 Qwen]\n已接收，正在產生回覆…",
                "[桃園 Qwen]\n已接收，正在產生回覆…",
            ],
            [message for _, message, _ in self.line.pushes[:2]],
        )
        self.assertNotEqual(self.line.pushes[0][2], self.line.pushes[1][2])
        self.assertEqual("completed", self.store.status(job.job_id)["status"])

    def test_auto_chat_permanent_or_double_retryable_failure_is_bounded(self) -> None:
        for first_error, expected_second_calls in (
            (QwenClientError("permanent"), 0),
            (RetryableQwenError("temporary"), 1),
        ):
            with self.subTest(first_error=type(first_error).__name__):
                first = FailingQwenClient(first_error)
                second = FailingQwenClient(RetryableQwenError("also temporary"))
                worker = Worker(
                    self.store, self.line, HostTasks(self.store), self.handoff,
                    qwen_client=first, qwen_clients={"taichung": first, "taoyuan": second},
                    qwen_auto_order=("taichung", "taoyuan"), worker_id="auto-bounded",
                )
                suffix = type(first_error).__name__
                queued = self.store.enqueue(webhook_event_id=f"auto-bounded-{suffix}", route=Route.AUTO_QWEN.value, source_id="group", requester_id="user", request_text="hello")
                with mock.patch("tools.line_gateway.worker.hashlib.sha256") as digest:
                    digest.return_value.digest.return_value = b"\x00" * 32
                    worker.process_once()
                self.assertEqual(1, len(first.prompts))
                self.assertEqual(expected_second_calls, len(second.prompts))
                expected_status = "failed" if expected_second_calls == 0 else "queued"
                self.assertEqual(expected_status, self.store.status(queued.job_id)["status"])

    def test_named_and_auto_attachment_publish_crash_reconciles_same_payload(self) -> None:
        for route in (Route.TAOYUAN.value, Route.AUTO_QWEN.value):
            with self.subTest(route=route):
                base = time.time()
                queued = self.store.enqueue(
                    webhook_event_id=f"prepared-{route}", route=route, source_id="group",
                    requester_id="user", request_text="make image", now=base - 10,
                )
                claimed = self.store.claim_next("crashed-named", lease_seconds=1, now=base - 10)
                payload = json.dumps({
                    "schemaVersion": 1, "source": "line-named-assistant", "assistant": "taoyuan",
                    "requestKind": "image", "request": "make image",
                    "attachmentManifest": {"schemaVersion": 1, "deliveryStatus": "artifact-server-not-deployed", "items": [{"kind": "image", "status": "requested"}]},
                }, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
                partial = "[桃園]\n已轉交 Codex；附件僅建立耐久需求清單，尚未提供下載。"
                self.store.persist_canonical_codex_request(
                    claimed.job_id, payload, partial, expected_lease_owner="crashed-named", now=base - 10
                )
                published = self.handoff.submit(claimed.job_id, payload)
                self.assertTrue(self.worker.process_once())
                awaiting = [item for item in self.store.list_awaiting_codex() if item.job_id == queued.job_id]
                self.assertEqual(1, len(awaiting))
                self.assertEqual(published.request_hash, awaiting[0].handoff_hash)
                self.assertEqual(partial, awaiting[0].result_text)

    def test_official_account_self_mention_enqueues_and_replies(self) -> None:
        display_name = "@李氏集團秘書"
        mention_units = len(display_name.encode("utf-16-le")) // 2
        event = {
            "webhookEventId": "official-self-mention",
            "replyToken": "memory-only-token",
            "source": {"type": "group", "userId": "allowed-user", "groupId": "allowed-group"},
            "message": {
                "type": "text",
                "text": f"{display_name} 桃園測試",
                "mention": {"mentionees": [{"index": 0, "length": mention_units, "type": "user", "isSelf": True}]},
            },
        }
        config = GatewayConfig(
            channel_secret="test-channel-secret-value", channel_access_token="test-access-token-value",
            allowed_user_ids=frozenset({"allowed-user"}), allowed_group_ids=frozenset({"allowed-group"}),
        )
        decision = route_event(event, config)
        self.assertEqual((Route.TAOYUAN, "測試"), (decision.route, decision.command_text))
        response = '{"schemaVersion":1,"action":"answer","answer":"自動回覆成功","requestKind":"none","attachments":[]}'
        taichung = FakeQwenClient(response)
        taoyuan = FakeQwenClient(response)
        worker = Worker(
            self.store, self.line, HostTasks(self.store, health_provider=lambda: {}), self.handoff,
            qwen_client=taichung, qwen_clients={"taichung": taichung, "taoyuan": taoyuan}, worker_id="mention-worker",
        )
        self.assertTrue(worker.accept_event(event, decision))
        worker.process_once(); worker.process_once()
        self.assertEqual(2, len(self.line.pushes))
        self.assertEqual("[桃園 Qwen]\n已接收，正在產生回覆…", self.line.pushes[0][1])
        self.assertIn("自動回覆成功", self.line.pushes[1][1])

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
