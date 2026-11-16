"""駕駛兩個獨立程序連上同一個本機河岸伺服器。"""
from contextlib import ExitStack, closing
from datetime import datetime, timezone
from hashlib import sha256
import json
import os
from pathlib import Path
import queue
import socket
import struct
import subprocess
import sys
import threading
import time

HEADER = struct.Struct('<4sHBBII32s')
SNAPSHOT = struct.Struct('<5QIBII')
ENTITY = struct.Struct('<QQ3q3i3fI')
ENCOUNTER_FRAME = 256


def crc32c_entry(value):
    for _ in range(8):
        value = (value >> 1) ^ (0x82f63b78 if value & 1 else 0)
    return value


# 逐位元多項式折疊成單一位元組表，仍核對傳輸原有的 CRC32C 與 SHA-256。
CRC32C_TABLE = tuple(crc32c_entry(value) for value in range(256))


def crc32c(payload):
    crc = 0xffffffff
    for byte in payload:
        crc = (crc >> 8) ^ CRC32C_TABLE[(crc ^ byte) & 0xff]
    return crc ^ 0xffffffff


def exact(peer, n):
    data = bytearray()
    while len(data) < n:
        item = peer.recv(n - len(data))
        if not item:
            raise EOFError('connection closed mid-frame')
        data.extend(item)
    return data


def packet(peer):
    size, = struct.unpack('!I', exact(peer, 4))
    assert HEADER.size <= size <= 65536
    raw = exact(peer, size)
    if raw[:4] == b'AECB':
        assert size == 89 and raw[:6] == b'AECB\x01\x02'
        return ENCOUNTER_FRAME, raw[6:]
    magic, version, kind, reserved, length, crc, digest = HEADER.unpack_from(raw)
    data = raw[HEADER.size:]
    assert (magic, version, reserved, length) == (b'AENP', 1, 0, len(data))
    assert sha256(data).digest() == digest and crc32c(data) == crc
    return kind, data


def connect(port):
    return socket.create_connection(('127.0.0.1', port), timeout=5)


def hello(peer):
    kind, body = packet(peer)
    assert kind == 1
    epoch, player, version = struct.unpack('<QIH', body)
    assert epoch and player and version == 1
    return epoch, player


def choose(peer, job):
    payload = b'AECL\x01' + bytes([job])
    peer.sendall(struct.pack('!I', len(payload)) + payload)


def snapshot_with_flags(peer, epoch, player):
    kind, body = packet(peer)
    if kind == ENCOUNTER_FRAME:
        kind, body = packet(peer)
    assert kind == 3
    session, tick, _, _, _, destination, full, entities, gone = SNAPSHOT.unpack_from(body)
    assert session == epoch and destination == player and full == 1
    assert len(body) == SNAPSHOT.size + entities * ENTITY.size + gone * 16
    states, entity_flags, offset = {}, {}, SNAPSHOT.size
    for _ in range(entities):
        hi, low, chunk_x, _, chunk_z, x_mm, _, z_mm, *_, flags = ENTITY.unpack_from(body, offset)
        assert hi == epoch and low not in states
        assert bool(flags & 1) == (low == player)
        states[low] = (chunk_x * 32 + x_mm / 1000, chunk_z * 32 + z_mm / 1000)
        entity_flags[low] = flags
        offset += ENTITY.size
    removed = [struct.unpack_from('<QQ', body, offset + 16 * i) for i in range(gone)]
    return tick, states, removed, entity_flags


def snapshot_with_classes(peer, epoch, player):
    tick, states, removed, flags = snapshot_with_flags(peer, epoch, player)
    return tick, states, removed, {actor: (value >> 8) & 255 for actor, value in flags.items()}


def snapshot(peer, epoch, player):
    tick, states, removed, _ = snapshot_with_classes(peer, epoch, player)
    return tick, states, removed


def wait(peer, epoch, player, predicate):
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        view = snapshot(peer, epoch, player)
        if predicate(*view):
            return view
    raise AssertionError('shared world condition did not arrive')


def move(port):
    with closing(connect(port)) as peer:
        epoch, player = hello(peer)
        choose(peer, 2)
        tick, _, _ = wait(peer, epoch, player, lambda _, states, __: len(states) == 2)
        data = struct.pack('<QQQIhhhB', epoch, tick + 8, 1, player, 32767, 0, 0, 0)
        payload = HEADER.pack(b'AENP', 1, 2, 0, len(data), crc32c(data), sha256(data).digest()) + data
        frame = struct.pack('!I', len(payload)) + payload
        peer.sendall(frame[:2])
        time.sleep(.075)
        peer.sendall(frame[2:])
        event_tick, states, _ = wait(peer, epoch, player, lambda _, states, __: states.get(player) == (.1, 0.))
        assert event_tick == tick + 8
        print(json.dumps(dict(pid=os.getpid(), player=player, tick=event_tick, position=states[player])))


