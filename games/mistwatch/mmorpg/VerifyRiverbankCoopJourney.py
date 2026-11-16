"""以四個真實 TCP 客戶端驗證完整巡林流程，含可選的 Tailscale 路徑。"""
import argparse
from contextlib import closing, contextmanager, ExitStack
from hashlib import file_digest
import json
from math import dist
from pathlib import Path
import socket
import sqlite3
import subprocess
from tempfile import mkdtemp
import time

from DriveRiverbankParty import Player as Actor, play_encounter
import RiverbankTailnet as tailnet
from VerifyRiverbankBossGear import EQUIP, GearPlayer, VIEW as GEAR_VIEW
from VerifyRiverbankExpedition import (ADVANCED, AVAILABLE, BOARD, CHECK_TRAIL, COMPLETE, HUNT,
                                       REPORT, TRAIL, VIEWED, expedition)
from VerifyRiverbankParty import ACCEPT, INVITE, OK, VIEW as PARTY_VIEW
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world

ROLES = (1, 2, 3, 2)


class Character(GearPlayer):
    def __init__(self, port, epoch, resources, role, ticket, connect):
        super().__init__(port, epoch, resources, role, ticket, connect=connect)
        self.quest_sequence = 0

    def quest(self, use=False):
        self.quest_sequence += 1
        return expedition(self.peer, self.quest_sequence, use)


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.wait(timeout=5)


@contextmanager
def game_endpoint(server, database, output, label, tunnel):
    prior = tailnet.tailscale('serve', 'status', '--json') if tunnel else None
    with start(server, database) as (port, epoch), ExitStack() as resources:
        if tunnel:
            node, _ = tailnet.network_nodes()
            with closing(socket.socket()) as reservation:
                reservation.bind(('127.0.0.1', 0))
                entry_port = reservation.getsockname()[1]
            if port == entry_port or tailnet.port_reserved(prior, entry_port):
                raise RuntimeError('The isolated route needs two unused, distinct ports')
            relay_log = resources.enter_context((output / f'{label}-relay.log').open('xb'))
            relay = resources.enter_context(subprocess.Popen(
                ['tailscale.exe', 'serve', f'--tcp={entry_port}', f'tcp://127.0.0.1:{port}'],
                stdin=subprocess.DEVNULL, stdout=relay_log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW))
            resources.callback(stop, relay)
            tailnet.wait_for_forwarding(relay, node, entry_port, epoch)

            def connect(check_port):
                return tailnet.open_peer(node.address, node.address, check_port)
        else:
            entry_port, connect = port, world.connect
        yield entry_port, epoch, connect
    if tunnel:
        for _ in range(10):
            if tailnet.tailscale('serve', 'status', '--json') == prior:
                break
            time.sleep(.3)
        else:
            raise RuntimeError('The private route did not close cleanly')


def actors_read(actors, epoch):
    for actor in actors:
        actor.poll(epoch)
    latest = max(actor.tick for actor in actors)
    for actor in actors:
        while actor.tick < latest:
            actor.poll(epoch)


def walk(actors, movers, epoch, target):
    deadline = time.monotonic() + 35
    while time.monotonic() < deadline:
        actors_read(actors, epoch)
        if all(dist(actor.position, target) < .25 for actor in movers):
            return
        for actor in movers:
            actor.move_towards(epoch, target)
    raise RuntimeError(f'Players did not reach {target}: {[actor.position for actor in movers]}')


