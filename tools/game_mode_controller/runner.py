"""Authenticated loopback hub and local main-node agent runner."""

from __future__ import annotations

import argparse
import json
import os
import secrets
import signal
import threading
import time
from dataclasses import dataclass
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Callable, Mapping
from urllib import error, request

from .api import ApiResponse, HubApi
from .atomic_store import AtomicJsonStore
from .coordinator import HubCoordinator
from .credential_store import read_node_secret, read_qwen_api_key
from .inventory import HUB_PORT, NODES, validate_bind
from .models import GameModeState, QwenState
from .process_guard import ProcessIdentityError, RuntimeManifest, load_runtime_manifest
from .security import ReplayLedger, RequestAuthenticator, sign_request, verify_response
from .state_machine import GameModeController
from .windows_runtime import WindowsQwenLifecycle, capture_main_runtime_manifest, capture_remote_runtime_manifest


class RunnerError(RuntimeError):
    pass


@dataclass(frozen=True, slots=True)
class RunnerConfig:
    node_id: str
    runtime_manifest_path: Path
    runtime_manifest_sha256: str
    python_executable: Path


def load_runner_config(install_root: str | Path, *, allow_test_root: bool = False) -> tuple[RunnerConfig, RuntimeManifest]:
    root = Path(install_root).resolve()
    if not allow_test_root:
        program_data = os.environ.get("ProgramData")
        if not program_data or os.path.normcase(str(root)) != os.path.normcase(str(Path(program_data, "Aevrynd", "GameModeController").resolve())):
            raise RunnerError("install root is not allowlisted")
    path = root / "config" / "node.json"
    try:
        raw = path.read_bytes()
    except OSError as cause:
        raise RunnerError("node configuration is unavailable") from cause
    if not 2 <= len(raw) <= 65_536:
        raise RunnerError("node configuration size is invalid")
    try:
        document = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object)
    except (UnicodeError, json.JSONDecodeError, ValueError) as cause:
        raise RunnerError("node configuration is invalid") from cause
    expected = {"schemaVersion", "nodeId", "runtimeManifest", "runtimeManifestSha256", "pythonExecutable"}
    node_id = document.get("nodeId") if isinstance(document, dict) else None
    if not isinstance(document, dict) or set(document) != expected or document.get("schemaVersion") != 2 or node_id not in NODES:
        raise RunnerError("node configuration schema is invalid")
    manifest_path = (root / "manifests" / str(document["runtimeManifest"])).resolve()
    manifest_root = (root / "manifests").resolve()
    if manifest_path.parent != manifest_root or manifest_path.name != "runtime-manifest.json":
        raise RunnerError("runtime manifest path is not allowlisted")
    python_executable = Path(str(document["pythonExecutable"])).resolve()
    if not python_executable.is_absolute() or not python_executable.is_file() or python_executable.name.casefold() not in {"python.exe", "python3.exe"}:
        raise RunnerError("Python executable is invalid")
    config = RunnerConfig(str(node_id), manifest_path, str(document["runtimeManifestSha256"]), python_executable)
    manifest = load_runtime_manifest(manifest_path, expected_node_id=config.node_id, expected_sha256=config.runtime_manifest_sha256)
    return config, manifest


