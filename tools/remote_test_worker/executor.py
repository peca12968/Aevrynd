"""Fail-closed construction and execution of the only permitted native commands."""

import hashlib
import os
import re
import shutil
import subprocess
import threading
import time
from dataclasses import dataclass
from pathlib import Path

from .policy import FIXED_ORIGIN, MAX_LOG_BYTES, PRESETS
from .protocol import ProtocolError
from .worker import SingleJobLock

FIXED_CLONE_ROOT = Path(r"C:\ProgramData\Aevrynd\RemoteTestWorker\Repository")
FIXED_WORK_ROOT = Path(r"C:\ProgramData\Aevrynd\RemoteTestWorker\Jobs")
FIXED_LOCK = Path(r"C:\ProgramData\Aevrynd\RemoteTestWorker\worker.lock")
_SHA = re.compile(r"^[0-9a-f]{40}$")


@dataclass(frozen=True)
class CommandPlan:
    fetch: tuple[str, ...]
    verify: tuple[str, ...]
    worktree: tuple[str, ...]
    configure: tuple[str, ...]
    build: tuple[str, ...]
    test: tuple[str, ...]


def build_command_plan(commit_sha: str, preset_name: str, clone_root: Path, work_root: Path,
                       observed_origin: str) -> CommandPlan:
    """所有 argv 都由簽章欄位驗證後映射；不接受 shell、路徑或額外參數。"""
    if observed_origin != FIXED_ORIGIN:
        raise ProtocolError("origin_mismatch")
    if not _SHA.fullmatch(commit_sha):
        raise ProtocolError("commit_denied")
    preset = PRESETS.get(preset_name)
    if preset is None:
        raise ProtocolError("preset_denied")
    if (not clone_root.is_absolute() or not work_root.is_absolute()
            or clone_root.anchor.startswith("\\\\") or work_root.anchor.startswith("\\\\")):
        raise ProtocolError("unsafe_fixed_root")
    job_root = work_root / commit_sha
    return CommandPlan(
        fetch=("git", "-C", str(clone_root), "fetch", "--no-tags", "origin", commit_sha),
        verify=("git", "-C", str(clone_root), "rev-parse", "--verify", f"{commit_sha}^{{commit}}"),
        worktree=("git", "-C", str(clone_root), "worktree", "add", "--detach", str(job_root), commit_sha),
        configure=("cmake", "--preset", preset.configure_preset),
        build=("cmake", "--build", "--preset", preset.build_preset, "--target", *preset.targets),
        test=("ctest", "--preset", preset.build_preset, "-L", f"^{preset.test_label}$", "--output-on-failure"),
    )


WINDOWS_CREATION_FLAGS = 0x08000000 | 0x00004000  # CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS


class BoundedProcessRunner:
    """持續排空 pipe，但只保留固定上限，避免 log 造成記憶體耗盡。"""

    def run(self, argv: tuple[str, ...], cwd: Path, timeout: float) -> tuple[int, str]:
        environment = {key: value for key, value in os.environ.items()
                       if key.upper() in {"PATH", "SYSTEMROOT", "WINDIR", "TEMP", "TMP"}}
        environment.update({"GIT_TERMINAL_PROMPT": "0", "GCM_INTERACTIVE": "Never"})
        process = subprocess.Popen(argv, cwd=cwd, shell=False, stdin=subprocess.DEVNULL,
                                   stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                   creationflags=WINDOWS_CREATION_FLAGS, env=environment)
        kept = bytearray()

        def drain() -> None:
            assert process.stdout is not None
            while chunk := process.stdout.read(65536):
                remaining = MAX_LOG_BYTES - len(kept)
                if remaining > 0:
                    kept.extend(chunk[:remaining])

        reader = threading.Thread(target=drain, daemon=True)
        reader.start()
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            # PID 由本程序產生且 argv 固定；僅清理由本次測試建立的 process tree。
            subprocess.run(("taskkill.exe", "/PID", str(process.pid), "/T", "/F"),
                           stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, creationflags=WINDOWS_CREATION_FLAGS, timeout=30)
            try:
                process.wait(timeout=30)
            finally:
                if process.stdout is not None:
                    process.stdout.close()
                reader.join(timeout=30)
            raise TimeoutError("step_timeout")
        reader.join(timeout=30)
        if reader.is_alive():
            if process.stdout is not None:
                process.stdout.close()
            raise RuntimeError("log_drain_failed")
        return code, kept.decode("utf-8", "replace")


