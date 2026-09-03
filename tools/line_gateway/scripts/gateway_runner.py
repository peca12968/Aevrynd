"""Windows deployment runner for the Aevrynd LINE gateway."""

from __future__ import annotations

import os
import signal
import sqlite3
import sys
import threading
import time
from pathlib import Path
from typing import Callable


LANE_NAMES = ("taichung", "taoyuan", "auto", "control")
NOTIFICATION_DRAINER_COUNT = 4
RUNTIME_WORKER_COUNT = len(LANE_NAMES) + NOTIFICATION_DRAINER_COUNT + 1


class RuntimeHealth:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._active: set[str] = set()

    def enter(self, role: str) -> None:
        with self._lock:
            self._active.add(role)

    def leave(self, role: str) -> None:
        with self._lock:
            self._active.discard(role)

    def snapshot(self) -> dict[str, object]:
        with self._lock:
            count = len(self._active)
        healthy = count == RUNTIME_WORKER_COUNT
        return {
            "status": "ok" if healthy else "degraded",
            "workerCount": count,
            "expectedWorkerCount": RUNTIME_WORKER_COUNT,
            "workersHealthy": healthy,
        }


def _request_stop(stopped: threading.Event, _signum: int, _frame: object) -> None:
    stopped.set()


def _serve_runtime(
    server: object,
    stopped: threading.Event,
    worker_factory: Callable[[str], object],
    runtime_health: RuntimeHealth | None = None,
    shutdown_timeout: float = 5.0,
) -> None:
    """Run isolated lanes plus physically separate notification and side-effect drainers."""
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    lane_threads: list[threading.Thread] = []

    def run_lane(lane: str) -> None:
        lane_worker = worker_factory(lane)
        if runtime_health is not None:
            runtime_health.enter(lane)
        try:
            while not stopped.is_set():
                try:
                    worked = lane_worker.process_lane_once(lane)
                except (OSError, sqlite3.Error):
                    # lane 必須在暫時性 SQLite／本機錯誤後繼續服務；細節不寫入 secret-safe log。
                    worked = False
                stopped.wait(0.02 if worked else 0.1)
        finally:
            if runtime_health is not None:
                runtime_health.leave(lane)

    for lane_name in LANE_NAMES:
        thread = threading.Thread(target=run_lane, args=(lane_name,), daemon=True)
        thread.start()
        lane_threads.append(thread)
    notification_threads: list[threading.Thread] = []

    def run_notifications(role: str) -> None:
        notification_drainer = worker_factory(role)
        if runtime_health is not None:
            runtime_health.enter(role)
        try:
            while not stopped.is_set():
                try:
                    worked = notification_drainer.drain_notification_once()
                except (OSError, sqlite3.Error):
                    worked = False
                stopped.wait(0.02 if worked else 0.1)
        finally:
            if runtime_health is not None:
                runtime_health.leave(role)

    for index in range(NOTIFICATION_DRAINER_COUNT):
        role = "notification-drainer" if index == 0 else f"notification-drainer-{index}"
        thread = threading.Thread(target=run_notifications, args=(role,), daemon=True)
        thread.start()
        notification_threads.append(thread)
    drainer = worker_factory("drainer")

    def run_side_effects() -> None:
        if runtime_health is not None:
            runtime_health.enter("drainer")
        try:
            while not stopped.is_set():
                try:
                    worked = drainer.drain_side_effect_once()
                except (OSError, sqlite3.Error):
                    worked = False
                stopped.wait(0.05 if worked else 0.25)
        finally:
            if runtime_health is not None:
                runtime_health.leave("drainer")

    side_effect_thread = threading.Thread(target=run_side_effects, daemon=True)
    side_effect_thread.start()
    try:
        while not stopped.is_set():
            if not server_thread.is_alive():
                raise RuntimeError("gateway HTTP server stopped")
            if any(not thread.is_alive() for thread in lane_threads):
                raise RuntimeError("a gateway work lane stopped")
            if any(not thread.is_alive() for thread in notification_threads):
                raise RuntimeError("gateway notification drainer stopped")
            if not side_effect_thread.is_alive():
                raise RuntimeError("gateway side-effect drainer stopped")
            stopped.wait(0.05)
    finally:
        stopped.set()
        server.shutdown()
        server.server_close()
        runtime_threads = [server_thread, *lane_threads, *notification_threads, side_effect_thread]
        deadline = time.monotonic() + shutdown_timeout
        for thread in runtime_threads:
            thread.join(timeout=max(0.0, deadline - time.monotonic()))
        if any(thread.is_alive() for thread in runtime_threads):
            raise RuntimeError("gateway runtime did not stop cleanly")


