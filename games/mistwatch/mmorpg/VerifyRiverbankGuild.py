"""Exercise guild permissions and v1 character migration against real TCP server processes."""
from contextlib import ExitStack, closing
from pathlib import Path
import json
import os
import secrets
import sqlite3
import struct
import subprocess
import sys
import time
from tempfile import mkdtemp

import VerifyWorldServer as world
from VerifyRiverbankProfiles import login_request, login_response, start

VIEW, CREATE, INVITE, JOIN, DECLINE, LEAVE, PROMOTE, DEMOTE, TRANSFER = range(9)
OK, REJECTED, INVALID_NAME, NAME_TAKEN, TRANSFER_REQUIRED = range(5)
MEMBER, OFFICER, LEADER = range(1, 4)


class Player:
    def __init__(self, peer, ticket, epoch, role, create=False):
        self.peer = peer
        session, self.id = world.hello(peer)
        assert session == epoch
        self.sequence = 0
        login_request(peer, ticket, role if create else 0)
        assert login_response(peer) == role

    def guild(self, action, target=0, name=b'', replay=False):
        self.sequence += not replay
        return guild_command(self.peer, self.sequence, action, target, name)


def guild_command(peer, sequence, action, target=0, name=b''):
    request = b'AEGD\x01\x01' + struct.pack('<QBQ', sequence, action, target) + name
    peer.sendall(struct.pack('!I', len(request)) + request)
    deadline = time.monotonic() + 4
    while time.monotonic() < deadline:
        size, = struct.unpack('!I', world.exact(peer, 4))
        assert 1 <= size <= 65536
        data = world.exact(peer, size)
        if data[:4] in (b'AENP', b'AECB'):
            continue
        assert data.startswith(b'AEGD\x01\x02') and len(data) >= 34
        received_sequence, code, rank, guild_id, invite_id = struct.unpack_from('<QBBQQ', data, 6)
        assert received_sequence == sequence
        name_size = data[32]
        assert name_size <= 48 and len(data) >= 34 + name_size
        invite_size = data[33 + name_size]
        assert invite_size <= 48 and len(data) == 34 + name_size + invite_size
        return code, (guild_id, rank, data[33:33 + name_size].decode('utf-8'),
                      invite_id, data[34 + name_size:].decode('utf-8'))
    raise AssertionError('Guild response did not arrive')


def seed_legacy_profile(database, ticket):
    with closing(sqlite3.connect(database)) as db, db:
        db.executescript('''
            CREATE TABLE riverbank_character (
                id INTEGER PRIMARY KEY,
                ticket_sha256 BLOB NOT NULL UNIQUE CHECK(typeof(ticket_sha256) = 'blob' AND length(ticket_sha256) = 32),
                profession INTEGER NOT NULL CHECK(profession BETWEEN 1 AND 3)
            );
            CREATE TABLE riverbank_reward (
                character_id INTEGER NOT NULL REFERENCES riverbank_character(id),
                world INTEGER NOT NULL CHECK(world > 0),
                encounter INTEGER NOT NULL CHECK(encounter > 0),
                PRIMARY KEY (character_id, world, encounter)
            ) WITHOUT ROWID;
            PRAGMA user_version = 1;
        ''')
        db.execute('INSERT INTO riverbank_character VALUES (9, ?, 1)', (world.sha256(ticket).digest(),))
        db.execute('INSERT INTO riverbank_reward VALUES (9, 17, 1)')


