"""Local fail-closed game-mode state machine."""

from __future__ import annotations

from dataclasses import replace
from datetime import datetime, timezone
from typing import Callable, Protocol

from .atomic_store import AtomicJsonStore
from .inventory import get_node
from .models import GameModeState, QwenState


class QwenLifecycle(Protocol):
    def is_running(self) -> bool: ...
    def start_validated(self, profile: str) -> None: ...
    def stop_validated(self) -> None: ...
    def accept(self) -> None: ...


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


class GameModeController:
    def __init__(self, node_id: str, store: AtomicJsonStore, lifecycle: QwenLifecycle, *, clock: Callable[[], str] = utc_now) -> None:
        self.spec = get_node(node_id)
        self.store = store
        self.lifecycle = lifecycle
        self.clock = clock

    def load_or_initialize(self) -> GameModeState:
        document = self.store.read()
        if document is None:
            state = GameModeState(self.spec.node_id, False, QwenState.UNKNOWN, self.spec.qwen_profile, 0, self.clock())
            self._write(state)
            return state
        if set(document) != {"schema_version", "state"} or document.get("schema_version") != 1 or not isinstance(document.get("state"), dict):
            raise ValueError("local state document is invalid")
        state = GameModeState.from_dict(document["state"])  # type: ignore[arg-type]
        if state.node_id != self.spec.node_id or state.qwen_profile != self.spec.qwen_profile:
            raise ValueError("local state identity differs from fixed node inventory")
        return state

    def set_game_mode(self, enabled: bool) -> GameModeState:
        if type(enabled) is not bool:
            raise ValueError("enabled must be boolean")
        current = self.load_or_initialize()
        desired = replace(current, game_mode=enabled, revision=current.revision + 1, updated_utc=self.clock(), detail_code="reconciling")
        self._write(desired)
        return self._reconcile(desired)

    def reconcile_boot(self) -> GameModeState:
        return self._reconcile(self.load_or_initialize())

    def _reconcile(self, state: GameModeState) -> GameModeState:
        try:
            if state.game_mode:
                if self.lifecycle.is_running():
                    interim = replace(state, qwen_state=QwenState.STOPPING, updated_utc=self.clock())
                    self._write(interim)
                    self.lifecycle.stop_validated()
                if self.lifecycle.is_running():
                    raise RuntimeError("Qwen remained running after validated stop")
                final = replace(state, qwen_state=QwenState.STOPPED, updated_utc=self.clock(), detail_code="game_mode")
            else:
                if not self.lifecycle.is_running():
                    interim = replace(state, qwen_state=QwenState.STARTING, updated_utc=self.clock())
                    self._write(interim)
                    self.lifecycle.start_validated(self.spec.qwen_profile)
                self.lifecycle.accept()
                final = replace(state, qwen_state=QwenState.RUNNING, updated_utc=self.clock(), detail_code="accepted")
        except Exception:
            # 不洩漏程序、路徑或憑證細節；gameMode 意圖保留供下次開機收斂。
            final = replace(state, qwen_state=QwenState.DEGRADED, updated_utc=self.clock(), detail_code="reconcile_failed")
        self._write(final)
        return final

    def _write(self, state: GameModeState) -> None:
        self.store.write({"schema_version": 1, "state": state.to_dict()})
