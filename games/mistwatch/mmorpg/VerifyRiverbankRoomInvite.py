"""Verify that native reconnect to a different room cannot create a character."""
from contextlib import ExitStack, closing
from hashlib import sha256
import json
import os
from pathlib import Path
from secrets import token_bytes
import sqlite3
from subprocess import CREATE_NO_WINDOW, run
import sys
from tempfile import mkdtemp

from RiverbankTailnet import parse_room_invite, require_room_epoch, room_invite
from VerifyRiverbankParty import Player
from VerifyRiverbankProfiles import start


def verify_invite_parsing():
    address = '100.64.0.1'
    maximum_epoch = (1 << 64) - 1
    assert parse_room_invite(room_invite(address, 65535, maximum_epoch)) == (
        address, 65535, maximum_epoch)
    for invalid in (
        '100.64.0.1:0/1', '100.64.0.1:65536/1', '100.64.0.1:12/0',
        '100.64.0.1:12/18446744073709551616', '100.64.0.1:12/1/2',
        '100.64.0.1:12/\uff11', '192.168.1.10:12/1',
    ):
        try:
            parse_room_invite(invalid)
        except ValueError:
            continue
        raise AssertionError(f'Accepted malformed room invitation: {invalid}')


def verify(server, client, directory):
    verify_invite_parsing()
    output = Path(mkdtemp(prefix='room-invite-', dir=directory)).resolve()
    database = output / 'RiverbankProfiles.sqlite3'
    environment = {key: value for key, value in os.environ.items()
                   if not key.upper().startswith('AEVRYND_MISTWATCH_')}
    stale_ticket, exact_ticket, legacy_ticket = (token_bytes(32) for _ in range(3))
    with start(server, database) as (_, first_epoch):
        pass
    with start(server, database) as (port, epoch), ExitStack() as peers:
        assert epoch != first_epoch
        require_room_epoch(epoch, epoch)
        require_room_epoch(epoch, None)
        try:
            require_room_epoch(epoch, first_epoch)
        except ValueError:
            pass
        else:
            raise AssertionError('Launcher accepted the previous room epoch')

        def join(ticket, expected_epoch):
            env = dict(environment, AEVRYND_MISTWATCH_PROFILE_TICKET=ticket.hex())
            if expected_epoch is not None:
                env['AEVRYND_MISTWATCH_INITIAL_ROOM_EPOCH'] = str(expected_epoch)
            return run([str(client), str(port)], cwd=output, env=env, capture_output=True,
                       encoding='utf-8', timeout=12, creationflags=CREATE_NO_WINDOW)

        rejected = join(stale_ticket, first_epoch)
        assert rejected.returncode == 3 and not rejected.stdout.strip() and not rejected.stderr.strip(), rejected
        with closing(sqlite3.connect(database)) as store:
            assert store.execute('SELECT COUNT(*) FROM riverbank_character').fetchone() == (0,)
        # 客戶端煙霧測試需要觀察到兩位玩家。先查空資料庫，再加入觀察者。
        observer = Player(port, epoch, peers, 1)
        players = []
        for label, ticket, expected_epoch in (('exact', exact_ticket, epoch), ('legacy', legacy_ticket, None)):
            result = join(ticket, expected_epoch)
            assert result.returncode == 0, (label, result)
            actor, tick, movement = result.stdout.split()
            assert int(actor) > 0 and int(actor) != observer.id and int(tick) > 0 and float(movement) >= .1, result.stdout
            players.append(int(actor))
        assert len(set(players)) == 2
    with closing(sqlite3.connect(database)) as store:
        rows = dict(store.execute('SELECT ticket_sha256, profession FROM riverbank_character'))
        expected = {sha256(ticket).digest(): 2 for ticket in (exact_ticket, legacy_ticket)}
        expected[sha256(observer.ticket).digest()] = 1
        assert rows == expected, rows
    receipt = {'status': 'RIVERBANK_NATIVE_ROOM_INVITATION_VERIFIED', 'server_starts': 2,
               'stale_epoch_created_profiles': 0, 'exact_epoch_movement': True,
               'legacy_direct_connection_movement': True, 'persisted_profiles': len(rows) - 1}
    (output / 'RoomInviteVerification.json').write_text(
        json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], output, flush=True)


if __name__ == '__main__':
    verify(*(Path(argument).resolve() for argument in sys.argv[1:]))
