"""Verify shore, bridge, and rail contact against two independent game sockets."""
from contextlib import ExitStack, closing
from hashlib import file_digest
from math import dist, hypot
from pathlib import Path
from tempfile import mkdtemp
import json
import os
import subprocess
import sys
import time

import VerifyRiverbankBattle as battle
import VerifyWorldServer as wire


def verify(server, evidence):
    env = {key: value for key, value in os.environ.items() if key.upper() != 'AEVRYND_MISTWATCH_PROFILE_DB'}
    stage = Path(mkdtemp(prefix='server-crossing-', dir=evidence))
    proof = {}
    deadline = time.monotonic() + 75
    with subprocess.Popen([str(server), '0'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                          creationflags=subprocess.CREATE_NO_WINDOW, env=env) as service:
        try:
            port, epoch = wire.ready_line(service)
            with ExitStack() as stack:
                clients = [stack.enter_context(closing(wire.connect(port))) for _ in range(2)]
                identities = [wire.hello(peer) for peer in clients]
                assert all(session == epoch for session, _ in identities)
                actor, observer = [client_id for _, client_id in identities]
                wire.choose(clients[0], 1)
                wire.choose(clients[1], 2)
                sequence = 0

                def step(vector=(0, 0)):
                    nonlocal sequence
                    assert time.monotonic() < deadline, 'Crossing timed out'
                    first = battle.read(clients[0], epoch, actor)
                    second = battle.read(clients[1], epoch, observer)
                    while first[0] != second[0]:
                        if first[0] < second[0]:
                            first = battle.read(clients[0], epoch, actor)
                        else:
                            second = battle.read(clients[1], epoch, observer)
                    assert first[1] == second[1], (first[0], first[1], second[1])
                    if vector != (0, 0):
                        sequence += 1
                        battle.send_movement(clients[0], epoch, first[0], sequence, actor, *vector)
                    return first[0], first[1][actor]

                def go(target, point=None):
                    tick, current = point or step()
                    while dist(target, current) >= .21:
                        x, z = target[0]-current[0], target[1]-current[1]
                        length = hypot(x,z)
                        scale = min(1.0, 4.0 * length) * 32767 / length
                        tick, current = step((round(x*scale),round(z*scale)))
                    for _ in range(5):
                        tick, current = step()
                    assert dist(target,current) < .35, (target,current)
                    return tick,current

                def block(label, vector, legal):
                    for _ in range(30):
                        tick, point = step(vector)
                        assert legal(point), f'{label}: tick={tick}, position={point}'
                    proof[label] = {'tick':tick,'position':point}
                    return tick,point

                start = go((3.0,31.2))
                south = block('south_bank', (0,32767), lambda p: p[1] <= 32.001)
                assert south[1][1] > start[1][1] + .4
                slide = go((0.0,31.6), south)
                for _ in range(8):
                    slide = step()
                deck = go((0.0,36.0), slide)
                rail = block('bridge_rail', (32767,0), lambda p: p[0] <= 1.551 and abs(p[1]-36.) <= .35)
                assert 1.48 < rail[1][0] <= 1.551
                north = go((0.0,41.5), go((0.0,36.0),rail))
                proof['crossed_to_north'] = {'tick':north[0], 'position':north[1]}
                go((3.0,41.5), north)
                north = block('north_bank', (0,-32767), lambda p: p[1] >= 39.999)
                assert north[1][1] < 40.03
                returned = go((0.0,29.), go((0.0,36.), go((0.0,40.4),north)))
                proof['crossed_to_south'] = {'tick':returned[0], 'position':returned[1]}
        finally:
            service.terminate()
            service.wait(timeout=5)
    with server.open('rb') as stream:
        server_sha = file_digest(stream, 'sha256').hexdigest()
    receipt = stage/'RiverbankCrossing.json'
    receipt.write_text(json.dumps({'stage':'RIVERBANK_SERVER_CROSSING_VERIFIED','physical_hosts':1,
        'clients':2,'server_sha256':server_sha,'shared_observations':proof},
        indent=2)+'\n', encoding='utf-8')
    print(receipt,flush=True)


if __name__ == '__main__':
    verify(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
