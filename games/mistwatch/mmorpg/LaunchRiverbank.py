"""啟動河岸多人世界，或加入同一 Tailscale 私有網路的伺服器。"""
import argparse
from contextlib import ExitStack, closing, contextmanager
import json
import os
from pathlib import Path
from queue import Empty
from secrets import token_bytes
import subprocess
import sys
from tempfile import mkdtemp
import time
import winreg

import RiverbankTailnet as tailnet
from VerifyRiverbankProfiles import login_request, login_response
from VerifyWorldServer import connect, hello, ready_line, snapshot_with_classes


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.wait(timeout=5)


def character_ticket(slot, scope=None):
    registry = (r'Software\Aevrynd\MistwatchRiverbankTailnetProfiles' + '\\' + scope if scope else
                r'Software\Aevrynd\MistwatchRiverbankLocalProfiles')
    name = f'Slot {slot}'
    with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, registry,
                            access=winreg.KEY_QUERY_VALUE | winreg.KEY_SET_VALUE) as profiles:
        try:
            ticket, kind = winreg.QueryValueEx(profiles, name)
        except FileNotFoundError:
            ticket = token_bytes(32)
            winreg.SetValueEx(profiles, name, 0, winreg.REG_BINARY, ticket)
            return ticket
        if kind != winreg.REG_BINARY or not isinstance(ticket, bytes) or len(ticket) != 32 or not any(ticket):
            raise ValueError(f'角色欄位 {slot} 的票證格式無效')
        return ticket


def smoke_connection(peer, expected_epoch=None):
    epoch, player = hello(peer)
    if expected_epoch is not None and epoch != expected_epoch:
        raise RuntimeError('連線抵達不同的河岸世界')
    login_request(peer, token_bytes(32), 1)
    if login_response(peer) != 1:
        raise RuntimeError('無法建立測試角色')
    tick, positions, _, roles = snapshot_with_classes(peer, epoch, player)
    if not tick or player not in positions or roles.get(player) != 1:
        raise RuntimeError('世界快照沒有已登入的測試角色')
    return {'epoch': epoch, 'player': player, 'snapshot_tick': tick}


