"""以真實的伺服器、角色移動與存檔驗證巡林後的北岸勘查。"""
from contextlib import ExitStack, closing
from math import dist
from pathlib import Path
from tempfile import mkdtemp
import json
import secrets
import sqlite3
import sys
import time

from DriveRiverbankParty import Player as Actor
from VerifyRiverbankExpedition import (ADVANCED, AVAILABLE, BOARD, COMPLETE, FINISHED, HUNT,
                                        TOO_FAR, VIEWED, expedition, play)
from VerifyRiverbankGuild import Player
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world

WATCH_AVAILABLE, EAST, WEST, REPORT, RECORDED = range(5, 10)
EAST_POST = (21., 54.)
WEST_POST = (-15., 73.)


def walk(actor, epoch, *destinations):
    for destination in destinations:
        deadline = time.monotonic() + 35
        while time.monotonic() < deadline:
            actor.poll(epoch)
            if not actor.state['hp']:
                raise RuntimeError(f'勘查角色在 {actor.position} 倒地，目標 {destination}')
            if dist(actor.position, destination) < .8:
                break
            actor.move_towards(epoch, destination)
        else:
            raise RuntimeError(f'角色停在 {actor.position}，未抵達 {destination}')


def verify(server, evidence):
    root = Path(mkdtemp(prefix='north-watch-', dir=evidence))
    database = root / 'WatchSurvey.sqlite3'
    tickets = [secrets.token_bytes(32) for _ in range(4)]
    roles = (1, 2, 3, 2)
    started = time.monotonic()
    with start(server, database) as (port, first_epoch), ExitStack() as sockets:
        victory = play(port, first_epoch, sockets, tickets, roles)

    def connect(sockets, port, epoch, index=0):
        peer = sockets.enter_context(closing(world.connect(port)))
        player = Player(peer, tickets[index], epoch, roles[index], True)
        return Actor(peer, player.id, roles[index])

    with start(server, database) as (port, epoch), ExitStack() as sockets:
        assert epoch != first_epoch
        guard = connect(sockets, port, epoch)
        ranger = connect(sockets, port, epoch, 1)
        priest = connect(sockets, port, epoch, 2)
        assert expedition(ranger.peer, 1, watch=True) == (HUNT, VIEWED)
        assert expedition(priest.peer, 1, watch=True) == (AVAILABLE, VIEWED)
        ranger.peer.close()
        priest.peer.close()
        assert expedition(guard.peer, 1, True, True) == (WATCH_AVAILABLE, TOO_FAR)
        assert expedition(guard.peer, 2) == (COMPLETE, VIEWED)
        walk(guard, epoch, BOARD)
        assert expedition(guard.peer, 3, True, True) == (EAST, ADVANCED)
        assert expedition(guard.peer, 4, True, True) == (EAST, TOO_FAR)
        assert expedition(guard.peer, 5, True) == (COMPLETE, FINISHED)
        walk(guard, epoch, (0., 42.0), (11., 49.0), EAST_POST)
        assert expedition(guard.peer, 6, True, True) == (WEST, ADVANCED)
        assert expedition(guard.peer, 7, True, True) == (WEST, TOO_FAR)

    with start(server, database) as (port, restarted), ExitStack() as sockets:
        assert restarted != epoch
        guard = connect(sockets, port, restarted)
        assert expedition(guard.peer, 1, watch=True) == (WEST, VIEWED)
        assert expedition(guard.peer, 2, watch=True, interact=True) == (WEST, TOO_FAR)
        walk(guard, restarted, (0., 41.5), (-16., 45.5), WEST_POST)
        assert expedition(guard.peer, 3, True, True) == (REPORT, ADVANCED)
        assert expedition(guard.peer, 4, True, True) == (REPORT, TOO_FAR)
        walk(guard, restarted, (-16., 45.5), BOARD)
        assert expedition(guard.peer, 5, True, True) == (RECORDED, ADVANCED)
        assert expedition(guard.peer, 6, True, True) == (RECORDED, FINISHED)

    with start(server, database) as (port, final_epoch), ExitStack() as sockets:
        assert final_epoch != restarted
        guard = connect(sockets, port, final_epoch)
        assert expedition(guard.peer, 1, watch=True) == (RECORDED, VIEWED)
        assert expedition(guard.peer, 2) == (COMPLETE, VIEWED)
    with closing(sqlite3.connect(database)) as db:
        assert db.execute('PRAGMA user_version').fetchone() == (8,)
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
        assert db.execute('SELECT character_id,stage FROM riverbank_watch_survey').fetchall() == [(1, RECORDED)]
        assert db.execute('SELECT COUNT(*) FROM riverbank_reward').fetchone() == (3,)
    receipt = dict(status='RIVERBANK_NORTH_WATCH_VERIFIED', server_sha256=world.sha256(server.read_bytes()).hexdigest(),
                   visited=[EAST_POST, WEST_POST], routes_after_restart=['west', 'recorded'],
                   old_protocol_stage='complete', duplicate_boss_reward=False, victory=victory,
                   seconds=round(time.monotonic() - started, 2))
    path = root / 'NorthWatchVerification.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], path, flush=True)


if __name__ == '__main__':
    verify(*(Path(argument).resolve(strict=True) for argument in sys.argv[1:]))
