"""驗證共用世界中的組隊邀請、名單、離隊與重新連線。"""
from contextlib import closing, ExitStack
import json
import os
from pathlib import Path
from secrets import token_bytes
import queue
import struct
import subprocess
import sys
from tempfile import mkdtemp
import time
import threading

import VerifyWorldServer as world
from VerifyRiverbankProfiles import login_request, login_response, start

VIEW, INVITE, ACCEPT, DECLINE, LEAVE = range(5)
OK, REJECTED, FULL = range(3)


class Player:
    def __init__(self, port, epoch, sockets, role, ticket=None, *, connect=world.connect):
        self.peer = sockets.enter_context(closing(connect(port)))
        session, self.id = world.hello(self.peer)
        if session != epoch:
            raise AssertionError('Client joined a different world')
        self.ticket = ticket or token_bytes(32)
        self.sequence = 0
        login_request(self.peer, self.ticket, 0 if ticket else role)
        if login_response(self.peer) != role:
            raise AssertionError('Class assignment changed')

    def party(self, action, target=0, replay=False):
        self.sequence += int(not replay)
        request = b'AEPT\x01\x01' + struct.pack('<QBI', self.sequence, action, target)
        self.peer.sendall(struct.pack('!I', len(request)) + request)
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            size, = struct.unpack('!I', world.exact(self.peer, 4))
            assert 0 < size <= 65536
            response = world.exact(self.peer, size)
            if response[:4] in (b'AENP', b'AECB'):
                continue
            assert response.startswith(b'AEPT\x01\x02') and size >= 25
            seq, result, leader, inviter, total, count = struct.unpack_from('<QB I I B B', response, 6)
            assert seq == self.sequence and size == 25 + count * 7
            members = {actor: (role, health) for actor, role, health in struct.iter_unpack('<IBH', response[25:])}
            assert len(members) == count
            return result, leader, inviter, total, members
        raise AssertionError('Party response did not arrive')


def verify(server, output, native_client):
    path = Path(mkdtemp(prefix='party-loopback-', dir=output)).resolve()
    with start(server, path / 'Profiles.sqlite3') as (port, epoch), ExitStack() as sockets:
        guard, ranger, priest, fourth, outsider = (Player(port, epoch, sockets, role) for role in (1, 2, 3, 1, 2))
        assert ranger.party(VIEW) == (OK, 0, 0, 0, {}), 'Fresh party view'
        assert ranger.party(ACCEPT, guard.id)[0] == REJECTED, 'Uninvited member cannot join'
        assert guard.party(INVITE, ranger.id)[0] == OK
        assert outsider.party(INVITE, ranger.id)[0] == REJECTED, 'Existing invite cannot be replaced'
        assert ranger.party(VIEW)[2] == guard.id
        assert ranger.party(ACCEPT, outsider.id)[0] == REJECTED, 'Wrong inviter rejected'
        assert ranger.party(DECLINE, guard.id) == (OK, 0, 0, 0, {})
        assert ranger.party(ACCEPT, guard.id)[0] == REJECTED
        assert guard.party(INVITE, ranger.id)[0] == OK
        result = ranger.party(ACCEPT, guard.id)
        assert result == (OK, guard.id, 0, 2, {guard.id: (1, 150), ranger.id: (2, 105)})
        assert ranger.party(LEAVE, replay=True)[0] == REJECTED, 'Duplicate sequence must not mutate membership'
        assert ranger.party(INVITE, priest.id)[0] == REJECTED, 'Only leader can invite after joining'
        assert guard.party(INVITE, priest.id)[0] == OK
        assert priest.party(ACCEPT, guard.id)[4][priest.id] == (3, 100)
        assert guard.party(INVITE, fourth.id)[0] == OK
        assert fourth.party(ACCEPT, guard.id)[3] == 4
        assert guard.party(INVITE, outsider.id)[0] == FULL
        assert outsider.party(VIEW) == (OK, 0, 0, 0, {}), 'Outsider cannot query roster'

        original = ranger.id
        ranger.peer.close()
        deadline = time.monotonic() + 3
        while True:
            offline = guard.party(VIEW)
            if original not in offline[4]:
                break
            assert time.monotonic() < deadline
        assert offline[3] == 4 and len(offline[4]) == 3
        ranger = Player(port, epoch, sockets, 2, ranger.ticket)
        assert ranger.id != original
        restored = ranger.party(VIEW)
        assert restored[1] == guard.id and restored[3] == 4 and restored[4][ranger.id] == (2, 105)
        assert original not in restored[4]
        assert guard.party(LEAVE) == (OK, 0, 0, 0, {})
        assert priest.party(VIEW)[1:4] == (ranger.id, 0, 3), 'Next joined member becomes leader'

        flags = subprocess.CREATE_NO_WINDOW
        env = dict(os.environ, AEVRYND_MISTWATCH_PROFILE_TICKET=outsider.ticket.hex())
        old_outsider = outsider.id
        outsider.peer.close()
        since = time.monotonic()
        while True:
            _, current, _, _ = world.snapshot_with_classes(guard.peer, epoch, guard.id)
            if old_outsider not in current:
                break
            assert time.monotonic() < since + 3
        with subprocess.Popen([str(native_client), str(port), '--party-accept'], stdout=subprocess.PIPE,
                              stderr=subprocess.PIPE, text=True, creationflags=flags, env=env) as native:
            lines = queue.Queue()
            threading.Thread(target=lambda: lines.put(native.stdout.readline()), daemon=True).start()
            line = lines.get(timeout=5).strip()
            assert line.startswith('NATIVE_PARTY_READY '), line
            native_id = int(line.split()[-1])
            assert ranger.party(INVITE, native_id)[0] == OK
            stdout, stderr = native.communicate(timeout=8)
            assert native.returncode == 0 and stdout.strip() == 'NATIVE_PARTY_ACCEPT_PASS', stderr or stdout

    receipt = dict(status='RIVERBANK_PARTY_VERIFIED', simultaneous_clients=5,
                   invite_consent=True, leader_access=True, capacity=4, third_party_roster_withheld=True,
                   disconnect_kept_member_slot=True, member_rejoined_with_new_id=True,
                   leadership_passes_on_leave=True, native_client_accepted_invitation=True,
                   server_sha256=world.sha256(server.read_bytes()).hexdigest(),
                   native_client_sha256=world.sha256(native_client.read_bytes()).hexdigest())
    target = path / 'PartyLoopbackVerification.json'
    target.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], target)


if __name__ == '__main__':
    verify(*(Path(argument).resolve(strict=True) for argument in sys.argv[1:]))
