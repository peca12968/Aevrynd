"""Drive two clients through the real battle server and check authority on both connections."""
from contextlib import closing
from pathlib import Path
import json
import struct
import subprocess
import sys
import time

import VerifyWorldServer as wire

STATE = struct.Struct('<QQIBBIIIIIBffffIIQQ')


def frame(peer, data):
    peer.sendall(struct.pack('!I', len(data)) + data)


def send_movement(peer, epoch, tick, sequence, player, move_x=0, move_z=32767, *, lead_ticks=3):
    payload = struct.pack('<QQQIhhhB', epoch, tick + lead_ticks, sequence, player, move_x, move_z, 0, 0)
    frame(peer, wire.HEADER.pack(b'AENP', 1, 2, 0, len(payload), wire.crc32c(payload), wire.sha256(payload).digest()) + payload)


def action(peer, sequence, kind):
    frame(peer, b'AECB\x01\x01' + struct.pack('<QQB', sequence, 0, kind))


def read(peer, epoch, player):
    tick, positions, _, roles = wire.snapshot_with_classes(peer, epoch, player)
    kind, payload = wire.packet(peer)
    assert kind == wire.ENCOUNTER_FRAME
    session, combat_tick, owner, role, guarding, hp, resource, attack_cd, skill_cd, loot, phase, *boss = STATE.unpack(payload)
    assert (session, combat_tick, owner, role) == (epoch, tick, player, roles[player])
    return tick, positions, dict(role=role, guarding=guarding, hp=hp, resource=resource, attack=attack_cd, skill=skill_cd,
                                 loot=loot, roles=roles, boss_phase=phase, boss_position=boss[:2], strike=boss[2:4],
                                 boss_hp=boss[4], phase_ticks=boss[5], boss_target=boss[6], boss_run=boss[7])


def verify(server, receipt):
    with subprocess.Popen([str(server), '0'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                          creationflags=subprocess.CREATE_NO_WINDOW) as process:
        try:
            port, epoch = wire.ready_line(process)
            with closing(wire.connect(port)) as guardian, closing(wire.connect(port)) as ranger:
                guard_epoch, guard_id = wire.hello(guardian)
                ranger_epoch, ranger_id = wire.hello(ranger)
                assert guard_epoch == ranger_epoch == epoch
                wire.choose(guardian, 1)
                wire.choose(ranger, 2)
                deadline = time.monotonic() + 27
                sequence = 0

                def advance(target_z):
                    nonlocal sequence
                    while True:
                        read(guardian, epoch, guard_id)
                        tick, positions, state = read(ranger, epoch, ranger_id)
                        assert time.monotonic() < deadline, positions
                        if positions[ranger_id][1] >= target_z:
                            return positions, state
                        sequence += 1
                        send_movement(ranger, epoch, tick, sequence, ranger_id)

                advance(40)
                action(ranger, 1, 2)
                for _ in range(8):
                    read(guardian, epoch, guard_id)
                    _, outside_positions, outside = read(ranger, epoch, ranger_id)
                    assert 40 <= outside_positions[ranger_id][1] < 43, outside_positions
                    assert outside['boss_phase'] == 0 and outside['boss_hp'] == 330 and outside['boss_target'] == 0, outside
                    assert outside['resource'] == 100 and outside['skill'] == 0, outside

                positions, started = advance(43.5)
                assert started['boss_hp'] == 330
                action(guardian, 1, 2)  # Out of range
                action(ranger, 2, 1)    # In range
                deadline = time.monotonic() + 3
                while True:
                    _, _, guard_state = read(guardian, epoch, guard_id)
                    _, _, ranger_state = read(ranger, epoch, ranger_id)
                    assert time.monotonic() < deadline
                    if guard_state['boss_hp'] == ranger_state['boss_hp'] == 317:
                        break
                assert guard_state['resource'] == 100 and guard_state['skill'] == 0 and not guard_state['guarding']
                assert ranger_state['attack'] > 0
                action(ranger, 3, 2)
                action(ranger, 3, 2)    # Repeated sequence cannot cause extra damage
                deadline = time.monotonic() + 3
                while True:
                    _, _, guard_state = read(guardian, epoch, guard_id)
                    _, _, ranger_state = read(ranger, epoch, ranger_id)
                    assert time.monotonic() < deadline
                    if guard_state['boss_hp'] == ranger_state['boss_hp'] == 289:
                        break
                assert ranger_state['skill'] > 0 and ranger_state['resource'] < 100
                for _ in range(5):
                    assert read(guardian, epoch, guard_id)[2]['boss_hp'] == 289
                    assert read(ranger, epoch, ranger_id)[2]['boss_hp'] == 289
        finally:
            process.terminate()
            process.wait(timeout=5)
    evidence = {'status': 'SHARED_RIVERBANK_BATTLE_VERIFIED', 'server_sha256': wire.sha256(server.read_bytes()).hexdigest(),
                'clients': 2, 'guardian': guard_state, 'ranger': ranger_state, 'ranger_authoritative_position': positions[ranger_id],
                'approach': {'ranger_position': outside_positions[ranger_id], 'boss': outside, 'volley_rejected': True}}
    receipt.write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    print(evidence['status'], 'boss_hp=', ranger_state['boss_hp'])


if __name__ == '__main__':
    verify(Path(sys.argv[1]).resolve(strict=True), Path(sys.argv[2]))
