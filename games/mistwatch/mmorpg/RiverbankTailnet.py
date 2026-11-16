"""以 Tailscale 的既有裝置身分連線至私有河岸世界。"""
from contextlib import closing
from dataclasses import dataclass
from hashlib import sha256
from ipaddress import IPv4Address, IPv4Network
import json
import socket
import subprocess
import time

from VerifyWorldServer import hello

TAILNET_V4 = IPv4Network('100.64.0.0/10')


@dataclass(frozen=True)
class Node:
    address: str
    profile_scope: str


def tailscale(*args):
    response = subprocess.check_output(['tailscale.exe', *args], creationflags=subprocess.CREATE_NO_WINDOW, timeout=10)
    result = json.loads(response or '{}')
    if result is not None and not isinstance(result, dict):
        raise RuntimeError('Tailscale 並未提供有效的裝置狀態')
    return result or {}


def address(text):
    if IPv4Address(text) not in TAILNET_V4:
        raise ValueError('私有連線需要一台 Tailscale 裝置的 IPv4 位址')
    return text


def node_from_status(entry, requested=None):
    if not entry.get('Online') or not entry.get('ID'):
        raise RuntimeError('Tailscale 裝置目前離線或身分無法確認')
    values = [address(item) for item in entry.get('TailscaleIPs', ()) if ':' not in item and IPv4Address(item) in TAILNET_V4]
    if requested is not None:
        values = [item for item in values if item == requested]
    if len(values) != 1:
        raise RuntimeError('無法確認這台裝置唯一的 Tailscale IPv4 位址')
    return Node(values[0], sha256(entry['ID'].encode('utf-8')).hexdigest())


def network_nodes(destination=None):
    status = tailscale('status', '--json')
    if status.get('BackendState') != 'Running':
        raise RuntimeError('請先連上 Tailscale 私有網路')
    self_node = node_from_status(status.get('Self', {}))
    if destination is None or destination == self_node.address:
        return self_node, self_node
    matches = [peer for peer in (status.get('Peer') or {}).values() if destination in peer.get('TailscaleIPs', ())]
    if len(matches) != 1:
        raise RuntimeError('目標位址不在目前的 Tailscale 裝置清單')
    return self_node, node_from_status(matches[0], destination)


def open_peer(local_address, peer_address, port, timeout=5):
    return socket.create_connection((peer_address, port), timeout=timeout, source_address=(local_address, 0))


def port_reserved(configuration, port):
    return (str(port) in (configuration.get('TCP') or {}) or
            any(key.rsplit(':', 1)[-1] == str(port) and enabled for key, enabled in
                (configuration.get('AllowFunnel') or {}).items()) or
            any(port_reserved(entry, port) for entry in (configuration.get('Foreground') or {}).values()))


def wait_for_forwarding(relay, self_node, port, epoch):
    deadline = time.monotonic() + 12
    while time.monotonic() < deadline:
        if relay.poll() is not None:
            raise RuntimeError(f'Tailscale 私有分享未啟動，退出碼 {relay.returncode}，請查看 tailnet.log')
        try:
            with closing(open_peer(self_node.address, self_node.address, port, timeout=1)) as peer:
                received_epoch, _ = hello(peer)
        except (OSError, EOFError):
            time.sleep(.25)
            continue
        if received_epoch != epoch:
            raise RuntimeError('Tailscale 轉送的世界與本機啟動的伺服器不符')
        return
    raise RuntimeError('逾時等待 Tailscale 私有連線，請查看 tailnet.log')
