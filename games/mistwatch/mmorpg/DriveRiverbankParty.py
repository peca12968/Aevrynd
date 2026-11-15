"""Drive the same four connections against a transient or a profile-backed battle server."""

from contextlib import ExitStack, closing
from dataclasses import dataclass, field
import json
from math import dist, hypot
import os
from pathlib import Path
from socket import socket
import subprocess
import sys
import time

import VerifyRiverbankBattle as battle
import VerifyWorldServer as wire


@dataclass
class Player:
    peer: socket
    id: int
    role: int
    tick: int = 0
    position: tuple[float, float] = (0.0, 0.0)
    state: dict = field(default_factory=dict)
    movement_sequence: int = 0
    action_sequence: int = 0

    def poll(self, epoch):
        self.tick, positions, self.state = battle.read(self.peer, epoch, self.id)
        self.position = positions[self.id]
        if self.state['role'] != self.role:
            raise RuntimeError(f'Player {self.id} received the wrong class')

    def move_towards(self, epoch, target):
        x, z = target[0] - self.position[0], target[1] - self.position[1]
        length = hypot(x, z)
        if length <= .19 or not self.state['hp']:
            return
        scale = min(1.0, length * 4.0) * 32767 / length
        self.movement_sequence += 1
        battle.send_movement(self.peer, epoch, self.tick, self.movement_sequence, self.id, round(x * scale), round(z * scale))

    def act(self, kind):
        self.action_sequence += 1
        battle.action(self.peer, self.action_sequence, kind)


def play_encounter(epoch, guardian, ranger, priest, observer):
    players = guardian, ranger, priest, observer
    expected_loot = {guardian.id: 1, ranger.id: 1, priest.id: 1, observer.id: 0}
    started = time.monotonic()
    while time.monotonic() - started < 51:
        for player in players:
            player.poll(epoch)
        tick = max(player.tick for player in players)
        for player in players:
            while player.tick < tick:
                player.poll(epoch)
        if any(len(player.state['roles']) != len(players) for player in players):
            continue
        if all(player.state['boss_phase'] == 4 for player in players):
            observed_runs = {player.state['boss_run'] for player in players}
            observed_loot = {player.id: player.state['loot'] for player in players}
            if len(observed_runs) != 1 or observed_loot != expected_loot or any(player.state['boss_hp'] for player in players):
                raise RuntimeError(f'Victory did not award the three eligible players exactly once: {observed_loot}')
            encounter = observed_runs.pop()
            battle.action(ranger.peer, ranger.action_sequence, 1)
            for _ in range(8):
                for player in players:
                    player.poll(epoch)
                    if player.state['boss_phase'] != 4 or player.state['loot'] != expected_loot[player.id]:
                        raise RuntimeError('A repeated action after victory changed the loot')
            return dict(encounter=encounter, players=observed_loot, ticks=[player.tick for player in players],
                        seconds=round(time.monotonic() - started, 2), reward_repeat_rejected=True)

        boss = guardian.state['boss_position']
        if dist(guardian.position, boss) > 2.2:
            guardian.move_towards(epoch, boss)
        distance = dist(guardian.position, boss)
        guard = guardian.state
        if guard['hp']:
            if distance <= 5.6 and guard['resource'] >= 25 and guard['skill'] <= 1:
                guardian.act(2)
            elif distance <= 2.55 and guard['attack'] <= 1:
                guardian.act(1)

        for player, lane, reach in ((ranger, 4.0, 13.0), (priest, -4.0, 9.0)):
            state = player.state
            player.move_towards(epoch, (lane, 44.0))
            if not state['hp']:
                continue
            in_range = dist(player.position, state['boss_position']) <= reach - .15
            if player is ranger and in_range and state['skill'] <= 1 and state['resource'] >= 32:
                player.act(2)
            elif player is priest and guard['hp'] <= 108 and dist(player.position, guardian.position) <= 8.5 and state['skill'] <= 1 and state['resource'] >= 40:
                player.act(2)
            elif in_range and state['attack'] <= 1:
                player.act(1)
    positions = {player.id: {'position': player.position, 'hp': player.state['hp'], 'loot': player.state['loot']} for player in players}
    raise RuntimeError(f'Party did not defeat the boss in 51 seconds: {positions}; boss HP={guardian.state["boss_hp"]}')


def verify_transient(server, receipt):
    env = {key: value for key, value in os.environ.items() if key.upper() != 'AEVRYND_MISTWATCH_PROFILE_DB'}
    with subprocess.Popen([str(server), '0'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True,
                          creationflags=subprocess.CREATE_NO_WINDOW, env=env) as process:
        try:
            port, epoch = wire.ready_line(process)
            with ExitStack() as stack:
                players = []
                for role in (1, 2, 3, 2):
                    peer = stack.enter_context(closing(wire.connect(port)))
                    session, player = wire.hello(peer)
                    if session != epoch:
                        raise RuntimeError('Test client joined the wrong session')
                    wire.choose(peer, role)
                    players.append(Player(peer, player, role))
                result = play_encounter(epoch, *players)
        finally:
            process.terminate()
            process.wait(timeout=5)
    result.update(server_sha256=wire.sha256(server.read_bytes()).hexdigest(), profile_database=False)
    with receipt.open('x', encoding='utf-8') as output:
        output.write(json.dumps(result, indent=2) + '\n')
    print(json.dumps(result))


if __name__ == '__main__':
    verify_transient(Path(sys.argv[1]).resolve(strict=True), Path(sys.argv[2]))
