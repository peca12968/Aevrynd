"""實際連線驗證祭司支援依伺服器隊伍決定目標。"""
from contextlib import ExitStack
from math import dist
import json
from pathlib import Path
import struct
import sys
from tempfile import mkdtemp
import time

from DriveRiverbankParty import Player as Actor
from VerifyRiverbankBattle import frame
from VerifyRiverbankParty import Player, INVITE, ACCEPT, OK
from VerifyRiverbankProfiles import start
from VerifyWorldServer import sha256


def verify(server, output):
    root = Path(mkdtemp(prefix='party-combat-', dir=output)).resolve()
    with start(server, root / 'Profiles.sqlite3') as (port, epoch), ExitStack() as stack:
        guard, priest, outsider = (Player(port, epoch, stack, role) for role in (1, 3, 2))
        assert guard.party(INVITE, priest.id)[0] == OK
        assert priest.party(ACCEPT, guard.id)[3] == 2
        actors = [Actor(profile.peer, profile.id, role) for profile, role in ((guard, 1), (priest, 3), (outsider, 2))]
        tank, healer, guest = actors

        def read_tick():
            for actor in actors:
                actor.poll(epoch)
            tick = max(actor.tick for actor in actors)
            for actor in actors:
                while actor.tick < tick:
                    actor.poll(epoch)
            assert len({actor.tick for actor in actors}) == 1, [actor.tick for actor in actors]

        deadline = time.monotonic() + 38
        while time.monotonic() < deadline:
            read_tick()
            if (0 < tank.state['hp'] < 150 and 0 < guest.state['hp'] < 105 and
                    guest.state['hp'] / 105 < tank.state['hp'] / 150 and
                    healer.position[1] >= 43.1 and
                    max(dist(healer.position, actor.position) for actor in (tank, guest)) < 8.8):
                break
            for actor, target in ((tank, (0., 46.)), (guest, (0., 46.)), (healer, (4., 44.))):
                actor.move_towards(epoch, target)
        else:
            raise AssertionError(f'Could not reproduce damaged party and public players: {[(p.position, p.state) for p in actors]}')

        injured = {actor.id: actor.state['hp'] for actor in actors}
        frame(healer.peer, b'AECB\x01\x01' + struct.pack('<QQB', 1, guest.id, 2))
        for _ in range(6):
            read_tick()
        assert guest.state['hp'] == injured[guest.id] and healer.state['skill'] == 0 and healer.state['resource'] == 100, (
            f'Non-member was healed or consumed resources: outsider {injured[guest.id]}->{guest.state["hp"]}, '
            f'cooldown={healer.state["skill"]}, resource={healer.state["resource"]}')

        frame(healer.peer, b'AECB\x01\x01' + struct.pack('<QQB', 2, 0, 2))
        for _ in range(6):
            read_tick()
            if healer.state['skill']:
                break
        assert healer.state['resource'] < 100 and healer.state['skill'] > 0
        assert tank.state['hp'] == min(150, injured[tank.id] + 40) and guest.state['hp'] == injured[guest.id]
        receipt = {'status': 'RIVERBANK_PARTY_COMBAT_VERIFIED', 'simultaneous_clients': len(actors),
                   'team_expected': [tank.id, healer.id], 'outside_player': guest.id,
                   'hp_before': injured, 'hp_after': {actor.id: actor.state['hp'] for actor in actors},
                   'healer_resource': healer.state['resource'], 'healer_cooldown': healer.state['skill'],
                   'explicit_outside_target_rejected_without_charge': True,
                   'automatic_support_picked_injured_teammate': True,
                   'server_sha256': sha256(server.read_bytes()).hexdigest()}
    target = root / 'PartyCombatVerification.json'
    target.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], target)


if __name__ == '__main__':
    verify(*(Path(name).resolve(strict=True) for name in sys.argv[1:]))
