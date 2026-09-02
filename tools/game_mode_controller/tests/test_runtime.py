from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import unittest
from contextlib import contextmanager
from pathlib import Path
from unittest.mock import patch

from tools.game_mode_controller.api import HubApi
from tools.game_mode_controller.acceptance import QwenAcceptanceProbe
from tools.game_mode_controller.atomic_store import AtomicJsonStore
from tools.game_mode_controller.coordinator import HubCoordinator
from tools.game_mode_controller.deployment_transaction import DeploymentTransaction
from tools.game_mode_controller.models import GameModeState, QwenState
from tools.game_mode_controller.process_guard import ObservedProcess, ProcessIdentityError, command_digest, file_sha256, load_runtime_manifest
from tools.game_mode_controller.runner import MainAgent, SignedHubClient, load_runner_config, make_hub_server
from tools.game_mode_controller.security import ReplayLedger, RequestAuthenticator
from tools.game_mode_controller.state_machine import GameModeController
from tools.game_mode_controller.windows_runtime import WindowsQwenLifecycle, capture_main_runtime_manifest, minimal_runtime_environment

SECRET = "runtime-test-secret-value-123456789012345"


class FakeInspector:
    def __init__(self, observed: ObservedProcess) -> None:
        self.observed = observed

    def inspect(self, pid: int) -> ObservedProcess:
        if pid != self.observed.pid:
            raise ProcessIdentityError("missing")
        return self.observed


class FakeProcess:
    pid = 7712

    def __init__(self) -> None:
        self.terminated = False

    def terminate(self) -> None:
        self.terminated = True


class FakeProbe:
    def __init__(self) -> None:
        self.calls = 0

    def run(self) -> None:
        self.calls += 1


class RuntimeTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.executable = self.root / "llama-server.exe"
        self.model = self.root / "Qwen3-4B-Q4_K_M.gguf"
        self.executable.write_bytes(b"runtime")
        self.model.write_bytes(b"model")
        self.argv = (
            str(self.executable), "--model", str(self.model), "--host", "127.0.0.1",
            "--port", "18080", "--parallel", "1", "--threads", "4",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _manifest_document(self) -> dict[str, object]:
        return {
            "schema_version": 1,
            "runtime": {
                "node_id": "main", "profile": "light", "owner": "DOMAIN\\User",
                "executable_path": str(self.executable), "executable_sha256": file_sha256(self.executable),
                "argv": list(self.argv), "command_sha256": command_digest(self.argv),
                "model_path": str(self.model), "model_sha256": file_sha256(self.model),
                "model_id": "Qwen3-4B-Q4_K_M.gguf",
            },
        }

    def _write_manifest(self, document: dict[str, object] | None = None) -> tuple[Path, str]:
        path = self.root / "runtime-manifest.json"
        raw = json.dumps(document or self._manifest_document(), separators=(",", ":")).encode()
        path.write_bytes(raw)
        return path, hashlib.sha256(raw).hexdigest()

    def test_strict_manifest_requires_pin_closed_schema_endpoint_and_files(self) -> None:
        path, digest = self._write_manifest()
        manifest = load_runtime_manifest(path, expected_node_id="main", expected_sha256=digest)
        self.assertEqual("18080", manifest.argv[manifest.argv.index("--port") + 1])
        with self.assertRaises(ProcessIdentityError):
            load_runtime_manifest(path, expected_node_id="main", expected_sha256="0" * 64)
        changed = self._manifest_document()
        changed["runtime"]["argv"][changed["runtime"]["argv"].index("18080")] = "8080"  # type: ignore[index,union-attr]
        changed["runtime"]["command_sha256"] = command_digest(changed["runtime"]["argv"])  # type: ignore[index,arg-type]
        bad_path, bad_digest = self._write_manifest(changed)
        with self.assertRaisesRegex(ProcessIdentityError, "endpoint contract"):
            load_runtime_manifest(bad_path, expected_node_id="main", expected_sha256=bad_digest)

    def test_config_is_main_only_and_manifest_must_be_direct_child(self) -> None:
        install = self.root / "install"
        (install / "config").mkdir(parents=True)
        (install / "manifests").mkdir()
        python = self.root / "python.exe"
        python.write_bytes(b"python")
        source, digest = self._write_manifest()
        target = install / "manifests" / "runtime-manifest.json"
        target.write_bytes(source.read_bytes())
        config = {"schemaVersion": 2, "nodeId": "main", "runtimeManifest": "runtime-manifest.json", "runtimeManifestSha256": digest, "pythonExecutable": str(python)}
        (install / "config" / "node.json").write_text(json.dumps(config), encoding="utf-8")
        loaded, _ = load_runner_config(install, allow_test_root=True)
        self.assertEqual("main", loaded.node_id)
        config["runtimeManifest"] = "../runtime-manifest.json"
        (install / "config" / "node.json").write_text(json.dumps(config), encoding="utf-8")
        with self.assertRaisesRegex(Exception, "not allowlisted"):
            load_runner_config(install, allow_test_root=True)

    def test_launcher_persists_only_identity_checked_pid_and_injects_api_key(self) -> None:
        path, digest = self._write_manifest()
        manifest = load_runtime_manifest(path, expected_node_id="main", expected_sha256=digest)
        observed = ObservedProcess(7712, "domain\\user", str(self.executable), self.argv, file_sha256(self.executable))
        launched: dict[str, object] = {}
        process = FakeProcess()

        def launch(*args, **kwargs):
            launched.update(kwargs)
            return process

        probe = FakeProbe()
        lifecycle = WindowsQwenLifecycle(manifest, AtomicJsonStore(self.root / "pid.json"), inspector=FakeInspector(observed), launcher=launch, probe=probe, api_key=SECRET, port_in_use=lambda: False)
        lifecycle.start_validated("light")
        self.assertEqual(7712, lifecycle.pid_store.read()["pid"])
        self.assertEqual(SECRET, launched["env"]["LLAMA_API_KEY"])
        self.assertTrue(lifecycle.is_running())
        lifecycle.accept()
        self.assertEqual(1, probe.calls)

    def test_child_environment_is_allowlisted_and_drops_all_controller_line_and_unrelated_secrets(self) -> None:
        source = {
            "SystemRoot": "C:\\Windows", "TEMP": "C:\\Temp", "PATH": "unsafe-path",
            "AEVRYND_GAME_MODE_SECRET_MAIN": "controller-secret", "AEVRYND_LINE_TOKEN": "line-secret",
            "OPENAI_API_KEY": "unrelated-secret", "LLAMA_API_KEY": "stale-secret",
        }
        result = minimal_runtime_environment("only-qwen-secret", source)
        self.assertEqual({"SystemRoot": "C:\\Windows", "TEMP": "C:\\Temp", "LLAMA_API_KEY": "only-qwen-secret"}, result)

    def test_stop_validates_and_terminates_the_same_held_process_handle(self) -> None:
        path, digest = self._write_manifest()
        manifest = load_runtime_manifest(path, expected_node_id="main", expected_sha256=digest)
        valid = ObservedProcess(7712, "DOMAIN\\User", str(self.executable), self.argv, file_sha256(self.executable))

        class HeldInspector:
            def __init__(self, replacement: bool = False) -> None:
                self.events: list[str] = []
                self.replacement = replacement

            def inspect(self, pid: int) -> ObservedProcess:
                self.events.append("unsafe_pid_inspect")
                return valid

            @contextmanager
            def open_for_termination(self, pid: int):
                self.events.append("open")
                yield object()
                self.events.append("close")

            def inspect_opened(self, handle: object, pid: int) -> ObservedProcess:
                self.events.append("inspect_opened")
                return ObservedProcess(pid, valid.owner, valid.executable_path, valid.argv + (("--replacement",) if self.replacement else ()), valid.executable_sha256)

            def terminate_opened(self, handle: object) -> None:
                self.events.append("terminate_opened")

        pid_store = AtomicJsonStore(self.root / "held-pid.json"); pid_store.write({"schema_version": 1, "pid": 7712})
        inspector = HeldInspector()
        WindowsQwenLifecycle(manifest, pid_store, inspector=inspector, probe=FakeProbe()).stop_validated()
        self.assertEqual(["open", "inspect_opened", "terminate_opened", "close"], inspector.events)
        pid_store.write({"schema_version": 1, "pid": 7712})
        hostile = HeldInspector(replacement=True)
        with self.assertRaises(ProcessIdentityError):
            WindowsQwenLifecycle(manifest, pid_store, inspector=hostile, probe=FakeProbe()).stop_validated()
        self.assertNotIn("terminate_opened", hostile.events)

    @unittest.skipUnless(os.name == "nt", "Windows CIM path contract")
    def test_process_inspector_cim_reader_does_not_depend_on_path(self) -> None:
        from tools.game_mode_controller.windows_runtime import WindowsProcessInspector

        with patch.dict(os.environ, {"PATH": ""}, clear=False):
            command_line = WindowsProcessInspector._read_command_line(os.getpid())
        self.assertTrue(command_line)

    @unittest.skipUnless(os.name == "nt", "Windows held-file replacement contract")
    def test_held_file_identity_hash_blocks_replacement_until_release(self) -> None:
        from tools.game_mode_controller.windows_runtime import _held_file_pair

        original = self.root / "held.bin"; alias = self.root / "held-alias.bin"; replacement = self.root / "replacement.bin"
        original.write_bytes(b"trusted-bytes"); os.link(original, alias); replacement.write_bytes(b"hostile-bytes")
        with _held_file_pair(original, alias) as (size, digest):
            self.assertEqual(len(b"trusted-bytes"), size)
            self.assertEqual(hashlib.sha256(b"trusted-bytes").hexdigest(), digest)
            with self.assertRaises(PermissionError):
                os.replace(replacement, alias)
        os.replace(replacement, alias)
        self.assertEqual(b"hostile-bytes", alias.read_bytes())

    def test_existing_light_state_is_adopted_only_after_full_manifest_gate(self) -> None:
        path, digest = self._write_manifest()
        manifest = load_runtime_manifest(path, expected_node_id="main", expected_sha256=digest)
        legacy = self.root / "light-state.json"
        legacy.write_text(json.dumps({
            "schemaVersion": 1, "pid": 7712, "startedUtc": "2026-09-03T00:00:00Z",
            "executablePath": str(self.executable), "modelSha256": file_sha256(self.model),
            "host": "127.0.0.1", "port": 18080, "context": 2048, "parallel": 1,
            "threads": 4, "gpuLayers": 0,
        }), encoding="utf-8")
        observed = ObservedProcess(7712, "DOMAIN\\User", str(self.executable), self.argv, file_sha256(self.executable))
        lifecycle = WindowsQwenLifecycle(manifest, AtomicJsonStore(self.root / "pid.json"), inspector=FakeInspector(observed), probe=FakeProbe(), adoption_state=legacy)
        self.assertTrue(lifecycle.is_running())
        self.assertIsNone(lifecycle.pid_store.read())

    def test_capture_existing_runtime_emits_reloadable_pinned_manifest(self) -> None:
        runtime_root = self.root / "capture-runtime"
        executable = runtime_root / "runtime" / "llama-server.exe"
        model = runtime_root / "models" / "Qwen3-4B-Q4_K_M.gguf"
        executable.parent.mkdir(parents=True); model.parent.mkdir(parents=True)
        executable.write_bytes(b"runtime"); model.write_bytes(b"model")
        argv = (str(executable), "--model", str(model), "--host", "127.0.0.1", "--port", "18080", "--parallel", "1")
        legacy = self.root / "light-state.json"
        legacy.write_text(json.dumps({
            "schemaVersion": 1, "pid": 7712, "startedUtc": "2026-09-03T00:00:00Z",
            "executablePath": str(executable), "modelSha256": file_sha256(model),
            "host": "127.0.0.1", "port": 18080, "context": 2048, "parallel": 1,
            "threads": 4, "gpuLayers": 0,
        }), encoding="utf-8")
        observed = ObservedProcess(7712, "DOMAIN\\User", str(executable), argv, file_sha256(executable))
        output = self.root / "captured.json"
        digest = capture_main_runtime_manifest(output, "Qwen3-4B-Q4_K_M.gguf", state_path=legacy, inspector=FakeInspector(observed))
        loaded = load_runtime_manifest(output, expected_node_id="main", expected_sha256=digest)
        self.assertEqual(observed.argv, loaded.argv)

    def test_msix_capture_uses_file_identity_for_mixed_path_view(self) -> None:
        package = self.root / "msix-physical"
        scheduled = self.root / "scheduled-view"
        executable = package / "bin" / "llama-server.exe"
        physical_model = package / "models" / "Qwen3-4B-Q4_K_M.gguf"
        scheduled_executable = scheduled / "bin" / "llama-server.exe"
        scheduled_model = scheduled / "models" / "Qwen3-4B-Q4_K_M.gguf"
        executable.parent.mkdir(parents=True); physical_model.parent.mkdir(parents=True)
        scheduled_executable.parent.mkdir(parents=True); scheduled_model.parent.mkdir(parents=True)
        executable.write_bytes(b"runtime"); physical_model.write_bytes(b"model")
        os.link(executable, scheduled_executable); os.link(physical_model, scheduled_model)
        api_model_id = str(scheduled_model)
        argv = (str(scheduled_executable), "--model", api_model_id, "--host", "127.0.0.1", "--port", "18080", "--parallel", "1")
        legacy = self.root / "msix-state.json"
        legacy.write_text(json.dumps({
            "schemaVersion": 1, "pid": 7712, "startedUtc": "2026-09-03T00:00:00Z",
            "executablePath": str(executable), "modelSha256": file_sha256(physical_model),
            "host": "127.0.0.1", "port": 18080, "context": 2048, "parallel": 1,
            "threads": 4, "gpuLayers": 0,
        }), encoding="utf-8")
        observed = ObservedProcess(7712, "DOMAIN\\User", str(executable), argv, file_sha256(executable))
        output = self.root / "msix-captured.json"
        digest = capture_main_runtime_manifest(output, api_model_id, state_path=legacy, inspector=FakeInspector(observed))
        loaded = load_runtime_manifest(output, expected_node_id="main", expected_sha256=digest)
        self.assertEqual(str(scheduled_model), loaded.model_path)
        self.assertEqual(str(scheduled_executable), loaded.executable_path)
        self.assertEqual(api_model_id, loaded.model_id)
        lifecycle = WindowsQwenLifecycle(loaded, AtomicJsonStore(self.root / "mixed-pid.json"),
                                         inspector=FakeInspector(observed), probe=FakeProbe(), adoption_state=legacy)
        self.assertTrue(lifecycle.is_running())

        spoof_executable = self.root / "spoof" / "bin" / "llama-server.exe"
        spoof_executable.parent.mkdir(parents=True); spoof_executable.write_bytes(executable.read_bytes())
        spoof_argv = (str(spoof_executable), *argv[1:])
        spoof = ObservedProcess(7712, "DOMAIN\\User", str(executable), spoof_argv, file_sha256(executable))
        with self.assertRaisesRegex(ProcessIdentityError, "identity"):
            capture_main_runtime_manifest(self.root / "spoof.json", api_model_id, state_path=legacy, inspector=FakeInspector(spoof))

        model_spoof_root = self.root / "model-spoof"
        model_spoof_executable = model_spoof_root / "bin" / "llama-server.exe"
        model_spoof = model_spoof_root / "models" / physical_model.name
        model_spoof_executable.parent.mkdir(parents=True); model_spoof.parent.mkdir(parents=True)
        os.link(executable, model_spoof_executable); model_spoof.write_bytes(physical_model.read_bytes())
        model_spoof_argv = (str(model_spoof_executable), "--model", str(model_spoof), *argv[3:])
        observed_model_spoof = ObservedProcess(7712, "DOMAIN\\User", str(executable), model_spoof_argv, file_sha256(executable))
        with self.assertRaisesRegex(ProcessIdentityError, "identity"):
            capture_main_runtime_manifest(self.root / "model-spoof.json", str(model_spoof), state_path=legacy, inspector=FakeInspector(observed_model_spoof))

        class Response:
            def __init__(self, value: object) -> None: self.value = value
            def __enter__(self): return self
            def __exit__(self, *args): return None
            def getcode(self) -> int: return 200
            def read(self, _limit: int) -> bytes: return json.dumps(self.value).encode()

        def opener(request, timeout):
            del timeout
            if request.full_url.endswith("/health"): return Response({"status": "ok"})
            if request.full_url.endswith("/v1/models"): return Response({"data": [{"id": api_model_id}]})
            if request.full_url.endswith("/slots"): return Response([{"id": 0}])
            return Response({"choices": [{"message": {"content": "QWEN_LIGHT_OK"}}]})

        QwenAcceptanceProbe("http://127.0.0.1:18080", loaded.model_id, None, opener=opener).run()

    def test_degraded_agent_retries_with_backoff_but_never_during_game_mode(self) -> None:
        class TransientLifecycle:
            def __init__(self) -> None:
                self.running = False; self.starts = 0; self.stops = 0; self.accepts = 0
            def is_running(self) -> bool: return self.running
            def start_validated(self, profile: str) -> None:
                self.starts += 1; self.running = True
            def stop_validated(self) -> None:
                self.stops += 1; self.running = False
            def accept(self) -> None:
                self.accepts += 1
                if self.accepts == 1: raise RuntimeError("transient busy")

        now = [0.0]
        lifecycle = TransientLifecycle()
        controller = GameModeController("main", AtomicJsonStore(self.root / "retry-state.json"), lifecycle)
        agent = MainAgent(controller, object(), interval_seconds=1, retry_initial_seconds=5,
                          retry_max_seconds=20, monotonic=lambda: now[0])  # type: ignore[arg-type]
        state = controller.reconcile_boot()
        self.assertEqual(QwenState.DEGRADED, state.qwen_state)
        self.assertEqual("reconcile_accept_failed", state.detail_code)
        self.assertEqual((1, 1), (lifecycle.starts, lifecycle.accepts))
        state = agent.retry_degraded(state)  # schedules, but does not busy-loop
        for timestamp in (1.0, 2.0, 4.99):
            now[0] = timestamp; state = agent.retry_degraded(state)
        self.assertEqual((1, 1), (lifecycle.starts, lifecycle.accepts))
        now[0] = 5.0; state = agent.retry_degraded(state)
        self.assertEqual(QwenState.RUNNING, state.qwen_state)
        self.assertEqual((1, 2), (lifecycle.starts, lifecycle.accepts))

        game_state = GameModeState("main", True, QwenState.DEGRADED, "light", 1,
                                   "2026-09-03T00:00:00Z", "reconcile_failed")
        lifecycle.running = True
        before = (lifecycle.starts, lifecycle.stops, lifecycle.accepts)
        now[0] = 100.0
        self.assertIs(game_state, agent.retry_degraded(game_state))
        self.assertEqual(before, (lifecycle.starts, lifecycle.stops, lifecycle.accepts))

        class InspectFailure(TransientLifecycle):
            def is_running(self) -> bool: raise RuntimeError("hidden diagnostic")

        inspect_controller = GameModeController("main", AtomicJsonStore(self.root / "inspect-stage.json"), InspectFailure())
        inspect_state = inspect_controller.reconcile_boot()
        self.assertEqual("reconcile_inspect_failed", inspect_state.detail_code)
        self.assertNotIn("hidden", inspect_state.detail_code)

    def test_signed_loopback_client_rejects_unsigned_and_accepts_signed_status(self) -> None:
        import socket
        guard = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
                guard.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            guard.bind(("127.0.0.1", 8766))
        except OSError as error:
            guard.close()
            self.skipTest(f"fixed hub port already owned by a live controller: {error}")
        guard.close()
        coordinator = HubCoordinator(AtomicJsonStore(self.root / "hub.json"))
        api = HubApi(coordinator, RequestAuthenticator(lambda _: SECRET, ReplayLedger(self.root / "nonce.sqlite3")), lambda _: SECRET)
        try:
            server = make_hub_server(api)
        except OSError as error:
            self.skipTest(f"fixed hub port unavailable: {error}")
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            client = SignedHubClient("main", SECRET)
            result = client.call("GET", "/v1/status")
            self.assertEqual({"main", "taichung", "taoyuan"}, set(result))
        finally:
            server.shutdown(); server.server_close(); worker.join(timeout=5)

    def test_powershell_entrypoints_parse_and_tray_has_signed_actions(self) -> None:
        powershell = shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell 7 is unavailable")
        package = Path(__file__).resolve().parents[1]
        scripts = sorted((package / "scripts").glob("*.ps1"))
        for script in scripts:
            command = "$e=$null;$t=$null;[Management.Automation.Language.Parser]::ParseFile($env:AEVRYND_PARSE_FILE,[ref]$t,[ref]$e)|Out-Null;if($e.Count){$e|ForEach-Object{$_.Message};exit 7}"
            environment = dict(os.environ); environment["AEVRYND_PARSE_FILE"] = str(script)
            result = subprocess.run([powershell, "-NoProfile", "-Command", command], timeout=10, env=environment, capture_output=True, text=True)
            self.assertEqual(0, result.returncode, f"{script.name}: {result.stdout} {result.stderr}")
        tray = (package / "scripts" / "GameModeTray.ps1").read_text(encoding="utf-8-sig")
        startup = (package / "scripts" / "Register-Startup.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("Enable game mode (stop Qwen)", tray)
        self.assertIn("tools.game_mode_controller.runner", tray)
        self.assertIn("-RunLevel Limited", startup)
        self.assertIn("ReparsePoint", startup)
        self.assertIn("Scheduled launcher hash validation failed", startup)
        installer = (package / "scripts" / "Install.ps1").read_text(encoding="utf-8-sig")
        for contract in ("SetAccessRuleProtection($true,$false)", "SetOwner", "PurgeAccessRules", "S-1-5-18", "S-1-5-32-544", "ReparsePoint"):
            self.assertIn(contract, installer + (package / "scripts" / "InstallTrust.psm1").read_text(encoding="utf-8-sig"))
        launcher = (package / "scripts" / "Launch-GameModeController.ps1").read_text(encoding="utf-8-sig")
        self.assertEqual(2, launcher.count(".Environment.Clear()"))
        self.assertNotIn("AEVRYND_GAME_MODE_SECRET", launcher)

    def test_real_ancestor_junction_fixture_is_rejected_by_shared_trust_helper(self) -> None:
        powershell = shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell 7 is unavailable")
        trusted = self.root / "fixture-programdata"; outside = self.root / "fixture-outside"
        trusted.mkdir(); outside.mkdir()
        module = Path(__file__).resolve().parents[1] / "scripts" / "InstallTrust.psm1"
        environment = dict(os.environ)
        environment.update({"AEVRYND_TRUST_FIXTURE": str(trusted), "AEVRYND_OUTSIDE_FIXTURE": str(outside), "AEVRYND_TRUST_MODULE": str(module)})
        command = (
            "$junction=Join-Path $env:AEVRYND_TRUST_FIXTURE 'Aevrynd';"
            "New-Item -ItemType Junction -Path $junction -Target $env:AEVRYND_OUTSIDE_FIXTURE|Out-Null;"
            "try{Import-Module $env:AEVRYND_TRUST_MODULE -Force;"
            "try{Assert-NoReparseAncestorChain -TrustedRoot $env:AEVRYND_TRUST_FIXTURE -Target (Join-Path $junction 'GameModeController');exit 9}"
            "catch{if($_.Exception.Message -notmatch 'reparse point'){Write-Error $_;exit 8}}}"
            "finally{Remove-Item -LiteralPath $junction -Force -ErrorAction SilentlyContinue}"
        )
        result = subprocess.run([powershell, "-NoProfile", "-Command", command], env=environment, capture_output=True, text=True, timeout=15)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

    def test_real_ntfs_acl_allows_rx_and_rejects_write_capabilities(self) -> None:
        powershell = shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell 7 is unavailable")
        fixture = self.root / "acl-fixture"; fixture.mkdir()
        module = Path(__file__).resolve().parents[1] / "scripts" / "InstallTrust.psm1"
        environment = dict(os.environ)
        environment.update({"AEVRYND_ACL_FIXTURE": str(fixture), "AEVRYND_TRUST_MODULE": str(module)})
        command = (
            "Import-Module $env:AEVRYND_TRUST_MODULE -Force;"
            "$me=[Security.Principal.WindowsIdentity]::GetCurrent().User;"
            "$everyone=[Security.Principal.SecurityIdentifier]::new('S-1-1-0');"
            "$trusted=@('S-1-5-18','S-1-5-32-544',$me.Value);"
            "$rights=@([Security.AccessControl.FileSystemRights]::ReadAndExecute,[Security.AccessControl.FileSystemRights]::Write,"
            "[Security.AccessControl.FileSystemRights]::Modify,[Security.AccessControl.FileSystemRights]::FullControl,"
            "[Security.AccessControl.FileSystemRights]::Delete,[Security.AccessControl.FileSystemRights]::ChangePermissions);"
            "$module=Get-Module InstallTrust;"
            "for($i=0;$i -lt $rights.Count;$i++){"
            "$acl=[Security.AccessControl.DirectorySecurity]::new();$acl.SetAccessRuleProtection($true,$false);"
            "$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($me,'FullControl','ContainerInherit, ObjectInherit','None','Allow'))|Out-Null;"
            "$acl.AddAccessRule([Security.AccessControl.FileSystemAccessRule]::new($everyone,$rights[$i],'None','None','Allow'))|Out-Null;"
            "[IO.FileSystemAclExtensions]::SetAccessControl([IO.DirectoryInfo]::new($env:AEVRYND_ACL_FIXTURE),$acl);$diskAcl=Get-Acl -LiteralPath $env:AEVRYND_ACL_FIXTURE;"
            "$rejected=$false;try{& $module {param($a,$t) Assert-NoUntrustedWriter -Acl $a -TrustedWriterSids $t} $diskAcl $trusted}catch{$rejected=$true};"
            "if($i -eq 0 -and $rejected){exit 10};if($i -gt 0 -and -not $rejected){exit (10+$i)}}"
        )
        result = subprocess.run([powershell, "-NoProfile", "-Command", command], env=environment, capture_output=True, text=True, timeout=20)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)

        register = (Path(__file__).resolve().parents[1] / "scripts" / "Register-Startup.ps1").read_text(encoding="utf-8-sig")
        self.assertIn("$generatedMutationMask = 0x000D0156", register)
        self.assertIn("Import-Module -Name $trustModule", register)
        self.assertIn("Assert-ClosedDirectoryTrust -Path $path", register)
        self.assertNotIn("FileSystemRights]::Write -bor", register)
        mask_command = (
            "$m=[Security.AccessControl.FileSystemRights]::WriteData -bor [Security.AccessControl.FileSystemRights]::AppendData "
            "-bor [Security.AccessControl.FileSystemRights]::WriteExtendedAttributes -bor [Security.AccessControl.FileSystemRights]::WriteAttributes "
            "-bor [Security.AccessControl.FileSystemRights]::DeleteSubdirectoriesAndFiles -bor [Security.AccessControl.FileSystemRights]::Delete "
            "-bor [Security.AccessControl.FileSystemRights]::ChangePermissions -bor [Security.AccessControl.FileSystemRights]::TakeOwnership;"
            "if([int64]$m -ne 0x000D0156){exit 12}"
        )
        mask_result = subprocess.run([powershell, "-NoProfile", "-Command", mask_command], capture_output=True, text=True, timeout=10)
        self.assertEqual(0, mask_result.returncode, mask_result.stdout + mask_result.stderr)

    def test_bootstrap_snapshots_survive_hostile_manifest_and_module_swap(self) -> None:
        """來源在 snapshot 後被替換時，解析與 stage 仍只能看見已驗證 bytes。"""
        powershell = shutil.which("pwsh")
        if not powershell:
            self.skipTest("PowerShell 7 is unavailable")
        manifest = self.root / "manifest.json"; module = self.root / "trust.psm1"; staged = self.root / "staged.psm1"
        manifest.write_bytes(b'{"schema_version":1,"marker":"trusted"}')
        module.write_bytes(b"$script:TrustedBootstrapMarker = 'trusted'\n")
        environment = dict(os.environ)
        environment.update({"AEVRYND_SWAP_MANIFEST": str(manifest), "AEVRYND_SWAP_MODULE": str(module), "AEVRYND_SWAP_STAGE": str(staged)})
        command = (
            "[byte[]]$m=[IO.File]::ReadAllBytes($env:AEVRYND_SWAP_MANIFEST);"
            "[byte[]]$p=[IO.File]::ReadAllBytes($env:AEVRYND_SWAP_MODULE);"
            "[IO.File]::WriteAllText($env:AEVRYND_SWAP_MANIFEST,'{bad');"
            "[IO.File]::WriteAllText($env:AEVRYND_SWAP_MODULE,\"throw 'swapped'\");"
            "$j=([Text.UTF8Encoding]::new($false,$true)).GetString($m)|ConvertFrom-Json;"
            "$s=[IO.FileStream]::new($env:AEVRYND_SWAP_STAGE,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::None);"
            "try{$s.Write($p,0,$p.Length);$s.Flush($true)}finally{$s.Dispose()};"
            "if($j.marker -cne 'trusted'){exit 7};"
            "if([Convert]::ToBase64String([IO.File]::ReadAllBytes($env:AEVRYND_SWAP_STAGE)) -cne [Convert]::ToBase64String($p)){exit 8}"
        )
        result = subprocess.run([powershell, "-NoProfile", "-Command", command], env=environment, capture_output=True, text=True, timeout=15)
        self.assertEqual(0, result.returncode, result.stdout + result.stderr)
        installer = (Path(__file__).resolve().parents[1] / "scripts" / "Install.ps1").read_text(encoding="utf-8-sig")
        self.assertEqual(1, installer.count("ReadAllBytes($sourceManifest)"))
        self.assertEqual(1, installer.count("ReadAllBytes($trustModule)"))
        self.assertIn("Import-Module -Name $stagedTrustModule", installer)
        self.assertNotIn("Import-Module -Name $trustModule", installer)


class DeploymentTransactionTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(); self.root = Path(self.temporary.name)
        self.source = self.root / "source"; self.install = self.root / "install"; self.desktop = self.root / "desktop"
        (self.source / "scripts").mkdir(parents=True); self.desktop.mkdir()
        (self.source / "a.py").write_bytes(b"new-app"); (self.source / "scripts" / "launch.ps1").write_bytes(b"new-script")
        entries = []
        for source, destination in (("a.py", "app/a.py"), ("scripts/launch.ps1", "scripts/launch.ps1")):
            raw = (self.source / source).read_bytes(); entries.append({"source": source, "destination": destination, "size": len(raw), "sha256": hashlib.sha256(raw).hexdigest()})
        raw_manifest = json.dumps({"schema_version": 1, "files": entries}, separators=(",", ":")).encode()
        (self.source / "source-manifest.json").write_bytes(raw_manifest); self.source_hash = hashlib.sha256(raw_manifest).hexdigest()
        self.node = self.root / "node.json"; self.runtime = self.root / "runtime.json"; self.shortcut = self.root / "new.lnk"; self.shortcut_destination = self.desktop / "Aevrynd Game Mode.lnk"
        self.node.write_bytes(b"new-node"); self.runtime.write_bytes(b"new-runtime"); self.shortcut.write_bytes(b"new-shortcut")
        for component in ("app", "scripts", "config", "manifests"):
            (self.install / component).mkdir(parents=True, exist_ok=True)
        (self.install / "app" / "old").write_bytes(b"old-app"); (self.install / "scripts" / "old").write_bytes(b"old-script")
        (self.install / "config" / "node.json").write_bytes(b"old-node"); (self.install / "manifests" / "runtime-manifest.json").write_bytes(b"old-runtime"); self.shortcut_destination.write_bytes(b"old-shortcut")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _transaction(self) -> DeploymentTransaction:
        return DeploymentTransaction(self.source, self.install, self.source_hash, self.shortcut_destination)

    def test_hard_crash_during_post_asset_switch_recovers_every_old_component(self) -> None:
        script = (
            "import os,sys; from tools.game_mode_controller.deployment_transaction import DeploymentTransaction; "
            "t=DeploymentTransaction(sys.argv[1],sys.argv[2],sys.argv[3],sys.argv[7]); t.prepare(sys.argv[4],sys.argv[5],sys.argv[6],sys.argv[7],failure_hook=lambda n: os._exit(19) if n=='runtime' else None)"
        )
        process = subprocess.run([sys.executable, "-B", "-c", script, str(self.source), str(self.install), self.source_hash, str(self.node), str(self.runtime), str(self.shortcut), str(self.shortcut_destination)], cwd=str(Path(__file__).resolve().parents[3]), timeout=20)
        self.assertEqual(19, process.returncode)
        self._transaction().recover()
        self.assertEqual(b"old-app", (self.install / "app" / "old").read_bytes())
        self.assertEqual(b"old-node", (self.install / "config" / "node.json").read_bytes())
        self.assertEqual(b"old-runtime", (self.install / "manifests" / "runtime-manifest.json").read_bytes())
        self.assertEqual(b"old-shortcut", self.shortcut_destination.read_bytes())

    def test_power_loss_after_commit_intent_recovers_forward_as_one_deployment(self) -> None:
        transaction = self._transaction(); transaction.prepare(self.node, self.runtime, self.shortcut, self.shortcut_destination)
        document = transaction.journal.read(); document["status"] = "committing"; transaction.journal.write(document)
        self._transaction().recover()
        self.assertEqual(b"new-app", (self.install / "app" / "a.py").read_bytes())
        self.assertEqual(b"new-node", (self.install / "config" / "node.json").read_bytes())
        self.assertEqual(b"new-runtime", (self.install / "manifests" / "runtime-manifest.json").read_bytes())
        self.assertEqual(b"new-shortcut", self.shortcut_destination.read_bytes())
        self.assertFalse((self.install / "transactions" / "deployment-transaction.json").exists())

    def test_writable_journal_cannot_redirect_elevated_recovery(self) -> None:
        transaction = self._transaction(); transaction.prepare(self.node, self.runtime, self.shortcut, self.shortcut_destination)
        document = transaction.journal.read(); document["assets"][0]["backup"] = str(self.root / "outside-target")
        transaction.journal.write(document)
        with self.assertRaisesRegex(Exception, "escaped allowlist"):
            self._transaction().recover()


if __name__ == "__main__":
    unittest.main()
