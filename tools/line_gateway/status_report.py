"""Fast, evidence-backed status reporting for LINE control commands."""

from __future__ import annotations

import json
import os
import socket
import sqlite3
import subprocess
import time
import uuid
from pathlib import Path
from typing import Callable, Mapping

from .queue_store import QueueStore


class StatusReport:
    """Read fixed local state and bounded reachability probes without model inference."""

    _NODES = (
        ("main", "主電腦", "127.0.0.1", 18080),
        ("taichung", "台中", "192.168.50.98", 8080),
        ("taoyuan", "桃園", "100.89.42.36", 8080),
    )

    def __init__(
        self,
        queue_store: QueueStore,
        *,
        repository_root: str | Path = r"D:\Download\Aevrynd\Games",
        controller_root: str | Path | None = None,
        dispatcher_database: str | Path | None = None,
        node_slot_probes: Mapping[str, Callable[[], bool]] | None = None,
        probe_timeout: float = 0.35,
    ) -> None:
        self._queue_store = queue_store
        self._repository_root = Path(repository_root).resolve()
        program_data = Path(os.environ.get("ProgramData", r"C:\ProgramData"))
        self._controller_root = (
            program_data / "Aevrynd" / "GameModeController"
            if controller_root is None
            else Path(controller_root).resolve()
        )
        self._probe_timeout = max(0.05, min(float(probe_timeout), 1.0))
        local_app_data = Path(os.environ.get("LOCALAPPDATA", str(program_data)))
        self._dispatcher_database = (
            local_app_data / "Aevrynd" / "RemoteQwenDispatcher" / "tasks.sqlite3"
            if dispatcher_database is None
            else Path(dispatcher_database).resolve()
        )
        self._node_slot_probes = dict(node_slot_probes or {})

    def render(self, command_text: str = "") -> str:
        snapshot = self.snapshot()
        requested = command_text.strip()
        requested_ids = {"main", "taichung", "taoyuan"}
        if requested.startswith("台中"):
            requested_ids = {"taichung"}
        elif requested.startswith("桃園"):
            requested_ids = {"taoyuan"}
        elif requested.startswith("主電腦") or requested.casefold().startswith("codex"):
            requested_ids = {"main"}
        title = "[三台即時狀態]" if len(requested_ids) == 3 else "[指定電腦即時狀態]"
        lines = [title]
        for node in snapshot["nodes"]:
            if node["id"] not in requested_ids:
                continue
            mode = "開" if node["gameMode"] is True else "關" if node["gameMode"] is False else "未知"
            lines.append(f"- {node['label']}：Qwen {node['qwenState']}；遊戲模式 {mode}；狀態資料 {node['stateFreshness']}")
            activity = snapshot["activity"][node["id"]]
            if activity["current"]:
                lines.extend(f"  進行中：{item}" for item in activity["current"])
            else:
                lines.append("  目前未偵測到進行中工作")
            if activity["recent"]:
                lines.append(f"  最近完成：{activity['recent']}")
        repository = snapshot["repository"]
        if "main" in requested_ids:
            lines.extend(
                (
                    "",
                    "[主電腦開發進度]",
                    f"- 版本：{repository['head']} {repository['subject']}",
                    f"- 工作樹：{'有 ' + str(repository['dirtyCount']) + ' 項未提交變更' if repository['dirty'] else '乾淨'}",
                    f"- LINE 工作：等待 {snapshot['queue']['pending']}、處理中 {snapshot['queue']['running']}、等待 Codex {snapshot['queue']['awaitingCodex']}",
                    "- Codex 即時執行內容：Gateway 無法直接讀取 Codex 桌面工作階段；以上為 Git 與持久佇列的可驗證狀態。",
                )
            )
        return "\n".join(lines)

    def render_job(self, job_id: str) -> str:
        job = self._queue_store.status(job_id)
        if job is None:
            return f"[工作狀態]\n找不到工作：{job_id}"
        labels = {
            "queued": "等待處理",
            "running": "處理中",
            "awaiting_codex": "等待主電腦 Codex 接手",
            "completed": "已完成",
            "failed": "失敗，可重試",
            "dead_letter": "已停止，需人工檢查",
        }
        return (
            "[工作狀態]\n"
            f"ID：{job_id}\n"
            f"類型：{job['route']}\n"
            f"狀態：{labels.get(str(job['status']), str(job['status']))}\n"
            f"更新：{time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(float(job['updated_at'])))}"
        )

    def snapshot(self) -> dict[str, object]:
        state = self._controller_state()
        nodes: list[dict[str, object]] = []
        now = time.time()
        recent_jobs = self._queue_store.recent(50)
        activity = {node_id: self._activity(node_id, recent_jobs) for node_id, _label, _host, _port in self._NODES}
        for node_id, label, host, port in self._NODES:
            saved = state.get(node_id, {})
            age = saved.get("ageSeconds")
            freshness = "即時" if isinstance(age, float) and age <= 15 else "過期" if isinstance(age, float) else "無資料"
            reachable = self._tcp_reachable(host, port)
            qwen_state = "無法連線"
            if reachable:
                probe = self._node_slot_probes.get(node_id)
                if probe is None:
                    qwen_state = "可達／忙碌（依工作佇列）" if activity[node_id]["current"] else "可達／閒置（依工作佇列）"
                else:
                    try:
                        qwen_state = "可達／閒置" if probe() else "可達／忙碌"
                    except Exception:
                        qwen_state = "可達／忙碌狀態未知"
            nodes.append(
                {
                    "id": node_id,
                    "label": label,
                    "qwenReachable": reachable,
                    "qwenState": qwen_state,
                    "gameMode": saved.get("gameMode"),
                    "stateFreshness": freshness,
                }
            )
        counts = self._queue_store.counts()
        return {
            "capturedAt": int(now),
            "nodes": nodes,
            "activity": activity,
            "repository": self._repository(),
            "queue": {
                "pending": int(counts.get("queued", 0)),
                "running": int(counts.get("running", 0)),
                "awaitingCodex": int(counts.get("awaiting_codex", 0)),
            },
        }

    @staticmethod
    def _short_id(value: object) -> str:
        try:
            return str(uuid.UUID(str(value)))[:8]
        except (ValueError, AttributeError):
            return "unknown"

    @staticmethod
    def _safe_dispatcher_type(value: object) -> str:
        text = str(value)
        return text if text in {"summary", "classification", "test-template", "inventory"} else "unknown"

    def _activity(self, node_id: str, recent_jobs: list[dict[str, object]]) -> dict[str, object]:
        route_sets = {
            "main": {"codex", "artifact", "host", "all"},
            "taichung": {"taichung"},
            "taoyuan": {"taoyuan"},
        }
        routes = route_sets[node_id]
        current = [
            f"LINE {job['route']} #{self._short_id(job['job_id'])}"
            for job in recent_jobs
            if job.get("route") in routes and job.get("status") in {"queued", "running", "awaiting_codex"}
        ][:3]
        completed = next(
            (
                f"LINE {job['route']} #{self._short_id(job['job_id'])}"
                for job in recent_jobs
                if job.get("route") in routes and job.get("status") == "completed"
            ),
            "",
        )
        dispatcher = self._dispatcher_activity(node_id)
        current.extend(dispatcher["current"])
        if dispatcher["recent"]:
            completed = dispatcher["recent"]
        return {"current": current[:4], "recent": completed}

    def _dispatcher_activity(self, node_id: str) -> dict[str, object]:
        if node_id not in {"taichung", "taoyuan"} or not self._dispatcher_database.is_file():
            return {"current": [], "recent": ""}
        try:
            database_uri = self._dispatcher_database.as_uri() + "?mode=ro"
            connection = sqlite3.connect(database_uri, uri=True, timeout=0.1)
            connection.row_factory = sqlite3.Row
            try:
                rows = connection.execute(
                    "SELECT task_id,task_type,status,updated_at FROM tasks "
                    "WHERE assigned_node=? AND status IN ('processing','completed') "
                    "ORDER BY updated_at DESC LIMIT 5",
                    (node_id,),
                ).fetchall()
            finally:
                connection.close()
        except (OSError, sqlite3.Error, ValueError):
            return {"current": [], "recent": ""}
        current = [
            f"唯讀派工 {self._safe_dispatcher_type(row['task_type'])} #{self._short_id(row['task_id'])}"
            for row in rows
            if row["status"] == "processing"
        ][:2]
        recent = next(
            (
                f"唯讀派工 {self._safe_dispatcher_type(row['task_type'])} #{self._short_id(row['task_id'])}"
                for row in rows
                if row["status"] == "completed"
            ),
            "",
        )
        return {"current": current, "recent": recent}

    def _tcp_reachable(self, host: str, port: int) -> bool:
        try:
            with socket.create_connection((host, port), timeout=self._probe_timeout):
                return True
        except OSError:
            return False

    def _controller_state(self) -> dict[str, dict[str, object]]:
        path = self._controller_root / "state" / "hub.json"
        try:
            raw = path.read_bytes()
            if not 2 <= len(raw) <= 262_144:
                return {}
            document = json.loads(raw.decode("utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            return {}
        if not isinstance(document, Mapping) or not isinstance(document.get("nodes"), Mapping):
            return {}
        result: dict[str, dict[str, object]] = {}
        for node_id in ("main", "taichung", "taoyuan"):
            item = document["nodes"].get(node_id)
            if not isinstance(item, Mapping) or not isinstance(item.get("state"), Mapping):
                continue
            stamp = item.get("last_seen_utc")
            age: float | None = None
            if isinstance(stamp, str):
                try:
                    age = max(0.0, time.time() - __import__("datetime").datetime.fromisoformat(stamp.replace("Z", "+00:00")).timestamp())
                except ValueError:
                    pass
            game_mode = item["state"].get("game_mode")
            result[node_id] = {
                "gameMode": game_mode if type(game_mode) is bool else None,
                "ageSeconds": age,
            }
        return result

    def _git(self, *arguments: str) -> str:
        if not (self._repository_root / ".git").exists():
            return ""
        try:
            completed = subprocess.run(
                ("git", "-C", str(self._repository_root), *arguments),
                check=False,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=1.5,
                shell=False,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
        except (OSError, subprocess.TimeoutExpired):
            return ""
        return completed.stdout.strip() if completed.returncode == 0 else ""

    def _repository(self) -> dict[str, object]:
        head = self._git("rev-parse", "--short=8", "HEAD") or "不可用"
        subject = self._git("log", "-1", "--format=%s") or "無可驗證提交說明"
        status = self._git("status", "--short", "--untracked-files=no")
        dirty_count = len(status.splitlines()) if status else 0
        return {
            "head": head[:16],
            "subject": subject[:160].replace("\r", " ").replace("\n", " "),
            "dirty": dirty_count > 0,
            "dirtyCount": dirty_count,
        }
