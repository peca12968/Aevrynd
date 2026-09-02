"""Closed three-node inventory and network topology."""

from __future__ import annotations

import ipaddress
from dataclasses import dataclass


@dataclass(frozen=True, slots=True)
class NodeSpec:
    node_id: str
    display_name: str
    addresses: tuple[str, ...]
    qwen_profile: str
    hub_url: str

    def __post_init__(self) -> None:
        if self.node_id not in {"main", "taichung", "taoyuan"}:
            raise ValueError("node is not allowlisted")
        if self.qwen_profile not in {"light", "full"}:
            raise ValueError("Qwen profile is invalid")
        if not self.addresses or any(
            ipaddress.ip_address(address).version != 4 for address in self.addresses
        ):
            raise ValueError("node address is invalid")


# 節點、位址、角色和主機連線路徑皆為封閉清單，設定檔不得覆寫。
NODES: dict[str, NodeSpec] = {
    "main": NodeSpec(
        "main",
        "Main PC",
        ("127.0.0.1", "100.115.174.48", "192.168.50.18"),
        "light",
        "http://127.0.0.1:8766",
    ),
    "taichung": NodeSpec(
        "taichung",
        "Taichung PC",
        ("192.168.50.98",),
        "full",
        "http://192.168.50.18:8766",
    ),
    "taoyuan": NodeSpec(
        "taoyuan",
        "Taoyuan PC",
        ("100.89.42.36",),
        "full",
        "http://100.115.174.48:8766",
    ),
}

NODE_IDS = frozenset(NODES)
AGENT_BIND = ("127.0.0.1", 8765)
HUB_PORT = 8766
HUB_BIND_ADDRESSES = frozenset({"127.0.0.1", "100.115.174.48", "192.168.50.18"})


def get_node(node_id: str) -> NodeSpec:
    try:
        return NODES[node_id]
    except (KeyError, TypeError) as error:
        raise ValueError("node is not allowlisted") from error


def validate_bind(role: str, host: str, port: int) -> tuple[str, int]:
    """Reject wildcard, hostname and configuration-selected listener addresses."""

    if role == "agent" and (host, port) == AGENT_BIND:
        return host, port
    if role == "hub" and host in HUB_BIND_ADDRESSES and port == HUB_PORT:
        return host, port
    raise ValueError("listener address is not allowlisted")
