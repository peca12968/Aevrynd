"""Evidence-backed fast status and persistent artifact request tests."""

from __future__ import annotations

import json
import os
import sqlite3
import subprocess
import tempfile
import time
import unittest
import uuid
from types import SimpleNamespace
from contextlib import closing
from pathlib import Path
from unittest.mock import patch
from datetime import datetime, timezone, timedelta

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

    def test_game_mode_invokes_only_fixed_hidden_controller_arguments(self) -> None:
        root = Path(self.temporary.name) / "controller"
        python = root / "python.exe"
        runner = root / "app" / "tools" / "game_mode_controller" / "runner.py"
        (root / "config").mkdir(parents=True)
        runner.parent.mkdir(parents=True)
        python.write_bytes(b"test")
        runner.write_text("# test", encoding="utf-8")
        (root / "config" / "node.json").write_text(json.dumps({
            "schemaVersion": 2, "nodeId": "main", "runtimeManifest": "runtime-manifest.json",
            "runtimeManifestSha256": "0" * 64, "pythonExecutable": str(python),
        }), encoding="utf-8")
        calls = []

        def run(*args, **kwargs):
            calls.append((args, kwargs))
            return SimpleNamespace(returncode=0, stdout="{}", stderr="")

        status = StatusReport(
            self.store, repository_root=self.temporary.name, controller_root=root,
            process_runner=run, probe_timeout=0.01,
        )
        with patch.object(StatusReport, "_tcp_reachable", return_value=False):
            rendered = status.change_game_mode("開啟台中遊戲模式")
        command = calls[0][0][0]
        self.assertEqual(
            (str(python), "-B", "-m", "tools.game_mode_controller.runner", "set", "--target", "taichung", "--game-mode", "on"),
            command,
        )
        self.assertFalse(calls[0][1]["shell"])
        self.assertEqual(45.0, calls[0][1]["timeout"])
        self.assertEqual(getattr(__import__("subprocess"), "CREATE_NO_WINDOW", 0), calls[0][1]["creationflags"])
        self.assertEqual(
            {key for key in ("SystemRoot", "WINDIR", "TEMP", "TMP", "ProgramData") if os.environ.get(key)}
            | {"PYTHONPATH"},
            set(calls[0][1]["env"]),
        )
        self.assertNotIn("USERPROFILE", calls[0][1]["env"])
        self.assertNotIn("LOCALAPPDATA", calls[0][1]["env"])
        self.assertIn("已安全送出", rendered)
        self.assertIn("[三台即時狀態]", rendered)

    def test_game_mode_rejects_free_form_and_controller_failure(self) -> None:
        status = StatusReport(self.store, repository_root=self.temporary.name, controller_root=self.temporary.name)
        with self.assertRaises(ValueError):
            status.change_game_mode("幫我隨便開遊戲模式")

    def test_game_mode_controller_absent_malformed_and_timeout_are_bounded(self) -> None:
        root = Path(self.temporary.name) / "controller-bounded"
        calls = []
        status = StatusReport(
            self.store, repository_root=self.temporary.name, controller_root=root,
            process_runner=lambda *args, **kwargs: calls.append((args, kwargs)), controller_timeout=5,
        )
        with self.assertRaisesRegex(ValueError, "unavailable"):
            status.change_game_mode("開啟桃園遊戲模式")
        self.assertEqual([], calls)

        config = root / "config" / "node.json"
        config.parent.mkdir(parents=True)
        config.write_bytes(b"{" + b"x" * 8192 + b"}")
        with self.assertRaisesRegex(ValueError, "unavailable"):
            status.change_game_mode("開啟桃園遊戲模式")
        self.assertEqual([], calls)

        python = root / "python.exe"
        runner = root / "app" / "tools" / "game_mode_controller" / "runner.py"
        runner.parent.mkdir(parents=True)
        python.write_bytes(b"test")
        runner.write_text("# test", encoding="utf-8")
        config.write_text(json.dumps({
            "schemaVersion": 2, "nodeId": "main", "runtimeManifest": "../outside.json",
            "runtimeManifestSha256": "0" * 64, "pythonExecutable": str(python),
        }), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "configuration is invalid"):
            status.change_game_mode("開啟桃園遊戲模式")
        self.assertEqual([], calls)

        config.write_text(json.dumps({
            "schemaVersion": 2, "nodeId": "main", "runtimeManifest": "runtime-manifest.json",
            "runtimeManifestSha256": "0" * 64, "pythonExecutable": str(python),
        }), encoding="utf-8")
        timeout_status = StatusReport(
            self.store, repository_root=self.temporary.name, controller_root=root,
            process_runner=lambda *args, **kwargs: (_ for _ in ()).throw(subprocess.TimeoutExpired(args[0], 5)),
            controller_timeout=5,
        )
        started = time.monotonic()
        with self.assertRaisesRegex(ValueError, "command failed"):
            timeout_status.change_game_mode("開啟桃園遊戲模式")
        self.assertLess(time.monotonic() - started, 1.0)

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

    def test_codex_status_uses_only_fresh_valid_published_contract(self) -> None:
        root = Path(self.temporary.name)
        live = root / "status.json"
        live.write_text(json.dumps({
            "schemaVersion": 1, "status": "working", "task": "LINE authority 收斂",
            "phase": "執行測試", "progress": 72,
            "updatedUtc": datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z"),
            "ttl": 300,
        }), encoding="utf-8")
        status = StatusReport(self.store, repository_root=root, controller_root=root,
                              codex_status_path=live, probe_timeout=.01)
        rendered = status.render("codex目前進度")
        self.assertIn("[Codex 即時進度]", rendered)
        self.assertIn("LINE authority 收斂", rendered)
        self.assertNotIn("[三台即時狀態]", rendered)
        self.assertNotIn("台中：", rendered)
        self.assertNotIn("桃園：", rendered)

    def test_codex_status_expired_invalid_or_sensitive_falls_back_to_git(self) -> None:
        root = Path(self.temporary.name)
        live = root / "status.json"
        base = {
            "schemaVersion": 1, "status": "working", "task": "safe task",
            "phase": "safe phase", "progress": 10, "ttl": 10,
            "updatedUtc": (datetime.now(timezone.utc) - timedelta(seconds=20)).isoformat().replace("+00:00", "Z"),
        }
        status = StatusReport(self.store, repository_root=root, controller_root=root,
                              codex_status_path=live, probe_timeout=.01)
        fresh = datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")
        for document in (base, {**base, "updatedUtc": "bad"},
                         {**base, "updatedUtc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
                          "task": r"C:\secret\token.txt"},
                         {**base, "updatedUtc": fresh, "task": "client secret rotation"},
                         {**base, "updatedUtc": fresh, "phase": "safe\u202eexe"},
                         {**base, "updatedUtc": fresh, "phase": "safe\tphase"},
                         {**base, "updatedUtc": fresh, "unexpected": True},
                         {**base, "updatedUtc": "2026-09-03T00:00:00+00:00"},
                         {**base, "updatedUtc": (datetime.now(timezone.utc) + timedelta(seconds=31)).isoformat().replace("+00:00", "Z")}):
            with self.subTest(document=document):
                live.write_text(json.dumps(document), encoding="utf-8")
                rendered = status.render("Codex進度")
                self.assertIn("[Aevrynd 開發進度]", rendered)
                self.assertNotIn("[三台即時狀態]", rendered)
                self.assertNotIn("secret", rendered.casefold())

        live.write_bytes(b"{" + b"x" * 8192 + b"}")
        self.assertIn("[Aevrynd 開發進度]", status.render("Codex進度"))


if __name__ == "__main__":
    unittest.main()
