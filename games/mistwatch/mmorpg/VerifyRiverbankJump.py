"""Check two clients agree on a timed jump, a held key, and a resume on the ground."""
from contextlib import ExitStack, closing
from hashlib import file_digest
import json
from pathlib import Path
from secrets import token_bytes
import struct
import sys
from tempfile import mkdtemp
import time

from VerifyRiverbankProfiles import login_request,login_response,start
import VerifyWorldServer as wire

def input_packet(peer,epoch,target,sequence,actor,jump):
    body=struct.pack("<QQQIhhhB",epoch,target,sequence,actor,0,0,0,int(jump))
    data=wire.HEADER.pack(b"AENP",1,2,0,len(body),wire.crc32c(body),wire.sha256(body).digest())+body
    peer.sendall(struct.pack("!I",len(data))+data)

def view(peer,epoch,actor):
    kind,raw=wire.packet(peer)
    if kind==wire.ENCOUNTER_FRAME:
        kind,raw=wire.packet(peer)
    assert kind==3
    session,tick,_,_,_,recipient,full,count,gone=wire.SNAPSHOT.unpack_from(raw)
    assert (session,recipient,full)==(epoch,actor,1)
    assert len(raw)==wire.SNAPSHOT.size+count*wire.ENTITY.size+gone*16
    players={}
    for offset in range(wire.SNAPSHOT.size,wire.SNAPSHOT.size+count*wire.ENTITY.size,wire.ENTITY.size):
        hi,identity,cx,cy,cz,x,y,z,*_=wire.ENTITY.unpack_from(raw,offset)
        assert hi==epoch
        players[identity]=(cx*32000+x,cy*32000+y,cz*32000+z)
    return tick, players

def common(a,b,epoch):
    left=view(a[0],epoch,a[1])
    right=view(b[0],epoch,b[1])
    deadline=time.monotonic()+5
    while left[0]!=right[0]:
        assert time.monotonic()<deadline,"World snapshots did not align"
        if left[0]<right[0]:
            left=view(a[0],epoch,a[1])
        else:
            right=view(b[0],epoch,b[1])
    assert left[1]==right[1],(left,right)
    return left

def attach(port,epoch,peers,ticket,role,reconnect=False):
    peer=peers.enter_context(closing(wire.connect(port)))
    hello,actor=wire.hello(peer)
    assert hello==epoch
    login_request(peer,ticket,0 if reconnect else role)
    assert login_response(peer)==role
    return peer,actor

def run(server,results,reference):
    output=Path(mkdtemp(prefix="BaselineJump-" if reference else "AuthoritativeJump-",dir=results))
    jump_ticket,friend_ticket=token_bytes(32),token_bytes(32)
    with start(server,output/"Profiles.sqlite3") as (port,epoch),ExitStack() as peers:
        jumper=attach(port,epoch,peers,jump_ticket,2)
        watcher=attach(port,epoch,peers,friend_ticket,1)
        while True:
            first,ground=common(jumper,watcher,epoch)
            if set(ground)=={jumper[1],watcher[1]}:
                break
        assert all(point[1]==0 for point in ground.values())
        landing_xz=ground[jumper[1]][::2]
        sequence=1
        takeoff=first+3
        input_packet(jumper[0],epoch,takeoff,sequence,jumper[1],True)
        snapshots={}
        while len(snapshots)<24:
            tick,players=common(jumper,watcher,epoch)
            if tick<=first:
                continue
            assert players[jumper[1]][::2]==landing_xz
            assert players[watcher[1]]==ground[watcher[1]]
            snapshots[tick]=players[jumper[1]][1]
            sequence+=1
            input_packet(jumper[0],epoch,tick+3,sequence,jumper[1],True)
        if reference:
            assert set(snapshots.values())=={0},snapshots
        else:
            raised={tick:height for tick,height in snapshots.items() if height}
            expected={takeoff+age-1:round(4*720*(age/16)*(1-age/16)) for age in range(1,16)}
            assert set(raised)==set(expected),(raised,expected)
            assert all(abs(raised[tick]-height)<=1 for tick,height in expected.items()),(raised,expected)
            assert snapshots[takeoff+15]==0 and max(raised.values())==720
            release_tick=tick+4
            sequence+=1
            input_packet(jumper[0],epoch,release_tick,sequence,jumper[1],False)
            second=release_tick+2
            sequence+=1
            input_packet(jumper[0],epoch,second,sequence,jumper[1],True)
            while tick<second:
                tick,players=common(jumper,watcher,epoch)
            assert tick==second and players[jumper[1]][1]>0
            jumper[0].close()
            deadline=time.monotonic()+4
            while time.monotonic()<deadline:
                _,players=view(watcher[0],epoch,watcher[1])
                if jumper[1] not in players:
                    break
            else:
                raise RuntimeError("Disconnected player did not leave the world")
            resumed=attach(port,epoch,peers,jump_ticket,2,reconnect=True)
            assert resumed[1]!=jumper[1]
            while True:
                restored_tick,players=common(resumed,watcher,epoch)
                if resumed[1] in players:
                    break
            assert players[resumed[1]][1]==0 and players[resumed[1]][::2]==landing_xz
    with server.open("rb") as stream:
        server_sha=file_digest(stream,"sha256").hexdigest()
    report=dict(stage="LEGACY_JUMP_INPUT_REMAINS_GROUNDED" if reference else "AUTHORITATIVE_JUMP_LOOPBACK_VERIFIED",
                physical_hosts=1,clients=2,sample_unit="millimetres",jump_held_samples=snapshots,
                input_tick=takeoff,server_sha256=server_sha)
    if not reference:
        report.update(first_airborne_tick=min(raised),apex_mm=max(raised.values()),touchdown_tick=takeoff+15,
                      reconnect_from_airborne_at_same_xz=True,reconnect_tick=restored_tick)
    receipt=output/"JumpProof.json"
    receipt.write_text(json.dumps(report,indent=2)+"\n",encoding="utf-8")
    print(report["stage"],receipt,flush=True)

if __name__=="__main__":
    run(Path(sys.argv[1]).resolve(strict=True),Path(sys.argv[2]).resolve(strict=True),"--reference" in sys.argv[3:])
