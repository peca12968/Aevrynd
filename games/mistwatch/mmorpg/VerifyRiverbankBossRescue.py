"""Verify that a priest can recover a downed ally after a real boss victory."""
import argparse
from contextlib import ExitStack, closing
import json
from math import dist
from pathlib import Path
import sqlite3
from tempfile import mkdtemp
import time

from VerifyRiverbankBossGear import EQUIP, VIEW as GEAR_VIEW, GearPlayer
from VerifyRiverbankCoopJourney import Actor, actors_read, walk
from VerifyRiverbankParty import ACCEPT, INVITE, OK, VIEW as PARTY_VIEW, Player
from VerifyRiverbankProfiles import start
from VerifyWorldServer import sha256

DEFEATED = 4
PRIEST_RECOVERY = 42


def wait_for(actors, epoch, seconds, step, stage):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        actors_read(actors, epoch)
        if step():
            return
    detail = {actor.id: dict(role=actor.role, position=actor.position, hp=actor.state["hp"],
                               loot=actor.state["loot"], boss_hp=actor.state["boss_hp"],
                               phase=actor.state["boss_phase"], target=actor.state["boss_target"], strike=actor.state["strike"]) for actor in actors}
    raise AssertionError(f"{stage}: {detail}")


def verify(server, destination, offline_until_victory=False):
    root = Path(mkdtemp(prefix="boss-rescue-", dir=destination)).resolve()
    database = root / "Riverbank.sqlite3"
    evidence = {"stage": "RUNNING", "server_sha256": sha256(server.read_bytes()).hexdigest()}
    try:
        with start(server, database) as (port, epoch), ExitStack() as sockets:
            profiles = [Player(port, epoch, sockets, role) for role in (1, 2, 3, 2)]
            guardian, ranger, priest, observer = (
                Actor(profile.peer, profile.id, role)
                for profile, role in zip(profiles, (1, 2, 3, 2), strict=True)
            )
            actors = (guardian, ranger, priest, observer)
            for guest in profiles[1:3]:
                assert profiles[0].party(INVITE, guest.id)[0] == OK
                assert guest.party(ACCEPT, profiles[0].id)[0] == OK

            walk(actors, (guardian, ranger, priest), epoch, (0., 42.5))

            def draw_boss():
                if not ranger.state["hp"]:
                    return True
                boss_position = ranger.state["boss_position"]
                target = (0., 46.8) if ranger.position[1] < 43.15 else boss_position
                ranger.move_towards(epoch, target)
                if ranger.position[1] >= 43.1 and ranger.state["attack"] == 0:
                    ranger.act(1)
                return False

            wait_for(actors, epoch, 20, draw_boss, "Ranger never went down")
            boss_run = ranger.state["boss_run"]
            assert ranger.state["boss_hp"] < 330
            fallen = ranger.position
            assert priest.position[1] < 43 and dist(priest.position, fallen) < 9.
            evidence.update(downed=True, downed_position=fallen, boss_run=boss_run,
                            hp_at_downed=ranger.state["boss_hp"], downed_tick=ranger.tick)

            # 角色在同一場首領戰中斷線。保留既有隊伍、倒地位置及戰利品資格。
            old_ranger = ranger.id
            ranger_profile = profiles[1]
            ranger_profile.peer.close()
            peers = (guardian, priest, observer)
            departure = None
            deadline = time.monotonic() + 4
            while time.monotonic() < deadline:
                actors_read(peers, epoch)
                departure = profiles[0].party(PARTY_VIEW)
                if old_ranger not in departure[4]:
                    break
            assert departure and departure[3] == 3 and set(departure[4]) == {guardian.id, priest.id}, departure
            evidence.update(old_connection=old_ranger, offline_party_count=departure[3])
            def rejoin_ranger():
                profiles[1] = Player(port, epoch, sockets, 2, ranger_profile.ticket)
                ranger = Actor(profiles[1].peer, profiles[1].id, 2)
                actors_read((ranger,), epoch)
                restore = profiles[1].party(PARTY_VIEW)
                assert ranger.id != old_ranger and restore[1] == guardian.id and restore[3] == 3
                assert set(restore[4]) == {guardian.id, ranger.id, priest.id} and restore[4][ranger.id] == (2, 0)
                assert ranger.state["boss_run"] == boss_run and ranger.state["hp"] == 0 and ranger.position == fallen, (
                    ranger.state, ranger.position, fallen)
                evidence.update(new_connection=ranger.id,
                                restored_downed_hp=ranger.state["hp"], restored_downed_position=ranger.position,
                                same_boss_encounter=True)
                return ranger

            if not offline_until_victory:
                ranger = rejoin_ranger()
                peers = (guardian, ranger, priest, observer)

            def clear_boss():
                if guardian.state["boss_phase"] == DEFEATED:
                    return True
                if not offline_until_victory:
                    assert ranger.state["hp"] == 0 and ranger.position == fallen
                assert guardian.state["boss_run"] == boss_run and guardian.state["hp"] > 0
                position = guardian.state["boss_position"]
                strike = guardian.state["strike"]
                evading = (guardian.state["boss_phase"] == 2 and guardian.state["phase_ticks"] <= 16
                           and dist(guardian.position, strike) < 2.65)
                if evading:
                    escape = (strike[0] + 3.1, strike[1]) if strike[0] < 3. else (strike[0] - 3.1, strike[1])
                    guardian.move_towards(epoch, escape)
                elif dist(guardian.position, position) > 2.05:
                    guardian.move_towards(epoch, position)
                if dist(guardian.position, position) < 5.8 and guardian.state["skill"] == 0 and guardian.state["resource"] >= 25:
                    guardian.act(2)
                elif dist(guardian.position, position) < 2.55 and guardian.state["attack"] == 0:
                    guardian.act(1)
                priest.move_towards(epoch, (-3., 44.))
                if priest.state["attack"] == 0 and priest.position[1] >= 43.1:
                    priest.act(1)
                return False

            wait_for(peers, epoch, 37, clear_boss, "Party did not finish the same encounter")
            if offline_until_victory:
                with closing(sqlite3.connect(database)) as db:
                    rows = db.execute(
                        "SELECT character_id, world, encounter FROM riverbank_reward ORDER BY character_id"
                    ).fetchall()
                evidence.update(victory_tick_while_offline=guardian.tick, paid_before_rejoin=rows)
                assert rows == [(1, epoch, boss_run), (2, epoch, boss_run), (3, epoch, boss_run)], rows
                ranger = rejoin_ranger()
            actors = (guardian, ranger, priest, observer)
            actors_read(actors, epoch)
            rewards = {a.id: a.state["loot"] for a in actors}
            assert rewards == {guardian.id: 1, ranger.id: 1, priest.id: 1, observer.id: 0}, rewards
            assert priest.state["resource"] == 100 and priest.state["skill"] == 0
            assert ranger.position == fallen and ranger.state["hp"] == 0
            evidence.update(victory=True, victory_tick=ranger.tick, rewards=rewards,
                            downed_at_victory=True, priest_position=priest.position)
            priest.act(2)

            wait_for(actors, epoch, 2.5, lambda: ranger.state["hp"] > 0,
                     "Priest could not revive a downed party member during the victory phase")
            assert ranger.state["hp"] == PRIEST_RECOVERY and ranger.position == fallen
            assert ranger.state["boss_phase"] == DEFEATED and ranger.state["boss_run"] == boss_run
            assert 0 < priest.state["skill"] <= 120 and 60 <= priest.state["resource"] < 100
            evidence.update(revived_tick=ranger.tick, revived_hp=ranger.state["hp"],
                            priest_resource=priest.state["resource"], priest_cooldown_ticks=priest.state["skill"])
            def move_recovered_player():
                if ranger.position[0] > fallen[0] + .24:
                    return True
                ranger.move_towards(epoch, (fallen[0] + 1.2, fallen[1]))
                return False

            wait_for(actors, epoch, 2.0, move_recovered_player, "Revived player could not move")
            evidence["after_movement"] = ranger.position

            def observe_reset():
                assert {a.id: a.state["loot"] for a in actors} == rewards
                if ranger.state["boss_run"] == boss_run:
                    return False
                return ranger.state["boss_hp"] == 330

            wait_for(actors, epoch, 14., observe_reset, "Boss did not reset to full health")
        with closing(sqlite3.connect(database)) as db:
            rows = db.execute("SELECT character_id, world, encounter FROM riverbank_reward ORDER BY character_id").fetchall()
            assert rows == [(1, epoch, boss_run), (2, epoch, boss_run), (3, epoch, boss_run)], rows
        if offline_until_victory:
            with start(server, database) as (port, restarted), ExitStack() as sockets:
                assert restarted != epoch
                recovered = GearPlayer(port, restarted, sockets, 2, ranger_profile.ticket)
                spectator = GearPlayer(port, restarted, sockets, 2, profiles[3].ticket)
                assert recovered.gear(GEAR_VIEW) == (OK, True, False)
                assert recovered.gear(EQUIP) == (OK, True, True)
                assert spectator.gear(GEAR_VIEW) == (OK, False, False)
            with closing(sqlite3.connect(database)) as db:
                assert db.execute(
                    "SELECT character_id, world, encounter FROM riverbank_reward ORDER BY character_id"
                ).fetchall() == rows
            evidence.update(offline_recipient_gear_equip_after_restart=True, restarted_world=restarted)
        stage = ("RIVERBANK_OFFLINE_BOSS_CREDIT_VERIFIED" if offline_until_victory
                 else "RIVERBANK_POST_VICTORY_RESCUE_VERIFIED")
        evidence.update(stage=stage, clients=4, physical_hosts=1, matching_reward_rows=rows, boss_reset=True)
    except Exception as error:
        evidence.update(stage="FIRST_DIVERGENCE", difference=str(error))
        raise
    finally:
        path = root / "BossRescueVerification.json"
        path.write_text(json.dumps(evidence, indent=2) + "\n", encoding="utf-8")
        print(evidence["stage"], path, flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--offline-until-victory", action="store_true")
    arguments = parser.parse_args()
    verify(arguments.server.resolve(strict=True), arguments.output.resolve(strict=True), arguments.offline_until_victory)
