from __future__ import annotations

import hashlib
import json
import os
import subprocess
import sys
import tempfile
import threading
import unittest
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from unittest import mock
from urllib import error as urlerror

from tools.game_mode_controller.acceptance import AcceptanceError, QwenAcceptanceProbe
from tools.game_mode_controller.api import HubApi
from tools.game_mode_controller.atomic_store import AtomicJsonStore, StateStoreError, replace_with_retry
from tools.game_mode_controller.coordinator import HubCoordinator, OutboxFullError
from tools.game_mode_controller.installer_transaction import InstallError, InstallTransaction, load_source_manifest
from tools.game_mode_controller.line_notifications import LineNotificationDrainer, NoRedirectLineClient
from tools.game_mode_controller.inventory import NODES, validate_bind
from tools.game_mode_controller.models import GameModeState, QwenState
from tools.game_mode_controller.process_guard import (
    ObservedProcess, ProcessIdentityError, RuntimeManifest, command_digest,
    file_sha256, validate_stop_eligibility,
)
from tools.game_mode_controller.security import (
    AuthenticationError, ReplayLedger, RequestAuthenticator, sign_request,
    sign_response, verify_response,
)
from tools.game_mode_controller.state_machine import GameModeController
from tools.line_gateway.line_client import LineClient, PermanentLineError

SECRET = "test-only-node-secret-value-1234567890"
NOW = "2026-09-02T12:00:00Z"


class FakeLifecycle:
    def __init__(self, running: bool = False) -> None:
        self.running = running
        self.starts: list[str] = []
        self.stops = 0
        self.accepts = 0
        self.fail_stop = False
        self.fail_accept = False

    def is_running(self) -> bool:
        return self.running

    def start_validated(self, profile: str) -> None:
        self.starts.append(profile)
        self.running = True

    def stop_validated(self) -> None:
        self.stops += 1
        if self.fail_stop:
            raise ProcessIdentityError("test")
        self.running = False

    def accept(self) -> None:
        self.accepts += 1
        if self.fail_accept:
            raise AcceptanceError("test")


class CoreTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_inventory_and_bindings_are_closed(self) -> None:
        self.assertEqual({"main", "taichung", "taoyuan"}, set(NODES))
        self.assertEqual("light", NODES["main"].qwen_profile)
        self.assertEqual("full", NODES["taichung"].qwen_profile)
        self.assertEqual(("127.0.0.1", 8765), validate_bind("agent", "127.0.0.1", 8765))
        with self.assertRaises(ValueError):
            validate_bind("hub", "0.0.0.0", 8766)
        with self.assertRaises(ValueError):
            validate_bind("agent", "192.168.50.98", 8765)

    def test_atomic_store_rejects_duplicate_keys_and_keeps_valid_json(self) -> None:
        path = self.root / "state.json"
        store = AtomicJsonStore(path)
        store.write({"schema_version": 1, "value": "ok"})
        self.assertEqual("ok", store.read()["value"])
        path.write_text('{"a":1,"a":2}', encoding="utf-8")
        with self.assertRaises(StateStoreError):
            store.read()

    def test_state_machine_stops_only_qwen_and_restores_fixed_profile(self) -> None:
        life = FakeLifecycle(running=True)
        controller = GameModeController("main", AtomicJsonStore(self.root / "node.json"), life, clock=lambda: NOW)
        stopped = controller.set_game_mode(True)
        self.assertTrue(stopped.game_mode)
        self.assertEqual(QwenState.STOPPED, stopped.qwen_state)
        self.assertEqual(1, life.stops)
        restored = controller.set_game_mode(False)
        self.assertFalse(restored.game_mode)
        self.assertEqual(QwenState.RUNNING, restored.qwen_state)
        self.assertEqual(["light"], life.starts)
        self.assertEqual(1, life.accepts)

    def test_boot_reconcile_honors_persisted_game_mode_and_fails_closed(self) -> None:
        path = self.root / "node.json"
        life = FakeLifecycle(running=True)
        controller = GameModeController("taichung", AtomicJsonStore(path), life, clock=lambda: NOW)
        controller.set_game_mode(True)
        life.running = True
        life.fail_stop = True
        state = GameModeController("taichung", AtomicJsonStore(path), life, clock=lambda: NOW).reconcile_boot()
        self.assertTrue(state.game_mode)
        self.assertEqual(QwenState.DEGRADED, state.qwen_state)
        self.assertTrue(life.running)

    def test_local_state_rejects_cross_node_substitution(self) -> None:
        store = AtomicJsonStore(self.root / "substitution.json")
        substituted = GameModeState("taichung", False, QwenState.RUNNING, "full", 7, NOW, "accepted")
        store.write({"schema_version": 1, "state": substituted.to_dict()})
        with self.assertRaises(ValueError):
            GameModeController("main", store, FakeLifecycle(), clock=lambda: NOW).load_or_initialize()

    def test_manifest_validates_owner_command_and_file_hashes(self) -> None:
        executable = self.root / "llama-server.exe"
        model = self.root / "model.gguf"
        executable.write_bytes(b"exe")
        model.write_bytes(b"model")
        argv = (str(executable), "--model", str(model), "--slots", "1")
        manifest = RuntimeManifest(
            "main", "light", "DOMAIN\\User", str(executable), file_sha256(executable),
            argv, command_digest(argv), str(model), file_sha256(model),
        )
        manifest.verify_files()
        observed = ObservedProcess(42, "DOMAIN\\User", str(executable), argv, file_sha256(executable))
        validate_stop_eligibility(persisted_pid=42, observed=observed, manifest=manifest)
        with self.assertRaises(ProcessIdentityError):
            validate_stop_eligibility(persisted_pid=42, observed=ObservedProcess(42, "DOMAIN\\Other", str(executable), argv, file_sha256(executable)), manifest=manifest)
        executable.write_bytes(b"replaced")
        with self.assertRaises(ProcessIdentityError):
            manifest.verify_files()

    def test_process_identity_rejects_every_pid_owner_path_argv_hash_permutation(self) -> None:
        executable = self.root / "server.exe"
        other_executable = self.root / "other.exe"
        model = self.root / "model.gguf"
        executable.write_bytes(b"server")
        other_executable.write_bytes(b"server")
        model.write_bytes(b"model")
        argv = (str(executable), "--model", str(model))
        manifest = RuntimeManifest("main", "light", "owner", str(executable), file_sha256(executable), argv, command_digest(argv), str(model), file_sha256(model))
        valid = ObservedProcess(55, "owner", str(executable), argv, file_sha256(executable))
        permutations = (
            (valid, 56),
            (ObservedProcess(55, "other", str(executable), argv, file_sha256(executable)), 55),
            (ObservedProcess(55, "owner", str(other_executable), argv, file_sha256(executable)), 55),
            (ObservedProcess(55, "owner", str(executable), argv + ("--extra",), file_sha256(executable)), 55),
            (ObservedProcess(55, "owner", str(executable), argv, "0" * 64), 55),
        )
        for observed, pid in permutations:
            with self.subTest(observed=observed, pid=pid), self.assertRaises(ProcessIdentityError):
                validate_stop_eligibility(persisted_pid=pid, observed=observed, manifest=manifest)
        model.write_bytes(b"changed")
        with self.assertRaises(ProcessIdentityError):
            manifest.verify_files()

    def test_hmac_tamper_stale_and_replay_are_rejected(self) -> None:
        ledger = ReplayLedger(self.root / "nonce.sqlite3")
        auth = RequestAuthenticator(lambda node: SECRET, ledger, clock=lambda: 1000)
        body = b'{"state":{}}'
        headers = sign_request(SECRET, "main", "POST", "/v1/status", 1000, "abcdefghijklmnopqrstuv", body)
        accepted = auth.verify("POST", "/v1/status", headers, body)
        self.assertEqual("main", accepted.node_id)
        with self.assertRaises(AuthenticationError):
            auth.verify("POST", "/v1/status", headers, body)
        other_headers = sign_request(SECRET, "main", "POST", "/v1/status", 1000, "abcdefghijklmnopqrstuw", body)
        with self.assertRaises(AuthenticationError):
            auth.verify("POST", "/v1/status", other_headers, body + b" ")
        stale = sign_request(SECRET, "main", "POST", "/v1/status", 900, "abcdefghijklmnopqrstux", body)
        with self.assertRaises(AuthenticationError):
            auth.verify("POST", "/v1/status", stale, body)

    def test_replay_survives_ledger_reopen_and_response_mutations_fail(self) -> None:
        nonce = "abcdefghijklmnopqrstuv"
        body = b"{}"
        ledger_path = self.root / "reopened.sqlite3"
        headers = sign_request(SECRET, "main", "POST", "/v1/status", 1000, nonce, body)
        RequestAuthenticator(lambda node: SECRET, ReplayLedger(ledger_path), clock=lambda: 1000).verify("POST", "/v1/status", headers, body)
        with self.assertRaises(AuthenticationError):
            RequestAuthenticator(lambda node: SECRET, ReplayLedger(ledger_path), clock=lambda: 1000).verify("POST", "/v1/status", headers, body)
        signature = sign_response(SECRET, "main", nonce, body)
        self.assertTrue(verify_response(SECRET, "main", nonce, body, signature))
        self.assertFalse(verify_response(SECRET, "main", nonce, body + b"x", signature))
        self.assertFalse(verify_response(SECRET, "main", "abcdefghijklmnopqrstuw", body, signature))
        self.assertFalse(verify_response(SECRET, "taichung", nonce, body, signature))
        self.assertFalse(verify_response(SECRET, "main", nonce, body, signature[:-2] + "AA"))

    def test_coordinator_allows_cross_node_control_and_aggregates_one_snapshot(self) -> None:
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "hub.json"), clock=lambda: NOW)
        for node_id in NODES:
            coordinator.report(node_id, GameModeState(node_id, False, QwenState.UNKNOWN, NODES[node_id].qwen_profile, 0, NOW, "none"))
        command = coordinator.issue("taoyuan", "taichung", True)
        self.assertEqual(command.command_id, coordinator.poll("taichung").command_id)
        completed = GameModeState("taichung", True, QwenState.STOPPED, "full", 1, NOW, "game_mode")
        coordinator.acknowledge("taichung", command.command_id, completed)
        self.assertIsNone(coordinator.poll("taichung"))
        pending = coordinator.pending_notifications()
        self.assertEqual(1, len(pending))
        self.assertIn("Taichung PC", pending[0].text)
        self.assertIn("Main PC", pending[0].text)
        self.assertIn("Taoyuan PC", pending[0].text)
        coordinator.mark_notification_delivered(pending[0].notification_id)
        self.assertEqual((), coordinator.pending_notifications())

    def test_coordinator_rejects_stale_status_and_false_ack(self) -> None:
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "hub-stale.json"), clock=lambda: NOW)
        current = GameModeState("main", False, QwenState.RUNNING, "light", 2, NOW, "accepted")
        coordinator.report("main", current)
        stale = GameModeState("main", False, QwenState.STOPPED, "light", 1, NOW, "game_mode")
        with self.assertRaises(ValueError):
            coordinator.report("main", stale)
        command = coordinator.issue("taoyuan", "main", True)
        wrong = GameModeState("main", False, QwenState.RUNNING, "light", 3, NOW, "accepted")
        with self.assertRaises(ValueError):
            coordinator.acknowledge("main", command.command_id, wrong)
        self.assertEqual(command.command_id, coordinator.poll("main").command_id)

    def test_hub_rejects_persisted_node_key_substitution_and_stale_ack(self) -> None:
        store = AtomicJsonStore(self.root / "hub-substitution.json")
        coordinator = HubCoordinator(store, clock=lambda: NOW)
        coordinator.report("main", GameModeState("main", False, QwenState.RUNNING, "light", 5, NOW, "accepted"))
        document = store.read()
        document["nodes"]["main"]["state"] = GameModeState("taichung", False, QwenState.RUNNING, "full", 5, NOW, "accepted").to_dict()
        store.write(document)
        with self.assertRaises(ValueError):
            coordinator.snapshot()

        clean = HubCoordinator(AtomicJsonStore(self.root / "hub-ack-stale.json"), clock=lambda: "2026-09-02T12:00:10Z")
        clean.report("main", GameModeState("main", False, QwenState.RUNNING, "light", 5, "2026-09-02T12:00:05Z", "accepted"))
        command = clean.issue("taichung", "main", True)
        stale_revision = GameModeState("main", True, QwenState.STOPPED, "light", 4, "2026-09-02T12:00:06Z", "game_mode")
        with self.assertRaises(ValueError):
            clean.acknowledge("main", command.command_id, stale_revision)
        stale_time = GameModeState("main", True, QwenState.STOPPED, "light", 5, "2026-09-02T12:00:04Z", "game_mode")
        with self.assertRaises(ValueError):
            clean.acknowledge("main", command.command_id, stale_time)
        self.assertEqual(command.command_id, clean.poll("main").command_id)

    def test_long_line_partition_backpressures_without_losing_pending_or_dead_letter(self) -> None:
        ticks = iter(f"2026-09-02T12:00:{second:02d}Z" for second in range(40))
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "partition.json"), clock=lambda: next(ticks), max_notification_records=3, max_notification_bytes=4096)
        states = (QwenState.RUNNING, QwenState.STOPPED, QwenState.RUNNING)
        for revision, qwen_state in enumerate(states, 1):
            coordinator.report("main", GameModeState("main", qwen_state is QwenState.STOPPED, qwen_state, "light", revision, f"2026-09-02T12:00:{revision:02d}Z", "accepted"))
        original_ids = tuple(item.notification_id for item in coordinator.pending_notifications())
        self.assertEqual(3, len(original_ids))
        with self.assertRaises(OutboxFullError):
            coordinator.report("main", GameModeState("main", True, QwenState.STOPPED, "light", 4, "2026-09-02T12:00:04Z", "game_mode"))
        self.assertEqual(original_ids, tuple(item.notification_id for item in coordinator.pending_notifications()))
        coordinator.mark_notification_dead_letter(original_ids[0], "line_permanent_rejection")
        coordinator.mark_notification_delivered(original_ids[1])
        coordinator.report("main", GameModeState("main", True, QwenState.STOPPED, "light", 4, "2026-09-02T12:00:04Z", "game_mode"))
        document = coordinator.store.read()
        records = document["notifications"]
        self.assertEqual(3, len(records))
        self.assertIn(original_ids[0], {item["notification_id"] for item in records})
        self.assertNotIn(original_ids[1], {item["notification_id"] for item in records})

    def test_permanent_line_failure_is_retained_as_dead_letter_audit(self) -> None:
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "dead-letter.json"), clock=lambda: NOW)
        coordinator.report("main", GameModeState("main", False, QwenState.RUNNING, "light", 1, NOW, "accepted"))

        def rejecting_opener(req, timeout):
            raise urlerror.HTTPError(req.full_url, 400, "rejected", {}, None)

        client = NoRedirectLineClient(SECRET, opener=rejecting_opener, api_root="http://127.0.0.1:1/push", allow_test_root=True)
        self.assertTrue(LineNotificationDrainer(coordinator, client, "group-id", clock=lambda: datetime.fromisoformat(NOW.replace("Z", "+00:00")).timestamp()).drain_one())
        record = coordinator.store.read()["notifications"][0]
        self.assertEqual("dead_letter", record["status"])
        self.assertEqual("line_permanent_rejection", record["last_error_code"])
        self.assertEqual((), coordinator.pending_notifications())

    def test_drainer_rejects_redirect_capable_legacy_line_client(self) -> None:
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "unsafe-line.json"), clock=lambda: NOW)
        with self.assertRaisesRegex(TypeError, "redirect-proof LINE client is required"):
            LineNotificationDrainer(coordinator, LineClient(SECRET), "group-id")  # type: ignore[arg-type]

    def test_line_pending_older_than_24_hours_dead_letters_without_send(self) -> None:
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "expired-line.json"), clock=lambda: NOW)
        coordinator.report("main", GameModeState("main", False, QwenState.RUNNING, "light", 1, NOW, "accepted"))
        calls = []
        client = NoRedirectLineClient(SECRET, opener=lambda *args, **kwargs: calls.append(args), api_root="http://127.0.0.1:1/push", allow_test_root=True)
        now = datetime.fromisoformat(NOW.replace("Z", "+00:00")).timestamp() + 86_401
        self.assertTrue(LineNotificationDrainer(coordinator, client, "group-id", clock=lambda: now).drain_one())
        self.assertEqual([], calls)
        record = coordinator.store.read()["notifications"][0]
        self.assertEqual("dead_letter", record["status"])
        self.assertEqual("line_delivery_expired", record["last_error_code"])

    def test_atomic_store_serializes_two_instances_and_multiple_processes(self) -> None:
        path = self.root / "counter.json"
        AtomicJsonStore(path).write({"count": 0})
        stores = (AtomicJsonStore(path), AtomicJsonStore(path))

        def increment(store):
            for _ in range(50):
                store.update(lambda value: {"count": int(value["count"]) + 1})

        threads = [threading.Thread(target=increment, args=(store,)) for store in stores]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(100, AtomicJsonStore(path).read()["count"])
        script = "from tools.game_mode_controller.atomic_store import AtomicJsonStore; import sys; s=AtomicJsonStore(sys.argv[1]); [s.update(lambda v:{'count':int(v['count'])+1}) for _ in range(30)]"
        environment = dict(os.environ)
        environment["PYTHONDONTWRITEBYTECODE"] = "1"
        processes = [subprocess.Popen([sys.executable, "-B", "-c", script, str(path)], cwd=str(Path(__file__).resolve().parents[3]), env=environment) for _ in range(3)]
        self.assertEqual([0, 0, 0], [process.wait(timeout=20) for process in processes])
        self.assertEqual(190, AtomicJsonStore(path).read()["count"])

    def test_atomic_store_cross_process_lock_timeout_is_bounded(self) -> None:
        path = self.root / "locked.json"
        ready = self.root / "ready.flag"
        AtomicJsonStore(path).write({"value": 1})
        script = (
            "from pathlib import Path; import sys,time; "
            "from tools.game_mode_controller.atomic_store import AtomicJsonStore; "
            "s=AtomicJsonStore(sys.argv[1]); c=s._locked(); c.__enter__(); Path(sys.argv[2]).write_text('ready'); time.sleep(2); c.__exit__(None,None,None)"
        )
        process = subprocess.Popen([sys.executable, "-B", "-c", script, str(path), str(ready)], cwd=str(Path(__file__).resolve().parents[3]))
        try:
            for _ in range(100):
                if ready.exists():
                    break
                threading.Event().wait(0.01)
            self.assertTrue(ready.exists())
            started = __import__("time").monotonic()
            with self.assertRaisesRegex(StateStoreError, "lock timed out"):
                AtomicJsonStore(path, lock_timeout_seconds=0.2).read()
            self.assertLess(__import__("time").monotonic() - started, 1.0)
        finally:
            process.wait(timeout=5)

    def test_windows_replace_retries_transient_access_denied_with_bound(self) -> None:
        source = self.root / "source.tmp"
        destination = self.root / "destination.json"
        source.write_text("new", encoding="utf-8")
        calls = {"count": 0}
        real_replace = os.replace

        def flaky_replace(left, right):
            calls["count"] += 1
            if calls["count"] <= 3:
                failure = PermissionError(5, "transient")
                failure.winerror = 5
                raise failure
            return real_replace(left, right)

        with mock.patch("tools.game_mode_controller.atomic_store.os.name", "nt"), mock.patch("tools.game_mode_controller.atomic_store.os.replace", side_effect=flaky_replace):
            replace_with_retry(source, destination, timeout_seconds=1.0)
        self.assertEqual(4, calls["count"])
        self.assertEqual("new", destination.read_text(encoding="utf-8"))

    def test_api_requires_signature_and_signs_command_response(self) -> None:
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "hub-api.json"), clock=lambda: NOW)
        authenticator = RequestAuthenticator(lambda node: SECRET, ReplayLedger(self.root / "api-nonce.sqlite3"), clock=lambda: 1000)
        api = HubApi(coordinator, authenticator, lambda node: SECRET)
        body = json.dumps({"target_node_id": "main", "game_mode": True}, separators=(",", ":")).encode()
        nonce = "abcdefghijklmnopqrstuv"
        headers = sign_request(SECRET, "taichung", "POST", "/v1/command", 1000, nonce, body)
        response = api.dispatch("POST", "/v1/command", headers, body)
        self.assertEqual(200, response.status)
        self.assertTrue(verify_response(SECRET, "taichung", nonce, response.body, response.headers["X-Aevrynd-Response-Signature"]))
        self.assertEqual(401, api.dispatch("POST", "/v1/command", {}, body).status)


