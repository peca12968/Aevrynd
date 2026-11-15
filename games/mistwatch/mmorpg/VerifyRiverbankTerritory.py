"""Resolve a contested takeover through real movement, then restart the dedicated server."""
from contextlib import ExitStack, closing
from math import dist
from pathlib import Path
import json
import secrets
import sqlite3
import struct
import sys
import time
from tempfile import mkdtemp

from DriveRiverbankParty import Player as Actor
from VerifyRiverbankGuild import CREATE, INVITE, JOIN, LEAVE, OK, Player, VIEW
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world

LOOKOUT = (14., 54.)
WEST = (10.5, 54.)
EAST = (17.5, 54.)
INSIDE_WEST = (12.1, 54.)
INSIDE_EAST = (15.9, 54.)
STATE = struct.Struct('<QQQQIIB')


def territory(peer, sequence):
    request = b'AETR\x01\x01' + struct.pack('<Q', sequence)
    peer.sendall(struct.pack('!I', len(request)) + request)
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        size, = struct.unpack('!I', world.exact(peer, 4))
        assert 1 <= size <= 65536
        data = world.exact(peer, size)
        if data[:4] in (b'AENP', b'AECB'):
            continue
        assert data.startswith(b'AETR\x01\x02') and len(data) >= 49
        received, revision, owner, challenger, progress, rewards, contested = STATE.unpack_from(data, 6)
        assert received == sequence and contested in (0, 1) and progress < 240
        offset = 6 + STATE.size
        owner_length = data[offset]
        challenger_offset = offset + 1 + owner_length
        assert challenger_offset < len(data)
        challenger_length = data[challenger_offset]
        assert len(data) == challenger_offset + 1 + challenger_length
        return dict(revision=revision, owner=owner, challenger=challenger, progress=progress, rewards=rewards,
                    contested=bool(contested), owner_name=data[offset + 1:challenger_offset].decode('utf-8'),
                    challenger_name=data[challenger_offset + 1:].decode('utf-8'))
    raise AssertionError('Territory reply did not arrive')


def play(port, epoch, sockets, tickets):
    roles = (1, 2, 3, 2)
    players = [Player(sockets.enter_context(closing(world.connect(port))), ticket, epoch, role, True)
               for ticket, role in zip(tickets, roles, strict=True)]
    north, south, member, observer = players
    status, (north_id, *_rest) = north.guild(CREATE, name='北岸守望'.encode())
    assert status == OK
    status, (south_id, *_rest) = south.guild(CREATE, name='河谷巡衛'.encode())
    assert status == OK and north_id != south_id
    assert north.guild(INVITE, member.id)[0] == OK and member.guild(JOIN, north_id)[0] == OK
    actors = [Actor(player.peer, player.id, role) for player, role in zip(players, roles, strict=True)]
    sequence = 0

    def step(targets=()):
        for actor in actors:
            actor.poll(epoch)
        tick = max(actor.tick for actor in actors)
        for actor in actors:
            while actor.tick < tick:
                actor.poll(epoch)
        for actor, target in targets:
            actor.move_towards(epoch, target)

    def inspect():
        nonlocal sequence
        sequence += 1
        return territory(observer.peer, sequence)

    def walk(*targets):
        deadline = time.monotonic() + 45
        while time.monotonic() < deadline:
            step(targets)
            if all(dist(actor.position, target) <= .25 for actor, target in targets):
                return
        raise AssertionError(f'Players did not reach their destinations: {[actor.position for actor, _ in targets]}')

    def wait(predicate, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            step()
            state = inspect()
            if predicate(state):
                return state
        raise AssertionError(f'Territory condition did not arrive: {state}')

    n, s, m, o = actors
    walk((n, WEST), (s, EAST), (m, INSIDE_WEST), (o, LOOKOUT))
    step()
    lone_member = inspect()
    assert lone_member['owner'] == lone_member['challenger'] == lone_member['progress'] == 0
    walk((n, INSIDE_WEST))
    first_progress = wait(lambda state: state['challenger'] == north_id and state['progress'] > 0, 4)
    assert first_progress['owner'] == 0 and first_progress['challenger_name'] == '北岸守望'
    north_claim = wait(lambda state: state['owner'] == north_id, 16)
    assert north_claim['revision'] == 1 and north_claim['owner_name'] == '北岸守望' and north_claim['rewards'] == 0

    walk((n, WEST), (m, WEST), (s, INSIDE_EAST))
    started = wait(lambda state: state['challenger'] == south_id and state['progress'] >= 24, 5)
    assert started['owner'] == north_id
    walk((n, INSIDE_WEST))
    paused = wait(lambda state: state['contested'] and state['challenger'] == south_id, 4)
    assert paused['owner'] == north_id and paused['progress'] >= started['progress']
    for _ in range(10):
        step()
    held = inspect()
    assert held == paused, (held, paused)

    walk((n, WEST))
    south_claim = wait(lambda state: state['owner'] == south_id, 16)
    assert south_claim['revision'] == 2 and south_claim['owner_name'] == '河谷巡衛' and south_claim['rewards'] == 0
    assert south.guild(LEAVE)[0] == 5
    return north_id, south_id, paused['progress']


def verify(server, evidence):
    root = Path(mkdtemp(prefix='territory-loopback-', dir=evidence)).resolve()
    database = root / 'Territory.sqlite3'
    tickets = [secrets.token_bytes(32) for _ in range(4)]
    started = time.monotonic()
    with start(server, database) as (port, epoch), ExitStack() as sockets:
        north, south, paused = play(port, epoch, sockets, tickets)
    with start(server, database) as (port, restarted), ExitStack() as sockets:
        assert restarted != epoch
        restores = []
        for index in (3, 2, 1, 0):
            peer = sockets.enter_context(closing(world.connect(port)))
            player = Player(peer, tickets[index], restarted, (1, 2, 3, 2)[index])
            assert player.guild(VIEW)[0] == OK
            state = territory(peer, 1)
            assert (state['revision'], state['owner'], state['challenger'], state['progress']) == (2, south, 0, 0)
            restores.append(state['rewards'])
        assert restores == [0, 1, 1, 1]
    with closing(sqlite3.connect(database)) as db:
        claims = db.execute('SELECT revision, previous_guild_id, guild_id FROM riverbank_territory_claim ORDER BY revision').fetchall()
        rewards = db.execute('SELECT revision, character_id FROM riverbank_territory_reward ORDER BY revision, character_id').fetchall()
        assert claims == [(1, None, north), (2, north, south)]
        assert rewards == [(1, 1), (1, 3), (2, 2)]
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
    receipt = dict(status='RIVERBANK_TERRITORY_LOOPBACK_VERIFIED', simultaneous_clients=4, guilds=2,
                   defender_held_progress=paused, claims=claims, rewards=rewards, restored_rewards=restores,
                   server_sha256=world.sha256(server.read_bytes()).hexdigest(), seconds=round(time.monotonic() - started, 2))
    output = root / 'TerritoryLoopbackVerification.json'
    output.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], output)


if __name__ == '__main__':
    verify(Path(sys.argv[1]).resolve(strict=True), Path(sys.argv[2]).resolve(strict=True))
