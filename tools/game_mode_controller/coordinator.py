"""Durable hub model for status, fixed commands and aggregated notifications."""

from __future__ import annotations

import uuid
import json
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
from typing import Callable, Mapping

from .atomic_store import AtomicJsonStore
from .inventory import NODES, NODE_IDS, get_node
from .models import GameModeState, NodeStatus, QwenState, validate_snapshot


@dataclass(frozen=True, slots=True)
class Command:
    command_id: str
    actor_node_id: str
    target_node_id: str
    game_mode: bool
    created_utc: str

    def __post_init__(self) -> None:
        uuid.UUID(self.command_id)
        get_node(self.actor_node_id)
        get_node(self.target_node_id)
        if type(self.game_mode) is not bool:
            raise ValueError("game_mode must be boolean")
        _parse_utc(self.created_utc)

    def to_dict(self) -> dict[str, object]:
        return {
            "command_id": self.command_id, "actor_node_id": self.actor_node_id,
            "target_node_id": self.target_node_id, "game_mode": self.game_mode,
            "created_utc": self.created_utc,
        }


@dataclass(frozen=True, slots=True)
class Notification:
    notification_id: str
    event_node_id: str
    event_qwen_state: str
    text: str
    created_utc: str
    status: str = "pending"
    attempts: int = 0
    last_error_code: str = "none"

    def __post_init__(self) -> None:
        uuid.UUID(self.notification_id)
        get_node(self.event_node_id)
        if self.event_qwen_state not in {"running", "stopped"}:
            raise ValueError("notification event state is invalid")
        if self.status not in {"pending", "delivered", "dead_letter"}:
            raise ValueError("notification status is invalid")
        if type(self.attempts) is not int or not 0 <= self.attempts <= 1000:
            raise ValueError("notification attempts are invalid")
        if not self.text or len(self.text) > 5000:
            raise ValueError("notification text is invalid")
        _parse_utc(self.created_utc)
        if not self.last_error_code or len(self.last_error_code) > 64 or not self.last_error_code.replace("_", "").isalnum():
            raise ValueError("notification error code is invalid")

    def to_dict(self) -> dict[str, object]:
        return self.__dict__.copy() if hasattr(self, "__dict__") else {
            "notification_id": self.notification_id, "event_node_id": self.event_node_id,
            "event_qwen_state": self.event_qwen_state, "text": self.text,
            "created_utc": self.created_utc, "status": self.status,
            "attempts": self.attempts, "last_error_code": self.last_error_code,
        }


def _now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def _parse_utc(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))


def format_line_status(event_node_id: str, event_qwen_state: QwenState, snapshot: Mapping[str, NodeStatus]) -> str:
    validate_snapshot(snapshot)
    event = get_node(event_node_id)
    action = "started" if event_qwen_state is QwenState.RUNNING else "stopped"
    lines = [f"Qwen service {action}: {event.display_name}", "Three-node status:"]
    for node_id in ("main", "taichung", "taoyuan"):
        item = snapshot[node_id]
        connection = "online" if item.online else "offline"
        game = "ON" if item.state.game_mode else "OFF"
        lines.append(f"- {NODES[node_id].display_name}: gameMode {game}, Qwen {item.state.qwen_state.value}, {connection}")
    return "\n".join(lines)