class FakeHttpResponse:
    def __init__(self, status: int, value: object) -> None:
        self.status = status
        self.raw = json.dumps(value).encode()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        return False

    def getcode(self) -> int:
        return self.status

    def read(self, maximum: int) -> bytes:
        return self.raw


class AcceptanceTests(unittest.TestCase):
    def test_redirect_is_permanently_rejected_without_reaching_location(self) -> None:
        reached = {"redirect": 0, "authorization": None}

        class Handler(BaseHTTPRequestHandler):
            def do_GET(self):
                if self.path == "/health":
                    reached["authorization"] = self.headers.get("Authorization")
                    self.send_response(302)
                    self.send_header("Location", "http://127.0.0.1:8081/redirect-target")
                    self.end_headers()
                else:
                    reached["redirect"] += 1
                    self.send_response(200)
                    self.end_headers()

            def log_message(self, format, *args):
                return

        try:
            server = ThreadingHTTPServer(("127.0.0.1", 8081), Handler)
        except OSError as error:
            self.skipTest(f"fixed test port unavailable: {error}")
        thread = threading.Thread(target=server.serve_forever, daemon=True)
        thread.start()
        try:
            with self.assertRaisesRegex(AcceptanceError, "redirect is permanently forbidden"):
                QwenAcceptanceProbe("http://127.0.0.1:8081", "model", SECRET).run()
        finally:
            server.shutdown()
            server.server_close()
            thread.join(timeout=5)
        self.assertEqual(f"Bearer {SECRET}", reached["authorization"])
        self.assertEqual(0, reached["redirect"])

    def test_line_cross_origin_redirect_never_receives_authorization(self) -> None:
        reached = {"source_auth": None, "target_calls": 0, "target_auth": None}

        class TargetHandler(BaseHTTPRequestHandler):
            def do_POST(self):
                reached["target_calls"] += 1
                reached["target_auth"] = self.headers.get("Authorization")
                self.send_response(200)
                self.end_headers()

            def log_message(self, format, *args):
                return

        target = ThreadingHTTPServer(("127.0.0.1", 0), TargetHandler)
        target_url = f"http://127.0.0.1:{target.server_port}/stolen"

        class SourceHandler(BaseHTTPRequestHandler):
            def do_POST(self):
                reached["source_auth"] = self.headers.get("Authorization")
                self.send_response(307)
                self.send_header("Location", target_url)
                self.end_headers()

            def log_message(self, format, *args):
                return

        source = ThreadingHTTPServer(("127.0.0.1", 0), SourceHandler)
        threads = [threading.Thread(target=server.serve_forever, daemon=True) for server in (source, target)]
        for thread in threads:
            thread.start()
        try:
            client = NoRedirectLineClient(SECRET, api_root=f"http://127.0.0.1:{source.server_port}/push", allow_test_root=True)
            with self.assertRaisesRegex(PermanentLineError, "redirect is permanently forbidden"):
                client.push("group", "message", "11111111-1111-4111-8111-111111111111")
        finally:
            source.shutdown()
            target.shutdown()
            source.server_close()
            target.server_close()
            for thread in threads:
                thread.join(timeout=5)
        self.assertEqual(f"Bearer {SECRET}", reached["source_auth"])
        self.assertEqual(0, reached["target_calls"])
        self.assertIsNone(reached["target_auth"])

    def test_all_four_acceptance_checks_are_required(self) -> None:
        paths: list[str] = []

        def opener(req, timeout):
            paths.append(req.full_url)
            if req.full_url.endswith("/health"):
                return FakeHttpResponse(200, {"status": "ok"})
            if req.full_url.endswith("/v1/models"):
                return FakeHttpResponse(200, {"data": [{"id": "light-model"}]})
            if req.full_url.endswith("/slots"):
                return FakeHttpResponse(200, [{"id": 0}])
            return FakeHttpResponse(200, {"choices": [{"message": {"content": "QWEN_LIGHT_OK"}}]})

        QwenAcceptanceProbe("http://127.0.0.1:8080", "light-model", None, opener=opener).run()
        self.assertEqual(4, len(paths))

    def test_multi_slot_fails(self) -> None:
        def opener(req, timeout):
            if req.full_url.endswith("/health"):
                return FakeHttpResponse(200, {"status": "ok"})
            if req.full_url.endswith("/v1/models"):
                return FakeHttpResponse(200, {"data": [{"id": "model"}]})
            return FakeHttpResponse(200, [{"id": 0}, {"id": 1}])

        with self.assertRaises(AcceptanceError):
            QwenAcceptanceProbe("http://127.0.0.1:8080", "model", None, opener=opener).run()


class InstallerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.source = self.root / "source"
        self.install = self.root / "install"
        (self.source / "scripts").mkdir(parents=True)
        (self.source / "a.py").write_bytes(b"new-app")
        (self.source / "scripts" / "launch.ps1").write_bytes(b"new-script")
        entries = [
            {"source": "a.py", "destination": "app/tools/game_mode_controller/a.py", "size": 7, "sha256": hashlib.sha256(b"new-app").hexdigest()},
            {"source": "scripts/launch.ps1", "destination": "scripts/launch.ps1", "size": 10, "sha256": hashlib.sha256(b"new-script").hexdigest()},
        ]
        self.manifest = self.source / "source-manifest.json"
        self.manifest.write_text(json.dumps({"schema_version": 1, "files": entries}, separators=(",", ":")), encoding="utf-8")
        self.manifest_hash = hashlib.sha256(self.manifest.read_bytes()).hexdigest()

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _entries(self):
        return load_source_manifest(self.source, self.manifest, self.manifest_hash)

    def test_manifest_hash_and_copy_time_toctou_are_rejected(self) -> None:
        with self.assertRaises(InstallError):
            load_source_manifest(self.source, self.manifest, "0" * 64)
        entries = self._entries()
        (self.source / "a.py").write_bytes(b"tampered")
        with self.assertRaises(InstallError):
            InstallTransaction(self.source, self.install, entries).install()
        self.assertFalse((self.install / "app").exists())
        staging = self.install / "staging"
        self.assertFalse(staging.exists() and any(staging.iterdir()))

    def test_manifest_hash_and_parse_use_one_immutable_read(self) -> None:
        original_read = Path.read_bytes
        reads = {"manifest": 0}

        def hostile_read(path: Path) -> bytes:
            data = original_read(path)
            if path.resolve() == self.manifest.resolve():
                reads["manifest"] += 1
                self.manifest.write_text('{"schema_version":999,"files":[]}', encoding="utf-8")
            return data

        with mock.patch.object(Path, "read_bytes", hostile_read):
            entries = load_source_manifest(self.source, self.manifest, self.manifest_hash)
        self.assertEqual(1, reads["manifest"])
        self.assertEqual(2, len(entries))

    def test_repository_manifest_matches_all_sources_and_pinned_installer_digest(self) -> None:
        package = Path(__file__).resolve().parents[1]
        manifest = package / "source-manifest.json"
        digest = hashlib.sha256(manifest.read_bytes()).hexdigest()
        installer = (package / "scripts" / "Install.ps1").read_text(encoding="utf-8-sig")
        self.assertIn(digest, installer)
        entries = load_source_manifest(package, manifest, digest)
        expected_destinations = {entry.destination for entry in entries}
        self.assertIn("app/tools/game_mode_controller/atomic_store.py", expected_destinations)
        self.assertIn("scripts/GameModeTray.ps1", expected_destinations)

    def test_failure_after_component_swap_rolls_back_all_old_components(self) -> None:
        for component in ("app", "scripts", "manifests"):
            directory = self.install / component
            directory.mkdir(parents=True)
            (directory / "old.txt").write_text(component, encoding="utf-8")

        def fail(component: str) -> None:
            if component == "app":
                raise RuntimeError("injected failure")

        with self.assertRaises(RuntimeError):
            InstallTransaction(self.source, self.install, self._entries()).install(failure_hook=fail)
        for component in ("app", "scripts", "manifests"):
            self.assertEqual(component, (self.install / component / "old.txt").read_text(encoding="utf-8"))

    def test_deferred_post_install_failure_rolls_back_prepared_core(self) -> None:
        for component in ("app", "scripts", "manifests"):
            directory = self.install / component
            directory.mkdir(parents=True)
            (directory / "old.txt").write_text(component, encoding="utf-8")
        transaction = InstallTransaction(self.source, self.install, self._entries())
        transaction.install(defer_commit=True)
        self.assertTrue((self.install / "transactions" / "install-transaction.json").exists())
        transaction.rollback()
        for component in ("app", "scripts", "manifests"):
            self.assertEqual(component, (self.install / component / "old.txt").read_text(encoding="utf-8"))

    def test_hard_crash_journal_is_recovered_on_next_run(self) -> None:
        for component in ("app", "scripts", "manifests"):
            directory = self.install / component
            directory.mkdir(parents=True)
            (directory / "old.txt").write_text(component, encoding="utf-8")
        script = (
            "from pathlib import Path; import hashlib,os,sys; "
            "from tools.game_mode_controller.installer_transaction import load_source_manifest,InstallTransaction; "
            "s=Path(sys.argv[1]); m=s/'source-manifest.json'; e=load_source_manifest(s,m,sys.argv[3]); "
            "InstallTransaction(s,sys.argv[2],e).install(failure_hook=lambda c: os._exit(9) if c=='app' else None)"
        )
        process = subprocess.run([sys.executable, "-B", "-c", script, str(self.source), str(self.install), self.manifest_hash], cwd=str(Path(__file__).resolve().parents[3]), timeout=20)
        self.assertEqual(9, process.returncode)
        InstallTransaction(self.source, self.install, self._entries()).recover()
        for component in ("app", "scripts", "manifests"):
            self.assertEqual(component, (self.install / component / "old.txt").read_text(encoding="utf-8"))

    def test_install_wide_cross_process_mutex_survives_repeated_pressure(self) -> None:
        script = (
            "from pathlib import Path; import sys; "
            "from tools.game_mode_controller.installer_transaction import load_source_manifest,InstallTransaction; "
            "s=Path(sys.argv[1]); m=s/'source-manifest.json'; e=load_source_manifest(s,m,sys.argv[3]); InstallTransaction(s,sys.argv[2],e).install()"
        )
        for _ in range(3):
            processes = [subprocess.Popen([sys.executable, "-B", "-c", script, str(self.source), str(self.install), self.manifest_hash], cwd=str(Path(__file__).resolve().parents[3])) for _ in range(3)]
            self.assertEqual([0, 0, 0], [process.wait(timeout=20) for process in processes])
        self.assertEqual(b"new-app", (self.install / "app" / "tools" / "game_mode_controller" / "a.py").read_bytes())
        self.assertEqual(b"new-script", (self.install / "scripts" / "launch.ps1").read_bytes())
        self.assertFalse((self.install / "transactions" / "install-transaction.json").exists())


if __name__ == "__main__":
    unittest.main()
