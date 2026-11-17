"""使用正式啟動器連線實際伺服器，核對啟動收據與隔離存檔。"""
from contextlib import ExitStack, closing, contextmanager
import json
import os
from queue import Queue
from secrets import token_bytes
import socket
from pathlib import Path
import subprocess
import sys
from tempfile import mkdtemp
from threading import Thread
import time

from RiverbankTailnet import network_nodes, open_peer, tailscale
from VerifyRiverbankProfiles import login_request, login_response
from VerifyWorldServer import hello, snapshot_with_classes


def check_room(server, evidence_dir, capacity=None):
    data = Path(mkdtemp(prefix='launcher-receipt-', dir=evidence_dir))
    launcher = Path(__file__).with_name('LaunchRiverbank.py')
    command = [sys.executable, '-B', str(launcher), '--server', str(server), '--data', str(data), '--check-server']
    if capacity is not None:
        command.extend(('--capacity', str(capacity)))
    result = subprocess.run(command, capture_output=True, check=True, text=True, timeout=8)
    (receipt,) = (data / 'sessions').glob('riverbank-*/launch.json')
    persisted = json.loads(receipt.read_text(encoding='utf-8'))
    if persisted != json.loads(result.stdout) or persisted['room_capacity'] != (capacity or 16):
        raise AssertionError('主控台與啟動收據不一致')
    if not (0 < persisted['port'] <= 65535 and persisted['epoch'] > 0
            and persisted['server_pid'] > 0 and persisted['player'] == 1
            and persisted['snapshot_tick'] > 0):
        raise AssertionError('啟動收據缺少實際連線證明')
    if (data / 'RiverbankProfiles.sqlite3').exists() or not (receipt.parent / 'RiverbankProfiles.sqlite3').is_file():
        raise AssertionError('檢查模式未隔離角色存檔')
    if receipt.with_suffix('.json.tmp').exists():
        raise AssertionError('原子寫入留下暫存收據')
    print(f'RIVERBANK_LAUNCH_RECEIPT_VERIFIED {receipt}')


@contextmanager
def dedicated_room(server, data, private):
    launcher = Path(__file__).with_name('LaunchRiverbank.py')
    command = [sys.executable, '-B', str(launcher), '--server', str(server), '--data', str(data),
               '--host-only', '--capacity', '32']
    if private:
        command.append('--share-tailnet')
    previous_sessions = set((data / 'sessions').glob('riverbank-*'))
    process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                               text=True, encoding='utf-8', env=os.environ | {'PYTHONUTF8': '1'},
                               creationflags=subprocess.CREATE_NO_WINDOW)
    try:
        announcements = Queue(maxsize=1)
        Thread(target=lambda: announcements.put(process.stdout.readline()), daemon=True).start()
        raw = announcements.get(timeout=24 if private else 12)
        if not raw:
            raise RuntimeError(f'專用伺服器無法啟動：{process.stderr.read()}')
        report = json.loads(raw)
        session, = set((data / 'sessions').glob('riverbank-*')) - previous_sessions
        persisted = json.loads((session / 'launch.json').read_text(encoding='utf-8'))
        assert persisted == report and report['mode'] == 'dedicated'
        assert report['room_capacity'] == 32 and 0 < report['port'] <= 65535 and report['server_pid'] > 0
        assert 'client_pids' not in report
        yield report
        assert process.poll() is None
        stdout, stderr = process.communicate('\n', timeout=14)
        assert process.returncode == 0 and not stdout and not stderr, (process.returncode, stdout, stderr)
        with socket.socket() as probe:
            probe.settimeout(1)
            assert probe.connect_ex(('127.0.0.1', report['port'])) != 0, '伺服器仍在監聽'
    finally:
        if process.poll() is None:
            try:
                process.communicate('\n', timeout=14)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate(timeout=3)
        process.stdout.close()
        process.stderr.close()


def room_players(report, roles, endpoint=None):
    roster = {}
    with ExitStack() as peers:
        for index, (ticket, role) in enumerate(roles):
            peer = peers.enter_context(closing(
                open_peer(endpoint, endpoint, report['port']) if endpoint and index else
                socket.create_connection(('127.0.0.1', report['port']), timeout=5)))
            epoch, player = hello(peer)
            assert epoch == report['epoch']
            login_request(peer, ticket, role)
            selected = login_response(peer)
            assert selected == (role if role else 2)
            roster[player] = selected
            current_peer = peer
            current_player = player
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            tick, _, _, professions = snapshot_with_classes(current_peer, epoch, current_player)
            if all(professions.get(player) == role for player, role in roster.items()):
                assert tick > 0 and len(roster) == len(roles)
                return tick
    raise AssertionError('多人快照未收到所有已登入職業')


def check_dedicated(server, evidence_dir, private=False):
    data = Path(mkdtemp(prefix='dedicated-room-', dir=evidence_dir))
    endpoint, before = None, None
    if private:
        node, _ = network_nodes()
        endpoint = node.address
        before = tailscale('serve', 'status', '--json')
    guardian, ranger = token_bytes(32), token_bytes(32)
    with dedicated_room(server, data, private) as room:
        if private:
            assert room['tailnet_address'] == endpoint
        tick = room_players(room, ((guardian, 1), (ranger, 2)), endpoint)
        assert (data / 'RiverbankProfiles.sqlite3').is_file()
    if private:
        assert tailscale('serve', 'status', '--json') == before
    with dedicated_room(server, data, False) as restarted:
        assert restarted['epoch'] != room['epoch']
        retained = room_players(restarted, ((ranger, 0),))

    result = dict(stage='DEDICATED_ROOM_VERIFIED', first_world=room['epoch'], snapshot_tick=tick,
                  persisted_role=2, restored_world=restarted['epoch'], restored_snapshot_tick=retained,
                  local_clients=1 if private else 2, private_clients=int(private), physical_hosts=1)
    proof = data / 'DedicatedRoomVerification.json'
    proof.write_text(json.dumps(result, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(f'RIVERBANK_DEDICATED_ROOM_VERIFIED {proof}')


if __name__ == '__main__':
    server, evidence_dir = map(Path, sys.argv[1:3])
    check_room(server, evidence_dir)
    check_room(server, evidence_dir, capacity=32)
    check_dedicated(server, evidence_dir, private='--private' in sys.argv[3:])
