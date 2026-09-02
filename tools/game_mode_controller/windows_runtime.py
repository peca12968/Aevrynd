"""Fail-closed Windows process inspection, launch and termination adapter."""

from __future__ import annotations

import base64
import ctypes
import hashlib
import json
import os
import socket
import subprocess
import time
from contextlib import contextmanager
from ctypes import wintypes
from pathlib import Path
from typing import Callable

from .acceptance import QwenAcceptanceProbe
from .atomic_store import AtomicJsonStore
from .process_guard import ObservedProcess, ProcessIdentityError, RuntimeManifest, canonical_path, command_digest, file_sha256, validate_stop_eligibility


class WindowsProcessInspector:
    """Inspect a fixed PID using Win32 handles and CIM's exact CommandLine field."""

    def __init__(self, *, command_line_reader: Callable[[int], str] | None = None) -> None:
        self._command_line_reader = command_line_reader or self._read_command_line

    def inspect(self, pid: int) -> ObservedProcess:
        if os.name != "nt" or type(pid) is not int or pid <= 0:
            raise ProcessIdentityError("Windows process inspection is unavailable")
        with self.open_for_termination(pid, terminate_access=False) as handle:
            return self.inspect_opened(handle, pid)

    @contextmanager
    def open_for_termination(self, pid: int, *, terminate_access: bool = True):  # type: ignore[no-untyped-def]
        if os.name != "nt" or type(pid) is not int or pid <= 0:
            raise ProcessIdentityError("Windows process handle is unavailable")
        kernel32 = _kernel32()
        access = 0x1000 | 0x0400 | 0x00100000 | (0x0001 if terminate_access else 0)
        handle = kernel32.OpenProcess(access, False, pid)
        if not handle:
            raise ProcessIdentityError("managed Qwen process is unavailable")
        try:
            yield handle
        finally:
            kernel32.CloseHandle(handle)

    def inspect_opened(self, handle: object, pid: int) -> ObservedProcess:
        kernel32 = _kernel32()
        if kernel32.GetProcessId(handle) != pid or kernel32.WaitForSingleObject(handle, 0) != 258:
            raise ProcessIdentityError("managed Qwen process changed before inspection")
        executable = self._query_image(kernel32, handle)
        owner = self._query_owner(handle)  # type: ignore[arg-type]
        command_line = self._command_line_reader(pid)
        argv = tuple(self._command_line_to_argv(command_line))
        if kernel32.GetProcessId(handle) != pid or kernel32.WaitForSingleObject(handle, 0) != 258:
            raise ProcessIdentityError("managed Qwen process changed during inspection")
        return ObservedProcess(pid, owner, executable, argv, file_sha256(executable))

    @staticmethod
    def terminate_opened(handle: object) -> None:
        kernel32 = _kernel32()
        if kernel32.WaitForSingleObject(handle, 0) != 258 or not kernel32.TerminateProcess(handle, 0):
            raise ProcessIdentityError("managed Qwen termination failed")
        if kernel32.WaitForSingleObject(handle, 10_000) != 0:
            raise ProcessIdentityError("managed Qwen termination timed out")

    @staticmethod
    def _query_image(kernel32, handle: int) -> str:  # type: ignore[no-untyped-def]
        size = wintypes.DWORD(32_768)
        buffer = ctypes.create_unicode_buffer(size.value)
        if not kernel32.QueryFullProcessImageNameW(handle, 0, buffer, ctypes.byref(size)):
            raise ProcessIdentityError("managed Qwen executable path is unavailable")
        return buffer.value

    @staticmethod
    def _query_owner(process_handle: int) -> str:
        advapi32 = ctypes.WinDLL("advapi32.dll", use_last_error=True)
        advapi32.OpenProcessToken.argtypes = [wintypes.HANDLE, wintypes.DWORD, ctypes.POINTER(wintypes.HANDLE)]
        advapi32.OpenProcessToken.restype = wintypes.BOOL
        advapi32.GetTokenInformation.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.POINTER(wintypes.DWORD)]
        advapi32.GetTokenInformation.restype = wintypes.BOOL
        advapi32.LookupAccountSidW.argtypes = [wintypes.LPCWSTR, ctypes.c_void_p, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD), wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD), ctypes.POINTER(wintypes.DWORD)]
        advapi32.LookupAccountSidW.restype = wintypes.BOOL
        token = wintypes.HANDLE()
        if not advapi32.OpenProcessToken(process_handle, 0x0008, ctypes.byref(token)):
            raise ProcessIdentityError("managed Qwen owner is unavailable")
        try:
            needed = wintypes.DWORD()
            advapi32.GetTokenInformation(token, 1, None, 0, ctypes.byref(needed))
            buffer = ctypes.create_string_buffer(needed.value)
            if not advapi32.GetTokenInformation(token, 1, buffer, needed, ctypes.byref(needed)):
                raise ProcessIdentityError("managed Qwen owner is unavailable")
            sid = ctypes.cast(buffer, ctypes.POINTER(ctypes.c_void_p)).contents.value
            name_size, domain_size, use = wintypes.DWORD(256), wintypes.DWORD(256), wintypes.DWORD()
            name, domain = ctypes.create_unicode_buffer(256), ctypes.create_unicode_buffer(256)
            if not advapi32.LookupAccountSidW(None, sid, name, ctypes.byref(name_size), domain, ctypes.byref(domain_size), ctypes.byref(use)):
                raise ProcessIdentityError("managed Qwen owner is unavailable")
            return f"{domain.value}\\{name.value}" if domain.value else name.value
        finally:
            kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
            kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
            kernel32.CloseHandle.restype = wintypes.BOOL
            kernel32.CloseHandle(token)

    @staticmethod
    def _read_command_line(pid: int) -> str:
        # 排程 runner 採最小環境且刻意不繼承 PATH；固定由 Windows 目錄解析系統 CIM host。
        kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
        kernel32.GetWindowsDirectoryW.argtypes = [wintypes.LPWSTR, wintypes.UINT]
        kernel32.GetWindowsDirectoryW.restype = wintypes.UINT
        windows_directory = ctypes.create_unicode_buffer(32_768)
        length = kernel32.GetWindowsDirectoryW(windows_directory, len(windows_directory))
        if length == 0 or length >= len(windows_directory):
            raise ProcessIdentityError("Windows CIM process reader is unavailable")
        powershell = Path(windows_directory.value, "System32", "WindowsPowerShell", "v1.0", "powershell.exe")
        if powershell.is_symlink() or not powershell.is_file():
            raise ProcessIdentityError("Windows CIM process reader is unavailable")
        script = f"$p=Get-CimInstance Win32_Process -Filter 'ProcessId = {pid}' -ErrorAction Stop;if($null -eq $p){{exit 3}};$p.CommandLine|ConvertTo-Json -Compress"
        encoded = base64.b64encode(script.encode("utf-16-le")).decode("ascii")
        try:
            result = subprocess.run(
                [str(powershell), "-NoLogo", "-NoProfile", "-NonInteractive", "-EncodedCommand", encoded],
                capture_output=True, text=True, timeout=5, check=False,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
        except (OSError, subprocess.TimeoutExpired) as error:
            raise ProcessIdentityError("Windows CIM process reader failed") from error
        if result.returncode != 0 or len(result.stdout) > 65_536:
            raise ProcessIdentityError("managed Qwen command line is unavailable")
        try:
            value = json.loads(result.stdout)
        except json.JSONDecodeError as error:
            raise ProcessIdentityError("managed Qwen command line is invalid") from error
        if not isinstance(value, str) or not value:
            raise ProcessIdentityError("managed Qwen command line is unavailable")
        return value

    @staticmethod
    def _command_line_to_argv(command_line: str) -> list[str]:
        shell32 = ctypes.WinDLL("shell32.dll", use_last_error=True)
        argc = ctypes.c_int()
        shell32.CommandLineToArgvW.argtypes = [wintypes.LPCWSTR, ctypes.POINTER(ctypes.c_int)]
        shell32.CommandLineToArgvW.restype = ctypes.POINTER(wintypes.LPWSTR)
        pointer = shell32.CommandLineToArgvW(command_line, ctypes.byref(argc))
        if not pointer or argc.value < 1 or argc.value > 512:
            raise ProcessIdentityError("managed Qwen argument vector is invalid")
        try:
            return [pointer[index] for index in range(argc.value)]
        finally:
            kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
            kernel32.LocalFree.argtypes = [ctypes.c_void_p]
            kernel32.LocalFree.restype = ctypes.c_void_p
            kernel32.LocalFree(pointer)


class WindowsQwenLifecycle:
    """Own exactly one manifest-pinned Qwen process and its durable PID record."""

    def __init__(self, manifest: RuntimeManifest, pid_store: AtomicJsonStore, *, inspector: WindowsProcessInspector | None = None, launcher: Callable[..., subprocess.Popen[bytes]] = subprocess.Popen, probe: QwenAcceptanceProbe | None = None, api_key: str | None = None, adoption_state: str | Path | None = None, port_in_use: Callable[[], bool] | None = None) -> None:
        self.manifest = manifest
        self.pid_store = pid_store
        self.inspector = inspector or WindowsProcessInspector()
        self.launcher = launcher
        self.api_key = api_key
        if adoption_state is not None and Path(adoption_state).is_symlink():
            raise ProcessIdentityError("legacy Qwen state path is invalid")
        self.adoption_state = Path(adoption_state).resolve() if adoption_state is not None else None
        self.port_in_use = port_in_use or _qwen_port_in_use
        self.probe = probe or QwenAcceptanceProbe("http://127.0.0.1:18080", manifest.model_id, api_key, timeout=10)

    def _pid(self) -> int | None:
        value = self.pid_store.read()
        if value is None:
            return self._adopt_existing()
        if set(value) != {"schema_version", "pid"} or value.get("schema_version") != 1 or type(value.get("pid")) is not int or int(value["pid"]) <= 0:
            raise ProcessIdentityError("managed Qwen PID record is invalid")
        return int(value["pid"])

    def _adopt_existing(self) -> int | None:
        if self.adoption_state is None or not self.adoption_state.is_file() or self.adoption_state.is_symlink():
            return None
        try:
            raw = self.adoption_state.read_bytes()
            value = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object)
        except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
            raise ProcessIdentityError("legacy Qwen state is invalid") from error
        required = {"schemaVersion", "pid", "startedUtc", "executablePath", "modelSha256", "host", "port", "context", "parallel", "threads", "gpuLayers"}
        if not isinstance(value, dict) or set(value) != required or value.get("schemaVersion") != 1 or value.get("host") != "127.0.0.1" or value.get("port") != 18080 or value.get("parallel") != 1:
            raise ProcessIdentityError("legacy Qwen state contract is invalid")
        pid = value.get("pid")
        if type(pid) is not int or pid <= 0 or value.get("modelSha256") != self.manifest.model_sha256:
            raise ProcessIdentityError("legacy Qwen identity is invalid")
        observed = self.inspector.inspect(pid)
        self._validate_observed(pid, observed, allow_legacy_paths=True)
        return pid

    def _validate_observed(self, pid: int, observed: ObservedProcess, *, allow_legacy_paths: bool = False) -> None:
        try:
            validate_stop_eligibility(persisted_pid=pid, observed=observed, manifest=self.manifest)
            return
        except ProcessIdentityError:
            if not allow_legacy_paths or self.adoption_state is None:
                raise
        # MSIX 啟動的舊 light service 可能在 argv 留下虛擬路徑，但 image path 是實體 LocalCache。
        normalized = list(observed.argv)
        if not normalized:
            raise ProcessIdentityError("legacy Qwen argument vector is invalid")
        normalized[0] = self.manifest.argv[0]
        model_indices = [index + 1 for index, value in enumerate(normalized[:-1]) if value == "--model"]
        if len(model_indices) != 1:
            raise ProcessIdentityError("legacy Qwen model argument is invalid")
        normalized[model_indices[0]] = self.manifest.model_path
        self.manifest.verify_files()
        if (
            observed.pid != pid
            or observed.owner.casefold() != self.manifest.owner.casefold()
            or canonical_path(observed.executable_path) != canonical_path(self.manifest.executable_path)
            or observed.executable_sha256 != self.manifest.executable_sha256
            or tuple(normalized) != self.manifest.argv
        ):
            raise ProcessIdentityError("legacy Qwen process identity validation failed")

    def is_running(self) -> bool:
        pid = self._pid()
        if pid is None:
            return False
        try:
            observed = self.inspector.inspect(pid)
            self._validate_observed(pid, observed, allow_legacy_paths=not self.pid_store.path.exists())
            return True
        except ProcessIdentityError:
            return False

    def start_validated(self, profile: str) -> None:
        if profile != self.manifest.profile or self._pid() is not None:
            raise ProcessIdentityError("managed Qwen start precondition failed")
        self.manifest.verify_files()
        if self.port_in_use():
            raise ProcessIdentityError("Qwen loopback port is already in use")
        try:
            environment = minimal_runtime_environment(self.api_key)
            process = self.launcher(
                list(self.manifest.argv), executable=self.manifest.executable_path,
                cwd=str(Path(self.manifest.executable_path).parent), stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, close_fds=True,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0) | getattr(subprocess, "CREATE_NEW_PROCESS_GROUP", 0),
                env=environment,
            )
        except OSError as error:
            raise ProcessIdentityError("managed Qwen launch failed") from error
        pid = int(process.pid)
        try:
            deadline = time.monotonic() + 5
            while True:
                try:
                    observed = self.inspector.inspect(pid)
                    validate_stop_eligibility(persisted_pid=pid, observed=observed, manifest=self.manifest)
                    break
                except ProcessIdentityError:
                    if time.monotonic() >= deadline:
                        raise
                    time.sleep(0.05)
            self.pid_store.write({"schema_version": 1, "pid": pid})
        except Exception:
            try:
                process.terminate()
            finally:
                raise

    def stop_validated(self) -> None:
        pid = self._pid()
        if pid is None:
            return
        # 從 open、完整 identity 驗證到 terminate 都持有同一個 process object handle。
        with self.inspector.open_for_termination(pid) as handle:
            observed = self.inspector.inspect_opened(handle, pid)
            self._validate_observed(pid, observed, allow_legacy_paths=not self.pid_store.path.exists())
            self.inspector.terminate_opened(handle)
            self.pid_store.path.unlink(missing_ok=True)
            if self.adoption_state is not None:
                self.adoption_state.unlink(missing_ok=True)

    def accept(self) -> None:
        self.probe.run()


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


