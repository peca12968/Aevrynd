"""證明北岸同一瞭望樁的進度只在到場且同階段的隊友之間共享。"""
from contextlib import ExitStack, closing
from pathlib import Path
from tempfile import mkdtemp
import json
import sqlite3
import sys
import time

from DriveRiverbankParty import Player as Actor, play_encounter
from VerifyRiverbankCoopJourney import Character, ROLES, walk
from VerifyRiverbankExpedition import (ADVANCED, AVAILABLE, BOARD, CHECK_TRAIL, COMPLETE,
                                       HUNT, REPORT, TOO_FAR, TRAIL, VIEWED, expedition)
from VerifyRiverbankParty import ACCEPT, INVITE, LEAVE, OK
from VerifyRiverbankProfiles import start
from VerifyRiverbankWatchSurvey import EAST, EAST_POST, RECORDED, WATCH_AVAILABLE, WEST, WEST_POST
import VerifyWorldServer as world

WATCH_REPORT = 8


def quest(player, interact=False):
    player.quest_sequence += 1
    return expedition(player.peer, player.quest_sequence, interact, watch=True)


def play(port, epoch, sockets):
    profiles = [Character(port, epoch, sockets, role, None, world.connect) for role in ROLES]
    guard, ranger, priest, visitor = profiles
    actors = [Actor(player.peer, player.id, role) for player, role in zip(profiles, ROLES, strict=True)]
    for guest in profiles[1:]:
        assert guard.party(INVITE, guest.id)[0] == OK
        assert guest.party(ACCEPT, guard.id)[0] == OK
    walk(actors, actors[:3], epoch, BOARD)
    assert all(player.quest(True) == (CHECK_TRAIL, ADVANCED) for player in profiles[:3])
    walk(actors, actors[:3], epoch, TRAIL)
    assert all(player.quest(True) == (HUNT, ADVANCED) for player in profiles[:3])
    play_encounter(epoch, *actors)
    assert all(player.quest() == (REPORT, VIEWED) for player in profiles[:3])
    walk(actors, actors[:3], epoch, BOARD)
    assert all(player.quest(True) == (COMPLETE, ADVANCED) for player in profiles[:3])
    assert all(quest(player, True) == (EAST, ADVANCED) for player in profiles[:3])
    assert quest(visitor) == (AVAILABLE, VIEWED)

    def eastbound(movers):
        for point in ((0., 42.), (11., 49.), EAST_POST):
            walk(actors, movers, epoch, point)

    eastbound((actors[0], actors[1], actors[3]))
    walk(actors, actors[1:2], epoch, (EAST_POST[0]+3.2,EAST_POST[1]))
    assert quest(ranger, True) == (EAST, TOO_FAR)
    assert quest(guard, True) == (WEST, ADVANCED)
    assert quest(ranger) == (WEST, VIEWED), 'Nearby eligible party member did not get the East survey'
    assert quest(priest) == (EAST, VIEWED), 'Distant party member got credit'
    assert quest(visitor) == (AVAILABLE, VIEWED), 'Nearby party member without the quest got credit'

    assert priest.party(LEAVE)[0] == OK
    eastbound(actors[2:3])
    assert quest(priest, True) == (WEST, ADVANCED)
    for point in ((14., 64.), WEST_POST):
        walk(actors, actors, epoch, point)
    assert quest(guard, True) == (WATCH_REPORT, ADVANCED)
    assert quest(ranger) == (WATCH_REPORT, VIEWED), 'Nearby eligible party member did not get the West survey'
    assert quest(priest) == (WEST, VIEWED), 'Nearby outsider with the same stage got credit'
    assert quest(visitor) == (AVAILABLE, VIEWED)

    for point in ((-16., 45.5), BOARD):
        walk(actors, actors[:2], epoch, point)
    assert quest(guard, True) == (RECORDED, ADVANCED)
    assert quest(ranger) == (WATCH_REPORT, VIEWED), 'Camp turn-in was shared'
    return [player.ticket for player in profiles]


def verify(server, evidence):
    out = Path(mkdtemp(prefix='party-watch-', dir=evidence))
    database = out/'PartyWatch.sqlite3'
    started = time.monotonic()
    with start(server,database) as (port, epoch), ExitStack() as sockets:
        tickets = play(port, epoch, sockets)
    with start(server,database) as (port, restarted), ExitStack() as sockets:
        assert restarted != epoch
        players = [Character(port, restarted, sockets, role, ticket, world.connect)
                   for role,ticket in zip(ROLES,tickets,strict=True)]
        assert [quest(player) for player in players] == [(stage,VIEWED)
            for stage in (RECORDED,WATCH_REPORT,WEST,AVAILABLE)]
    with closing(sqlite3.connect(database)) as db:
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
        assert db.execute('SELECT COUNT(*) FROM riverbank_reward').fetchone() == (3,)
        assert db.execute('SELECT stage FROM riverbank_watch_survey ORDER BY character_id').fetchall() == [(9,),(8,),(7,)]
    report=dict(status='RIVERBANK_PARTY_WATCH_VERIFIED',clients=4,both_posts_shared=True,turn_in_personal=True,
                outside_party_did_not_advance=True,distant_did_not_advance=True,unaccepted_did_not_advance=True,
                stages_after_restart=[RECORDED,WATCH_REPORT,WEST,AVAILABLE],seconds=round(time.monotonic()-started,2),
                server_sha256=world.sha256(server.read_bytes()).hexdigest())
    result=out/'PartyWatchVerification.json'
    result.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(report['status'],result,flush=True)


if __name__=='__main__':
    verify(*(Path(arg).resolve(strict=True) for arg in sys.argv[1:]))
