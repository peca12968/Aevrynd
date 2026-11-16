"""驗證草坡倒地、營地恢復與原連線狀態。"""
from contextlib import ExitStack, closing
from hashlib import sha256
import json
from pathlib import Path
import sys
from tempfile import mkdtemp
import time

from DriveRiverbankParty import Player as Actor
import VerifyRiverbankBattle as battle
from VerifyRiverbankProfiles import login_request, login_response, start
from VerifyWorldServer import ENCOUNTER_FRAME, connect, hello, packet, snapshot_with_flags
from secrets import token_bytes

RETURN_TO_CAMP = 3
DOWNED_ENTITY_FLAG = 1 << 19


def observe_stance(peer, epoch, observer, target, expected, minimum_tick):
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        tick, positions, _, flags = snapshot_with_flags(peer, epoch, observer)
        assert packet(peer)[0] == ENCOUNTER_FRAME
        if tick >= minimum_tick and target in flags:
            assert flags[target] & (1 << 18), '復歸功能旗標遺失'
            assert (flags[target] >> 8) & 255 == 2, '遊俠職業旗標遺失'
            assert bool(flags[target] & DOWNED_ENTITY_FLAG) == expected, '旁觀者收到錯誤的倒地狀態'
            return positions[target]
    raise AssertionError('旁觀者未能觀測到角色狀態')


def verify(server, output, baseline=False):
    root = Path(mkdtemp(prefix='camp-recovery-', dir=output))
    with start(server, root / 'Profiles.sqlite3') as (port, epoch), ExitStack() as sockets:
        peer = sockets.enter_context(closing(connect(port)))
        session, actor_id = hello(peer)
        assert session == epoch
        login_request(peer, token_bytes(32), 2)
        assert login_response(peer) == 2
        player = Actor(peer, actor_id, 2)
        if not baseline:
            observer = sockets.enter_context(closing(connect(port)))
            observed_epoch, observer_id = hello(observer)
            assert observed_epoch == epoch
            login_request(observer, token_bytes(32), 1)
            assert login_response(observer) == 1
            assert observe_stance(observer, epoch, observer_id, actor_id, False, 0) == (0., 0.)
        deadline = time.monotonic() + 37
        sent_alive = False
        while time.monotonic() < deadline:
            player.poll(epoch)
            if not player.state['hp']:
                break
            if not baseline and not sent_alive and player.position[1] > 6:
                player.act(RETURN_TO_CAMP)
                sent_alive = True
                next_tick = player.tick + 5
                while player.tick < next_tick:
                    player.poll(epoch)
                    assert player.position[1] > 5 and player.state['hp'] == 105, '活著的角色被送回營地'
            player.move_towards(epoch, (15., 15.))
        else:
            raise RuntimeError(f'草坡未讓測試角色倒地：{player.position} {player.state}')

        defeated = player.position
        original_loot = player.state['loot']
        if not baseline:
            assert observe_stance(observer, epoch, observer_id, actor_id, True, player.tick + 1) == defeated
        for i in range(28):
            player.poll(epoch)
            assert player.state['hp'] == 0 and player.position == defeated, '倒地角色在營地指令前移動或恢復'
            if i < 6:
                player.movement_sequence += 1
                battle.send_movement(peer, epoch, player.tick, player.movement_sequence, actor_id, 32767, 0)
        receipt = {'server_sha256': sha256(server.read_bytes()).hexdigest(), 'downed_position_m': defeated,
                   'stayed_downed_without_priest': True, 'same_connection': actor_id}
        if baseline:
            receipt['status'] = 'PREVIOUS_BUILD_REPRODUCED_STUCK_WHEN_ALONE'
        else:
            assert sent_alive
            player.act(RETURN_TO_CAMP)
            returned = None
            for _ in range(20):
                player.poll(epoch)
                if player.state['hp']:
                    returned = player.position
                    break
            assert returned == (0., 0.) and player.state['hp'] == 105, '倒地角色無法在營地恢復'
            assert player.state['loot'] == original_loot
            assert observe_stance(observer, epoch, observer_id, actor_id, False, player.tick) == returned
            player.move_towards(epoch, (1., 0.))
            for _ in range(10):
                player.poll(epoch)
                if player.position[0] >= .1:
                    break
            assert player.position[0] >= .1, '營地恢復後仍無法操作'
            battle.action(peer, player.action_sequence, RETURN_TO_CAMP)
            for _ in range(5):
                player.poll(epoch)
                assert player.position[0] >= .1 and player.state['hp'] == 105, '已處理的操作重播'
            receipt.update(status='RIVERBANK_CAMP_RECOVERY_VERIFIED', recovered_position_m=returned,
                           after_movement_m=player.position, role_preserved=player.state['role'] == 2,
                           loot_preserved=player.state['loot'] == original_loot,
                           alive_return_rejected=True, replay_rejected=True,
                           independent_observer_saw_standing_downed_and_recovered=True)
    path = root / 'CampRecoveryVerification.json'
    path.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
    print(receipt['status'], path)


if __name__ == '__main__':
    server, output = (Path(value).resolve(strict=True) for value in sys.argv[1:3])
    verify(server, output, '--baseline' in sys.argv[3:])
