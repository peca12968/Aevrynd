"""Exercise authoritative displacement and wall clipping with two TCP clients."""
from contextlib import ExitStack
from hashlib import file_digest
from math import dist
from pathlib import Path
from tempfile import mkdtemp
import json
import struct
import sys

from VerifyRiverbankProfiles import start
from VerifyRiverbankParty import Player as Profile
from DriveRiverbankParty import Player
from VerifyRiverbankBattle import frame, STATE
from VerifyRiverbankCoopJourney import walk
from VerifyWorldServer import packet, snapshot_with_flags, ENCOUNTER_FRAME

CAPABLE=1<<25
DODGING=1<<26

def snap(player, epoch):
    tick, positions, _, flags=snapshot_with_flags(player.peer,epoch,player.id)
    kind,payload=packet(player.peer)
    assert kind==ENCOUNTER_FRAME
    session, combat_tick, owner, role, _, hp, resource, *_=STATE.unpack(payload)
    assert (session,combat_tick,owner,role)==(epoch,tick,player.id,player.role)
    return tick,positions,flags,hp,resource

def synced(actors, epoch):
    samples=[snap(actor,epoch) for actor in actors]
    while samples[0][0]!=samples[1][0]:
        oldest=0 if samples[0][0]<samples[1][0] else 1
        samples[oldest]=snap(actors[oldest],epoch)
    assert all(samples[0][1][actor.id]==samples[1][1][actor.id] for actor in actors)
    return samples

def send(actor, seq, x, z):
    packed=(x & 65535)|((z & 65535)<<16)
    frame(actor.peer,b'AECB\x01\x01'+struct.pack('<QQB',seq,packed,4))

def dodge(actors, epoch, sequence, direction, replay):
    actor,witness=actors
    before,_=synced(actors,epoch)
    origin=before[1][actor.id]
    assert before[2][actor.id]&CAPABLE and not before[2][actor.id]&DODGING
    send(actor,sequence,*direction)
    active=[]
    for _ in range(18):
        player,other=synced(actors,epoch)
        moving=bool(player[2][actor.id]&DODGING)
        assert moving==bool(other[2][actor.id]&DODGING)
        if moving:
            active.append((player[0], player[1][actor.id], player[4]))
            if replay and len(active)==1:
                send(actor,sequence,*direction)
                send(actor,sequence+1,-direction[0],-direction[1])
        elif active:
            end=player[1][actor.id]
            break
    else:
        raise AssertionError('The burst did not start and settle')
    assert len(active)==6,active
    assert before[4]-31 <= min(entry[2] for entry in active) <= before[4]-29
    return dict(start=origin,end=end,distance_m=dist(origin,end),active_ticks=[row[0] for row in active],
                minimum_resource=min(row[2] for row in active))

def verify(exe, receipt_dir):
    output=Path(mkdtemp(dir=receipt_dir,prefix='dodge-network-'))
    with start(exe,output/'Profiles.sqlite3') as (port,epoch), ExitStack() as stack:
        connections=[Profile(port,epoch,stack,role) for role in (2,1)]
        actors=[Player(connection.peer,connection.id,role) for connection,role in zip(connections,(2,1),strict=True)]
        runner,observer=actors
        open_air=dodge(actors,epoch,1,(32767,32767),True)
        assert 1.48 < open_air['distance_m'] < 1.52,open_air
        for _ in range(24):
            frame_after,_=synced(actors,epoch)
            assert frame_after[1][runner.id]==tuple(open_air['end']) and not frame_after[2][runner.id]&DODGING
        for point in ((0.,42.),(-16.,45.5),(-14.,59.),(-10.46,59.)):
            walk(actors,(runner,),epoch,point)
        for _ in range(8):
            synced(actors,epoch)
        wall=dodge(actors,epoch,3,(32767,0),False)
        assert wall['start'][0] < -10.14 and -10.01 < wall['end'][0] < -9.945,wall
        assert 0.15<wall['distance_m']<.8,wall

    with exe.open('rb') as binary:
        digest=file_digest(binary,'sha256').hexdigest()
    report=dict(stage='RIVERBANK_DODGE_TWO_CLIENTS_VERIFIED', server_sha256=digest,
                clients=2,physical_hosts=1,open_burst=open_air,workshop_wall=wall)
    receipt=output/'DodgeNetworkProof.json'
    receipt.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(report['stage'],receipt,flush=True)


if __name__=='__main__':
    verify(*(Path(arg).resolve(strict=True) for arg in sys.argv[1:]))
