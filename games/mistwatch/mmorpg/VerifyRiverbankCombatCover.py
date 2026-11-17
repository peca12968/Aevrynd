"""Verify that both TCP clients observe the server's workshop cover ruling."""
from contextlib import ExitStack
from math import dist
from pathlib import Path
from tempfile import mkdtemp
import json
import sys
import time

from DriveRiverbankParty import Player as Actor
from VerifyRiverbankCoopJourney import actors_read, walk
from VerifyRiverbankParty import Player
from VerifyRiverbankProfiles import start
from VerifyWorldServer import sha256


def verify(server, output):
    proof_dir = Path(mkdtemp(dir=output, prefix='combat-cover-'))
    with start(server, proof_dir / 'Profiles.sqlite3') as (port, epoch), ExitStack() as sockets:
        ranger_profile, observer_profile = (Player(port, epoch, sockets, role) for role in (2, 1))
        ranger = Actor(ranger_profile.peer, ranger_profile.id, 2)
        observer = Actor(observer_profile.peer, observer_profile.id, 1)
        actors = ranger, observer
        walk(actors, actors, epoch, (0., 42.))
        for point in ((11., 49.), (15., 63.)):
            walk(actors, actors, epoch, point)
        for point in ((-3.5, 63.), (-3.5, 59.25), (-6.6, 59.25), (-6.6, 57.85)):
            walk(actors, (ranger,), epoch, point)

        def sample():
            actors_read(actors, epoch)
            assert ranger.state['hp'], ranger.position
            assert ranger.state['boss_hp'] == observer.state['boss_hp']
            return ranger.state

        def await_attack_range():
            deadline = time.monotonic() + 12
            while time.monotonic() < deadline:
                state = sample()
                distance = dist(ranger.position, state['boss_position'])
                if distance < 12.85 and state['boss_phase'] == 0:
                    return state, distance
            raise AssertionError(f'No in-range patrol outside the workshop: {ranger.position}, {ranger.state}')

        covered_position = ranger.position
        _, covered_distance = await_attack_range()
        for kind in (1, 2):
            ranger.act(kind)
            for _ in range(8):
                state = sample()
                assert state['boss_hp'] == 330 and state['resource'] == 100
                assert state['attack'] == state['skill'] == 0

        for point in ((-6.6, 59.25), (-3.5, 59.25), (-3.5, 56.4)):
            walk(actors, (ranger,), epoch, point)
        assert dist(ranger.position, sample()['boss_position']) < 12.85
        ranger.act(1)
        for _ in range(12):
            state = sample()
            if state['attack']:
                break
        assert state['boss_hp'] == 317 and state['attack'] > 0
        ranger.act(2)
        for _ in range(12):
            state = sample()
            if state['skill']:
                break
        assert state['boss_hp'] == 289 and state['skill'] > 0 and state['resource'] <= 69
        report = dict(status='RIVERBANK_STRUCTURE_COMBAT_LOOPBACK_VERIFIED',
                      clients=len(actors), covered_position=covered_position,
                      covered_distance_m=covered_distance, covered_health=330,
                      open_position=ranger.position, open_health=state['boss_hp'],
                      observer_health=observer.state['boss_hp'],
                      server_sha256=sha256(server.read_bytes()).hexdigest())
    receipt = proof_dir / 'StructureCombatVerification.json'
    receipt.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(report['status'], receipt, flush=True)


if __name__ == '__main__':
    verify(*(Path(arg).resolve(strict=True) for arg in sys.argv[1:]))
