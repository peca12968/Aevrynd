"""Verify the durable profile path through three independent local server starts."""
from contextlib import ExitStack, closing, contextmanager
from pathlib import Path
import json
import os
import secrets
import sqlite3
import struct
import subprocess
import sys
from tempfile import mkdtemp

import VerifyRiverbankBattle as battle
import VerifyWorldServer as wire
from DriveRiverbankParty import Player, play_encounter


def login_request(peer, ticket, role=0):
    content = b'AERP\x01\x01' + bytes([role]) + ticket
    peer.sendall(struct.pack('!I', len(content)) + content)


def login_response(peer):
    size, = struct.unpack('!I', wire.exact(peer, 4))
    assert size == 7
    body = wire.exact(peer, size)
    assert body[:6] == b'AERP\x01\x02' and 0 <= body[6] <= 3
    return body[6]


@contextmanager
def start(executable, database):
    env = dict(os.environ, AEVRYND_MISTWATCH_PROFILE_DB=str(database))
    with subprocess.Popen([str(executable), '0'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                          creationflags=subprocess.CREATE_NO_WINDOW, env=env) as server:
        try:
            yield wire.ready_line(server)
        finally:
            server.terminate()
            server.wait(timeout=5)


def verify(executable, directory):
    receipt_dir = Path(mkdtemp(prefix='profile-reconnect-', dir=directory)).resolve()
    database = receipt_dir / 'RiverbankProfiles.sqlite3'
    roles = (1, 2, 3, 2)
    tickets = [secrets.token_bytes(32) for _ in roles]
    ranger, observer = tickets[1], tickets[3]
    with start(executable, database) as (port, first_epoch):
        with ExitStack() as cleanup:
            players = []
            for ticket, role in zip(tickets, roles, strict=True):
                client = cleanup.enter_context(closing(wire.connect(port)))
                epoch, player = wire.hello(client)
                assert epoch == first_epoch
                login_request(client, ticket)
                assert login_response(client) == 0
                login_request(client, ticket, role)
                assert login_response(client) == role
                actor = Player(client, player, role)
                actor.poll(epoch)
                assert actor.state['loot'] == 0
                players.append(actor)
            with closing(wire.connect(port)) as duplicate:
                wire.hello(duplicate)
                login_request(duplicate, ranger)
                assert duplicate.recv(1) == b''
            victory = play_encounter(first_epoch, *players)
    with closing(sqlite3.connect(database)) as db:
        rows = db.execute('SELECT id, ticket_sha256, profession FROM riverbank_character ORDER BY id').fetchall()
        assert [(row[1], row[2]) for row in rows] == [(wire.sha256(ticket).digest(), role) for ticket, role in zip(tickets, roles, strict=True)]
        ledger = db.execute('SELECT character_id, world, encounter FROM riverbank_reward ORDER BY character_id').fetchall()
        assert ledger == [(row[0], first_epoch, victory['encounter']) for row in rows[:3]]
    with start(executable, database) as (port, second_epoch):
        assert second_epoch != first_epoch
        with closing(wire.connect(port)) as restored:
            epoch, player = wire.hello(restored)
            login_request(restored, ranger)
            assert login_response(restored) == 2
            assert battle.read(restored, epoch, player)[2]['loot'] == 1
        with closing(wire.connect(port)) as overwrite:
            wire.hello(overwrite)
            login_request(overwrite, ranger, 3)
            assert overwrite.recv(1) == b''
    with start(executable, database) as (port, third_epoch):
        assert third_epoch not in [first_epoch, second_epoch]
        with closing(wire.connect(port)) as other:
            epoch, player = wire.hello(other)
            login_request(other, observer)
            assert login_response(other) == 2
            assert battle.read(other, epoch, player)[2]['loot'] == 0
    evidence = {'status': 'RIVERBANK_PROFILE_RECONNECT_VERIFIED', 'server_starts': 3, 'persistent_classes': roles,
                'duplicate_login_rejected': True, 'class_overwrite_rejected': True,
                'actual_encounter_victory': victory, 'durable_reward_rows': len(ledger), 'isolated_loot_after_restart': [1, 0],
                'server_sha256': wire.sha256(executable.read_bytes()).hexdigest()}
    (receipt_dir / 'ProfileReconnectVerification.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    print(evidence['status'], receipt_dir)


if __name__ == '__main__':
    verify(Path(sys.argv[1]).resolve(strict=True), Path(sys.argv[2]).resolve(strict=True))
