"""驗證領取、勘查、首領結算、回報與重啟後的任務狀態。"""
from contextlib import ExitStack, closing
from math import dist
from pathlib import Path
from tempfile import mkdtemp
import json
import secrets
import sqlite3
import struct
import sys
import time

from DriveRiverbankParty import Player as Actor, play_encounter
from VerifyRiverbankGuild import Player
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world

BOARD = (1.9, 2.8)
TRAIL = (0., 26.)
AVAILABLE, CHECK_TRAIL, HUNT, REPORT, COMPLETE = range(5)
VIEWED, ADVANCED, TOO_FAR, HUNT_REQUIRED, FINISHED = range(5)


def expedition(peer, sequence, interact=False, watch=False):
    request = b'AEEX\x01\x01' + struct.pack('<QB', sequence, int(interact) | (int(watch) << 1))
    peer.sendall(struct.pack('!I', len(request)) + request)
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        size, = struct.unpack('!I', world.exact(peer, 4))
        assert 1 <= size <= 65536
        data = world.exact(peer, size)
        if data[:4] in (b'AENP', b'AECB'):
            continue
        assert len(data) == 16 and data.startswith(b'AEEX\x01\x02')
        received, stage, result = struct.unpack_from('<QBB', data, 6)
        assert received == sequence and stage <= (9 if watch else COMPLETE)
        return stage, result
    raise AssertionError('Expedition reply did not arrive')


def play(port, epoch, sockets, tickets, roles):
    players = [Player(sockets.enter_context(closing(world.connect(port))), ticket, epoch, role, True)
               for ticket, role in zip(tickets, roles, strict=True)]
    actors = [Actor(player.peer, player.id, role) for player, role in zip(players, roles, strict=True)]
    guard, ranger, priest, observer = actors
    sequences = {player.id: 0 for player in players}

    def action(actor, interact=False):
        sequences[actor.id] += 1
        return expedition(actor.peer, sequences[actor.id], interact)

    def walk(*targets, seconds=33):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            for actor in actors:
                actor.poll(epoch)
            tick = max(actor.tick for actor in actors)
            for actor in actors:
                while actor.tick < tick:
                    actor.poll(epoch)
            if all(dist(actor.position, target) < .25 for actor, target in targets):
                return
            for actor, target in targets:
                actor.move_towards(epoch, target)
        raise AssertionError(f'Players did not reach the objective: {[(p.position, p.state["hp"], goal) for p, goal in targets]}')

    assert action(guard, True) == (AVAILABLE, TOO_FAR)
    walk((guard, BOARD), (ranger, BOARD), (priest, BOARD))
    assert action(guard, True) == (CHECK_TRAIL, ADVANCED)
    assert action(ranger, True) == (CHECK_TRAIL, ADVANCED)
    assert action(guard, True) == (CHECK_TRAIL, TOO_FAR)
    walk((guard, TRAIL), (ranger, TRAIL), (priest, TRAIL))
    assert action(guard, True) == (HUNT, ADVANCED)
    assert action(guard, True) == (HUNT, HUNT_REQUIRED)
    assert action(ranger) == (CHECK_TRAIL, VIEWED)

    victory = play_encounter(epoch, guard, ranger, priest, observer)
    assert action(guard) == (REPORT, VIEWED)
    assert action(guard, True) == (REPORT, TOO_FAR)
    assert action(ranger) == (CHECK_TRAIL, VIEWED)
    assert action(priest) == (AVAILABLE, VIEWED)
    walk((guard, BOARD), (ranger, TRAIL))
    assert action(guard, True) == (COMPLETE, ADVANCED)
    assert action(guard, True) == (COMPLETE, FINISHED)
    assert action(ranger, True) == (HUNT, ADVANCED)
    assert action(ranger) == (HUNT, VIEWED)
    return victory


def verify(server, evidence):
    root = Path(mkdtemp(prefix='expedition-loopback-', dir=evidence)).resolve()
    database = root / 'Expedition.sqlite3'
    tickets = [secrets.token_bytes(32) for _ in range(4)]
    roles = (1, 2, 3, 2)
    started = time.monotonic()
    with start(server, database) as (port, epoch), ExitStack() as sockets:
        victory = play(port, epoch, sockets, tickets, roles)
    with start(server, database) as (port, restarted), ExitStack() as sockets:
        assert restarted != epoch
        for index, expected in enumerate((COMPLETE, HUNT, AVAILABLE, AVAILABLE)):
            peer = sockets.enter_context(closing(world.connect(port)))
            Player(peer, tickets[index], restarted, roles[index])
            assert expedition(peer, 1) == (expected, VIEWED)
    with closing(sqlite3.connect(database)) as db:
        rows = db.execute('SELECT character_id, stage, combat_world, combat_encounter FROM riverbank_expedition ORDER BY character_id').fetchall()
        assert rows == [(1, COMPLETE, epoch, victory['encounter']), (2, HUNT, None, None)]
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
        assert db.execute('SELECT COUNT(*) FROM riverbank_reward').fetchone() == (3,)
    receipt = dict(status='RIVERBANK_EXPEDITION_LOOPBACK_VERIFIED', simultaneous_clients=4,
                   stages_after_restart=['complete', 'hunt', 'available', 'available'],
                   outside_interaction_rejected=True, old_reward_did_not_clear_new_hunt=True,
                   single_completed_medal=True, victory=victory, seconds=round(time.monotonic() - started, 2),
                   server_sha256=world.sha256(server.read_bytes()).hexdigest())
    path = root / 'ExpeditionLoopbackVerification.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], path)


if __name__ == '__main__':
    verify(*(Path(argument).resolve(strict=True) for argument in sys.argv[1:]))