def ready_line(server):
    lines = queue.Queue()
    threading.Thread(target=lambda: lines.put(server.stdout.readline()), daemon=True).start()
    line = lines.get(timeout=5).strip().split()
    assert len(line) == 3 and line[0] == 'READY', line
    return int(line[1].split('=')[1]), int(line[2].split('=')[1])


def verify_classes(port, epoch):
    with closing(connect(port)) as invalid:
        assert hello(invalid)[0] == epoch
        choose(invalid, 99)
        assert invalid.recv(1) == b''
    with ExitStack() as cleanup:
        choices, clients = {}, []
        for job in (1, 2, 3):
            peer = cleanup.enter_context(closing(connect(port)))
            session, player = hello(peer)
            assert session == epoch
            choices[player] = job
            clients.append((peer, player))
            choose(peer, job)
        for peer, player in clients:
            deadline = time.monotonic() + 4
            while True:
                _, _, _, observed = snapshot_with_classes(peer, epoch, player)
                if observed == choices:
                    break
                assert time.monotonic() < deadline, observed
        guardian, guardian_id = clients[0]
        choose(guardian, 2)
        ranger, ranger_id = clients[1]
        _, _, gone = wait(ranger, epoch, ranger_id, lambda _, states, gone: guardian_id not in states and
                           (epoch, guardian_id) in gone)
        assert gone == [(epoch, guardian_id)]
        return choices


def verify(executable, output, native_client):
    args = [str(executable), '0']
    with subprocess.Popen(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                          creationflags=subprocess.CREATE_NO_WINDOW) as server:
        try:
            port, epoch = ready_line(server)
            with closing(connect(port)) as observer:
                session, observer_id = hello(observer)
                assert session == epoch
                choose(observer, 3)
                command = ([str(native_client), str(port)] if native_client else
                           [sys.executable, '-B', __file__, '--move', str(port)])
                with subprocess.Popen(command, stdout=subprocess.PIPE,
                                      text=True, creationflags=subprocess.CREATE_NO_WINDOW) as mover:
                    tick, states, _ = wait(observer, epoch, observer_id, lambda _, states, __: len(states) == 2
                                           and any(x > 0 for x, _ in states.values()))
                    stdout, _ = mover.communicate(timeout=7)
                    assert mover.returncode == 0
                    if native_client:
                        player, frame, x = stdout.split()
                        movement = dict(pid=mover.pid, player=int(player), tick=int(frame), position=[float(x), 0.])
                    else:
                        movement = json.loads(stdout)
                assert movement['pid'] == mover.pid and movement['tick'] == tick
                assert states == {observer_id: (0., 0.), movement['player']: (.1, 0.)}
                _, remaining, gone = wait(observer, epoch, observer_id,
                    lambda _, states, despawn: len(states) == 1 and (epoch, movement['player']) in despawn)
                assert remaining == {observer_id: (0., 0.)} and gone == [(epoch, movement['player'])]
                with closing(connect(port)) as joined:
                    joined_epoch, joined_id = hello(joined)
                    assert joined_epoch == epoch and joined_id not in states
                    for _ in range(3):
                        assert snapshot(observer, epoch, observer_id)[1] == {observer_id: (0., 0.)}
                    choose(joined, 1)
                    _, current, _, roles = snapshot_with_classes(joined, epoch, joined_id)
                    assert set(current) == {observer_id, joined_id} and roles == {observer_id: 3, joined_id: 1}
            three_classes = verify_classes(port, epoch)
            assert server.poll() is None
        finally:
            server.terminate()
            server.wait(timeout=5)
    record = dict(status='TWO_PROCESSES_SHARED_WORLD_VERIFIED', at=datetime.now(timezone.utc).isoformat(),
                  server_sha256=sha256(executable.read_bytes()).hexdigest(), server_pid=server.pid,
                  simultaneous_clients=2, movement=movement, observer_id=observer_id, observer_pid=os.getpid(), despawn=True,
                  new_client_appeared=True, native_client=bool(native_client), server_scope='127.0.0.1',
                  class_selection_required=True, three_clients_shared_classes=three_classes,
                  invalid_selection_rejected=True, repeated_selection_rejected=True,
                  world_rendered=False, persistent_login_verified=False)
    output.write_text(json.dumps(record, indent=2) + '\n', encoding='utf-8')
    print(record['status'], 'tick', tick, 'position', movement['position'])


if __name__ == '__main__':
    if sys.argv[1] == '--move':
        move(int(sys.argv[2]))
    else:
        verify(Path(sys.argv[1]).resolve(strict=True), Path(sys.argv[2]),
               Path(sys.argv[3]).resolve(strict=True) if len(sys.argv) == 4 else None)
