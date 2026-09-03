"""Evidence-backed fast status and persistent artifact request tests."""

from __future__ import annotations

import json
import sqlite3
import tempfile
import time
import unittest
import uuid
from contextlib import closing
from pathlib import Path
from unittest.mock import patch

from tools.line_gateway.codex_handoff import CodexHandoff
from tools.line_gateway.host_tasks import HostTasks
from tools.line_gateway.queue_store import QueueStore
from tools.line_gateway.routing import Route
from tools.line_gateway.status_report import StatusReport
from tools.line_gateway.tests.test_codex_worker_e2e import FakeLineClient, FakeQwenClient
from tools.line_gateway.worker import Worker


class StatusReportTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.store = QueueStore(root / "queue.sqlite3")
        self.handoff = CodexHandoff(root / "handoff", allow_test_root=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_status_finishes_without_calling_qwen(self) -> None:
        class NoQwen(FakeQwenClient):
            def chat(self, *_args, **_kwargs):
                raise AssertionError("status must not call Qwen")

        queued = self.store.enqueue(
            webhook_event_id="status-fast",
            route=Route.STATUS.value,
            source_id="group",
            requester_id="user",
            request_text="三台狀態",
        )
        status = StatusReport(self.store, repository_root=self.temporary.name, controller_root=self.temporary.name, probe_timeout=0.01)
        worker = Worker(
            self.store,
            FakeLineClient(),
            HostTasks(self.store),
            self.handoff,
            qwen_client=NoQwen("unused"),
            status_report=status,
            worker_id="status-worker",
        )
        self.assertTrue(worker.process_lane_once("control"))
        result = self.store.status(queued.job_id)
        self.assertEqual("completed", result["status"])
        delivery = self.store.claim_delivery("status-delivery")
        self.assertIn("[三台即時狀態]", delivery.message_text)
        self.assertNotIn("[Aevrynd 開發進度]", delivery.message_text)

    def test_artifact_request_has_durable_id_and_handoff(self) -> None:
        queued = self.store.enqueue(
            webhook_event_id="artifact-job",
            route=Route.ARTIFACT.value,
            source_id="group",
            requester_id="user",
            request_text="幫我做一份開發歷史 PPT",
            reception_notice="工作 ID：{job_id}",
        )
        notice = self.store.claim_reception_notification("notice", now=__import__("time").time())
        self.assertIn(queued.job_id, notice.message_text)
        self.store.finish_reception_notification(notice.notification_id, expected_lease_owner="notice", success=True)
        worker = Worker(
            self.store,
            FakeLineClient(),
            HostTasks(self.store),
            self.handoff,
            qwen_client=FakeQwenClient("unused"),
            worker_id="artifact-worker",
        )
        self.assertTrue(worker.process_lane_once("control"))
        self.assertEqual("awaiting_codex", self.store.status(queued.job_id)["status"])
        request_path = self.handoff.root / "requests" / f"{queued.job_id}.json"
        document = json.loads(request_path.read_text(encoding="utf-8"))
        payload = json.loads(document["request"])
        self.assertEqual("line-artifact-request", payload["source"])
        self.assertFalse(payload["delivery"]["lineAttachmentSupported"])

    def test_city_status_uses_safe_authoritative_activity_without_prompts(self) -> None:
        root = Path(self.temporary.name)
        dispatcher = root / "dispatcher.sqlite3"
        secret_prompt = "PRIVATE_PROMPT_MUST_NOT_APPEAR"
        with closing(sqlite3.connect(dispatcher)) as connection:
            connection.execute(
                "CREATE TABLE tasks(task_id TEXT,task_type TEXT,evidence TEXT,status TEXT,"
                "assigned_node TEXT,updated_at REAL)"
            )
            processing_id = str(uuid.uuid4())
            completed_id = str(uuid.uuid4())
            connection.execute(
                "INSERT INTO tasks VALUES(?,?,?,?,?,?)",
                (processing_id, "classification", secret_prompt, "processing", "taoyuan", time.time()),
            )
            connection.execute(
                "INSERT INTO tasks VALUES(?,?,?,?,?,?)",
                (completed_id, "summary", secret_prompt, "completed", "taoyuan", time.time() - 1),
            )
            connection.commit()
        line_job = self.store.enqueue(
            webhook_event_id="taoyuan-authoritative-status",
            route=Route.TAOYUAN.value,
            source_id="group",
            requester_id="user",
            request_text=secret_prompt,
        )
        status = StatusReport(
            self.store,
            repository_root=root,
            controller_root=root,
            dispatcher_database=dispatcher,
            node_slot_probes={"taoyuan": lambda: False, "taichung": lambda: True},
            probe_timeout=0.01,
        )
        with patch.object(StatusReport, "_tcp_reachable", return_value=True):
            rendered = status.render("桃園電腦工作進度")
        self.assertIn("[桃園即時狀態]", rendered)
        self.assertIn("桃園：Qwen 可達／忙碌", rendered)
        self.assertNotIn("台中：", rendered)
        self.assertIn(f"LINE taoyuan #{line_job.job_id[:8]}", rendered)
        self.assertIn(f"唯讀派工 classification #{processing_id[:8]}", rendered)
        self.assertIn(f"最近完成：唯讀派工 summary #{completed_id[:8]}", rendered)
        self.assertNotIn(secret_prompt, rendered)
        self.assertNotRegex(rendered, r"(?:\d{1,3}\.){3}\d{1,3}")

    def test_no_activity_is_explicit_and_idle_probe_is_reported(self) -> None:
        status = StatusReport(
            self.store,
            repository_root=self.temporary.name,
            controller_root=self.temporary.name,
            dispatcher_database=Path(self.temporary.name) / "missing.sqlite3",
            node_slot_probes={"taichung": lambda: True},
            probe_timeout=0.01,
        )
        with patch.object(StatusReport, "_tcp_reachable", return_value=True):
            rendered = status.render("台中正在做什麼")
        self.assertIn("台中：Qwen 可達／閒置", rendered)
        self.assertIn("目前未偵測到進行中工作", rendered)
        self.assertNotIn("桃園：", rendered)

    def test_user_status_phrases_keep_city_project_and_overall_scopes_separate(self) -> None:
        status = StatusReport(
            self.store,
            repository_root=self.temporary.name,
            controller_root=self.temporary.name,
            dispatcher_database=Path(self.temporary.name) / "missing.sqlite3",
            probe_timeout=0.01,
        )
        with patch.object(StatusReport, "_tcp_reachable", return_value=True):
            city = status.render("桃園有工作進度嗎")
            taoyuan_exact = status.render("桃園電腦目前的工作？")
            taichung_exact = status.render("台中電腦目前的工作？")
            hinted = status.render("比較桃園狀態", scope_hint="taichung")
            project = status.render("Aevrynd開發進度")
            overall = status.render("目前整體工作進度")
        self.assertIn("[桃園即時狀態]", city)
        self.assertIn("桃園：", city)
        self.assertNotIn("主電腦：", city)
        self.assertNotIn("台中：", city)
        self.assertIn("[桃園即時狀態]", taoyuan_exact)
        self.assertNotIn("台中：", taoyuan_exact)
        self.assertIn("[台中即時狀態]", taichung_exact)
        self.assertNotIn("桃園：", taichung_exact)
        self.assertIn("[台中即時狀態]", hinted)
        self.assertNotIn("桃園：", hinted)
        self.assertEqual(1, project.count("[Aevrynd 開發進度]"))
        self.assertNotIn("[三台即時狀態]", project)
        self.assertNotIn("主電腦：", project)
        self.assertIn("[三台即時狀態]", overall)
        self.assertIn("[Aevrynd 開發進度]", overall)

    def test_current_status_job_is_not_reported_as_node_work_or_queue_work(self) -> None:
        queued = self.store.enqueue(
            webhook_event_id="current-status-query",
            route=Route.TAOYUAN.value,
            source_id="group",
            requester_id="user",
            request_text="桃園有工作進度嗎",
        )
        claimed = self.store.claim_next_for_lane("status-owner", "taoyuan")
        self.assertIsNotNone(claimed)
        status = StatusReport(
            self.store,
            repository_root=self.temporary.name,
            controller_root=self.temporary.name,
            dispatcher_database=Path(self.temporary.name) / "missing.sqlite3",
            probe_timeout=0.01,
        )
        with patch.object(StatusReport, "_tcp_reachable", return_value=True):
            rendered = status.render("桃園有工作進度嗎", current_job_id=queued.job_id)
            overall = status.render("目前整體工作進度", current_job_id=queued.job_id)
        self.assertNotIn(f"#{queued.job_id[:8]}", rendered)
        self.assertIn("LINE 工作：等待 0、處理中 0", overall)

    def test_project_ledger_summary_is_bounded_and_rejects_sensitive_lines(self) -> None:
        root = Path(self.temporary.name)
        games = root / "Games"
        info = root / "Info"
        games.mkdir()
        info.mkdir()
        (info / "TASK_STATE.md").write_text(
            "> 最後核對：2026-09-03（Asia/Taipei）\n\n"
            "## 正在工作\n\n"
            "- 安全摘要。\n"
            "- 機密位於 C:\\\\secret\\\\token.txt。\n"
            "- API key: must-not-appear。\n"
            "- " + ("長" * 400) + "\n\n"
            "## 下一步\n",
            encoding="utf-8",
        )
        status = StatusReport(
            self.store,
            repository_root=games,
            controller_root=root,
            dispatcher_database=root / "missing.sqlite3",
            probe_timeout=0.01,
        )
        rendered = status.render("Aevrynd開發進度")
        self.assertIn("安全摘要", rendered)
        self.assertNotIn("secret", rendered.casefold())
        self.assertNotIn("api key", rendered.casefold())
        self.assertNotRegex(rendered, r"[A-Za-z]:[\\/]")
        self.assertLess(max(map(len, rendered.splitlines())), 230)


if __name__ == "__main__":
    unittest.main()
