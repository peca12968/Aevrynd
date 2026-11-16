"""驗證兩人狩獵、圍區界線、閃避、脫戰、重生與持久化獸皮。"""
from contextlib import ExitStack
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
from VerifyRiverbankCoopJourney import Character, actors_read, walk
from VerifyRiverbankBossGear import OK, VIEW as GEAR_VIEW
from VerifyRiverbankExpedition import AVAILABLE, VIEWED
from VerifyRiverbankProfiles import start
import VerifyWorldServer as world


class Hunter(Character):
    def __init__(self, port, epoch, sockets, role, ticket=None):
        super().__init__(port, epoch, sockets, role, ticket, world.connect)
        self.field_sequence = 0

    def field(self):
        self.field_sequence += 1
        frame(self.peer, b"AEFH\x01\x01" + struct.pack("<Q", self.field_sequence))
        while True:
            size, = struct.unpack("!I", world.exact(self.peer, 4))
            assert 0 < size <= 65536
            data = world.exact(self.peer, size)
            if data[:4] in (b"AECB", b"AENP"):
                continue
            assert data[:6] == b"AEFH\x01\x02" and size == 59
            sequence, run, target, x, z, strike_x, strike_z, hp, ticks, hides, phase = struct.unpack_from("<QQQffffIIIB", data, 6)
            assert sequence == self.field_sequence
            return dict(run=run, target=target, position=(x, z), strike=(strike_x, strike_z),
                        hp=hp, ticks=ticks, hides=hides, phase=phase)