def play_journey(port, epoch, connect, sockets):
    profiles = [Character(port, epoch, sockets, role, None, connect) for role in ROLES]
    founder, ranger, priest, outsider = profiles
    for guest in (ranger, priest):
        assert founder.party(INVITE, guest.id)[0] == OK
        assert guest.party(ACCEPT, founder.id)[0] == OK
    assert set(founder.party(PARTY_VIEW)[4]) == {member.id for member in profiles[:3]}

    actors = [Actor(player.peer, player.id, role) for player, role in zip(profiles, ROLES, strict=True)]
    walk(actors, actors[:3], epoch, BOARD)
    assert all(player.quest(True) == (CHECK_TRAIL, ADVANCED) for player in profiles[:3])
    walk(actors, actors[:3], epoch, TRAIL)
    assert all(player.quest(True) == (HUNT, ADVANCED) for player in profiles[:3])
    win = play_encounter(epoch, *actors)
    assert all(player.quest() == (REPORT, VIEWED) for player in profiles[:3])
    assert outsider.quest() == (AVAILABLE, VIEWED) and outsider.gear(GEAR_VIEW) == (OK, False, False)
    assert ranger.gear(EQUIP) == (OK, True, True)

    walk(actors, actors[:3], epoch, BOARD)
    for _ in range(6):
        actors_read(actors, epoch)
    previous = actors[1].position
    old_ranger = ranger.id
    ranger.peer.close()
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        offline_view = founder.party(PARTY_VIEW)
        if old_ranger not in offline_view[4] and offline_view[3] == 3:
            break
    else:
        raise RuntimeError('Disconnected Ranger did not retain the party seat')

    ranger = Character(port, epoch, sockets, ROLES[1], ranger.ticket, connect)
    assert ranger.id != old_ranger
    actors[1] = Actor(ranger.peer, ranger.id, ROLES[1])
    actors[1].poll(epoch)
    assert actors[1].position == previous, (previous, actors[1].position)
    restored_roster = ranger.party(PARTY_VIEW)
    assert restored_roster[1] == founder.id and set(restored_roster[4]) == {founder.id, ranger.id, priest.id}
    assert ranger.quest() == (REPORT, VIEWED) and ranger.gear(GEAR_VIEW) == (OK, True, True)
    profiles[1] = ranger
    assert all(player.quest(True) == (COMPLETE, ADVANCED) for player in profiles[:3])
    return [player.ticket for player in profiles], win, {'before': previous, 'after': actors[1].position,
                                                        'old_connection': old_ranger, 'new_connection': ranger.id}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('server', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--tailnet', action='store_true')
    args = parser.parse_args()
    server = args.server.resolve(strict=True)
    output = Path(mkdtemp(prefix='coop-journey-', dir=args.output)).resolve()
    database = output / 'Journey.sqlite3'
    started = time.monotonic()
    with game_endpoint(server, database, output, 'first-world', args.tailnet) as (port, epoch, connect), ExitStack() as sockets:
        tickets, victory, reconnect = play_journey(port, epoch, connect, sockets)
    with game_endpoint(server, database, output, 'restarted-world', args.tailnet) as (port, restarted, connect), ExitStack() as sockets:
        assert restarted != epoch
        profiles = [Character(port, restarted, sockets, role, ticket, connect)
                    for role, ticket in zip(ROLES, tickets, strict=True)]
        stages = [p.quest() for p in profiles]
        assert stages == [(COMPLETE, VIEWED)] * 3 + [(AVAILABLE, VIEWED)]
        assert profiles[1].gear(GEAR_VIEW) == (OK, True, True)
    with closing(sqlite3.connect(database)) as store:
        assert store.execute('PRAGMA foreign_key_check').fetchall() == []
        assert store.execute('SELECT character_id, stage FROM riverbank_expedition ORDER BY 1').fetchall() == [(1, 4), (2, 4), (3, 4)]
        assert store.execute('SELECT * FROM riverbank_boss_kit').fetchall() == [(2, epoch, victory['encounter'])]
        assert store.execute('SELECT COUNT(*) FROM riverbank_reward').fetchone() == (3,)
    with server.open('rb') as binary:
        digest = file_digest(binary, 'sha256').hexdigest()
    evidence = {
        'status': 'MISTWATCH_COOP_JOURNEY_PASSED', 'server_sha256': digest, 'physical_hosts': 1, 'simultaneous_clients': 4,
        'transport': 'tailscale-serve' if args.tailnet else 'loopback', 'automated_seconds': round(time.monotonic() - started, 2),
        'completed_expeditions': 3, 'eligible_players_paid_once': 3, 'unaffiliated_observer_denied_reward': True,
        'ranger_gear_persisted': True, 'reconnect_retained_party_seat_and_progress': True, 'reconnect': reconnect,
        'post_restart_quest_stages': [stage for stage, _ in stages], 'boss': victory,
    }
    receipt = output / 'CoopJourneyVerification.json'
    receipt.write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    print(evidence['status'], receipt)


if __name__ == '__main__':
    main()