class SignedHubClient:
    def __init__(self, node_id: str, secret: str, *, opener: Callable[..., object] | None = None, clock: Callable[[], float] = time.time) -> None:
        if node_id not in NODES:
            raise ValueError("node is not allowlisted")
        self.node_id = node_id
        self.secret = secret
        self.clock = clock
        self._open = opener or request.build_opener(_NoRedirect()).open
        self.base_url = NODES[node_id].hub_url

    def call(self, method: str, path: str, payload: object | None = None) -> object:
        body = b"" if payload is None else json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
        nonce = secrets.token_urlsafe(24)
        headers = sign_request(self.secret, self.node_id, method, path, int(self.clock()), nonce, body)
        headers.update({"Accept": "application/json", "Content-Type": "application/json", "User-Agent": "Aevrynd-GameModeAgent/1"})
        req = request.Request(self.base_url + path, data=body if method == "POST" else None, headers=headers, method=method)
        try:
            with self._open(req, timeout=5) as response:  # type: ignore[attr-defined]
                raw = response.read(262_145)
                signature = response.headers.get("X-Aevrynd-Response-Signature")
                status = int(response.getcode())
        except (error.URLError, error.HTTPError, TimeoutError, OSError) as cause:
            raise RunnerError("authenticated hub request failed") from cause
        if status != 200 or len(raw) > 262_144 or not verify_response(self.secret, self.node_id, nonce, raw, signature):
            raise RunnerError("authenticated hub response failed validation")
        try:
            document = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object)
        except (UnicodeError, json.JSONDecodeError, ValueError) as cause:
            raise RunnerError("authenticated hub response is invalid") from cause
        if not isinstance(document, dict) or set(document) != {"schema_version", "payload"} or document.get("schema_version") != 1:
            raise RunnerError("authenticated hub response schema is invalid")
        return document["payload"]


class _NoRedirect(request.HTTPRedirectHandler):
    # 憑證簽章不可跟隨 redirect 離開固定 loopback hub。
    def redirect_request(self, req, fp, code, msg, headers, newurl):  # type: ignore[no-untyped-def]
        return None


def make_hub_server(
    api: HubApi, *, server_factory: Callable[..., ThreadingHTTPServer] = ThreadingHTTPServer,
    sleeper: Callable[[float], None] = time.sleep, host: str = "127.0.0.1",
) -> ThreadingHTTPServer:
    host, port = validate_bind("hub", host, HUB_PORT)

    class Handler(BaseHTTPRequestHandler):
        server_version = "AevryndGameModeHub/1"
        sys_version = ""

        def setup(self) -> None:
            super().setup()
            self.connection.settimeout(5.0)

        def do_GET(self) -> None:
            self._dispatch("GET")

        def do_POST(self) -> None:
            self._dispatch("POST")

        def _dispatch(self, method: str) -> None:
            if self.headers.get_all("Transfer-Encoding") or any(
                len(self.headers.get_all(name) or []) > 1
                for name in (
                    "Content-Length", "X-Aevrynd-Node", "X-Aevrynd-Timestamp",
                    "X-Aevrynd-Nonce", "X-Aevrynd-Signature",
                )
            ):
                self.send_error(400)
                return
            length_text = self.headers.get("Content-Length", "0")
            try:
                length = int(length_text)
            except ValueError:
                self.send_error(400)
                return
            if length < 0 or length > 262_144:
                self.send_error(413)
                return
            body = self.rfile.read(length)
            if len(body) != length:
                self.send_error(400)
                return
            result = api.dispatch(method, self.path, dict(self.headers.items()), body)
            self.send_response(result.status)
            for key, value in result.headers.items():
                self.send_header(key, value)
            self.send_header("Content-Length", str(len(result.body)))
            self.send_header("Cache-Control", "no-store")
            self.end_headers()
            self.wfile.write(result.body)

        def log_message(self, format: str, *args: object) -> None:
            return

    # 安裝切換剛結束時舊 listener 可能仍在釋放 socket；只對 address-in-use 做有限重試。
    server = None
    for attempt in range(33):
        try:
            server = server_factory((host, port), Handler)
            break
        except OSError as cause:
            address_in_use = getattr(cause, "winerror", None) == 10048 or cause.errno in {98, 10048}
            if not address_in_use or attempt == 32:
                raise
            sleeper(0.25)
    if server is None:
        raise AssertionError("unreachable")
    server.daemon_threads = False
    server.block_on_close = True
    return server


