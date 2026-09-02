"""Atomic, bounded JSON persistence."""

from __future__ import annotations

import json
import os
import threading
import time
import uuid
from contextlib import contextmanager
from pathlib import Path
from typing import Callable, Mapping


class StateStoreError(RuntimeError):
    pass


def replace_with_retry(source: str | Path, destination: str | Path, *, timeout_seconds: float = 1.0) -> None:
    """Retry only known transient Windows sharing/access failures within a strict bound."""

    if not 0.05 <= timeout_seconds <= 5.0:
        raise ValueError("replace timeout is outside the allowed range")
    deadline = time.monotonic() + timeout_seconds
    delay = 0.01
    while True:
        try:
            os.replace(source, destination)
            return
        except OSError as error:
            if os.name != "nt" or getattr(error, "winerror", None) not in {5, 32, 33} or time.monotonic() >= deadline:
                raise
            time.sleep(delay)
            delay = min(delay * 2, 0.1)


class AtomicJsonStore:
    _registry_guard = threading.Lock()
    _process_locks: dict[str, threading.RLock] = {}

    def __init__(self, path: str | Path, *, maximum_bytes: int = 1_048_576, lock_timeout_seconds: float = 5.0) -> None:
        self.path = Path(path).resolve()
        if not 1_024 <= maximum_bytes <= 8_388_608:
            raise ValueError("maximum_bytes is outside the allowed range")
        self.maximum_bytes = maximum_bytes
        if not 0.1 <= lock_timeout_seconds <= 30.0:
            raise ValueError("lock timeout is outside the allowed range")
        self.lock_timeout_seconds = float(lock_timeout_seconds)
        self.lock_path = self.path.with_name(self.path.name + ".lock")
        key = os.path.normcase(str(self.lock_path))
        with self._registry_guard:
            self._lock = self._process_locks.setdefault(key, threading.RLock())

    def read(self) -> dict[str, object] | None:
        with self._locked():
            return self._read_unlocked()

    def _read_unlocked(self) -> dict[str, object] | None:
            try:
                size = self.path.stat().st_size
            except FileNotFoundError:
                return None
            if size < 2 or size > self.maximum_bytes:
                raise StateStoreError("state file size is invalid")
            try:
                raw = self.path.read_bytes()
                value = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object)
            except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as error:
                raise StateStoreError("state file is invalid") from error
            if not isinstance(value, dict):
                raise StateStoreError("state root must be an object")
            return value

    def write(self, value: Mapping[str, object]) -> None:
        raw = json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
        if len(raw) > self.maximum_bytes:
            raise StateStoreError("state exceeds maximum_bytes")
        with self._locked():
            self._write_unlocked(raw)

    def _write_unlocked(self, raw: bytes) -> None:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            temporary = self.path.with_name(f".{self.path.name}.{uuid.uuid4().hex}.tmp")
            try:
                with temporary.open("xb") as stream:
                    stream.write(raw)
                    stream.flush()
                    os.fsync(stream.fileno())
                replace_with_retry(temporary, self.path, timeout_seconds=min(1.0, self.lock_timeout_seconds))
                # Windows 不支援對目錄 fsync；檔案內容與 rename 仍具原子性。
                if os.name != "nt":
                    descriptor = os.open(self.path.parent, os.O_RDONLY)
                    try:
                        os.fsync(descriptor)
                    finally:
                        os.close(descriptor)
            except OSError as error:
                raise StateStoreError("atomic state write failed") from error
            finally:
                try:
                    temporary.unlink(missing_ok=True)
                except OSError:
                    pass

    def update(self, mutator: Callable[[dict[str, object] | None], Mapping[str, object]]) -> dict[str, object]:
        with self._locked():
            result = dict(mutator(self._read_unlocked()))
            raw = json.dumps(result, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
            if len(raw) > self.maximum_bytes:
                raise StateStoreError("state exceeds maximum_bytes")
            self._write_unlocked(raw)
            return result

    @contextmanager
    def _locked(self):  # type: ignore[no-untyped-def]
        deadline = time.monotonic() + self.lock_timeout_seconds
        if not self._lock.acquire(timeout=self.lock_timeout_seconds):
            raise StateStoreError("state lock timed out")
        stream = None
        file_locked = False
        try:
            self.path.parent.mkdir(parents=True, exist_ok=True)
            stream = self.lock_path.open("a+b")
            if stream.tell() == 0:
                stream.write(b"0")
                stream.flush()
            while True:
                try:
                    _lock_file(stream)
                    file_locked = True
                    break
                except (BlockingIOError, OSError):
                    if time.monotonic() >= deadline:
                        raise StateStoreError("state lock timed out")
                    time.sleep(0.01)
            yield
        finally:
            if stream is not None:
                try:
                    if file_locked:
                        _unlock_file(stream)
                finally:
                    stream.close()
            self._lock.release()

    def exclusive_lock(self):  # type: ignore[no-untyped-def]
        """Hold the same bounded cross-process lock across a larger transaction."""

        return self._locked()


def _unique_object(pairs: list[tuple[str, object]]) -> dict[str, object]:
    result: dict[str, object] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON key")
        result[key] = value
    return result


if os.name == "nt":
    import msvcrt

    def _lock_file(stream) -> None:  # type: ignore[no-untyped-def]
        stream.seek(0)
        msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)

    def _unlock_file(stream) -> None:  # type: ignore[no-untyped-def]
        stream.seek(0)
        msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)
else:
    import fcntl

    def _lock_file(stream) -> None:  # type: ignore[no-untyped-def]
        fcntl.flock(stream.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)

    def _unlock_file(stream) -> None:  # type: ignore[no-untyped-def]
        fcntl.flock(stream.fileno(), fcntl.LOCK_UN)
