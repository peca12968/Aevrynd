"""從屋外站姿走到南岸低棚，核對兩條 TCP 連線收到的權威姿勢。"""
from contextlib import ExitStack
from hashlib import file_digest
from math import dist
from pathlib import Path
from secrets import token_bytes
from tempfile import mkdtemp
import json
import struct
import sys

import VerifyWorldServer as wire
from VerifyRiverbankBattle import frame
from VerifyRiverbankJump import attach
from VerifyRiverbankExpedition import ADVANCED, CHECK_TRAIL, VIEWED, expedition
from VerifyRiverbankProfiles import start

CAPABILITY = 1 << 31
CROUCHED = 1 << 1

def view(actor, epoch):
    kind, raw = wire.packet(actor[0])
    if kind == wire.ENCOUNTER_FRAME:
        kind, raw = wire.packet(actor[0])
    assert kind == 3
    session, tick, _, _, _, recipient, full, count, gone = wire.SNAPSHOT.unpack_from(raw)
    assert (session, recipient, full) == (epoch, actor[1], 1)
    assert len(raw) == wire.SNAPSHOT.size + count * wire.ENTITY.size + gone * 16
    result = {}
    for offset in range(wire.SNAPSHOT.size, wire.SNAPSHOT.size + count * wire.ENTITY.size, wire.ENTITY.size):
        hi, identity, cx, cy, cz, x, y, z, *tail = wire.ENTITY.unpack_from(raw, offset)
        assert hi == epoch
        result[identity] = ((cx * 32000 + x, cy * 32000 + y, cz * 32000 + z), tail[-1] & (CAPABILITY | CROUCHED))
    assert wire.packet(actor[0])[0] == wire.ENCOUNTER_FRAME
    return tick, result