def _qwen_port_in_use() -> bool:
    try:
        with socket.create_connection(("127.0.0.1", 18080), timeout=0.2):
            return True
    except (ConnectionRefusedError, TimeoutError, socket.timeout, OSError):
        return False


def minimal_runtime_environment(api_key: str | None, source: dict[str, str] | None = None) -> dict[str, str]:
    """Build an explicit child allowlist so controller, LINE and unrelated secrets never propagate."""

    values = os.environ if source is None else source
    allowed = ("SystemRoot", "WINDIR", "TEMP", "TMP", "USERPROFILE", "LOCALAPPDATA")
    result = {key: values[key] for key in allowed if key in values and values[key]}
    if api_key is not None:
        result["LLAMA_API_KEY"] = api_key
    return result


def _kernel32():  # type: ignore[no-untyped-def]
    kernel32 = ctypes.WinDLL("kernel32.dll", use_last_error=True)
    kernel32.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.CloseHandle.argtypes = [wintypes.HANDLE]
    kernel32.CloseHandle.restype = wintypes.BOOL
    kernel32.QueryFullProcessImageNameW.argtypes = [wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)]
    kernel32.QueryFullProcessImageNameW.restype = wintypes.BOOL
    kernel32.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel32.TerminateProcess.restype = wintypes.BOOL
    kernel32.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.GetProcessId.argtypes = [wintypes.HANDLE]
    kernel32.GetProcessId.restype = wintypes.DWORD
    return kernel32


