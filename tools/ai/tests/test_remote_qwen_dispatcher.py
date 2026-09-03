from __future__ import annotations

import json
import contextlib
import io
import tempfile
import unittest
from pathlib import Path

from tools.ai.remote_qwen_dispatcher import (
    MAX_EVIDENCE_BYTES,
    ClaimedTask,
    DispatcherError,
    TaskStore,
    build_parser,
    run_one,
    validate_result,
)
from tools.line_gateway.qwen_client import QwenReply, RetryableQwenError


class FakeClient:
    def __init__(self, task: ClaimedTask, *, ready: bool = True, retry: bool = False) -> None:
        self.task = task
        self.ready = ready
        self.retry = retry

    def slots_ready(self) -> bool:
        return self.ready

    def chat(self, _prompt: str, *, system_prompt: str = "") -> QwenReply:
        self.assert_read_only(system_prompt)
        if self.retry:
            raise RetryableQwenError("temporary")
        payload = {
            "schemaVersion": 1,
            "taskId": self.task.task_id,
            "taskType": self.task.task_type,
            "answer": "bounded result",
            "items": ["one"],
            "confidence": 0.8,
            "needsCodex": False,
        }
        return QwenReply(json.dumps(payload), "test-model")

    @staticmethod
    def assert_read_only(prompt: str) -> None:
        assert "read-only" in prompt
        assert "Git operations" in prompt


class DispatcherTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.database = Path(self.temporary.name) / "tasks.sqlite3"
        self.store = TaskStore(self.database)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_only_fixed_task_types_are_allowed(self) -> None:
        for task_type in ("summary", "classification", "test-template", "inventory"):
            task_id = self.store.submit(task_type, "evidence")
            self.assertEqual("queued", self.store.status(task_id)["status"])
        with self.assertRaises(DispatcherError):
            self.store.submit("write-code", "evidence")

    def test_evidence_is_bounded(self) -> None:
        self.store.submit("summary", "a" * MAX_EVIDENCE_BYTES)
        with self.assertRaises(DispatcherError):
            self.store.submit("summary", "a" * (MAX_EVIDENCE_BYTES + 1))

    def test_claim_uses_durable_lease_and_round_robin(self) -> None:
        first_id = self.store.submit("summary", "first", now=1)
        second_id = self.store.submit("summary", "second", now=2)
        first, first_order = self.store.claim(now=3)
        second, second_order = self.store.claim(now=3)
        self.assertEqual(first_id, first.task_id)
        self.assertEqual(second_id, second.task_id)
        self.assertEqual(("taichung", "taoyuan"), first_order)
        self.assertEqual(("taoyuan", "taichung"), second_order)

    def test_expired_lease_is_reclaimed(self) -> None:
        task_id = self.store.submit("inventory", "evidence", now=1)
        self.store.claim(now=2)
        self.assertIsNone(self.store.claim(now=3))
        reclaimed, _ = self.store.claim(now=500)
        self.assertEqual(task_id, reclaimed.task_id)
        self.assertEqual(2, reclaimed.attempts)

    def test_valid_result_must_match_task_identity(self) -> None:
        task = ClaimedTask("10000000-0000-0000-0000-000000000001", "summary", "x", 1)
        payload = {
            "schemaVersion": 1,
            "taskId": task.task_id,
            "taskType": "summary",
            "answer": "ok",
            "items": [],
            "confidence": 1,
            "needsCodex": False,
        }
        self.assertEqual("ok", validate_result(json.dumps(payload), task)["answer"])
        payload["taskId"] = "wrong"
        with self.assertRaises(Exception):
            validate_result(json.dumps(payload), task)

    def test_unknown_result_property_is_rejected(self) -> None:
        task = ClaimedTask("10000000-0000-0000-0000-000000000001", "summary", "x", 1)
        payload = {
            "schemaVersion": 1, "taskId": task.task_id, "taskType": "summary",
            "answer": "ok", "items": [], "confidence": 1, "needsCodex": False,
            "command": "git status",
        }
        with self.assertRaises(Exception):
            validate_result(json.dumps(payload), task)

    def test_run_one_fails_over_to_second_node(self) -> None:
        task_id = self.store.submit("classification", "evidence")
        task = ClaimedTask(task_id, "classification", "evidence", 1)
        visited: list[str] = []

        def factory(node: str, _budget: float) -> FakeClient:
            visited.append(node)
            return FakeClient(task, retry=node == "taichung")

        receipt = run_one(self.store, factory)
        self.assertTrue(receipt["ok"])
        self.assertEqual(["taichung", "taoyuan"], visited)
        self.assertEqual("completed", self.store.status(task_id)["status"])
        self.assertEqual("taoyuan", self.store.status(task_id)["node"])

    def test_retryable_dual_failure_requeues(self) -> None:
        task_id = self.store.submit("summary", "evidence")
        task = ClaimedTask(task_id, "summary", "evidence", 1)
        receipt = run_one(self.store, lambda _node, _budget: FakeClient(task, retry=True))
        self.assertFalse(receipt["ok"])
        self.assertEqual("queued", self.store.status(task_id)["status"])

    def test_keyboard_interrupt_immediately_requeues(self) -> None:
        task_id = self.store.submit("summary", "evidence")

        class InterruptingClient:
            def chat(self, _prompt: str, *, system_prompt: str = "") -> QwenReply:
                raise KeyboardInterrupt

        with self.assertRaises(KeyboardInterrupt):
            run_one(self.store, lambda _node, _budget: InterruptingClient())
        state = self.store.status(task_id)
        self.assertEqual("queued", state["status"])
        self.assertEqual("Interrupted", state["errorCode"])

    def test_dual_node_budgets_fit_total_deadline(self) -> None:
        task_id = self.store.submit("summary", "evidence")
        task = ClaimedTask(task_id, "summary", "evidence", 1)
        budgets: list[float] = []

        def factory(_node: str, budget: float) -> FakeClient:
            budgets.append(budget)
            return FakeClient(task, retry=True)

        ticks = iter((0.0, 0.0, 30.0))
        run_one(self.store, factory, deadline_seconds=45, clock=lambda: next(ticks))
        self.assertEqual(2, len(budgets))
        self.assertLessEqual(budgets[0], 30)
        self.assertLessEqual(sum(budgets), 45)

    def test_deadline_exhaustion_requeues_without_second_node(self) -> None:
        task_id = self.store.submit("summary", "evidence")
        task = ClaimedTask(task_id, "summary", "evidence", 1)
        ticks = iter((0.0, 0.0, 44.0))
        visited: list[str] = []

        def factory(node: str, _budget: float) -> FakeClient:
            visited.append(node)
            return FakeClient(task, retry=True)

        run_one(self.store, factory, deadline_seconds=45, clock=lambda: next(ticks))
        self.assertEqual(["taichung"], visited)
        self.assertEqual("queued", self.store.status(task_id)["status"])

    def test_status_does_not_return_evidence(self) -> None:
        task_id = self.store.submit("summary", "private evidence")
        encoded = json.dumps(self.store.status(task_id))
        self.assertNotIn("private evidence", encoded)

    def test_cli_rejects_file_and_command_capabilities(self) -> None:
        parser = build_parser()
        for option in ("--path", "--command", "--shell", "--git", "--output"):
            with self.subTest(option=option), contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    parser.parse_args(
                        ["submit", "--task-type", "summary", "--evidence", "facts", option, "x"]
                    )


if __name__ == "__main__":
    unittest.main()