def check(server, output, baseline):
    result = {"physical_hosts": 1, "clients": 2}
    with server.open("rb") as stream:
        result["server_sha256"] = file_digest(stream, "sha256").hexdigest()
    profile = output / "Profiles.sqlite3"
    ticket = token_bytes(32)
    with start(server, profile) as (port, epoch), ExitStack() as sockets:
        actor = attach(port, epoch, sockets, ticket, 1)
        observer = attach(port, epoch, sockets, token_bytes(32), 2)
        clients = (actor, observer)
        seq = 0
        def shared():
            parts = [view(person, epoch) for person in clients]
            for _ in range(50):
                if parts[0][0] == parts[1][0]:
                    assert parts[0] == parts[1], parts
                    return parts[0]
                older = int(parts[1][0] < parts[0][0])
                parts[older] = view(clients[older], epoch)
            raise AssertionError("The two snapshots did not align")

        def input_at(tick, x=0, z=0, flags=0):
            nonlocal seq
            seq += 1
            body = struct.pack("<QQQIhhhB", epoch, tick + 4, seq, actor[1], x, z, 0, flags)
            header = wire.HEADER.pack(b"AENP", 1, 2, 0, len(body), wire.crc32c(body), wire.sha256(body).digest())
            frame(actor[0], header + body)

        def route(xz, limit):
            for _ in range(limit):
                tick, state = shared()
                (x, _, z), posture = state[actor[1]]
                error = (xz[0] - x, xz[1] - z)
                length = dist((0, 0), error)
                if length < 95:
                    return (x, z)
                speed = min(32767, length * 100)
                input_at(tick, *(round(component * speed / length) for component in error))
            return x, z

        for _ in range(16):
            tick, state = shared()
            if len(state) == 2:
                break
        assert len(state) == 2
        assert bool(state[actor[1]][1] & CAPABILITY) != baseline
        assert dist(route((1900, 2800), 65), (1900, 2800)) < 110
        assert expedition(actor[0], 1, True) == (CHECK_TRAIL, ADVANCED)
        assert dist(route((0, 17180), 225), (0, 17180)) < 110
        assert dist(route((-4560, 17180), 85), (-4560, 17180)) < 110
        stood = route((-6700, 17180), 38 if not baseline else 65)
        if baseline:
            assert stood[0] < -6400, stood
            result.update(stage="R4_STANDING_PASSES_THROUGH_LOW_SHELTER", stood=stood)
            return result
        assert -5110 < stood[0] < -5000, stood
        frame(actor[0], b"AECS\x01\x01")
        lowering = []
        for _ in range(7):
            tick, snapshot = shared()
            position, flags = snapshot[actor[1]]
            if flags & CROUCHED:
                lowering.append((tick, position))
            input_at(tick, -32767)
        assert len(lowering) >= 3
        assert lowering[0][1][0] >= stood[0] - 1 and lowering[1][1] == lowering[0][1], lowering
        assert snapshot[actor[1]][1] & CROUCHED
        # 衝刺或起跳位元同時送到伺服器也不能突破蹲行限制。
        previous = snapshot[actor[1]][0]
        motion = []
        for _ in range(38):
            tick, snapshot = shared()
            posture, flags = snapshot[actor[1]]
            assert flags & CROUCHED and posture[1] == 0, (tick, posture, flags)
            motion.append(dist(previous, posture))
            previous = posture
            input_at(tick, -32767, flags=3)
        assert max(motion) <= 46 and max(motion) >= 44, motion
        assert dist(route((-6900, 17180), 68), (-6900, 17180)) < 110
        frame(actor[0], b"AECS\x01\x00")
        for _ in range(12):
            _, snapshot = shared()
            assert snapshot[actor[1]][1] & CROUCHED
        at_reconnect = snapshot[actor[1]][0]
        old_identity = actor[1]
        actor[0].close()
        for _ in range(45):
            _, observed = view(observer, epoch)
            if old_identity not in observed:
                break
        else:
            raise AssertionError("The disconnected crouched avatar remained in the shared world")
        actor = attach(port, epoch, sockets, ticket, 1, reconnect=True)
        clients = (actor, observer)
        assert actor[1] != old_identity
        seq = 0
        for _ in range(5):
            tick, snapshot = shared()
        assert snapshot[actor[1]] == (at_reconnect, CAPABILITY | CROUCHED), snapshot
        assert expedition(actor[0], 1) == (CHECK_TRAIL, VIEWED)

        assert dist(route((-4610, 17180), 93), (-4610, 17180)) < 110
        for _ in range(9):
            _, snapshot = shared()
        assert not snapshot[actor[1]][1] & CROUCHED
        frame(actor[0], b"AECS\x01\x01")
        for _ in range(5):
            _, snapshot = shared()
        assert dist(route((-6800, 17180), 85), (-6800, 17180)) < 110
        for _ in range(10):
            _, snapshot = shared()
        before_restart = snapshot[actor[1]][0]
        assert snapshot[actor[1]][1] & CROUCHED
    with start(server, profile) as (port, second_epoch), ExitStack() as sockets:
        assert second_epoch != epoch
        restored = attach(port, second_epoch, sockets, ticket, 1, reconnect=True)
        for _ in range(5):
            _, rejoined = view(restored, second_epoch)
        # Restart creates a new world. Existing characters return to camp; their quest stage is durable.
        camp, posture = rejoined[restored[1]]
        assert camp[1] == 0 and dist(camp[::2], (0, 0)) < 5500 and posture == CAPABILITY, rejoined
        assert camp != before_restart and expedition(restored[0], 1) == (CHECK_TRAIL, VIEWED)
    result.update(stage="RIVERBANK_CROUCH_NETWORK_VERIFIED", standing_stop_mm=stood,
                  maximum_crouched_step_mm=max(motion), stand_blocked_under_roof=True,
                  stands_after_exit=True, server_rejected_jump_while_crouched=True,
                  first_lowering_frames=lowering[:2],
                  crouched_reconnect_preserved=at_reconnect,
                  restart={"from_shelter_mm": before_restart, "camp_mm": camp,
                           "position_policy": "camp_on_new_world", "expedition_stage_preserved": CHECK_TRAIL})
    return result

if __name__ == "__main__":
    server = Path(sys.argv[1]).resolve(strict=True)
    output = Path(mkdtemp(dir=sys.argv[2], prefix="shelter-stance-"))
    data = check(server, output, "--baseline" in sys.argv)
    (output / "ShelterCrouchProof.json").write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(data["stage"], output, flush=True)