def capture_main_runtime_manifest(output_path: str | Path, model_id: str, *, state_path: str | Path | None = None, inspector: WindowsProcessInspector | None = None) -> str:
    """Capture the already-running fixed light service after authoritative inspection."""

    if not isinstance(model_id, str) or not model_id or len(model_id) > 512 or any(character in model_id for character in "\r\n\x00"):
        raise ProcessIdentityError("Qwen API model ID is invalid")

    local_app_data = os.environ.get("LOCALAPPDATA")
    if state_path is None:
        if not local_app_data:
            raise ProcessIdentityError("LOCALAPPDATA is unavailable")
        state_path = Path(local_app_data, "AevryndQwenLight", "state", "light-state.json")
    supplied_state = Path(state_path)
    state_file = supplied_state.resolve()
    if supplied_state.is_symlink() or not state_file.is_file() or state_file.stat().st_size > 65_536:
        raise ProcessIdentityError("legacy Qwen state is unavailable")
    try:
        state = json.loads(state_file.read_text(encoding="utf-8"), object_pairs_hook=_unique_object)
    except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
        raise ProcessIdentityError("legacy Qwen state is invalid") from error
    required = {"schemaVersion", "pid", "startedUtc", "executablePath", "modelSha256", "host", "port", "context", "parallel", "threads", "gpuLayers"}
    if not isinstance(state, dict) or set(state) != required or state.get("schemaVersion") != 1 or state.get("host") != "127.0.0.1" or state.get("port") != 18080 or state.get("parallel") != 1:
        raise ProcessIdentityError("legacy Qwen state contract is invalid")
    pid = state.get("pid")
    if type(pid) is not int or pid <= 0:
        raise ProcessIdentityError("legacy Qwen PID is invalid")
    observed = (inspector or WindowsProcessInspector()).inspect(pid)
    model_arguments = [observed.argv[index + 1] for index, value in enumerate(observed.argv[:-1]) if value == "--model"]
    if len(model_arguments) != 1:
        raise ProcessIdentityError("managed Qwen model argument is invalid")
    observed_root = Path(observed.executable_path).resolve().parent.parent
    physical_model = (observed_root / "models" / Path(model_arguments[0]).name).resolve()
    model_path = physical_model if physical_model.is_file() else Path(model_arguments[0]).resolve()
    normalized_argv = list(observed.argv)
    normalized_argv[0] = observed.executable_path
    normalized_argv[normalized_argv.index("--model") + 1] = str(model_path)
    manifest = RuntimeManifest(
        "main", "light", observed.owner, observed.executable_path, observed.executable_sha256,
        tuple(normalized_argv), command_digest(normalized_argv), str(model_path), file_sha256(model_path), model_id,
    )
    manifest._validate_network_contract()
    manifest.verify_files()
    if state.get("modelSha256") != manifest.model_sha256:
        raise ProcessIdentityError("legacy Qwen state does not match the live runtime")
    supplied_output = Path(output_path)
    output = supplied_output.resolve()
    if output.suffix.casefold() != ".json" or supplied_output.is_symlink() or not output.parent.is_dir():
        raise ProcessIdentityError("runtime manifest output path is invalid")
    document = {
        "schema_version": 1,
        "runtime": {
            "node_id": manifest.node_id, "profile": manifest.profile, "owner": manifest.owner,
            "executable_path": manifest.executable_path, "executable_sha256": manifest.executable_sha256,
            "argv": list(manifest.argv), "command_sha256": manifest.command_sha256,
            "model_path": manifest.model_path, "model_sha256": manifest.model_sha256,
            "model_id": manifest.model_id,
        },
    }
    raw = json.dumps(document, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
    try:
        with output.open("xb") as stream:
            stream.write(raw); stream.flush(); os.fsync(stream.fileno())
    except OSError as error:
        raise ProcessIdentityError("runtime manifest output could not be created") from error
    return hashlib.sha256(raw).hexdigest()
