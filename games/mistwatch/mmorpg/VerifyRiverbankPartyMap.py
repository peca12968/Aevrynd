"""以真實客戶端驗證跨區隊友地圖、舊名單與離隊後的資料範圍。"""
from contextlib import ExitStack
from math import dist
from pathlib import Path
import json
import struct
import sys
from tempfile import mkdtemp
import time

from DriveRiverbankParty import Player as Actor
from VerifyRiverbankCoopJourney import actors_read, walk
from VerifyRiverbankParty import Player, ACCEPT, INVITE, LEAVE, OK, VIEW
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world


def map_roster(client, action=VIEW, target=0):
    client.sequence += 1
    request = b'AEPT\x01\x03' + struct.pack('<QBI', client.sequence, action, target)
    client.peer.sendall(struct.pack('!I', len(request)) + request)
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        size, = struct.unpack('!I', world.exact(client.peer, 4))
        assert 0 < size <= 65536
        response = world.exact(client.peer, size)
        if response[:4] in (b'AENP', b'AECB'):
            continue
        assert response.startswith(b'AEPT\x01\x04')
        sequence, result, _, _, count, online = struct.unpack_from('<QBIIBB', response, 6)
        assert sequence == client.sequence and size == 25 + 15 * online
        return result, count, {actor: (x, z) for actor, _, _, x, z in struct.iter_unpack('<IBHff', response[25:])}
    raise RuntimeError('No party map reply')


def verify(server, output):
    out = Path(mkdtemp(dir=output, prefix='party-map-'))
    started = time.monotonic()
    with start(server, out/'Profiles.sqlite3') as (port, epoch), ExitStack() as sockets:
        guard, ranger, outsider = [Player(port, epoch, sockets, role) for role in (1,2,3)]
        assert guard.party(INVITE, ranger.id)[0] == OK
        joined = map_roster(ranger, ACCEPT, guard.id)
        assert joined[:2] == (OK, 2) and set(joined[2]) == {guard.id,ranger.id}
        assert map_roster(outsider) == (OK, 0, {})
        actors = [Actor(player.peer, player.id, role) for player, role in zip((guard,ranger,outsider),(1,2,3),strict=True)]
        for point in ((0.,42.), (11.,49.), (22.,58.), (22.,98.)):
            walk(actors, actors[1:2], epoch, point)
        # 等本輪移動指令消化後，以伺服器當下的位置作為重連基準。
        for _ in range(10):
            actors_read(actors, epoch)
        here, there = actors[0].position, actors[1].position
        assert dist(here, there) > 96 and there[1] < 100
        _, world_positions, _, _ = world.snapshot_with_classes(guard.peer, epoch, guard.id)
        assert guard.id in world_positions and ranger.id not in world_positions
        roster = map_roster(guard)
        assert roster[:2] == (OK,2) and set(roster[2]) == {guard.id,ranger.id}
        assert dist(roster[2][guard.id], here) < .15 and dist(roster[2][ranger.id], there) < .15
        assert guard.party(VIEW)[3] == 2 and map_roster(outsider) == (OK, 0, {})
        disconnected_id = ranger.id
        ranger.peer.close()
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            state = map_roster(guard)
            assert state[:2] == (OK,2)
            if set(state[2]) == {guard.id}:
                break
            time.sleep(.075)
        else:
            raise RuntimeError('Disconnected party member kept a visible map position')
        resumed = Player(port, epoch, sockets, 2, ranger.ticket)
        assert resumed.id != disconnected_id
        resumed_actor = Actor(resumed.peer, resumed.id, 2)
        resumed_actor.poll(epoch)
        locations = map_roster(guard)[2]
        assert set(locations) == {guard.id,resumed.id}, locations
        assert dist(locations[resumed.id],resumed_actor.position) < .02, (locations,resumed_actor.position)
        assert dist(locations[resumed.id],roster[2][disconnected_id]) < .02, (locations,roster)
        assert map_roster(resumed, LEAVE) == (OK,0,{})
        assert map_roster(guard) == (OK,0,{})
    proof=dict(status='RIVERBANK_PARTY_MAP_VERIFIED',clients=3,distance_m=round(dist(here,there),2),
        beyond_world_interest=True,legacy_roster_compatible=True,outsider_excluded=True,
        offline_removed=True,resume_position_restored=True,leave_removed=True,
        seconds=round(time.monotonic()-started,2),server_sha256=world.sha256(server.read_bytes()).hexdigest())
    receipt=out/'PartyMapVerification.json'
    receipt.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    print(proof['status'],receipt,flush=True)


if __name__=='__main__':
    verify(*(Path(arg).resolve(strict=True) for arg in sys.argv[1:]))
