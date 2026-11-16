"""以本機已建置的專用伺服器量測多人快照、移動、回應時間與記憶體。"""
import argparse
from contextlib import ExitStack, closing
import ctypes
from ctypes import wintypes
from datetime import datetime, timezone
from hashlib import sha256
import json
from math import ceil
import os
from pathlib import Path
from secrets import token_bytes
from statistics import median
import subprocess
from tempfile import mkdtemp
import time

import VerifyRiverbankBattle as battle
from VerifyRiverbankGuild import OK, VIEW, guild_command
from VerifyRiverbankProfiles import login_request, login_response
from VerifyWorldServer import connect, hello, ready_line, wait


class MemoryCounters(ctypes.Structure):
    _fields_ = [('size', wintypes.DWORD), ('faults', wintypes.DWORD)] + [
        (name, ctypes.c_size_t) for name in (
            'peak_working_set', 'working_set', 'peak_paged_pool', 'paged_pool',
            'peak_nonpaged_pool', 'nonpaged_pool', 'pagefile', 'peak_pagefile', 'private_bytes')]


class ProcessCounters:
    def __init__(self, process_id):
        self.kernel = ctypes.WinDLL('kernel32', use_last_error=True)
        self.psapi = ctypes.WinDLL('psapi', use_last_error=True)
        self.kernel.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.CloseHandle.argtypes = (wintypes.HANDLE,)
        self.kernel.GetProcessTimes.argtypes = (wintypes.HANDLE,) + (ctypes.POINTER(wintypes.FILETIME),) * 4
        self.psapi.GetProcessMemoryInfo.argtypes = (wintypes.HANDLE, ctypes.POINTER(MemoryCounters), wintypes.DWORD)
        self.handle = self.kernel.OpenProcess(0x0410, False, process_id)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())

    def close(self):
        self.kernel.CloseHandle(self.handle)

    def sample(self):
        times = [wintypes.FILETIME() for _ in range(4)]
        if not self.kernel.GetProcessTimes(self.handle, *(ctypes.byref(value) for value in times)):
            raise ctypes.WinError(ctypes.get_last_error())
        memory = MemoryCounters()
        memory.size = ctypes.sizeof(memory)
        if not self.psapi.GetProcessMemoryInfo(self.handle, ctypes.byref(memory), memory.size):
            raise ctypes.WinError(ctypes.get_last_error())
        cpu_100ns = sum((value.dwHighDateTime << 32) | value.dwLowDateTime for value in times[2:])
        return dict(cpu_ms=cpu_100ns / 10000, working_set_bytes=memory.working_set, private_bytes=memory.private_bytes)


# 採樣依序讀取每條連線，留出排程時間後仍要求所有移動在指定的更新生效。
MOVEMENT_LEAD_TICKS = 8
DIRECTIONS = ((32767, 0), (0, 32767), (-32767, 0), (0, -32767),
              (23169, 23169), (-23169, 23169), (-23169, -23169), (23169, -23169))


def attach(resources, port, epoch, role, ticket, reconnect=False):
    peer = resources.enter_context(closing(connect(port)))
    session, player = hello(peer)
    if session != epoch:
        raise RuntimeError('測試角色連到不同的世界')
    login_request(peer, ticket, 0 if reconnect else role)
    if login_response(peer) != role:
        raise RuntimeError('伺服器拒絕測試角色')
    return peer, player, role


def shared_frame(clients, epoch):
    frames = [battle.read(peer, epoch, player) for peer, player, _ in clients]
    tick = max(frame[0] for frame in frames)
    for index, (peer, player, _role) in enumerate(clients):
        while frames[index][0] < tick:
            frames[index] = battle.read(peer, epoch, player)
    _, positions, first = frames[0]
    roles = {player: role for _peer, player, role in clients}
    if first['roles'] != roles or any(frame[0] != tick or frame[1] != positions or frame[2]['roles'] != roles for frame in frames):
        raise RuntimeError(f'同一世界更新的玩家或座標不一致，tick={tick}')
    return tick, positions


