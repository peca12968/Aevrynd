"""Drive opposing guilds through a real contest, defeat, capture and server restart."""
import argparse
from contextlib import ExitStack, closing
from hashlib import file_digest
import json
from pathlib import Path
import secrets
import sqlite3
from tempfile import mkdtemp
import time

from VerifyRiverbankCoopJourney import Actor, actors_read, walk
from VerifyRiverbankGuild import CREATE, INVITE, JOIN, OK, Player
from VerifyRiverbankProfiles import start
from VerifyRiverbankTerritory import EAST, INSIDE_EAST, INSIDE_WEST, LOOKOUT, territory, territory_history
from VerifyWorldServer import connect

ROLES = (1, 2, 3, 2)
WEST = (10.4, 54.)
FRIEND = (14.8, 54.6)


def verify(server, destination):
    root = Path(mkdtemp(prefix="territory-combat-", dir=destination))
    db = root / "TerritoryCombat.sqlite3"
    tickets = [secrets.token_bytes(32) for _ in ROLES]
    with server.open("rb") as stream:
        evidence = {"stage": "RUNNING", "server_sha256": file_digest(stream, "sha256").hexdigest()}
    try:
        with start(server, db) as (port, epoch), ExitStack() as stack:
            profiles = [Player(stack.enter_context(closing(connect(port))), ticket, epoch, role, True)
                        for ticket, role in zip(tickets, ROLES, strict=True)]
            founder, challenger, peer, observer = profiles
            owner_result, (north_id, *_) = founder.guild(CREATE, name="北岸守望".encode())
            attack_result, (south_id, *_) = challenger.guild(CREATE, name="河谷巡衛".encode())
            assert owner_result == attack_result == OK and north_id != south_id
            assert challenger.guild(INVITE, peer.id)[0] == OK and peer.guild(JOIN, south_id)[0] == OK
            actors = [Actor(player.peer, player.id, role) for player, role in zip(profiles, ROLES, strict=True)]
            defender, attacker, ally, witness = actors
            sequence = 0

            def inspect():
                nonlocal sequence
                sequence += 1
                return territory(observer.peer, sequence)

            def wait(predicate, seconds, stage):
                deadline = time.monotonic() + seconds
                while time.monotonic() < deadline:
                    actors_read(actors, epoch)
                    result = predicate()
                    if result:
                        return result
                raise AssertionError(stage)

            def state_when(predicate):
                state = inspect()
                return state if predicate(state) else None

            # 從戰場東側道路接近領地，避開北岸野豬造成的其他傷害。
            walk(actors, actors, epoch, (9.15, 42.))
            walk(actors, actors, epoch, WEST)
            walk(actors, (defender,), epoch, INSIDE_WEST)
            walk(actors, (attacker, ally, witness), epoch, EAST)
            claimed = wait(lambda: state_when(lambda state: state["owner"] == north_id), 18,
                           "The defenders never secured the initial claim")
            assert claimed["revision"] == 1
            walk(actors, (defender,), epoch, WEST)
            walk(actors, (attacker, ally), epoch, INSIDE_EAST)
            walk(actors, (ally,), epoch, FRIEND)
            walk(actors, (witness,), epoch, LOOKOUT)
            progressed = wait(lambda: state_when(lambda state: state["challenger"] == south_id and state["progress"] >= 24),
                              4, "The challenged claim did not begin")
            assert progressed["owner"] == north_id
            attacker.act(1)
            for _ in range(8):
                actors_read(actors, epoch)
                assert [a.state["hp"] for a in actors] == [150, 105, 100, 105]
                assert attacker.state["attack"] == 0
            evidence["outside_and_neutral_targets_protected"] = True

            walk(actors, (defender,), epoch, INSIDE_WEST)
            held = wait(lambda: state_when(lambda state: state["contested"] and state["challenger"] == south_id),
                        4, "The returning defender did not stop the capture")
            for _ in range(10):
                actors_read(actors, epoch)
            assert inspect() == held
            evidence.update(owner=north_id, challenger=south_id, held_progress=held["progress"])
            attacker.act(1)
            wait(lambda: defender.state["hp"] == 137, 2,
                 "A landed attack in the contested ring did not damage the opposing guild member")
            assert attacker.state["attack"] > 0
            assert all(a.state["hp"] == health for a, health in ((attacker, 105), (ally, 100), (witness, 105)))
            evidence["first_guild_hit"] = 13
            attacker.act(2)
            wait(lambda: defender.state["hp"] == 109, 2, "Ranger volley failed to hit the defender")
            assert attacker.state["resource"] < 100 and attacker.state["skill"] > 0

            def defeat():
                assert ally.state["hp"] == 100 and witness.state["hp"] == 105
                if not defender.state["hp"]:
                    return True
                if attacker.state["attack"] == 0:
                    attacker.act(1)
                return False

            wait(defeat, 9, "Defender did not fall after valid attacks")
            evidence["defender_downed"] = True
            after = wait(lambda: state_when(lambda state: not state["contested"] and state["progress"] > held["progress"]),
                         3, "Capture did not resume after the defender was downed")
            assert after["owner"] == north_id
            won = wait(lambda: state_when(lambda state: state["owner"] == south_id), 16,
                       "The challengers did not capture the territory")
            assert won["revision"] == 2 and won["rewards"] == 0
            evidence["progress_resumed_at"] = after["progress"]
            history = territory_history(observer.peer, 1)
            assert [row[:3] for row in history] == [(2, "北岸守望", "河谷巡衛"), (1, "", "北岸守望")]
        with start(server, db) as (port, restarted):
            assert epoch != restarted
            rewards = []
            for ticket, role in zip(tickets, ROLES, strict=True):
                with closing(connect(port)) as conn:
                    Player(conn, ticket, restarted, role)
                    state = territory(conn, 1)
                    assert (state["owner"], state["revision"], state["challenger"]) == (south_id, 2, 0)
                    assert territory_history(conn, 1) == history
                    rewards.append(state["rewards"])
            assert rewards == [1, 1, 1, 0], rewards
        with closing(sqlite3.connect(db)) as store:
            rows = store.execute("SELECT revision, character_id FROM riverbank_territory_reward ORDER BY 1, 2").fetchall()
            assert rows == [(1, 1), (2, 2), (2, 3)], rows
        evidence.update(stage="RIVERBANK_TERRITORY_COMBAT_VERIFIED", restarted=True, physical_hosts=1,
                        clients=4, rewards=rewards, reward_records=rows)
    except Exception as error:
        evidence.update(stage="FIRST_DIVERGENCE", reason=str(error))
        raise
    finally:
        receipt = root / "TerritoryCombat.json"
        receipt.write_text(json.dumps(evidence, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(evidence["stage"], receipt, flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("server", type=Path)
    parser.add_argument("destination", type=Path)
    args = parser.parse_args()
    verify(args.server.resolve(strict=True), args.destination.resolve(strict=True))
