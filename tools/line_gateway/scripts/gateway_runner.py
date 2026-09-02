"""Windows deployment runner for the Aevrynd LINE gateway."""

from __future__ import annotations

import os
import signal
import sys
import threading
import time
from pathlib import Path


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
    from tools.line_gateway.qwen_client import QwenClient, QwenClientPool
    from tools.line_gateway.routing import route_event
    from tools.line_gateway.server import create_server
    from tools.line_gateway.worker import Worker

    config = load_config()
    if config.bind_host != "127.0.0.1" or config.port != 8787:
        raise RuntimeError("gateway bind policy was rejected")

    queue = QueueStore(root / "data" / "queue.sqlite3")
    line = LineClient(config.channel_access_token)
    qwen = None
    if config.qwen_api_key is not None:
        model = os.environ.get("AEVRYND_QWEN_MODEL", "").strip()
        if not model:
            raise RuntimeError("Qwen model is not configured")
        primary_qwen = QwenClient(config.qwen_base_url, config.qwen_api_key, model)
        secondary_qwen = None
        if config.secondary_qwen_api_key is not None:
            secondary_qwen = QwenClient(
                config.secondary_qwen_base_url,
                config.secondary_qwen_api_key,
                model,
            )
        qwen = QwenClientPool(primary_qwen, secondary_qwen)
        qwen_by_role = {"primary": primary_qwen}
        if secondary_qwen is not None:
            qwen_by_role["secondary"] = secondary_qwen
    handoff = CodexHandoff()
    host_tasks = HostTasks(
        queue,
        health_provider=lambda: {
            "bind": "127.0.0.1:8787",
            "workerCount": 1,
            "qwenConfigured": qwen is not None,
            "qwenSecondaryConfigured": config.secondary_qwen_api_key is not None,
        },
    )
    worker = Worker(
        queue,
        line,
        host_tasks,
        handoff,
        qwen_client=qwen,
        qwen_clients={
            "taichung": qwen_by_role.get(config.taichung_qwen_role),
            "taoyuan": qwen_by_role.get(config.taoyuan_qwen_role),
        },
        qwen_auto_order=(
            "taichung" if config.taichung_qwen_role == "primary" else "taoyuan",
            "taichung" if config.taichung_qwen_role == "secondary" else "taoyuan",
        ),
        worker_id="line-worker-1",
    )

    def accept_event(event: dict[str, object]) -> bool:
        return worker.accept_event(event, route_event(event, config))

    server = create_server(config, accept_event)
    stopped = threading.Event()

    def request_stop(_signum: int, _frame: object) -> None:
        stopped.set()

    signal.signal(signal.SIGINT, request_stop)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, request_stop)

    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    print("gateway_started", flush=True)
    try:
        while not stopped.is_set():
            did_work = worker.process_once()
            stopped.wait(0.05 if did_work else 0.25)
    finally:
        server.shutdown()
        server.server_close()
        server_thread.join(timeout=5)
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