def distribution(values):
    ordered = sorted(values)
    return {'samples': len(values), 'p50': round(median(ordered), 3),
            'p95': round(ordered[ceil(len(ordered) * .95) - 1], 3), 'max': round(ordered[-1], 3)}


def measure(clients, epoch, process, seconds):
    # 先排空依序登入累積的快照，再用讀取公會請求的回覆核對近期世界更新。
    shared_frame(clients, epoch)
    witness, _, _ = clients[0]
    code, state = guild_command(witness, 1, VIEW)
    if code != OK or state[0] != 0:
        raise RuntimeError('無法以公會查詢同步量測起點')
    initial_tick, initial_positions = shared_frame(clients, epoch)
    process_samples = [process.sample()]
    times, rtts = [], []
    last_tick = initial_tick
    first_input_tick = initial_tick + MOVEMENT_LEAD_TICKS
    begin = last_time = time.perf_counter()
    next_sample = begin + 2
    sequence, guild_sequence = 0, 1
    first_moved_tick = None
    last_positions = initial_positions
    observed_moves = {player: 0 for _peer, player, _role in clients}
    agreed = 0
    while time.perf_counter() - begin < seconds:
        sequence += 1
        sign = 1 if sequence // 24 % 2 == 0 else -1
        for index, (peer, player, _role) in enumerate(clients):
            dx, dz = DIRECTIONS[index % len(DIRECTIONS)]
            battle.send_movement(peer, epoch, last_tick, sequence, player, dx * sign, dz * sign,
                                 lead_ticks=MOVEMENT_LEAD_TICKS)
        tick, positions = shared_frame(clients, epoch)
        now = time.perf_counter()
        if tick <= last_tick:
            raise RuntimeError('更新編號未前進')
        agreed += 1
        if tick == last_tick + 1:
            times.append((now - last_time) * 1000)
        if first_moved_tick is None and all(positions[player] != initial_positions[player] for _peer, player, _role in clients):
            first_moved_tick = tick
        for player, position in positions.items():
            observed_moves[player] += position != last_positions[player]
        last_positions = positions
        if now >= next_sample:
            process_samples.append(process.sample())
            next_sample += 2
        last_tick, last_time = tick, now
        if sequence % 31 == 0:
            guild_sequence += 1
            watch_peer, _player, _role = clients[0]
            sent = time.perf_counter()
            code, state = guild_command(watch_peer, guild_sequence, VIEW)
            if code != OK or state[0] != 0:
                raise RuntimeError('測試角色的公會查詢回覆錯誤')
            rtts.append((time.perf_counter() - sent) * 1000)
    duration = time.perf_counter() - begin
    process_samples.append(process.sample())
    if first_moved_tick != first_input_tick:
        raise RuntimeError(f'首次移動未在預定更新套用，expected={first_input_tick} observed={first_moved_tick}')
    if not all(observed_moves.values()):
        raise RuntimeError('有已登入的測試角色從未同步移動')
    advanced = last_tick - initial_tick
    return dict(connected_players=len(clients), agreement_frames=agreed, server_updates=advanced, observed_moves_by_player=observed_moves,
                seconds=round(duration, 3), updates_per_second=round(advanced / duration, 3),
                snapshot_arrival_ms=distribution(times), guild_request_roundtrip_ms=distribution(rtts),
                mean_server_cpu_ms_per_update=round((process_samples[-1]['cpu_ms'] - process_samples[0]['cpu_ms']) / advanced, 3),
                sampled_working_set_bytes=max(sample['working_set_bytes'] for sample in process_samples),
                sampled_private_memory_bytes=max(sample['private_bytes'] for sample in process_samples),
                first_movement_applied_at_scheduled_tick=True)