class MainAgent:
    def __init__(self, controller: GameModeController, client: SignedHubClient, *, interval_seconds: float = 2.0,
                 retry_initial_seconds: float = 5.0, retry_max_seconds: float = 60.0,
                 monotonic: Callable[[], float] = time.monotonic) -> None:
        if interval_seconds <= 0 or retry_initial_seconds < interval_seconds or retry_max_seconds < retry_initial_seconds:
            raise ValueError("agent retry intervals are invalid")
        self.controller = controller
        self.client = client
        self.interval_seconds = interval_seconds
        self.retry_initial_seconds = retry_initial_seconds
        self.retry_max_seconds = retry_max_seconds
        self.monotonic = monotonic
        self._next_retry: float | None = None
        self._retry_delay = retry_initial_seconds
        self.stop_event = threading.Event()

    def retry_degraded(self, state: GameModeState) -> GameModeState:
        """Retry a persisted non-game degraded state with bounded exponential backoff."""

        if state.game_mode or state.qwen_state is not QwenState.DEGRADED:
            self._next_retry = None
            self._retry_delay = self.retry_initial_seconds
            return state
        now = self.monotonic()
        if self._next_retry is None:
            self._next_retry = now + self._retry_delay
            return state
        if now < self._next_retry:
            return state
        state = self.controller.reconcile_boot()
        if state.game_mode or state.qwen_state is not QwenState.DEGRADED:
            self._next_retry = None
            self._retry_delay = self.retry_initial_seconds
        else:
            self._retry_delay = min(self._retry_delay * 2.0, self.retry_max_seconds)
            self._next_retry = now + self._retry_delay
        return state

    def run(self) -> None:
        state = self.controller.reconcile_boot()
        while not self.stop_event.is_set():
            state = self.retry_degraded(state)
            try:
                self.client.call("POST", "/v1/status", {"state": state.to_dict()})
                command = self.client.call("GET", "/v1/poll")
                if isinstance(command, dict):
                    if set(command) != {"command_id", "actor_node_id", "target_node_id", "game_mode", "created_utc"} or command.get("target_node_id") != self.controller.spec.node_id or type(command.get("game_mode")) is not bool:
                        raise RunnerError("hub command schema is invalid")
                    state = self.controller.set_game_mode(command["game_mode"])
                    self.client.call("POST", "/v1/ack", {"command_id": command["command_id"], "state": state.to_dict()})
            except Exception:
                # 網路或 runtime 錯誤不終止 agent；持久意圖會在下一輪繼續收斂。
                pass
            self.stop_event.wait(self.interval_seconds)


def load_startup_runtime(
    install_root: str | Path, *, loader: Callable[..., tuple[RunnerConfig, RuntimeManifest]] = load_runner_config,
    sleeper: Callable[[float], None] = time.sleep,
) -> tuple[RunnerConfig, RuntimeManifest]:
    """Repeat the complete fail-closed file identity gate across a short task-start race."""

    for attempt in range(21):
        try:
            return loader(install_root)
        except ProcessIdentityError as cause:
            if attempt == 20:
                safe_stages = {
                    "runtime manifest identity is invalid": "manifest_identity",
                    "runtime manifest is unavailable": "manifest_unavailable",
                    "runtime manifest hash mismatch": "manifest_hash",
                    "runtime manifest JSON is invalid": "manifest_schema",
                    "runtime manifest schema is invalid": "manifest_schema",
                    "runtime manifest content is invalid": "manifest_schema",
                    "runtime manifest node identity mismatch": "manifest_schema",
                    "runtime manifest model ID is missing": "manifest_schema",
                    "runtime manifest endpoint contract is invalid": "manifest_schema",
                    "Qwen runtime file is unavailable": "runtime_file_unavailable",
                    "Qwen runtime file hash mismatch": "runtime_file_hash",
                    "Qwen runtime executable stat unavailable": "executable_stat",
                    "Qwen runtime model stat unavailable": "model_stat",
                    "Qwen runtime executable hash open unavailable": "executable_hash_open",
                    "Qwen runtime model hash open unavailable": "model_hash_open",
                    "Qwen runtime executable hash read unavailable": "executable_hash_read",
                    "Qwen runtime model hash read unavailable": "model_hash_read",
                    "Qwen runtime executable hash mismatch": "executable_hash_mismatch",
                    "Qwen runtime model hash mismatch": "model_hash_mismatch",
                }
                stage = safe_stages.get(str(cause), "identity_unknown")
                raise RunnerError(f"startup-runtime-identity-{stage}") from cause
            sleeper(0.5)
    raise AssertionError("unreachable")


