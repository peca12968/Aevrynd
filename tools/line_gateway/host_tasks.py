"""Fixed read-only host task registry with no shell execution."""

from __future__ import annotations

import time
import uuid
from collections.abc import Callable, Mapping
from typing import Any

from .queue_store import QueueStore


class HostTaskError(RuntimeError):
    pass


class HostTasks:
    def __init__(
        self,
        queue_store: QueueStore,
        *,
        health_provider: Callable[[], Mapping[str, object]] | None = None,
        status_provider: Callable[[], Mapping[str, object]] | None = None,
    ) -> None:
        self._queue_store = queue_store
        self._health_provider = health_provider
        self._status_provider = status_provider
        # 固定 registry 禁止把使用者輸入轉交 shell 或動態 import。
        self._registry: dict[str, Callable[[Mapping[str, object]], dict[str, object]]] = {
            "health": self._health,
            "queue": self._queue,
            "status": self._status,
        }

    @property
    def task_names(self) -> tuple[str, ...]:
        return tuple(sorted(self._registry))

    def execute(self, task_name: str, arguments: Mapping[str, object] | None = None) -> dict[str, object]:
        if not isinstance(task_name, str) or task_name not in self._registry:
            raise HostTaskError("host task is not registered")
        args = {} if arguments is None else arguments
        if not isinstance(args, Mapping):
            raise HostTaskError("host task arguments must be an object")
        return self._registry[task_name](args)

    def execute_text(self, command_text: str) -> dict[str, object]:
        if not isinstance(command_text, str):
            raise HostTaskError("host command must be text")
        parts = command_text.strip().split()
        if not parts:
            raise HostTaskError("host command is empty")
        task_name = parts[0].lower()
        if task_name == "status":
            if len(parts) != 2:
                raise HostTaskError("status requires one job UUID")
            return self.execute("status", {"job_id": parts[1]})
        if len(parts) != 1:
            raise HostTaskError("host command contains unsupported arguments")
        return self.execute(task_name)

    def _health(self, arguments: Mapping[str, object]) -> dict[str, object]:
        if arguments:
            raise HostTaskError("health does not accept arguments")
        result: dict[str, object] = {
            "healthy": True,
            "time": int(time.time()),
            "queue": self._queue_store.counts(),
        }
        if self._health_provider is not None:
            extra = self._health_provider()
            if not isinstance(extra, Mapping):
                raise HostTaskError("health provider returned an invalid result")
            result["provider"] = dict(extra)
        return result

    def _queue(self, arguments: Mapping[str, object]) -> dict[str, object]:
        allowed = {"limit"}
        if any(key not in allowed for key in arguments):
            raise HostTaskError("queue received an unsupported argument")
        limit = arguments.get("limit", 10)
        if not isinstance(limit, int) or isinstance(limit, bool):
            raise HostTaskError("queue limit must be an integer")
        return {"counts": self._queue_store.counts(), "recent": self._queue_store.recent(limit)}

    def _status(self, arguments: Mapping[str, object]) -> dict[str, object]:
        if set(arguments) != {"job_id"}:
            raise HostTaskError("status requires exactly one job_id")
        job_id = arguments["job_id"]
        if not isinstance(job_id, str):
            raise HostTaskError("job_id must be text")
        try:
            uuid.UUID(job_id)
        except (ValueError, TypeError, AttributeError) as exc:
            raise HostTaskError("job_id must be a UUID") from exc
        result = self._queue_store.status(job_id)
        response: dict[str, object] = {"job": result}
        if self._status_provider is not None:
            extra = self._status_provider()
            if not isinstance(extra, Mapping):
                raise HostTaskError("status provider returned an invalid result")
            response["provider"] = dict(extra)
        return response