def _safe_fixed_directory(path: Path) -> None:
    if not path.is_absolute() or str(path).startswith("\\\\") or len(str(path)) > 180:
        raise ProtocolError("unsafe_fixed_root")
    cursor = path
    while cursor != cursor.parent:
        is_junction = getattr(cursor, "is_junction", lambda: False)
        if cursor.exists() and (cursor.is_symlink() or is_junction()):
            raise ProtocolError("reparse_root")
        cursor = cursor.parent


class RestrictedExecutor:
    """Production executor. Constructor intentionally accepts no roots or command options."""

    def __init__(self, runner: BoundedProcessRunner | None = None):
        self.runner = runner or BoundedProcessRunner()

    def _step(self, argv, cwd, remaining):
        if remaining <= 0:
            raise TimeoutError("step_timeout")
        began = time.monotonic()
        code, log = self.runner.run(tuple(argv), cwd, remaining)
        return code, log, int((time.monotonic() - began) * 1000)

    def run(self, commit_sha: str, preset_name: str, timeout_seconds: int):
        _safe_fixed_directory(FIXED_CLONE_ROOT)
        _safe_fixed_directory(FIXED_WORK_ROOT)
        if not FIXED_CLONE_ROOT.is_dir():
            raise ProtocolError("clone_missing")
        origin_code, origin_log, _ = self._step(
            ("git", "-C", str(FIXED_CLONE_ROOT), "remote", "get-url", "origin"),
            FIXED_CLONE_ROOT, 30)
        if origin_code or origin_log.strip() != FIXED_ORIGIN:
            raise ProtocolError("origin_mismatch")
        plan = build_command_plan(commit_sha, preset_name, FIXED_CLONE_ROOT, FIXED_WORK_ROOT, origin_log.strip())
        job_root = FIXED_WORK_ROOT / commit_sha
        FIXED_WORK_ROOT.mkdir(parents=True, exist_ok=True)
        _safe_fixed_directory(job_root)
        if job_root.exists():
            raise ProtocolError("worktree_already_exists")
        logs: list[str] = []
        durations = {"fetch": 0, "configure": 0, "build": 0, "test": 0, "total": 0}
        began = time.monotonic()
        status = "infrastructure_error"
        counts = {"total": 0, "passed": 0, "failed": 0, "skipped": 0}
        with SingleJobLock(FIXED_LOCK):
            try:
                for name, argv, cwd in (
                    ("fetch", plan.fetch, FIXED_CLONE_ROOT),
                    ("configure", plan.configure, job_root),
                    ("build", plan.build, job_root),
                    ("test", plan.test, job_root),
                ):
                    if name == "configure":
                        code, verified, _ = self._step(plan.verify, FIXED_CLONE_ROOT,
                                                       timeout_seconds - (time.monotonic() - began))
                        if code or verified.strip() != commit_sha:
                            raise ProtocolError("commit_verification_failed")
                        code, out, elapsed = self._step(plan.worktree, FIXED_CLONE_ROOT,
                                                        timeout_seconds - (time.monotonic() - began))
                        logs.append(out)
                        if code:
                            status = "infrastructure_error"
                            break
                    code, out, elapsed = self._step(argv, cwd, timeout_seconds - (time.monotonic() - began))
                    durations[name] = elapsed
                    logs.append(out)
                    if name == "test":
                        match = re.search(r"(\d+)% tests passed, (\d+) tests failed out of (\d+)", out)
                        if match:
                            total, failed = int(match.group(3)), int(match.group(2))
                            counts.update(total=total, failed=failed, passed=total - failed)
                    if code:
                        status = "test_failed" if name == "test" else "build_failed" if name == "build" else "infrastructure_error"
                        break
                else:
                    status = "passed"
            except TimeoutError:
                status = "timeout"
            finally:
                durations["total"] = int((time.monotonic() - began) * 1000)
                if job_root.exists():
                    self.runner.run(("git", "-C", str(FIXED_CLONE_ROOT), "worktree", "remove", "--force", str(job_root)),
                                    FIXED_CLONE_ROOT, 60)
        def version(name):
            code, text, _ = self._step((name, "--version"), FIXED_CLONE_ROOT, 10)
            found = re.search(r"\d+(?:\.\d+){1,3}", text)
            return found.group(0) if code == 0 and found else "unknown"
        return {"status": status, "log": "\n".join(logs),
                "toolchain": {"git": version("git"), "cmake": version("cmake"), "ctest": version("ctest")},
                "tests": counts, "durations_ms": durations}
