"""Test-only fakes for bounded HTTP clients."""

from __future__ import annotations

import io
from typing import Callable


class FakeResponse:
    def __init__(self, status: int, body: bytes = b"{}") -> None:
        self._status = status
        self._body = io.BytesIO(body)

    def __enter__(self) -> "FakeResponse":
        return self

    def __exit__(self, *_arguments: object) -> None:
        return None

    def getcode(self) -> int:
        return self._status

    def read(self, size: int = -1) -> bytes:
        return self._body.read(size)


class RecordingOpener:
    def __init__(self, factory: Callable[..., object]) -> None:
        self.factory = factory
        self.requests: list[object] = []
        self.timeouts: list[float] = []

    def __call__(self, request: object, *, timeout: float) -> object:
        self.requests.append(request)
        self.timeouts.append(timeout)
        return self.factory(request)