def _deployment_root() -> Path:
    program_data = Path(os.environ.get("ProgramData", r"C:\ProgramData")).resolve()
    expected = (program_data / "Aevrynd" / "LineGateway").resolve()
    selected = Path(os.environ.get("AEVRYND_LINE_INSTALL_ROOT", str(expected))).resolve()
    if selected != expected:
        raise RuntimeError("deployment root is not allowed")
    return selected


def _run() -> int:
    root = _deployment_root()
    sys.path.insert(0, str(root / "app"))

    from tools.line_gateway.codex_handoff import CodexHandoff
    from tools.line_gateway.config import load_config
    from tools.line_gateway.host_tasks import HostTasks
    from tools.line_gateway.line_client import LineClient
    from tools.line_gateway.queue_store import QueueStore
    from tools.line_gateway.qwen_client import QwenClient, QwenClientGate, QwenClientPool
    from tools.line_gateway.routing import route_event
    from tools.line_gateway.server import create_server
    from tools.line_gateway.worker import Worker

    config = load_config()
    if config.bind_host != "127.0.0.1" or config.port != 8787:
        raise RuntimeError("gateway bind policy was rejected")

    queue = QueueStore(root / "data" / "queue.sqlite3")
    line = LineClient(config.channel_access_token)
    qwen = None
    qwen_by_role: dict[str, object] = {}
    if config.qwen_api_key is not None:
        model = os.environ.get("AEVRYND_QWEN_MODEL", "").strip()
        if not model:
            raise RuntimeError("Qwen model is not configured")
        primary_qwen = QwenClientGate(
            QwenClient(config.qwen_base_url, config.qwen_api_key, model)
        )
        secondary_qwen = None
        if config.secondary_qwen_api_key is not None:
            secondary_qwen = QwenClientGate(
                QwenClient(
                    config.secondary_qwen_base_url,
                    config.secondary_qwen_api_key,
                    model,
                )
            )
        qwen = QwenClientPool(primary_qwen, secondary_qwen)
        qwen_by_role = {"primary": primary_qwen}
        if secondary_qwen is not None:
            qwen_by_role["secondary"] = secondary_qwen
    handoff = CodexHandoff()
    runtime_health = RuntimeHealth()
    host_tasks = HostTasks(
        queue,
        health_provider=lambda: {
            "bind": "127.0.0.1:8787",
            "qwenConfigured": qwen is not None,
            "qwenSecondaryConfigured": config.secondary_qwen_api_key is not None,
            **runtime_health.snapshot(),
        },
    )
    worker_options = {
        "qwen_client": qwen,
        "qwen_clients": {
            "taichung": qwen_by_role.get(config.taichung_qwen_role),
            "taoyuan": qwen_by_role.get(config.taoyuan_qwen_role),
        },
        "qwen_auto_order": (
            "taichung" if config.taichung_qwen_role == "primary" else "taoyuan",
            "taichung" if config.taichung_qwen_role == "secondary" else "taoyuan",
        ),
    }
    ingress_worker = Worker(
        queue, line, host_tasks, handoff, worker_id="line-ingress", **worker_options
    )

    def accept_event(event: dict[str, object]) -> bool:
        return ingress_worker.accept_event(event, route_event(event, config))

    server = create_server(config, accept_event, runtime_health.snapshot)
    stopped = threading.Event()

    request_stop = lambda signum, frame: _request_stop(stopped, signum, frame)
    signal.signal(signal.SIGINT, request_stop)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, request_stop)

    def worker_factory(lane: str) -> Worker:
        return Worker(
            queue, line, host_tasks, handoff,
            worker_id=f"line-{lane}-worker", **worker_options
        )
    print("gateway_started", flush=True)
    stopped_cleanly = False
    try:
        _serve_runtime(server, stopped, worker_factory, runtime_health)
        stopped_cleanly = True
    finally:
        if stopped_cleanly:
            print("gateway_stopped", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(_run())
    except KeyboardInterrupt:
        raise SystemExit(0)
    except Exception:
        # 不輸出例外內容，避免未來上游例外夾帶 prompt 或 credential。
        print("gateway_fatal", file=sys.stderr, flush=True)
        raise SystemExit(1)