@contextmanager
def launch_clients(args, session, environment, port, scope, route):
    with ExitStack() as clients:
        games = []
        for slot in range(1, 3 if args.two else 2):
            directory = session / f'client-{slot}'
            directory.mkdir()
            log = clients.enter_context((directory / 'client.log').open('xb'))
            client_environment = dict(environment, AEVRYND_MISTWATCH_LOCAL_WORLD_PORT=str(port),
                                      AEVRYND_MISTWATCH_PROFILE_TICKET=character_ticket(slot, scope).hex(),
                                      AEVRYND_FATAL_DIAGNOSTIC_PATH=str(directory / 'engine.txt'), **route)
            game = subprocess.Popen([str(args.client)], cwd=directory, env=client_environment,
                                    stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
            clients.callback(stop, game)
            games.append(game)
        yield games


def write_launch_report(session, report):
    receipt = session / 'launch.json'
    temporary = receipt.with_suffix('.json.tmp')
    temporary.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    temporary.replace(receipt)


def run(args, session):
    environment = {key: value for key, value in os.environ.items() if not key.upper().startswith('AEVRYND_')}
    database = (session if args.check_server else args.data) / 'RiverbankProfiles.sqlite3'
    local_node, server_node = tailnet.network_nodes(args.join_tailnet) if args.share_tailnet or args.join_tailnet else (None, None)
    with ExitStack() as resources:
        server = relay = None
        smoke = None
        if args.join_tailnet:
            port = args.port
            with closing(tailnet.open_peer(local_node.address, server_node.address, port)) as peer:
                if args.check_server:
                    smoke = smoke_connection(peer)
                    epoch = smoke['epoch']
                else:
                    epoch, _ = hello(peer)
        else:
            if args.share_tailnet and args.port and tailnet.port_reserved(tailnet.tailscale('serve', 'status', '--json'), args.port):
                raise RuntimeError('該連接埠已有 Tailscale 服務，請改用其他連接埠')
            diagnostics = resources.enter_context((session / 'server.log').open('xb'))
            command = [str(args.server), str(args.port)]
            if args.capacity is not None:
                command.extend(('--capacity', str(args.capacity)))
            server = subprocess.Popen(command, cwd=session, stdin=subprocess.DEVNULL,
                                      stdout=subprocess.PIPE, stderr=diagnostics, text=True,
                                      env=dict(environment, AEVRYND_MISTWATCH_PROFILE_DB=str(database)),
                                      creationflags=subprocess.CREATE_NO_WINDOW)
            resources.callback(server.stdout.close)
            resources.callback(stop, server)
            port, epoch = ready_line(server)
            if args.share_tailnet:
                if tailnet.port_reserved(tailnet.tailscale('serve', 'status', '--json'), port):
                    raise RuntimeError('該連接埠已有 Tailscale 服務，請改用其他連接埠')
                log = resources.enter_context((session / 'tailnet.log').open('xb'))
                relay = subprocess.Popen(['tailscale.exe', 'serve', f'--tcp={port}', f'tcp://127.0.0.1:{port}'],
                                         cwd=session, stdout=log, stderr=subprocess.STDOUT, stdin=subprocess.DEVNULL,
                                         creationflags=subprocess.CREATE_NO_WINDOW)
                resources.callback(stop, relay)
                tailnet.wait_for_forwarding(relay, local_node, port, epoch)

        report = {'port': port, 'epoch': epoch, 'server_pid': server.pid if server else None}
        if server:
            report['room_capacity'] = args.capacity or 16
        if server_node:
            report['tailnet_address'] = server_node.address
        if args.join_tailnet and args.check_server:
            report['separate_tailnet_device'] = local_node.profile_scope != server_node.profile_scope
        if args.check_server:
            if server:
                connection = tailnet.open_peer(local_node.address, local_node.address, port) if relay else connect(port)
                with closing(connection) as peer:
                    smoke = smoke_connection(peer, epoch)
            report.update(smoke)
            print(json.dumps(report), flush=True)
            write_launch_report(session, report)
            return
        route = ({'AEVRYND_MISTWATCH_TAILNET_LOCAL': local_node.address,
                  'AEVRYND_MISTWATCH_TAILNET_REMOTE': server_node.address} if args.join_tailnet else {})
        scope = server_node.profile_scope if server_node else None
        games = resources.enter_context(launch_clients(args, session, environment, port, scope, route))
        report['client_pids'] = [game.pid for game in games]
        write_launch_report(session, report)
        if args.share_tailnet:
            print(json.dumps(report), flush=True)
        while any(game.poll() is None for game in games):
            if ((server and server.poll() is not None) or (relay and relay.poll() is not None) or
                    any(game.returncode for game in games)):
                raise RuntimeError('多人測試程序提前結束，請查看啟動紀錄')
            time.sleep(.25)
        if any(game.returncode for game in games):
            raise RuntimeError('遊戲視窗以錯誤狀態結束')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', type=Path, help='MistwatchWorldServer.exe 的路徑')
    parser.add_argument('--client', type=Path, help='Aevrynd-Mistwatch-Local-World.exe 的路徑')
    parser.add_argument('--data', required=True, type=Path, help='多人存檔與啟動記錄的資料夾')
    parser.add_argument('--two', action='store_true', help='同時開啟兩個遊戲視窗')
    parser.add_argument('--check-server', action='store_true', help='測試登入及快照。加入私網檢查時請使用專用測試房間')
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--share-tailnet', action='store_true', help='遊戲執行時在現有 Tailscale 私有網路分享')
    mode.add_argument('--join-tailnet', type=tailnet.address, metavar='IP', help='加入這個 Tailscale IPv4 位址的河岸世界')
    parser.add_argument('--port', type=int, default=0, help='連接埠。主機使用 0 自動選擇，加入時填入主機提供的連接埠')
    parser.add_argument('--capacity', type=int, choices=range(1, 33), metavar='1..32',
                        help='開房人數上限，預設 16')
    args = parser.parse_args()
    if args.capacity is not None and args.join_tailnet:
        parser.error('房間容量由開房者設定')
    if not 0 <= args.port <= 65535 or (args.join_tailnet and (not args.port or args.server)):
        parser.error('加入私網時請提供有效連接埠，主機執行檔由開房者指定')
    args.data = args.data.resolve()
    args.server = args.server.resolve() if args.server else None
    args.client = args.client.resolve() if args.client else None
    if not args.join_tailnet and (not args.server or not args.server.is_file()):
        parser.error('找不到指定的伺服器執行檔')
    if (args.client and not args.client.is_file()) or (not args.client and not args.check_server):
        parser.error('找不到指定的遊戲執行檔')
    sessions = args.data / 'sessions'
    sessions.mkdir(parents=True, exist_ok=True)
    session = Path(mkdtemp(prefix='riverbank-', dir=sessions))
    try:
        run(args, session)
    except (AssertionError, Empty, EOFError, OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        (session / 'launch-error.txt').write_text(str(error) + '\n', encoding='utf-8')
        print(f'{error}\n{session}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
