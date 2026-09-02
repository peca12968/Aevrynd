"""Validated state models for the game-mode controller."""

from __future__ import annotations

from dataclasses import asdict, dataclass
from enum import Enum
from typing import Mapping

from .inventory import NODE_IDS, get_node


class QwenState(str, Enum):
    UNKNOWN = "unknown"
    STARTING = "starting"
    RUNNING = "running"
    STOPPING = "stopping"
    STOPPED = "stopped"
    DEGRADED = "degraded"


@dataclass(frozen=True, slots=True)
class GameModeState:
    node_id: str
    game_mode: bool
    qwen_state: QwenState
    qwen_profile: str
    revision: int
    updated_utc: str
    detail_code: str = "none"

    def __post_init__(self) -> None:
        spec = get_node(self.node_id)
        if type(self.game_mode) is not bool:
            raise ValueError("game_mode must be boolean")
        if not isinstance(self.qwen_state, QwenState):
            raise ValueError("qwen_state is invalid")
        if self.qwen_profile != spec.qwen_profile:
            raise ValueError("Qwen profile differs from fixed node inventory")
        if type(self.revision) is not int or not 0 <= self.revision <= 2**63 - 1:
            raise ValueError("revision is invalid")
        if not isinstance(self.updated_utc, str) or not 20 <= len(self.updated_utc) <= 40:
            raise ValueError("updated_utc is invalid")
        if (
            not isinstance(self.detail_code, str)
            or not self.detail_code
            or len(self.detail_code) > 64
            or not self.detail_code.replace("_", "").isalnum()
        ):
            raise ValueError("detail_code is invalid")

    def to_dict(self) -> dict[str, object]:
        result = asdict(self)
        result["qwen_state"] = self.qwen_state.value
        return result

    @classmethod
    def from_dict(cls, value: Mapping[str, object]) -> "GameModeState":
        if set(value) != {
            "node_id", "game_mode", "qwen_state", "qwen_profile", "revision",
            "updated_utc", "detail_code",
        }:
            raise ValueError("state schema is invalid")
        return cls(
            node_id=str(value["node_id"]),
            game_mode=value["game_mode"],  # type: ignore[arg-type]
            qwen_state=QwenState(value["qwen_state"]),
            qwen_profile=str(value["qwen_profile"]),
            revision=value["revision"],  # type: ignore[arg-type]
            updated_utc=str(value["updated_utc"]),
            detail_code=str(value["detail_code"]),
        )


@dataclass(frozen=True, slots=True)
class NodeStatus:
    state: GameModeState
    online: bool
    last_seen_utc: str

    def to_dict(self) -> dict[str, object]:
        return {
            "state": self.state.to_dict(),
            "online": self.online,
            "last_seen_utc": self.last_seen_utc,
        }


def validate_snapshot(snapshot: Mapping[str, NodeStatus]) -> None:
    if set(snapshot) != NODE_IDS:
        raise ValueError("snapshot must contain exactly three nodes")
    if any(key != item.state.node_id for key, item in snapshot.items()):
        raise ValueError("snapshot node identity mismatch")
