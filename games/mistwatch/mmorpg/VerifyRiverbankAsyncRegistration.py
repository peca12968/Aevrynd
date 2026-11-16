"""驗證角色註冊等待磁碟寫入時，在線玩家仍收到快照且能移動。"""
from contextlib import closing, ExitStack
from hashlib import sha256
import json
import math
from pathlib import Path
from secrets import token_bytes
import select
import sqlite3
import sys
from tempfile import mkdtemp
from time import monotonic, perf_counter

from DriveRiverbankParty import Player
from VerifyRiverbankProfiles import login_request, login_response, start
import VerifyWorldServer as world


def connect(port, epoch, stack):
    peer=stack.enter_context(closing(world.connect(port)))
    found,id=world.hello(peer)
    assert found==epoch
    return peer,id


def observe_active_player(player, epoch, duration):
    deadline=monotonic()+duration
    start_tick=player.tick
    initial=player.position
    samples=[]
    current=perf_counter()
    while monotonic()<deadline:
        player.move_towards(epoch,(initial[0]+4,initial[1]))
        player.poll(epoch)
        fresh=perf_counter()
        samples.append((fresh-current)*1000)
        current=fresh
    assert player.tick-start_tick>=12,(start_tick,player.tick,samples)
    assert max(samples)<250,samples
    assert math.dist(player.position,initial)>0.5,(player.position,initial)
    return {'start_tick':start_tick,'end_tick':player.tick,'snapshots':len(samples),
            'max_interval_ms':round(max(samples),3),'travel_m':round(math.dist(player.position,initial),3)}


def verify(server, output):
    folder=Path(mkdtemp(prefix='registration-while-playing-',dir=output)).resolve()
    database=folder/'RiverbankProfiles.sqlite3'
    active, repeated=token_bytes(32),token_bytes(32)
    others=[token_bytes(32) for _ in range(5)]
    with start(server,database) as (port,epoch), ExitStack() as stack:
        witness,witness_id=connect(port,epoch,stack)
        login_request(witness,active,1)
        assert login_response(witness)==1
        guard=Player(witness,witness_id,1)
        guard.poll(epoch)
        participants=[connect(port,epoch,stack) for _ in range(7)]
        # SQLite 仍採 FULL 持久化。用另一個本機連線暫時持有合法交易以重現等待。
        with closing(sqlite3.connect(database)) as held:
            held.execute('BEGIN IMMEDIATE')
            try:
                for (peer,_),ticket in zip(participants,[repeated,repeated,*others],strict=True):
                    login_request(peer,ticket,2)
                movement=observe_active_player(guard,epoch,.78)
                assert not select.select([peer for peer,_ in participants],[],[],0)[0],(
                    'One or more new players were admitted before their write could commit')
            finally:
                held.rollback()

        responses=[]
        for peer,_ in participants:
            readable,_,_=select.select([peer],[],[],5)
            assert readable,'Registration reply timed out'
            try:
                responses.append(login_response(peer))
            except (ConnectionResetError,ConnectionAbortedError,EOFError):
                responses.append(None)
        assert sorted(str(role) for role in responses)==['2']*6+['None'],responses
        for (peer,id),role in zip(participants,responses,strict=True):
            if role:
                joined=Player(peer,id,role)
                joined.poll(epoch)
                assert joined.state['role']==role
        with closing(sqlite3.connect(database)) as saved:
            roles=saved.execute('SELECT profession, ticket_sha256 FROM riverbank_character ORDER BY id').fetchall()
            assert len(roles)==7
            assert {digest:role for role,digest in roles}=={
                sha256(t).digest():role for t,role in [(active,1),(repeated,2),*((t,2) for t in others)]}
    with start(server,database) as (port,epoch), ExitStack() as stack:
        for ticket,role in [(active,1),(repeated,2)]:
            peer,_=connect(port,epoch,stack)
            login_request(peer,ticket)
            assert login_response(peer)==role
    receipt={'status':'RIVERBANK_ASYNC_REGISTRATION_VERIFIED','server_sha256':sha256(server.read_bytes()).hexdigest(),
             'database_lock_seconds':.78,'active_player':movement,'pending_creation_requests':7,
             'unique_new_profiles':6,'duplicate_session_refused':True,'durable_after_restart':True}
    out=folder/'RegistrationVerification.json'
    out.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    print(receipt['status'],out)


if __name__=='__main__':
    verify(Path(sys.argv[1]).resolve(),Path(sys.argv[2]).resolve())
