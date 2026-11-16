"""走完兩次野外狩獵、在營地兌換、並驗證重啟後的資源恢復。"""
from contextlib import ExitStack, closing
from math import dist
from pathlib import Path
import json
import sqlite3
import struct
import sys
from tempfile import mkdtemp
import time

from DriveRiverbankParty import Player as Actor
from VerifyRiverbankBattle import frame
from VerifyRiverbankCoopJourney import actors_read, walk
from VerifyRiverbankExpedition import BOARD
from VerifyRiverbankFieldHunt import Hunter
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world

VIEWED, EXCHANGED, NOT_ENOUGH, TOO_FAR, DOWNED, ALREADY_OWNED, REJECTED = range(7)
# 集合點靠牧場北側，保留與巡邏野豬的空間。
MEADOW = (11.4, 18.2)


class Customer(Hunter):
    def __init__(self, port, epoch, sockets, role, ticket=None):
        super().__init__(port, epoch, sockets, role, ticket)
        self.supply_sequence = 0

    def supplies(self, buy=False, replay=False):
        if not replay:
            self.supply_sequence += 1
        frame(self.peer, b'AESC\x01\x01' + struct.pack('<QB', self.supply_sequence, int(buy)))
        while True:
            size, = struct.unpack('!I', world.exact(self.peer, 4))
            assert 0 < size <= 65536
            data = world.exact(self.peer, size)
            if data[:4] in (b'AENP', b'AECB'):
                continue
            assert data.startswith(b'AESC\x01\x02') and size == 20
            sequence, result, earned, owned = struct.unpack_from('<QBIB', data, 6)
            assert sequence == self.supply_sequence and owned in (0, 1)
            return result, earned, bool(owned)


def measure_recovery(actors, ranger, epoch):
    actors_read(actors, epoch)
    assert ranger.state['resource'] == 100 and not ranger.state['skill']
    ranger.act(2)
    for _ in range(10):
        actors_read(actors, epoch)
        if ranger.state['skill']:
            break
    else:
        raise AssertionError('Ranger did not cast Q in the field')
    start_tick = ranger.tick
    depleted = ranger.state['resource']
    assert depleted < 75 and ranger.state['hp']
    while ranger.tick < start_tick + 10:
        actors_read(actors, epoch)
    assert ranger.tick == start_tick + 10
    return ranger.state['resource'] - depleted


def hunt(actors, customer, epoch):
    ranger, tank, outsider = actors
    deadline = time.monotonic() + 15
    while time.monotonic() < deadline:
        actors_read(actors, epoch)
        field = customer.field()
        if field['phase'] == 4:
            return field['run']
        for actor in (ranger, tank):
            assert actor.state['hp'] > 0, ('Downed in the pasture', actor.position, field)
        if dist(tank.position, field['position']) > 1.4:
            tank.move_towards(epoch, field['position'])
        if dist(tank.position, field['position']) < 5.5 and not tank.state['skill']:
            tank.act(2)
        elif dist(tank.position, field['position']) <= 2.5 and not tank.state['attack']:
            tank.act(1)
        if not ranger.state['skill'] and ranger.state['resource'] >= 32:
            ranger.act(2)
        elif not ranger.state['attack']:
            ranger.act(1)
    raise AssertionError('Wild boar did not fall before the hunt deadline')


def wait_respawn(actors, scout, customer, epoch, current):
    deadline = time.monotonic() + 11
    while time.monotonic() < deadline:
        actors_read(actors, epoch)
        if scout.tick % 4 == 0:
            field = customer.field()
            if field['run'] > current:
                assert field['hp'] == 84
                return
    raise AssertionError('No second hunt available')


def verify(server, output):
    folder = Path(mkdtemp(prefix='trail-supplies-', dir=output)).resolve()
    database = folder / 'Profiles.sqlite3'
    started = time.monotonic()
    with start(server, database) as (port, epoch), ExitStack() as sockets:
        customer, defender, spectator = [Customer(port, epoch, sockets, role) for role in (2, 1, 3)]
        actors = [Actor(p.peer, p.id, role) for p, role in zip((customer, defender, spectator), (2, 1, 3), strict=True)]
        ranger, tank, outsider = actors
        assert customer.supplies() == (VIEWED, 0, False)
        walk(actors, [ranger], epoch, BOARD)
        assert customer.supplies(buy=True) == (NOT_ENOUGH, 0, False)

        walk(actors, [ranger, tank], epoch, MEADOW)
        original_recovery = measure_recovery(actors, ranger, epoch)
        assert original_recovery == 2
        first = hunt(actors, customer, epoch)
        assert customer.supplies() == (VIEWED, 1, False)

        wait_respawn(actors, ranger, customer, epoch, first)
        second = hunt(actors, customer, epoch)
        assert second > first
        assert customer.supplies(buy=True) == (TOO_FAR, 2, False)
        assert spectator.supplies() == (VIEWED, 0, False)

        walk(actors, [ranger], epoch, BOARD)
        assert customer.supplies(buy=True) == (EXCHANGED, 2, True)
        assert customer.supplies(buy=True, replay=True) == (REJECTED, 2, True)
        assert customer.supplies(buy=True) == (ALREADY_OWNED, 2, True)
        assert defender.supplies() == (VIEWED, 2, False)
        assert all(actor.state['loot'] == 0 for actor in actors)
        ticket = customer.ticket

    with closing(sqlite3.connect(database)) as db:
        assert db.execute('PRAGMA user_version').fetchone() == (8,)
        assert db.execute('PRAGMA foreign_key_check').fetchall() == []
        assert db.execute('SELECT character_id,first_world,first_encounter,second_world,second_encounter '
                          'FROM riverbank_trail_pack').fetchall() == [(1, epoch, first, epoch, second)]
        assert db.execute('SELECT character_id,COUNT(*) FROM riverbank_field_hide GROUP BY character_id '
                          'ORDER BY character_id').fetchall() == [(1, 2), (2, 2)]
        assert db.execute('SELECT COUNT(*) FROM riverbank_reward').fetchone() == (0,)

    with start(server, database) as (port, restarted), ExitStack() as sockets:
        assert restarted != epoch
        customer = Customer(port, restarted, sockets, 2, ticket)
        assert customer.supplies() == (VIEWED, 2, True)
        ranger = Actor(customer.peer, customer.id, 2)
        walk([ranger], [ranger], restarted, (11.7, 15))
        upgraded_recovery = measure_recovery([ranger], ranger, restarted)
        assert upgraded_recovery == 4
    result = {
        'status': 'RIVERBANK_TRAIL_SUPPLIES_VERIFIED',
        'server_sha256': world.sha256(server.read_bytes()).hexdigest(),
        'simultaneous_clients': 3, 'field_runs': [first, second], 'paid_hides': 2,
        'remote_and_duplicate_exchange_denied': True, 'raw_hunt_history_preserved': True,
        'recovery_in_ten_ticks_before': original_recovery, 'recovery_in_ten_ticks_after_restart': upgraded_recovery,
        'seconds': round(time.monotonic() - started, 2),
    }
    receipt = folder / 'TrailSuppliesVerification.json'
    receipt.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(result['status'], receipt)


if __name__ == '__main__':
    verify(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
