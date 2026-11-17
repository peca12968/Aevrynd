"""以實際首領戰驗證職業裝備、持久化與傷害。"""
from contextlib import ExitStack, closing
import json
from pathlib import Path
import sqlite3
import struct
import sys
from tempfile import mkdtemp
import time

from DriveRiverbankParty import Player as Actor, play_encounter
from VerifyRiverbankParty import Player
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world

VIEW, EQUIP, UNEQUIP = range(3)
OK, NOT_EARNED, REJECTED = range(3)


class GearPlayer(Player):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.gear_sequence = 0

    def gear(self, action, replay=False):
        self.gear_sequence += int(not replay)
        packet = b'AEGR\x01\x01' + struct.pack('<QB', self.gear_sequence, action)
        self.peer.sendall(struct.pack('!I', len(packet)) + packet)
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            size, = struct.unpack('!I', world.exact(self.peer, 4))
            assert 0 < size <= 65536
            reply = world.exact(self.peer, size)
            if reply[:4] in (b'AENP', b'AECB'):
                continue
            assert size == 16 and reply.startswith(b'AEGR\x01\x02')
            seq, result, bits = struct.unpack_from('<QBB', reply, 6)
            assert seq == self.gear_sequence and bits in (0, 1, 3)
            return result, bool(bits & 1), bool(bits & 2)
        raise AssertionError('No response to gear request')


def verify_gear_damage(ranger, epoch):
    actor = Actor(ranger.peer, ranger.id, 2)
    deadline = time.monotonic() + 32
    while time.monotonic() < deadline:
        actor.poll(epoch)
        if actor.position[1] >= 43.4:
            break
        actor.move_towards(epoch, (2.5, 44.))
    else:
        raise RuntimeError(f'Ranger did not reach the clearing: {actor.position}')
    assert actor.state['boss_hp'] == 330
    actor.act(2)
    for _ in range(10):
        actor.poll(epoch)
        if actor.state['skill']:
            break
    assert actor.state['skill'] and actor.state['boss_hp'] == 298, actor.state
    return 330 - actor.state['boss_hp']


def verify(server, evidence):
    root = Path(mkdtemp(prefix='boss-gear-', dir=evidence)).resolve()
    database = root / 'Profiles.sqlite3'
    roles = (1, 2, 3, 2)
    with start(server, database) as (port, epoch), ExitStack() as sockets:
        profiles = [GearPlayer(port, epoch, sockets, role) for role in roles]
        guard, ranger, healer, outsider = profiles
        assert ranger.gear(EQUIP) == (NOT_EARNED, False, False), 'Unearned item cannot be equipped'
        actors = [Actor(player.peer, player.id, role) for player, role in zip(profiles, roles, strict=True)]
        victory = play_encounter(epoch, *actors)
        assert all(player.gear(VIEW) == (OK, True, False) for player in (guard, ranger, healer))
        assert outsider.gear(EQUIP) == (NOT_EARNED, False, False)
        assert ranger.gear(EQUIP) == (OK, True, True)
        assert ranger.gear(UNEQUIP, replay=True) == (REJECTED, True, True)
        assert ranger.gear(EQUIP) == (OK, True, True)
        ticket = ranger.ticket

    with start(server, database) as (port, restarted), ExitStack() as sockets:
        assert restarted != epoch
        ranger = GearPlayer(port, restarted, sockets, 2, ticket)
        assert ranger.gear(VIEW) == (OK, True, True)
        projectile_damage = verify_gear_damage(ranger, restarted)
        assert ranger.gear(UNEQUIP) == (OK, True, False)
        assert ranger.gear(EQUIP) == (OK, True, True)

    with closing(sqlite3.connect(database)) as db:
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
        assert db.execute('PRAGMA user_version').fetchone() == (9,)
        assert db.execute('SELECT COUNT(*) FROM riverbank_reward').fetchone() == (3,)
        kit = db.execute('SELECT character_id,reward_world,reward_encounter FROM riverbank_boss_kit').fetchall()
        assert kit == [(2, epoch, victory['encounter'])]
    receipt = dict(status='RIVERBANK_BOSS_GEAR_VERIFIED', simultaneous_clients=4,
                   eligible_profiles=3, observer_denied=True, equipped_accessory_rows=len(kit),
                   replay_did_not_change_equipment=True, gear_restored_after_restart=True,
                   ranger_base_volley_damage=28, ranger_equipped_volley_damage=projectile_damage,
                   server_sha256=world.sha256(server.read_bytes()).hexdigest())
    target = root / 'BossGearVerification.json'
    target.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], target)


if __name__ == '__main__':
    verify(*(Path(value).resolve(strict=True) for value in sys.argv[1:]))