def verify_full_room(resources, clients, port, epoch, ticket):
    # 停止送入新移動，等先前已排定的指令套用，再記錄斷線前的位置。
    tick, _ = shared_frame(clients, epoch)
    drained_tick = tick + MOVEMENT_LEAD_TICKS
    while tick < drained_tick:
        tick, positions = shared_frame(clients, epoch)
    with closing(connect(port)) as excess:
        if excess.recv(1) != b'':
            raise RuntimeError('滿員後仍傳送新玩家握手')

    peer, departed, role = clients[-1]
    peer.close()
    witness, witness_id, _ = clients[0]
    wait(witness, epoch, witness_id, lambda _, states, __: departed not in states)
    replacement = attach(resources, port, epoch, role, ticket, reconnect=True)
    if replacement[1] == departed:
        raise RuntimeError('重連重用了舊的連線識別')
    _, restored = shared_frame([*clients[:-1], replacement], epoch)
    if restored[replacement[1]] != positions[departed]:
        raise RuntimeError('重連後角色位置未恢復')
    return dict(refused_connection_when_full=True, resumed_after_full_room_disconnect=True,
                restored_position_m=restored[replacement[1]])


def run(server, output, seconds, count, verify_full, capacity):
    digest = sha256(server.read_bytes()).hexdigest()
    with ExitStack() as resources:
        errors = resources.enter_context((output / 'Server.stderr.log').open('xb'))
        env = {key: value for key, value in os.environ.items() if not key.upper().startswith('AEVRYND_')}
        env['AEVRYND_MISTWATCH_PROFILE_DB'] = str(output / 'CapacityProfiles.sqlite3')
        command = [str(server), '0']
        if capacity is not None:
            command.extend(('--capacity', str(capacity)))
        child = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE,
                                 stderr=errors, text=True, env=env, creationflags=subprocess.CREATE_NO_WINDOW)
        resources.callback(child.stdout.close)
        resources.callback(child.wait, 5)
        resources.callback(child.terminate)
        port, epoch = ready_line(child)
        counters = resources.enter_context(closing(ProcessCounters(child.pid)))
        tickets = [token_bytes(32) for _ in range(count)]
        clients = [attach(resources, port, epoch, index % 3 + 1, ticket)
                   for index, ticket in enumerate(tickets)]
        result = measure(clients, epoch, counters, seconds)
        if verify_full:
            result.update(verify_full_room(resources, clients, port, epoch, tickets[-1]))
        if child.poll() is not None:
            raise RuntimeError('專用伺服器提前結束')
    if digest != sha256(server.read_bytes()).hexdigest():
        raise RuntimeError('量測期間伺服器執行檔已變更')
    result.update(room_capacity=capacity if capacity is not None else 16,
                  server_path=str(server), server_sha256=digest, recorded_utc=datetime.now(timezone.utc).isoformat(),
                  scope='localhost, independent protocol clients, authoritative movement and guild view; no renderer')
    receipt = output / 'RiverbankCapacity.json'
    receipt.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(receipt)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--clients', type=int, default=8, choices=range(2, 33), metavar='2..32')
    parser.add_argument('--capacity', type=int, choices=range(1, 33), metavar='1..32',
                        help='明確設定伺服器房間人數上限')
    parser.add_argument('--verify-full', action='store_true', help='檢查滿員拒絕及離開後以同一角色重連')
    parser.add_argument('--seconds', type=int, default=30)
    args = parser.parse_args()
    if not 10 <= args.seconds <= 300:
        parser.error('量測時間請設定為 10 至 300 秒')
    limit = args.capacity if args.capacity is not None else 16
    if args.clients > limit or (args.verify_full and args.clients != limit):
        parser.error('測試人數必須符合房間容量。滿員測試的人數須等於上限')
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    run(args.server.resolve(strict=True), Path(mkdtemp(prefix='capacity-', dir=output)),
        args.seconds, args.clients, args.verify_full, args.capacity)