def serve_main(install_root: str | Path) -> None:
    config, manifest = load_startup_runtime(install_root)
    if config.node_id != "main":
        raise RunnerError("main runner requires the main node")
    secret = read_node_secret("main")
    if secret is None:
        raise RunnerError("main node credential is unavailable")
    root = Path(install_root).resolve()
    coordinator = HubCoordinator(AtomicJsonStore(root / "state" / "hub.json"))
    authenticator = RequestAuthenticator(read_node_secret, ReplayLedger(root / "state" / "hub-nonces.sqlite3"))
    api = HubApi(coordinator, authenticator, read_node_secret)
    local_app_data = os.environ.get("LOCALAPPDATA")
    adoption_state = None if not local_app_data else Path(local_app_data, "AevryndQwenLight", "state", "light-state.json")
    lifecycle = WindowsQwenLifecycle(
        manifest, AtomicJsonStore(root / "state" / "qwen-pid.json"),
        api_key=read_qwen_api_key("main"), adoption_state=adoption_state,
    )
    controller = GameModeController("main", AtomicJsonStore(root / "state" / "main.json"), lifecycle)
    client = SignedHubClient("main", secret)
    server = make_hub_server(api, host="127.0.0.1")
    external_servers: list[ThreadingHTTPServer] = []
    external_lock = threading.Lock()
    external_stop = threading.Event()
    external_failures: list[str] = []
    agent = MainAgent(controller, client)
    worker = threading.Thread(target=agent.run, name="AevryndMainAgent")
    worker.start()
    def run_external_listener(host: str) -> None:
        # LAN/Tailscale 介面在登入時可能尚未就緒；loopback hub 不可因此退出。
        while not external_stop.is_set():
            try:
                item = make_hub_server(api, host=host)
            except OSError as cause:
                address_unavailable = getattr(cause, "winerror", None) == 10049 or cause.errno in {99, 10049}
                if address_unavailable:
                    external_stop.wait(5)
                    continue
                with external_lock:
                    external_failures.append("external-listener-bind-failed")
                external_stop.set()
                return
            if external_stop.is_set():
                item.server_close()
                return
            with external_lock:
                external_servers.append(item)
            try:
                item.serve_forever(poll_interval=0.5)
            finally:
                item.server_close()
            if not external_stop.is_set():
                with external_lock:
                    external_failures.append("external-listener-stopped")
                external_stop.set()
            return

    listeners = [threading.Thread(target=run_external_listener, args=(host,), name=f"AevryndHub-{index}") for index, host in enumerate(NODES["main"].addresses[1:], 1)]
    for listener in listeners:
        listener.start()
    main_listener = threading.Thread(target=server.serve_forever, kwargs={"poll_interval": 0.5}, name="AevryndHub-Loopback")
    main_listener.start()
    def request_stop(*_: object) -> None:
        external_stop.set()

    for signal_name in ("SIGINT", "SIGTERM"):
        if hasattr(signal, signal_name):
            signal.signal(getattr(signal, signal_name), request_stop)
    try:
        while not external_stop.wait(0.1):
            with external_lock:
                if external_failures:
                    raise RunnerError(external_failures[0])
            if not main_listener.is_alive() or not worker.is_alive():
                raise RunnerError("controller runtime thread stopped")
        with external_lock:
            if external_failures:
                raise RunnerError(external_failures[0])
    finally:
        agent.stop_event.set()
        external_stop.set()
        server.shutdown(); server.server_close()
        with external_lock:
            active_external = list(external_servers)
        for item in active_external:
            item.shutdown(); item.server_close()
        for listener in listeners:
            listener.join(timeout=10)
        worker.join(timeout=10)
        main_listener.join(timeout=10)
        if any(listener.is_alive() for listener in listeners) or worker.is_alive() or main_listener.is_alive():
            raise RunnerError("controller runtime threads did not stop")