def verify(server, output):
    folder = Path(mkdtemp(prefix="field-hunt-", dir=output)).resolve()
    database = folder / "Profiles.sqlite3"
    began = time.monotonic()
    with start(server, database) as (port, epoch), ExitStack() as sockets:
        profiles = [Hunter(port, epoch, sockets, role) for role in (2, 1, 3)]
        ranger, guard, observer = profiles
        actors = [Actor(profile.peer, profile.id, role) for profile, role in zip(profiles, (2, 1, 3), strict=True)]
        scout, tank, outsider = actors

        # 在牧場邊緣，遊俠已進武器射程，但仍須踏入狩獵區才會出手。
        first = ranger.field()
        assert first["hp"] == 84 and first["phase"] == 0
        walk(actors, [scout], epoch, (10.55, 15.0))
        outside = ranger.field()
        assert first["position"] != outside["position"] and dist(scout.position, outside["position"]) < 13
        scout.act(2)
        for _ in range(5):
            actors_read(actors, epoch)
        assert scout.state["resource"] == 100 and scout.state["skill"] == 0
        assert ranger.field()["hp"] == 84

        # 一面接近一面讀取伺服器狀態，記下追擊與第一次預警。
        saw_pursuit = False
        sent_attack = False
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            actors_read(actors, epoch)
            sight = ranger.field()
            saw_pursuit |= sight["phase"] == 1
            if sight["phase"] == 2:
                break
            scout.move_towards(epoch, (13.8, 15.0))
            if scout.position[0] > 11.1 and not sent_attack:
                scout.act(1)
                sent_attack = True
        else:
            raise AssertionError(f"No field attack warning: {scout.position}, {sight}")
        assert saw_pursuit and sight["target"] == scout.id and sight["hp"] == 71
        hp_before = scout.state["hp"]
        sidestep = (scout.position[0], 17.8)
        while time.monotonic() < deadline:
            scout.move_towards(epoch, sidestep)
            actors_read(actors, epoch)
            if ranger.field()["phase"] != 2:
                break
        else:
            raise AssertionError("Ranger did not evade the field strike")
        assert hp_before == scout.state["hp"] == 105 and dist(scout.position, sight["strike"]) > 1.5, (sight, scout.position, scout.state)

        walk(actors, [scout], epoch, (10.5, 17.8))
        for _ in range(100):
            actors_read(actors, epoch)
            scout.move_towards(epoch, (9.8, 15.0))
            if scout.tick % 4 == 0:
                reset = ranger.field()
                if reset["run"] > sight["run"]:
                    break
        else:
            raise AssertionError("Field boar did not reset after losing its target")
        assert reset["hp"] == 84 and reset["phase"] == 0

        # 同一次狩獵兩名玩家貢獻，第三名角色旁觀。獨立結算不會提供首領裝備。
        saw_guard = False
        deadline = time.monotonic() + 14
        while time.monotonic() < deadline:
            actors_read(actors, epoch)
            status = ranger.field()
            if status["phase"] == 4:
                break
            if scout.position[0] < 11.35:
                scout.move_towards(epoch, (11.8, 16.4))
            if tank.position[0] < 11.35 or dist(tank.position, status["position"]) > 1.6:
                tank.move_towards(epoch, status["position"])
            if tank.position[0] > 11.1 and dist(tank.position, status["position"]) <= 6 and not tank.state["skill"]:
                tank.act(2)
            if tank.state["guarding"]:
                saw_guard = True
            if saw_guard and scout.position[0] > 11.1 and dist(scout.position, status["position"]) < 12:
                if not scout.state["skill"] and scout.state["resource"] >= 32:
                    scout.act(2)
                elif not scout.state["attack"]:
                    scout.act(1)
            if dist(tank.position, status["position"]) <= 2.4 and not tank.state["attack"]:
                tank.act(1)
        else:
            raise AssertionError(f"Field fight did not finish: {[(actor.position, actor.state['hp']) for actor in actors]}")
        assert saw_guard and status["hp"] == 0 and status["hides"] == 1
        assert guard.field()["hides"] == 1 and observer.field()["hides"] == 0
        assert all(actor.state["loot"] == 0 and actor.state["boss_hp"] == 330 for actor in actors)
        assert all(profile.gear(GEAR_VIEW) == (OK, False, False) and profile.quest() == (AVAILABLE, VIEWED)
                   for profile in profiles)
        defeated = status
        scout.act(1)
        for _ in range(10):
            actors_read(actors, epoch)
        assert ranger.field()["hides"] == 1 and guard.field()["hides"] == 1
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            actors_read(actors, epoch)
            if scout.tick % 4 == 0:
                respawn = ranger.field()
                if respawn["run"] > defeated["run"]:
                    break
        else:
            raise AssertionError("Field boar missed its respawn")
        assert respawn["hp"] == 84 and respawn["hides"] == 1
        tickets = [profile.ticket for profile in profiles]

    with sqlite3.connect(database) as db:
        assert db.execute("PRAGMA user_version").fetchone() == (8,)
        assert db.execute("SELECT character_id,world,encounter FROM riverbank_field_hide ORDER BY character_id").fetchall() == [
            (1, epoch, defeated["run"]), (2, epoch, defeated["run"])]
        assert db.execute("SELECT COUNT(*) FROM riverbank_reward").fetchone() == (0,)
        assert db.execute("PRAGMA foreign_key_check").fetchall() == []
    with start(server, database) as (port, new_epoch), ExitStack() as sockets:
        assert new_epoch != epoch
        profiles = [Hunter(port, new_epoch, sockets, role, ticket) for role, ticket in zip((2, 1, 3), tickets, strict=True)]
        restored = [profile.field()["hides"] for profile in profiles]
        assert restored == [1, 1, 0]
    receipt = {
        "status": "RIVERBANK_FIELD_HUNT_PASSED",
        "source_server_sha256": world.sha256(server.read_bytes()).hexdigest(),
        "simultaneous_clients": 3,
        "evasion": {"hp_before": hp_before, "hp_after": 105, "warning": sight},
        "reset_run": reset["run"], "defeat": defeated, "respawn_run": respawn["run"],
        "retained_hides": restored, "boss_rewards": 0, "boss_gear_awarded": False,
        "seconds": round(time.monotonic()-began, 2),
    }
    (folder / "FieldHuntVerification.json").write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
    print(folder / "FieldHuntVerification.json", flush=True)


if __name__ == "__main__":
    verify(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
