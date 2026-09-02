"""SQLite concurrency, lease and secret-persistence tests."""

from __future__ import annotations

import concurrent.futures
from contextlib import closing
import sqlite3
import tempfile
import time
from types import SimpleNamespace
import unittest
from pathlib import Path

from tools.line_gateway.queue_store import FAILURE_DELIVERY_TEXT, QueueStore, QueueStoreError
from tools.line_gateway.routing import Route, RouteDecision
from tools.line_gateway.worker import WORK_LEASE_SECONDS, Worker


class _Unused:
    pass


class _LeaseStealingQwen:
    def __init__(self, store: QueueStore) -> None:
        self.store = store
        self.reclaimed = None

    def chat(self, _prompt: str):
        # 不實際等待，直接把資料庫時鐘推過租約，模擬慢推論期間被 B 接手。
        self.reclaimed = self.store.claim_next(
            "worker-b", now=time.time() + WORK_LEASE_SECONDS + 1
        )
        return SimpleNamespace(text="stale worker-a result")


class QueueStoreTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.db_path = Path(self.temporary.name) / "queue.sqlite3"
        self.store = QueueStore(self.db_path)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _enqueue(self, event_id: str = "event-1", **kwargs: object):
        return self.store.enqueue(
            webhook_event_id=event_id,
            route="qwen",
            source_id="destination-1",
            requester_id="requester-1",
            request_text="test request",
            **kwargs,
        )

    def _assert_reclaimed_delivery_rejects_old_owner(
        self, event_id: str, **late_finish: object
    ) -> None:
        queued = self._enqueue(event_id=event_id, now=100)
        self.store.claim_next("job-worker", now=100)
        self.store.complete_with_delivery(
            queued.job_id,
            "delivery payload",
            expected_lease_owner="job-worker",
            now=100,
        )
        first = self.store.claim_delivery("delivery-a", lease_seconds=1, now=100)
        second = self.store.claim_delivery("delivery-b", lease_seconds=60, now=102)
        self.assertIsNotNone(first)
        self.assertIsNotNone(second)
        self.assertEqual(first.delivery_id, second.delivery_id)
        self.assertEqual(2, second.attempt_count)

        with self.assertRaises(QueueStoreError):
            self.store.finish_delivery(
                first.delivery_id,
                expected_lease_owner="delivery-a",
                now=102,
                **late_finish,
            )
        self.assertEqual(
            "completed",
            self.store.finish_delivery(
                second.delivery_id,
                expected_lease_owner="delivery-b",
                success=True,
                now=102,
            ),
        )
        self.assertIsNone(self.store.claim_delivery("delivery-c", now=102))

    def test_concurrent_duplicate_enqueue_creates_one_job(self) -> None:
        with concurrent.futures.ThreadPoolExecutor(max_workers=12) as executor:
            results = list(executor.map(lambda _: self._enqueue(), range(40)))
        self.assertEqual(1, sum(result.inserted for result in results))
        self.assertEqual(1, len({result.job_id for result in results}))
        self.assertEqual(1, self.store.counts()["queued"])

    def test_concurrent_claim_has_single_owner(self) -> None:
        self._enqueue()
        with concurrent.futures.ThreadPoolExecutor(max_workers=10) as executor:
            claimed = list(executor.map(lambda index: self.store.claim_next(f"worker-{index}"), range(10)))
        winners = [job for job in claimed if job is not None]
        self.assertEqual(1, len(winners))
        self.assertEqual(1, winners[0].attempt_count)

    def test_expired_lease_is_recovered_then_attempt_cap_dead_letters(self) -> None:
        result = self._enqueue(max_attempts=2, now=100)
        first = self.store.claim_next("worker-a", lease_seconds=1, now=100)
        self.assertIsNotNone(first)
        second = self.store.claim_next("worker-b", lease_seconds=1, now=102)
        self.assertIsNotNone(second)
        self.assertEqual(2, second.attempt_count)
        self.assertIsNone(self.store.claim_next("worker-c", now=104))
        status = self.store.status(result.job_id)
        self.assertEqual("dead_letter", status["status"])
        self.assertEqual("attempt_cap", status["error_code"])
        delivery = self.store.claim_delivery("delivery-worker", now=104)
        self.assertIsNotNone(delivery)
        self.assertEqual(FAILURE_DELIVERY_TEXT, delivery.message_text)

    def test_completion_and_delivery_rollback_together_on_injected_crash(self) -> None:
        queued = self._enqueue()
        self.store.claim_next("worker-a")
        original = self.store._insert_delivery_in_transaction
        self.store._insert_delivery_in_transaction = lambda *_args, **_kwargs: (_ for _ in ()).throw(
            RuntimeError("injected crash")
        )
        try:
            with self.assertRaises(RuntimeError):
                self.store.complete_with_delivery(
                    queued.job_id, "result", expected_lease_owner="worker-a"
                )
        finally:
            self.store._insert_delivery_in_transaction = original
        self.assertEqual("running", self.store.status(queued.job_id)["status"])
        self.assertIsNone(self.store.claim_delivery("delivery-worker"))

        self.store.complete_with_delivery(
            queued.job_id, "result", expected_lease_owner="worker-a"
        )
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        delivery = self.store.claim_delivery("delivery-worker")
        self.assertEqual("result", delivery.message_text)

    def test_awaiting_completion_and_delivery_rollback_together(self) -> None:
        queued = self.store.enqueue(
            webhook_event_id="event-awaiting-complete",
            route="codex",
            source_id="destination-1",
            requester_id="requester-1",
            request_text="request",
        )
        self.store.claim_next("worker-a")
        self.store.mark_awaiting_codex(queued.job_id, "b" * 64)
        original = self.store._insert_delivery_in_transaction
        self.store._insert_delivery_in_transaction = lambda *_args, **_kwargs: (_ for _ in ()).throw(
            RuntimeError("injected crash")
        )
        try:
            with self.assertRaises(RuntimeError):
                self.store.complete_awaiting_with_delivery(queued.job_id, "codex result")
        finally:
            self.store._insert_delivery_in_transaction = original
        self.assertEqual("awaiting_codex", self.store.status(queued.job_id)["status"])
        self.assertIsNone(self.store.claim_delivery("delivery-worker"))

        self.store.complete_awaiting_with_delivery(queued.job_id, "codex result")
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        self.assertEqual(
            "codex result", self.store.claim_delivery("delivery-worker").message_text
        )

    def test_concurrent_awaiting_completion_creates_exactly_one_delivery(self) -> None:
        queued = self.store.enqueue(
            webhook_event_id="event-concurrent-awaiting",
            route="codex",
            source_id="destination-1",
            requester_id="requester-1",
            request_text="request",
        )
        self.store.claim_next("worker-a")
        self.store.mark_awaiting_codex(queued.job_id, "c" * 64)

        def complete(index: int) -> bool:
            try:
                self.store.complete_awaiting_with_delivery(
                    queued.job_id, f"result-{index}"
                )
            except QueueStoreError:
                return False
            return True

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            outcomes = list(executor.map(complete, range(8)))
        self.assertEqual(1, sum(outcomes))
        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        delivery = self.store.claim_delivery("delivery-worker")
        self.assertIsNotNone(delivery)
        self.store.finish_delivery(
            delivery.delivery_id, expected_lease_owner="delivery-worker", success=True
        )
        self.assertIsNone(self.store.claim_delivery("delivery-worker"))

    def test_expired_codex_reconcile_lease_has_one_owner(self) -> None:
        base = time.time()
        queued = self.store.enqueue(
            webhook_event_id="event-reconcile-owner",
            route="codex",
            source_id="destination-1",
            requester_id="requester-1",
            request_text="request",
            max_attempts=1,
            now=base - 10,
        )
        self.store.claim_next("crashed-worker", lease_seconds=1, now=base - 10)

        def reconcile(index: int) -> tuple[str, int]:
            owner = f"recovery-{index}"
            return owner, len(self.store.claim_reconcilable_codex(owner, now=base))

        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            outcomes = list(executor.map(reconcile, range(8)))
        winners = [owner for owner, count in outcomes if count == 1]
        self.assertEqual(1, len(winners))
        with self.assertRaises(QueueStoreError):
            self.store.mark_awaiting_codex(
                queued.job_id, "d" * 64, worker_id="wrong-owner", now=base
            )
        self.store.mark_awaiting_codex(
            queued.job_id, "d" * 64, worker_id=winners[0], now=base
        )
        self.assertEqual("awaiting_codex", self.store.status(queued.job_id)["status"])

    def test_awaiting_ttl_and_hostile_attempt_cap_create_one_safe_delivery(self) -> None:
        prompt = "private-prompt-must-not-be-in-failure"
        queued = self.store.enqueue(
            webhook_event_id="event-awaiting",
            route="codex",
            source_id="destination-1",
            requester_id="requester-1",
            request_text=prompt,
            ttl_seconds=10,
            now=100,
        )
        self.store.claim_next("worker-a", now=100)
        self.store.mark_awaiting_codex(queued.job_id, "a" * 64, now=100)
        self.assertEqual([], self.store.list_awaiting_codex(now=111))
        self.assertEqual("dead_letter", self.store.status(queued.job_id)["status"])
        delivery = self.store.claim_delivery("delivery-worker", now=111)
        self.assertEqual(FAILURE_DELIVERY_TEXT, delivery.message_text)
        self.assertNotIn(prompt, delivery.message_text)
        retry_key = delivery.retry_key
        self.store.finish_delivery(
            delivery.delivery_id,
            expected_lease_owner="delivery-worker",
            success=False,
            now=111,
        )
        retried = self.store.claim_delivery("delivery-worker", now=116)
        self.assertEqual(retry_key, retried.retry_key)
        self.store.finish_delivery(
            retried.delivery_id,
            expected_lease_owner="delivery-worker",
            success=True,
            now=116,
        )
        self.assertEqual([], self.store.list_awaiting_codex(now=117))
        self.assertIsNone(self.store.claim_delivery("delivery-worker", now=117))

    def test_permanent_running_failure_is_atomic_and_notifies(self) -> None:
        queued = self._enqueue()
        self.store.claim_next("worker-a")
        status = self.store.fail_running_with_delivery(
            queued.job_id, "fixed_error", expected_lease_owner="worker-a"
        )
        self.assertEqual("failed", status)
        delivery = self.store.claim_delivery("delivery-worker")
        self.assertEqual(FAILURE_DELIVERY_TEXT, delivery.message_text)

    def test_reclaimed_job_rejects_old_owner_late_completion(self) -> None:
        queued = self._enqueue(event_id="event-stale-complete", now=100)
        self.assertIsNotNone(
            self.store.claim_next("worker-a", lease_seconds=1, now=100)
        )
        self.assertIsNotNone(
            self.store.claim_next("worker-b", lease_seconds=60, now=102)
        )

        with self.assertRaises(QueueStoreError):
            self.store.complete_with_delivery(
                queued.job_id,
                "stale-a-result",
                expected_lease_owner="worker-a",
                now=102,
            )
        self.store.complete_with_delivery(
            queued.job_id,
            "authoritative-b-result",
            expected_lease_owner="worker-b",
            now=102,
        )

        self.assertEqual("completed", self.store.status(queued.job_id)["status"])
        delivery = self.store.claim_delivery("delivery-worker", now=102)
        self.assertEqual("authoritative-b-result", delivery.message_text)
        self.store.finish_delivery(
            delivery.delivery_id,
            expected_lease_owner="delivery-worker",
            success=True,
            now=102,
        )
        self.assertIsNone(self.store.claim_delivery("delivery-worker", now=102))

    def test_reclaimed_job_rejects_old_owner_late_failure(self) -> None:
        queued = self._enqueue(event_id="event-stale-failure", now=100)
        self.assertIsNotNone(
            self.store.claim_next("worker-a", lease_seconds=1, now=100)
        )
        self.assertIsNotNone(
            self.store.claim_next("worker-b", lease_seconds=60, now=102)
        )

        with self.assertRaises(QueueStoreError):
            self.store.fail_running_with_delivery(
                queued.job_id,
                "stale_a_failure",
                expected_lease_owner="worker-a",
                now=102,
            )
        status = self.store.fail_running_with_delivery(
            queued.job_id,
            "authoritative_b_failure",
            expected_lease_owner="worker-b",
            now=102,
        )

        self.assertEqual("failed", status)
        self.assertEqual("authoritative_b_failure", self.store.status(queued.job_id)["error_code"])
        delivery = self.store.claim_delivery("delivery-worker", now=102)
        self.assertEqual(FAILURE_DELIVERY_TEXT, delivery.message_text)
        self.store.finish_delivery(
            delivery.delivery_id,
            expected_lease_owner="delivery-worker",
            success=True,
            now=102,
        )
        self.assertIsNone(self.store.claim_delivery("delivery-worker", now=102))

    def test_reclaimed_job_rejects_old_owner_late_dead_letter(self) -> None:
        queued = self._enqueue(event_id="event-stale-dead-letter", now=100)
        self.assertIsNotNone(
            self.store.claim_next("worker-a", lease_seconds=1, now=100)
        )
        self.assertIsNotNone(
            self.store.claim_next("worker-b", lease_seconds=60, now=102)
        )

        with self.assertRaises(QueueStoreError):
            self.store.dead_letter_with_delivery(
                queued.job_id,
                "stale_a_dead_letter",
                expected_lease_owner="worker-a",
                now=102,
            )
        self.store.dead_letter_with_delivery(
            queued.job_id,
            "authoritative_b_dead_letter",
            expected_lease_owner="worker-b",
            now=102,
        )

        self.assertEqual("dead_letter", self.store.status(queued.job_id)["status"])
        self.assertEqual(
            "authoritative_b_dead_letter", self.store.status(queued.job_id)["error_code"]
        )
        delivery = self.store.claim_delivery("delivery-worker", now=102)
        self.assertEqual(FAILURE_DELIVERY_TEXT, delivery.message_text)
        self.store.finish_delivery(
            delivery.delivery_id,
            expected_lease_owner="delivery-worker",
            success=True,
            now=102,
        )
        self.assertIsNone(self.store.claim_delivery("delivery-worker", now=102))

    def test_worker_drops_qwen_result_after_lease_is_reclaimed(self) -> None:
        queued = self._enqueue(event_id="event-worker-lease-loss")
        qwen = _LeaseStealingQwen(self.store)
        worker = Worker(
            self.store,
            _Unused(),
            _Unused(),
            _Unused(),
            qwen_client=qwen,
            worker_id="worker-a",
        )

        self.assertTrue(worker.process_once())
        self.assertIsNotNone(qwen.reclaimed)
        self.assertEqual("running", self.store.status(queued.job_id)["status"])
        self.assertIsNone(self.store.claim_delivery("delivery-worker"))

        self.store.complete_with_delivery(
            queued.job_id,
            "authoritative worker-b result",
            expected_lease_owner="worker-b",
        )
        delivery = self.store.claim_delivery("delivery-worker")
        self.assertEqual("authoritative worker-b result", delivery.message_text)

    def test_reclaimed_delivery_rejects_old_owner_late_success(self) -> None:
        self._assert_reclaimed_delivery_rejects_old_owner(
            "event-stale-delivery-success", success=True
        )

    def test_reclaimed_delivery_rejects_old_owner_late_retry(self) -> None:
        self._assert_reclaimed_delivery_rejects_old_owner(
            "event-stale-delivery-retry",
            success=False,
            error_code="late_retry",
            retry_delay_seconds=5,
        )

    def test_reclaimed_delivery_rejects_old_owner_late_permanent_failure(self) -> None:
        self._assert_reclaimed_delivery_rejects_old_owner(
            "event-stale-delivery-permanent",
            success=False,
            permanent=True,
            error_code="late_permanent",
        )

    def test_reply_token_is_never_persisted(self) -> None:
        reply_token = "reply-token-marker-never-store"
        worker = Worker(
            self.store,
            _Unused(),
            _Unused(),
            _Unused(),
            qwen_client=None,
            acknowledgement=lambda token, _message: self.assertEqual(reply_token, token),
        )
        decision = RouteDecision(Route.QWEN, True, "@Qwen task", "task", "explicit-prefix")
        event = {
            "webhookEventId": "event-secret-test",
            "replyToken": reply_token,
            "source": {"type": "user", "userId": "allowed-user"},
        }
        self.assertTrue(worker.accept_event(event, decision))
        with closing(sqlite3.connect(self.db_path)) as connection:
            connection.execute("PRAGMA wal_checkpoint(TRUNCATE)")
            columns = {
                row[1]
                for table in ("events", "jobs", "deliveries")
                for row in connection.execute(f"PRAGMA table_info({table})")
            }
        self.assertNotIn("replyToken", columns)
        self.assertNotIn("reply_token", columns)
        for suffix in ("", "-wal", "-shm"):
            candidate = Path(str(self.db_path) + suffix)
            if candidate.exists():
                self.assertNotIn(reply_token.encode(), candidate.read_bytes())


if __name__ == "__main__":
    unittest.main()