def serve_agent(install_root: str | Path) -> None:
    config, manifest = load_startup_runtime(install_root)
    if config.node_id not in {"taichung", "taoyuan"}:
        raise RunnerError("remote runner requires a remote node")
    secret = read_node_secret(config.node_id)
    if secret is None:
        raise RunnerError("remote node credential is unavailable")
    root = Path(install_root).resolve()
    lifecycle = WindowsQwenLifecycle(
        manifest, AtomicJsonStore(root / "state" / "qwen-pid.json"),
        api_key=read_qwen_api_key(config.node_id),
    )
    controller = GameModeController(config.node_id, AtomicJsonStore(root / "state" / f"{config.node_id}.json"), lifecycle)
    agent = MainAgent(controller, SignedHubClient(config.node_id, secret))
    for signal_name in ("SIGINT", "SIGTERM"):
        if hasattr(signal, signal_name):
            signal.signal(getattr(signal, signal_name), lambda *_: agent.stop_event.set())
    agent.run()


def _client_for_main() -> SignedHubClient:
    secret = read_node_secret("main")
    if secret is None:
        raise RunnerError("main node credential is unavailable")
    return SignedHubClient("main", secret)


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="aevrynd-game-mode")
    subparsers = parser.add_subparsers(dest="command", required=True)
    serve = subparsers.add_parser("serve-main")
    serve.add_argument("--install-root", required=True)
    remote = subparsers.add_parser("serve-agent")
    remote.add_argument("--install-root", required=True)
    subparsers.add_parser("status")
    change = subparsers.add_parser("set")
    change.add_argument("--target", choices=tuple(NODES), required=True)
    change.add_argument("--game-mode", choices=("on", "off"), required=True)
    capture = subparsers.add_parser("capture-main-manifest")
    capture.add_argument("--output", required=True)
    capture.add_argument("--model-id", required=True)
    remote_capture = subparsers.add_parser("capture-remote-manifest")
    remote_capture.add_argument("--output", required=True)
    remote_capture.add_argument("--node", choices=("taichung", "taoyuan"), required=True)
    remote_capture.add_argument("--model-id", required=True)
    remote_capture.add_argument("--pid", required=True, type=int)
    arguments = parser.parse_args(argv)
    try:
        if arguments.command == "serve-main":
            serve_main(arguments.install_root)
            return 0
        if arguments.command == "serve-agent":
            serve_agent(arguments.install_root)
            return 0
        if arguments.command == "capture-main-manifest":
            print(capture_main_runtime_manifest(arguments.output, arguments.model_id))
            return 0
        if arguments.command == "capture-remote-manifest":
            print(capture_remote_runtime_manifest(arguments.output, arguments.node, arguments.model_id, arguments.pid))
            return 0
        client = _client_for_main()
        if arguments.command == "status":
            value = client.call("GET", "/v1/status")
        else:
            value = client.call("POST", "/v1/command", {"target_node_id": arguments.target, "game_mode": arguments.game_mode == "on"})
        print(json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")))
        return 0
    except Exception as cause:
        candidate = str(cause)
        safe_stage = candidate if isinstance(cause, RunnerError) and candidate.startswith("startup-runtime-identity-") and candidate.removeprefix("startup-runtime-identity-") in {
            "manifest_identity", "manifest_unavailable", "manifest_hash", "manifest_schema",
            "runtime_file_unavailable", "runtime_file_hash", "identity_unknown",
            "executable_stat", "model_stat", "executable_hash_open", "model_hash_open",
            "executable_hash_read", "model_hash_read", "executable_hash_mismatch", "model_hash_mismatch",
        } else type(cause).__name__
        print(f"error:{safe_stage}")
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
