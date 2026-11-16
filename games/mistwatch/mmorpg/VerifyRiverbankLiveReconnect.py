"""驗證相同角色可回到仍在運行的河岸世界。"""
from contextlib import ExitStack, closing
from pathlib import Path
from tempfile import mkdtemp
import json
import os
import queue
import secrets
import select
import socket
import subprocess
import sys
import threading
import time

import VerifyRiverbankBattle as battle
from VerifyRiverbankGuild import CREATE, OK, VIEW, Player
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world


def until(peer, epoch, player, predicate):
    end = time.monotonic() + 4
    while time.monotonic() < end:
        frame = battle.read(peer, epoch, player)
        if predicate(frame):
            return frame
    raise RuntimeError('指定的多人快照未到達')


def native_roundtrip(server, client, folder, manual=False):
    ticket = secrets.token_bytes(32)
    with start(server, folder / 'NativeProfiles.sqlite3') as (port, _epoch), closing(socket.socket()) as listener:
        listener.bind(('127.0.0.1', 0))
        listener.listen(2)
        listener.settimeout(5)
        drop = threading.Event()
        retry_allowed = threading.Event()
        done = threading.Event()
        failure = queue.Queue()

        def relay():
            try:
                for first in (True, False):
                    if manual and not first:
                        listener.settimeout(.15)
                        while not done.is_set() and not retry_allowed.is_set():
                            try:
                                rejected, _ = listener.accept()
                                rejected.close()
                            except TimeoutError:
                                pass
                        if done.is_set():
                            return
                        listener.settimeout(7)
                    source, _ = listener.accept()
                    with closing(source), closing(world.connect(port)) as target:
                        peers = {source: target, target: source}
                        while not done.is_set() and not (first and drop.is_set()):
                            readable, _, _ = select.select(list(peers), [], [], .025)
                            for handle in readable:
                                data = handle.recv(4096)
                                if not data:
                                    return
                                peers[handle].sendall(data)
            except OSError as error:
                if not done.is_set():
                    failure.put(error)

        forwarding = threading.Thread(target=relay, daemon=True)
        forwarding.start()
        env = dict(os.environ, AEVRYND_MISTWATCH_PROFILE_TICKET=ticket.hex())
        mode = '--manual-reconnect' if manual else '--live-reconnect'
        with subprocess.Popen([str(client), str(listener.getsockname()[1]), mode], env=env, text=True,
                              stdout=subprocess.PIPE, stderr=subprocess.PIPE, creationflags=subprocess.CREATE_NO_WINDOW) as game:
            opened = queue.Queue()
            reader = threading.Thread(target=lambda: opened.put(game.stdout.readline().strip()), daemon=True)
            reader.start()
            try:
                initial = opened.get(timeout=6)
                if not initial.startswith('READY_FOR_TRANSPORT_DROP '):
                    raise RuntimeError(f'原生客戶端未完成首次登入：{initial}')
                dropped_at = time.monotonic()
                drop.set()
                if manual:
                    reader.join(timeout=1)
                    threading.Thread(target=lambda: opened.put(game.stdout.readline().strip()), daemon=True).start()
                    ended = opened.get(timeout=36)
                    elapsed = time.monotonic() - dropped_at
                    if ended != 'MANUAL_RETRY_REQUIRED ' + initial.split()[1] or elapsed < 29:
                        raise RuntimeError(f'自動重連尚未完整逾時：{elapsed:.2f} 秒 {ended}')
                    retry_allowed.set()
                output, errors = game.communicate(timeout=9 if manual else 8)
                accepted = output.strip()
                if game.returncode or not accepted.startswith('NATIVE_LIVE_RECONNECT_PASS '):
                    raise RuntimeError(f'原生客戶端重連未通過：{game.returncode} {accepted} {errors}')
                if not failure.empty():
                    raise failure.get_nowait()
                return {'original_connection': int(initial.split()[1]), 'new_connection': int(accepted.split()[1]),
                        'native_client_sha256': world.sha256(client.read_bytes()).hexdigest(),
                        'manual_retry': manual, 'offline_seconds': round(elapsed, 3) if manual else None}
            finally:
                done.set()
                if game.poll() is None:
                    game.terminate()
                    game.wait(timeout=3)
                forwarding.join(timeout=2)


