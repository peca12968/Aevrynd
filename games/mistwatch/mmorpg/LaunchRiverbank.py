"""以明確指定的建置檔啟動河岸伺服器與本機遊戲視窗。"""
import argparse
from contextlib import ExitStack, closing
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

from VerifyRiverbankProfiles import login_request, login_response
from VerifyWorldServer import connect, hello, ready_line, snapshot_with_classes


def stop(process):
    if process.poll() is None:
        process.terminate()
    process.wait(timeout=5)


def character_ticket(slot):
    name = f'Slot {slot}'
    with winreg.CreateKeyEx(winreg.HKEY_CURRENT_USER, r'Software\Aevrynd\MistwatchRiverbankLocalProfiles',
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


def smoke_connection(port, epoch):
    with closing(connect(port)) as peer:
        received, player = hello(peer)
        login_request(peer, token_bytes(32), 1)
        if login_response(peer) != 1:
            raise RuntimeError('無法建立測試角色')
        tick, positions, _, roles = snapshot_with_classes(peer, received, player)
        if received != epoch or player not in positions or roles.get(player) != 1:
            raise RuntimeError('本機角色連線驗證失敗')
        return {'player': player, 'snapshot_tick': tick}


def run(args, session):
    environment = {key: value for key, value in os.environ.items() if not key.upper().startswith('AEVRYND_')}
    database = (session if args.check_server else args.data) / 'RiverbankProfiles.sqlite3'
    with ExitStack() as resources:
        diagnostics = resources.enter_context((session / 'server.log').open('xb'))
        server = subprocess.Popen([str(args.server), '0'], cwd=session, stdin=subprocess.DEVNULL,
                                  stdout=subprocess.PIPE, stderr=diagnostics, text=True,
                                  env=dict(environment, AEVRYND_MISTWATCH_PROFILE_DB=str(database)),
                                  creationflags=subprocess.CREATE_NO_WINDOW)
        resources.callback(server.stdout.close)
        resources.callback(stop, server)
        port, epoch = ready_line(server)
        report = {'port': port, 'epoch': epoch, 'server_pid': server.pid}
        if args.check_server:
            report.update(smoke_connection(port, epoch))
            print(json.dumps(report))
        else:
            games = []
            for slot in range(1, 3 if args.two else 2):
                directory = session / f'client-{slot}'
                directory.mkdir()
                log = resources.enter_context((directory / 'client.log').open('xb'))
                client_environment = dict(environment, AEVRYND_MISTWATCH_LOCAL_WORLD_PORT=str(port),
                                          AEVRYND_MISTWATCH_PROFILE_TICKET=character_ticket(slot).hex(),
                                          AEVRYND_FATAL_DIAGNOSTIC_PATH=str(directory / 'engine.txt'))
                game = subprocess.Popen([str(args.client)], cwd=directory, env=client_environment,
                                        stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
                resources.callback(stop, game)
                games.append(game)
            report['client_pids'] = [game.pid for game in games]
        (session / 'launch.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        if args.check_server:
            return
        while any(game.poll() is None for game in games):
            if server.poll() is not None or any(game.returncode for game in games):
                raise RuntimeError('多人測試程序提前結束，請查看啟動紀錄')
            time.sleep(.25)
        if any(game.returncode for game in games):
            raise RuntimeError('遊戲視窗以錯誤狀態結束')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--server', required=True, type=Path, help='MistwatchWorldServer.exe 的路徑')
    parser.add_argument('--client', required=True, type=Path, help='Aevrynd-Mistwatch-Local-World.exe 的路徑')
    parser.add_argument('--data', required=True, type=Path, help='多人存檔與啟動記錄的資料夾')
    parser.add_argument('--two', action='store_true', help='同時開啟兩個遊戲視窗')
    parser.add_argument('--check-server', action='store_true', help='以隔離存檔測試連線')
    args = parser.parse_args()
    args.server, args.client, args.data = (path.resolve() for path in (args.server, args.client, args.data))
    if not args.server.is_file() or not args.client.is_file():
        parser.error('找不到指定的伺服器或遊戲執行檔')
    sessions = args.data / 'sessions'
    sessions.mkdir(parents=True, exist_ok=True)
    session = Path(mkdtemp(prefix='riverbank-', dir=sessions))
    try:
        run(args, session)
    except (AssertionError, Empty, OSError, RuntimeError, ValueError, subprocess.SubprocessError) as error:
        (session / 'launch-error.txt').write_text(str(error) + '\n', encoding='utf-8')
        print(f'{error}\n{session}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