class HubCoordinator:
    def __init__(self, store: AtomicJsonStore, *, clock: Callable[[], str] = _now, online_seconds: int = 45, max_notification_records: int = 256, max_notification_bytes: int = 524_288) -> None:
        if not 10 <= online_seconds <= 300:
            raise ValueError("online_seconds is outside the allowed range")
        self.store = store
        self.clock = clock
        self.online_seconds = online_seconds
        if not 1 <= max_notification_records <= 4096 or not 1024 <= max_notification_bytes <= 786_432:
            raise ValueError("notification outbox bounds are invalid")
        self.max_notification_records = max_notification_records
        self.max_notification_bytes = max_notification_bytes

    def _empty(self) -> dict[str, object]:
        return {"schema_version": 1, "nodes": {}, "commands": [], "notifications": []}

    def _load(self) -> dict[str, object]:
        document = self.store.read()
        if document is None:
            document = self._empty()
            self.store.write(document)
        self._validate_document(document)
        return document

    def _validate_document(self, document: Mapping[str, object]) -> None:
        if set(document) != {"schema_version", "nodes", "commands", "notifications"} or document.get("schema_version") != 1:
            raise ValueError("hub state schema is invalid")
        if not isinstance(document["nodes"], dict) or not isinstance(document["commands"], list) or not isinstance(document["notifications"], list):
            raise ValueError("hub state shape is invalid")
        nodes = document["nodes"]
        commands = document["commands"]
        notifications = document["notifications"]
        if any(node_id not in NODE_IDS for node_id in nodes) or len(commands) > len(NODE_IDS):
            raise ValueError("hub state contains an unbounded or unknown node record")
        for node_id, value in nodes.items():
            if not isinstance(value, dict) or set(value) != {"state", "last_seen_utc"} or not isinstance(value["state"], dict) or not isinstance(value["last_seen_utc"], str):
                raise ValueError("persisted node record is invalid")
            state = GameModeState.from_dict(value["state"])
            if state.node_id != node_id:
                raise ValueError("persisted node key and identity differ")
            _parse_utc(value["last_seen_utc"])
        for value in commands:
            if not isinstance(value, dict):
                raise ValueError("hub command record is invalid")
            Command(
                str(value.get("command_id")), str(value.get("actor_node_id")),
                str(value.get("target_node_id")), value.get("game_mode"),
                str(value.get("created_utc")),  # type: ignore[arg-type]
            )
        if len(notifications) > self.max_notification_records:
            raise ValueError("notification outbox record bound was exceeded")
        encoded = json.dumps(notifications, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
        if len(encoded) > self.max_notification_bytes:
            raise ValueError("notification outbox byte bound was exceeded")
        for value in notifications:
            if not isinstance(value, dict):
                raise ValueError("notification record is invalid")
            Notification(**value)

    def report(self, actor_node_id: str, state: GameModeState) -> None:
        if actor_node_id != state.node_id:
            raise ValueError("node may only report its own status")
        now = self.clock()

        def mutate(document: dict[str, object] | None) -> Mapping[str, object]:
            data = self._empty() if document is None else document
            self._validate_document(data)
            nodes = data["nodes"]
            assert isinstance(nodes, dict)
            previous = nodes.get(actor_node_id)
            previous_qwen = None
            if isinstance(previous, dict) and isinstance(previous.get("state"), dict):
                previous_state = GameModeState.from_dict(previous["state"])
                if state.revision < previous_state.revision or (
                    state.revision == previous_state.revision
                    and _parse_utc(state.updated_utc) < _parse_utc(previous_state.updated_utc)
                ):
                    raise ValueError("stale node status was rejected")
                previous_qwen = previous_state.qwen_state.value
            nodes[actor_node_id] = {"state": state.to_dict(), "last_seen_utc": now}
            if state.qwen_state.value in {"running", "stopped"} and previous_qwen != state.qwen_state.value:
                snapshot = self._snapshot_from_document(data, now)
                self._append_notification(data, Notification(
                    str(uuid.uuid4()), actor_node_id, state.qwen_state.value,
                    format_line_status(actor_node_id, state.qwen_state, snapshot), now,
                ))
            return data

        self.store.update(mutate)

    def issue(self, actor_node_id: str, target_node_id: str, game_mode: bool) -> Command:
        get_node(actor_node_id)
        get_node(target_node_id)
        command = Command(str(uuid.uuid4()), actor_node_id, target_node_id, game_mode, self.clock())

        def mutate(document: dict[str, object] | None) -> Mapping[str, object]:
            data = self._empty() if document is None else document
            self._validate_document(data)
            commands = data["commands"]
            assert isinstance(commands, list)
            # 每個 target 僅保留最新意圖，避免離線後重播過期切換序列。
            commands[:] = [item for item in commands if not isinstance(item, dict) or item.get("target_node_id") != target_node_id]
            commands.append(command.to_dict())
            return data

        self.store.update(mutate)
        return command

    def poll(self, actor_node_id: str) -> Command | None:
        get_node(actor_node_id)
        commands = self._load()["commands"]
        assert isinstance(commands, list)
        for value in commands:
            if isinstance(value, dict) and value.get("target_node_id") == actor_node_id:
                return Command(
                    str(value["command_id"]), str(value["actor_node_id"]), str(value["target_node_id"]),
                    value["game_mode"], str(value["created_utc"]),  # type: ignore[arg-type]
                )
        return None

    def acknowledge(self, actor_node_id: str, command_id: str, state: GameModeState) -> None:
        if actor_node_id != state.node_id:
            raise ValueError("node may only acknowledge itself")
        uuid.UUID(command_id)

        def mutate(document: dict[str, object] | None) -> Mapping[str, object]:
            if document is None:
                raise ValueError("hub state is unavailable")
            self._validate_document(document)
            commands = document["commands"]
            assert isinstance(commands, list)
            match = [item for item in commands if isinstance(item, dict) and item.get("command_id") == command_id and item.get("target_node_id") == actor_node_id]
            if len(match) != 1:
                raise ValueError("command acknowledgement is not pending")
            if state.game_mode is not match[0].get("game_mode"):
                raise ValueError("command acknowledgement does not match desired state")
            commands.remove(match[0])
            nodes = document["nodes"]
            assert isinstance(nodes, dict)
            previous = nodes.get(actor_node_id)
            previous_qwen = previous.get("state", {}).get("qwen_state") if isinstance(previous, dict) else None
            now = self.clock()
            if isinstance(previous, dict) and isinstance(previous.get("state"), dict):
                previous_state = GameModeState.from_dict(previous["state"])
                if state.revision < previous_state.revision or (
                    state.revision == previous_state.revision
                    and _parse_utc(state.updated_utc) < _parse_utc(previous_state.updated_utc)
                ):
                    raise ValueError("stale command acknowledgement was rejected")
            nodes[actor_node_id] = {"state": state.to_dict(), "last_seen_utc": now}
            if state.qwen_state.value in {"running", "stopped"} and previous_qwen != state.qwen_state.value:
                snapshot = self._snapshot_from_document(document, now)
                self._append_notification(document, Notification(str(uuid.uuid4()), actor_node_id, state.qwen_state.value, format_line_status(actor_node_id, state.qwen_state, snapshot), now))
            return document

        self.store.update(mutate)

    def snapshot(self) -> dict[str, NodeStatus]:
        return self._snapshot_from_document(self._load(), self.clock())

    def pending_notifications(self) -> tuple[Notification, ...]:
        values = self._load()["notifications"]
        assert isinstance(values, list)
        return tuple(Notification(**value) for value in values if isinstance(value, dict) and value.get("status") == "pending")

    def mark_notification_delivered(self, notification_id: str) -> None:
        uuid.UUID(notification_id)

        def mutate(document: dict[str, object] | None) -> Mapping[str, object]:
            if document is None:
                raise ValueError("hub state is unavailable")
            self._validate_document(document)
            values = document["notifications"]
            assert isinstance(values, list)
            matches = [item for item in values if isinstance(item, dict) and item.get("notification_id") == notification_id]
            if len(matches) != 1:
                raise ValueError("notification is unavailable")
            matches[0]["status"] = "delivered"
            matches[0]["attempts"] = int(matches[0].get("attempts", 0)) + 1
            return document

        self.store.update(mutate)

    def mark_notification_dead_letter(self, notification_id: str, error_code: str) -> None:
        uuid.UUID(notification_id)
        if not error_code or len(error_code) > 64 or not error_code.replace("_", "").isalnum():
            raise ValueError("notification error code is invalid")

        def mutate(document: dict[str, object] | None) -> Mapping[str, object]:
            if document is None:
                raise ValueError("hub state is unavailable")
            self._validate_document(document)
            values = document["notifications"]
            assert isinstance(values, list)
            matches = [item for item in values if isinstance(item, dict) and item.get("notification_id") == notification_id]
            if len(matches) != 1 or matches[0].get("status") != "pending":
                raise ValueError("pending notification is unavailable")
            matches[0]["status"] = "dead_letter"
            matches[0]["attempts"] = int(matches[0].get("attempts", 0)) + 1
            matches[0]["last_error_code"] = error_code
            return document

        self.store.update(mutate)

    def _append_notification(self, document: dict[str, object], item: Notification) -> None:
        values = document["notifications"]
        assert isinstance(values, list)
        candidate = item.to_dict()

        def fits() -> bool:
            encoded = json.dumps(values + [candidate], ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")
            return len(values) + 1 <= self.max_notification_records and len(encoded) <= self.max_notification_bytes

        # 只有已成功投遞的 audit 可在空間壓力下 compact；pending/dead-letter 永不靜默刪除。
        while not fits():
            delivered_index = next((index for index, value in enumerate(values) if isinstance(value, dict) and value.get("status") == "delivered"), None)
            if delivered_index is None:
                raise OutboxFullError("notification outbox is full")
            del values[delivered_index]
        values.append(candidate)

    def _snapshot_from_document(self, document: Mapping[str, object], now_text: str) -> dict[str, NodeStatus]:
        now = _parse_utc(now_text)
        raw_nodes = document["nodes"]
        assert isinstance(raw_nodes, dict)
        result: dict[str, NodeStatus] = {}
        for node_id in NODE_IDS:
            raw = raw_nodes.get(node_id)
            if isinstance(raw, dict) and isinstance(raw.get("state"), dict) and isinstance(raw.get("last_seen_utc"), str):
                state = GameModeState.from_dict(raw["state"])
                last_seen = str(raw["last_seen_utc"])
                online = now - _parse_utc(last_seen) <= timedelta(seconds=self.online_seconds)
            else:
                state = GameModeState(node_id, False, QwenState.UNKNOWN, NODES[node_id].qwen_profile, 0, now_text, "not_seen")
                last_seen = now_text
                online = False
            result[node_id] = NodeStatus(state, online, last_seen)
        return result


class OutboxFullError(RuntimeError):
    pass