def run(server, output, client=None):
    folder = Path(mkdtemp(prefix='live-reconnect-', dir=output)).resolve()
    database = folder / 'RiverbankProfiles.sqlite3'
    ticket, observer_ticket = secrets.token_bytes(32), secrets.token_bytes(32)
    with start(server, database) as (port, epoch), ExitStack() as sockets:
        watch_socket = sockets.enter_context(closing(world.connect(port)))
        observer = Player(watch_socket, observer_ticket, epoch, 3, True)
        with closing(world.connect(port)) as first_socket:
            original = Player(first_socket, ticket, epoch, 1, True)
            result, guild = original.guild(CREATE, name='Riverbank Lookout'.encode())
            assert result == OK and guild[0] > 0
            tick, positions, _ = until(first_socket, epoch, original.id, lambda f: len(f[1]) == 2)
            for sequence in range(1, 31):
                battle.send_movement(first_socket, epoch, tick, sequence, original.id, 32767, 0)
                tick, positions, _ = battle.read(first_socket, epoch, original.id)
            for _ in range(5):
                tick, positions, _ = battle.read(first_socket, epoch, original.id)
            before = positions[original.id]
            assert before[0] > 2
            before_tick = tick
        _, gone, _ = until(watch_socket, epoch, observer.id, lambda f: original.id not in f[1])
        assert observer.id in gone

        with closing(world.connect(port)) as next_socket:
            returning = Player(next_socket, ticket, epoch, 1)
            next_tick, positions, combat = until(next_socket, epoch, returning.id, lambda f: len(f[1]) == 2)
            restored = positions[returning.id]
            assert original.id != returning.id and original.id not in positions
            assert returning.guild(VIEW) == (OK, guild) and combat['role'] == 1
            receipt = dict(epoch=epoch, original_connection=original.id, new_connection=returning.id,
                           before=before, restored=restored, class_and_guild_restored=True,
                           server_sha256=world.sha256(server.read_bytes()).hexdigest())
            if before != restored:
                receipt.update(status='FIRST_DIVERGENCE', difference='same_world_position_reset')
            else:
                next_tick, _, _ = until(next_socket, epoch, returning.id, lambda f: f[0] > next_tick)
                battle.send_movement(next_socket, epoch, next_tick, 1, returning.id, 32767, 0)
                arrival, _, _ = until(next_socket, epoch, returning.id, lambda f: f[1][returning.id][0] > restored[0])
                assert arrival == next_tick + 3
                receipt.update(status='LIVE_RECONNECT_VERIFIED', tick_gap=next_tick - before_tick,
                               new_input_accepted_on_scheduled_tick=True)
        proof = folder / 'LiveReconnectVerification.json'
        if before != restored:
            proof.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
            print(proof, flush=True)
            raise RuntimeError(f'同一世界重新登入後座標重置，{before} → {restored}')
    if client:
        receipt['native_automatic_reconnect'] = native_roundtrip(server, client, folder)
    proof.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(proof)


if __name__ == '__main__':
    if len(sys.argv) == 5 and sys.argv[-1] == '--manual':
        server, destination, client = (Path(argument).resolve(strict=True) for argument in sys.argv[1:4])
        base = Path(mkdtemp(prefix='manual-reconnect-', dir=destination))
        receipt = native_roundtrip(server, client, base, manual=True)
        receipt.update(status='LIVE_MANUAL_RECONNECT_VERIFIED',
                       server_sha256=world.sha256(server.read_bytes()).hexdigest())
        output = base/'ManualReconnectVerification.json'
        output.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
        print(output,flush=True)
    else:
        run(*(Path(argument).resolve(strict=True) for argument in sys.argv[1:]))
