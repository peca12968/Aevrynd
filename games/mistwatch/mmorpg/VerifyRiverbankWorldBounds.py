"""Verify the server's rendered-terrain boundary on four live roles and tangent movement."""
from contextlib import ExitStack
from pathlib import Path
from tempfile import mkdtemp
import json
import sys
import time

from DriveRiverbankParty import Player as Actor
from VerifyRiverbankCoopJourney import actors_read
from VerifyRiverbankParty import Player
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world

WEST, EAST, SOUTH, NORTH = -49.55, 49.55, -19.55, 99.55
EPSILON = .004
ROLES = (1, 2, 3, 1)


def verify(server, output):
    out = Path(mkdtemp(prefix='world-bounds-',dir=output))
    began = time.monotonic()
    sampled = 0
    with start(server, out/'Profiles.sqlite3') as (port,epoch), ExitStack() as sockets:
        users = [Player(port,epoch,sockets,role) for role in ROLES]
        actors = [Actor(player.peer,player.id,role) for player,role in zip(users,ROLES,strict=True)]
        east, west, south, north = actors
        fixed = ((east,(53.,-8.)), (west,(-53.,-8.)), (south,(0.,-23.)))

        def drive(targets, arrived, label):
            nonlocal sampled
            deadline = time.monotonic()+38
            while time.monotonic()<deadline:
                actors_read(actors,epoch)
                for actor in actors:
                    x,z = actor.position
                    assert WEST-EPSILON<=x<=EAST+EPSILON and SOUTH-EPSILON<=z<=NORTH+EPSILON, actor.position
                    assert actor.state['hp'], (label,actor.id,actor.position)
                    sampled += 1
                if arrived():
                    return
                for actor,goal in targets:
                    actor.move_towards(epoch,goal)
            raise RuntimeError(f'{label} {[(a.position,a.state["hp"]) for a in actors]}')

        for point in ((0.,42.), (11.,49.), (22.,58.)):
            drive((*fixed,(north,point)),lambda: abs(north.position[0]-point[0])<.4 and
                   abs(north.position[1]-point[1])<.4, f'north approach {point}')
        drive((*fixed,(north,(22.,103.))),lambda:
              abs(east.position[0]-EAST)<.05 and abs(west.position[0]-WEST)<.05 and
              abs(south.position[1]-SOUTH)<.05 and abs(north.position[1]-NORTH)<.05, 'four edges')
        touches = dict(zip(('east','west','south','north'),(a.position for a in actors),strict=True))
        drive(((east,(53.,-13.)),),lambda: east.position[1]<-12.6 and abs(east.position[0]-EAST)<.05,
              'movement along east edge')
        tangent = east.position
        drive(((east,(45.,-13.)),),lambda: east.position[0]<45.6,'return to interior')
        back_inside = east.position
    proof=dict(status='RIVERBANK_WORLD_BOUNDARIES_VERIFIED',clients=4,
        boundaries=touches,edge_slide=tangent,returned_to_world=back_inside,samples=sampled,
        seconds=round(time.monotonic()-began,2),server_sha256=world.sha256(server.read_bytes()).hexdigest())
    path=out/'WorldBoundsVerification.json'
    path.write_text(json.dumps(proof,indent=2)+'\n',encoding='utf-8')
    print(proof['status'],path,flush=True)


if __name__=='__main__':
    verify(*(Path(arg).resolve(strict=True) for arg in sys.argv[1:]))
