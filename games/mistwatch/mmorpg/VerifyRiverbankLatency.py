"""以固定雙向延遲轉送原生客戶端，核對伺服器實際採用的移動。"""
from collections import deque
from contextlib import closing
from hashlib import sha256
import json
from pathlib import Path
import queue
import select
import socket
import struct
import subprocess
import sys
from tempfile import mkdtemp
import threading
import time

from VerifyWorldServer import HEADER, choose, connect, crc32c, hello, ready_line, snapshot, wait


ONE_WAY_SECONDS = .1


def delayed_relay(listener, server_port, done, errors):
    try:
        source, _ = listener.accept()
        with closing(source), closing(connect(server_port)) as target:
            peers = {source: target, target: source}
            queued = {peer: deque() for peer in peers}
            while not done.is_set():
                now = time.monotonic()
                for peer, pending in queued.items():
                    while pending and pending[0][0] <= now:
                        peers[peer].sendall(pending.popleft()[1])
                next_due = min((p[0][0] - now for p in queued.values() if p), default=.02)
                readable, _, _ = select.select(list(peers), [], [], max(0, min(.02, next_due)))
                for peer in readable:
                    data = peer.recv(65536)
                    if not data:
                        return
                    queued[peer].append((time.monotonic() + ONE_WAY_SECONDS, data))
    except OSError as error:
        if not done.is_set():
            errors.put(error)


def verify_native(port, epoch, observer, observer_id, native):
    with closing(socket.socket()) as listener:
        listener.bind(('127.0.0.1', 0))
        listener.settimeout(6)
        listener.listen(1)
        done, errors = threading.Event(), queue.Queue()
        relay = threading.Thread(target=delayed_relay, args=(listener, port, done, errors), daemon=True)
        relay.start()
        native_port = str(listener.getsockname()[1])
        with subprocess.Popen([str(native), native_port], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                              text=True, creationflags=subprocess.CREATE_NO_WINDOW) as game:
            first_movement = None
            try:
                while game.poll() is None:
                    tick, positions, _ = snapshot(observer, epoch, observer_id)
                    actor = [player for player in positions if player != observer_id]
                    if actor and positions[actor[0]] != (0, 0) and first_movement is None:
                        first_movement = (actor[0], tick, positions[actor[0]])
                output, stderr = game.communicate(timeout=2)
                if not errors.empty():
                    error = errors.get_nowait()
                    # 客戶端正常結束時可能關閉仍有未讀快照的 TCP 連線。
                    if game.returncode or not isinstance(error, ConnectionResetError):
                        raise error
                if game.returncode or first_movement is None:
                    raise RuntimeError(f'雙向延遲下未觀測到原生角色移動，退出碼 {game.returncode}: {stderr.strip()}')
                actor, frame, offset = output.split()
                if (int(actor), int(frame), float(offset)) != (first_movement[0], first_movement[1], .1):
                    raise RuntimeError(f'原生客戶端與伺服器的首次移動不一致：{output.strip()} / {first_movement}')
                return first_movement
            finally:
                done.set()
                if game.poll() is None:
                    game.terminate()
                    game.wait(timeout=3)
                relay.join(timeout=2)



def movement_frame(epoch, tick, sequence, actor):
    payload = struct.pack('<QQQIhhhB', epoch, tick, sequence, actor, 32767, 0, 0, 0)
    frame = HEADER.pack(b'AENP', 1, 2, 0, len(payload), crc32c(payload), sha256(payload).digest()) + payload
    return struct.pack('!I', len(frame)) + frame


def verify_late_input_bounds(port, epoch, observer, observer_id):
    with closing(connect(port)) as source:
        session, actor = hello(source)
        assert session == epoch
        choose(source, 2)
        tick, positions, _ = wait(observer, epoch, observer_id, lambda _, states, __: actor in states)
        for _ in range(12):
            tick, positions, _ = snapshot(observer, epoch, observer_id)
        origin = positions[actor]
        # 過期序號多於輸入歷程的 256 筆視窗，延遲恢復後仍須能執行新指令。
        expired_count = 260
        source.sendall(b''.join(movement_frame(epoch, tick - 10, sequence, actor)
                                for sequence in range(1, expired_count + 1)))
        for _ in range(12):
            tick, positions, _ = snapshot(observer, epoch, observer_id)
            assert positions[actor] == origin, '過舊移動仍被伺服器採用'

        source.sendall(movement_frame(epoch, tick - 1, expired_count + 1, actor) +
                       movement_frame(epoch, tick, expired_count + 2, actor))
        applied, positions, _ = wait(observer, epoch, observer_id, lambda _, states, __: states[actor] != origin)
        assert positions[actor] == (.1, 0), '同批遲到的指令累積了位移'
        for _ in range(5):
            tick, positions, _ = snapshot(observer, epoch, observer_id)
            assert positions[actor] == (.1, 0), '同批遲到的指令延後重播'

        source.sendall(movement_frame(epoch, tick + 3, expired_count + 1, actor))
        for _ in range(6):
            _, positions, _ = snapshot(observer, epoch, observer_id)
            assert positions[actor] == (.1, 0), '已採用的移動序號被重播'
        return dict(single_step_m=.1, applied_tick=applied, expired_input_ignored=True,
                    discarded_input_count=expired_count, coalesced_input_not_replayed=True,
                    repeated_sequence_ignored=True)


def verify(server, native, output):
    folder = Path(mkdtemp(prefix='latency-', dir=output))
    with subprocess.Popen([str(server), '0'], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          text=True, creationflags=subprocess.CREATE_NO_WINDOW) as child:
        try:
            port, epoch = ready_line(child)
            with closing(connect(port)) as observer:
                received, observer_id = hello(observer)
                assert received == epoch
                choose(observer, 3)
                movement = verify_native(port, epoch, observer, observer_id, native)
                late_policy = verify_late_input_bounds(port, epoch, observer, observer_id)
        finally:
            child.terminate()
            child.wait(timeout=5)
    receipt = dict(one_way_ms=ONE_WAY_SECONDS * 1000, native_actor=movement[0], movement_tick=movement[1],
                   movement_m=movement[2], late_policy=late_policy, server_sha256=sha256(server.read_bytes()).hexdigest(),
                   native_sha256=sha256(native.read_bytes()).hexdigest())
    path = folder / 'LatencyVerification.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(path)


if __name__ == '__main__':
    verify(*(Path(arg).resolve(strict=True) for arg in sys.argv[1:]))