def verify(server, evidence_directory, native_client):
    output = Path(mkdtemp(prefix='guild-loopback-', dir=evidence_directory)).resolve()
    database = output / 'RiverbankProfiles.sqlite3'
    tickets = [secrets.token_bytes(32) for _ in range(4)]
    seed_legacy_profile(database, tickets[0])
    with start(server, database) as (port, epoch), ExitStack() as sockets:
        def connect(index, role, create=False):
            return Player(sockets.enter_context(closing(world.connect(port))), tickets[index], epoch, role, create)

        founder, recruit, rival, candidate = (connect(0, 1), connect(1, 2, True), connect(2, 3, True), connect(3, 2, True))
        assert founder.guild(CREATE, name=b'\xff')[0] == INVALID_NAME
        assert founder.guild(CREATE, name=b'watch\x00')[0] == INVALID_NAME
        status, (north, rank, name, *_rest) = founder.guild(CREATE, name=b'North Watch')
        assert status == OK and north > 0 and rank == LEADER and name == 'North Watch'
        assert founder.guild(LEAVE, replay=True)[0] == REJECTED
        assert rival.guild(CREATE, name=b'north watch')[0] == NAME_TAKEN
        status, (south, rank, name, *_rest) = rival.guild(CREATE, name='南岸守望'.encode())
        assert status == OK and south != north and rank == LEADER and name == '南岸守望'
        assert recruit.guild(JOIN, north)[0] == REJECTED
        assert founder.guild(INVITE, recruit.id)[0] == OK
        assert recruit.guild(VIEW) == (OK, (0, 0, '', north, 'North Watch'))
        assert rival.guild(INVITE, recruit.id)[0] == REJECTED
        assert recruit.guild(JOIN, south)[0] == REJECTED
        assert recruit.guild(JOIN, north) == (OK, (north, MEMBER, 'North Watch', 0, ''))
        assert recruit.guild(INVITE, candidate.id)[0] == REJECTED
        assert founder.guild(PROMOTE, rival.id)[0] == REJECTED
        assert founder.guild(PROMOTE, recruit.id)[0] == OK
        assert recruit.guild(VIEW)[1][1] == OFFICER
        assert recruit.guild(INVITE, candidate.id)[0] == OK
        assert candidate.guild(DECLINE, north)[0] == OK
        assert candidate.guild(JOIN, north)[0] == REJECTED
        assert recruit.guild(INVITE, candidate.id)[0] == OK and candidate.guild(JOIN, north)[0] == OK
        assert recruit.guild(PROMOTE, candidate.id)[0] == REJECTED
        assert founder.guild(LEAVE)[0] == TRANSFER_REQUIRED
        assert founder.guild(TRANSFER, recruit.id)[1][1] == OFFICER
        assert recruit.guild(VIEW)[1][1] == LEADER
        assert recruit.guild(DEMOTE, founder.id)[0] == OK and founder.guild(VIEW)[1][1] == MEMBER
        assert candidate.guild(LEAVE) == (OK, (0, 0, '', 0, ''))
        assert recruit.guild(INVITE, candidate.id)[0] == OK

    with start(server, database) as (port, restarted), ExitStack() as sockets:
        assert restarted != epoch
        def reconnect(index, role):
            return Player(sockets.enter_context(closing(world.connect(port))), tickets[index], restarted, role)

        candidate, rival, recruit, founder = reconnect(3, 2), reconnect(2, 3), reconnect(1, 2), reconnect(0, 1)
        assert founder.guild(VIEW)[1] == (north, MEMBER, 'North Watch', 0, '')
        assert recruit.guild(VIEW)[1] == (north, LEADER, 'North Watch', 0, '')
        assert rival.guild(VIEW)[1] == (south, LEADER, '南岸守望', 0, '')
        assert candidate.guild(VIEW)[1] == (0, 0, '', north, 'North Watch')
        assert rival.guild(LEAVE)[0] == OK
        recreated = rival.guild(CREATE, name='南岸守望'.encode())
        assert recreated[0] == OK and recreated[1][0] not in (north, south)

    with start(server, database) as (port, native_epoch):
        assert native_epoch not in (epoch, restarted)
        env = dict(os.environ, AEVRYND_MISTWATCH_PROFILE_TICKET=tickets[0].hex())
        native = subprocess.run([str(native_client), str(port), '--guild-view'], env=env, capture_output=True,
                                creationflags=subprocess.CREATE_NO_WINDOW, text=True, check=True, timeout=8)
        assert native.stdout.strip() == 'NATIVE_GUILD_VIEW_PASS'

    with closing(sqlite3.connect(database)) as db:
        assert db.execute('PRAGMA user_version').fetchone() == (8,)
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
        assert db.execute('SELECT character_id, world, encounter FROM riverbank_reward').fetchall() == [(9, 17, 1)]
        assert db.execute('SELECT COUNT(*) FROM riverbank_guild WHERE retired=0').fetchone() == (2,)
    receipt = dict(status='RIVERBANK_GUILD_LOOPBACK_VERIFIED', simultaneous_clients=4, active_guilds=2,
                   server_starts=3, migrated_profile_reward_preserved=True, persisted_membership_rank_and_invite=True,
                   native_client_sha256=world.sha256(native_client.read_bytes()).hexdigest(),
                   duplicate_name_and_forbidden_actions_rejected=True, server_sha256=world.sha256(server.read_bytes()).hexdigest())
    path = output / 'GuildLoopbackVerification.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], path)


if __name__ == '__main__':
    verify(*(Path(argument).resolve(strict=True) for argument in sys.argv[1:]))
