"""Verify real TCP players are held outside authored walls and can enter through openings."""
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
import VerifyWorldServer as world


def verify(server, output):
    out=Path(mkdtemp(dir=output,prefix='static-structure-'))
    started=time.monotonic()
    with start(server,out/'Profiles.sqlite3') as (port,epoch),ExitStack() as sockets:
        clients=[Player(port,epoch,sockets,role) for role in (1,2)]
        guard,ranger=[Actor(player.peer,player.id,role) for player,role in zip(clients,(1,2),strict=True)]
        actors=[guard,ranger]
        walk(actors,actors,epoch,(0.,42.))
        for mover,points in ((guard,((-16.,45.5),(-14.,59.))),
                              (ranger,((11.,49.),(15.,63.),(14.,79.)))):
            for point in points:
                walk(actors,(mover,),epoch,point)

        def against_wall(actor, goal, safe, contact, description):
            deadline=time.monotonic()+18
            held=0
            while time.monotonic()<deadline:
                actors_read(actors,epoch)
                assert actor.state['hp'] and safe(actor.position), (description,actor.position)
                held=held+1 if contact(actor.position) else 0
                if held>=12:
                    return actor.position
                actor.move_towards(epoch,goal)
            raise RuntimeError(f'{description} contact not reached at {actor.position}')

        workshop=against_wall(guard,(-7.,59.),lambda p:p[0]<=-9.947,
                              lambda p:abs(p[0]+9.952)<.05,'workshop west wall')
        hut=against_wall(ranger,(8.,79.),lambda p:p[0]>=10.947,
                         lambda p:abs(p[0]-10.952)<.05,'ranger hut east wall')
        tangent=against_wall(guard,(-7.,60.4),lambda p:p[0]<=-9.947,
                             lambda p:abs(p[0]+9.952)<.05 and p[1]>60.25,'workshop wall glide')
        _,members,_,_=world.snapshot_with_classes(guard.peer,epoch,guard.id)
        assert ranger.id in members and dist(members[ranger.id],hut)<.05

        for point in ((-12.,63.),(-3.5,63.),(-3.5,59.),(-7.,59.)):
            walk(actors,(guard,),epoch,point)
        open_workshop=guard.position
        assert open_workshop[0]<-6.75
        for point in ((14.,83.8),(4.,83.8),(4.,79.),(7.5,79.)):
            walk(actors,(ranger,),epoch,point)
        open_hut=ranger.position
        assert open_hut[0]>7.24
    result=dict(status='RIVERBANK_STATIC_STRUCTURE_MOVEMENT_VERIFIED',clients=2,
        workshop_wall=workshop,hut_wall=hut,wall_glide=tangent,
        workshop_open_entrance=open_workshop,hut_open_entrance=open_hut,
        observer_matches_contact=True,seconds=round(time.monotonic()-started,2),
        server_sha256=world.sha256(server.read_bytes()).hexdigest())
    receipt=out/'StructureMovementVerification.json'
    receipt.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    print(result['status'],receipt,flush=True)


if __name__=='__main__':
    verify(*(Path(arg).resolve(strict=True) for arg in sys.argv[1:]))
