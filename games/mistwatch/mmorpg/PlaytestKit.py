"""Launch the Riverbank playtest from the extracted kit directory."""
import argparse
from hashlib import file_digest
from ipaddress import IPv4Address
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys

from RiverbankTailnet import address, network_nodes


ROOT = Path(__file__).resolve().parents[1]
CLIENT = ROOT / 'Client' / 'Aevrynd-Mistwatch-Local-World.exe'
SERVER = ROOT / 'Server' / 'MistwatchWorldServer.exe'
LAUNCHER = Path(__file__).with_name('LaunchRiverbank.py')
DATA = ROOT / 'Profiles'
CHOICES = {'1': ('local', '本機單人'), '2': ('two', '同機雙人'), '3': ('host', '建立私網房間'),
           '4': ('join', '加入私網房間'), '5': ('verify', '核對套件檔案'), '0': ('quit', '離開')}


def verify():
    manifest = json.loads((ROOT / 'PackageManifest.json').read_text(encoding='utf-8'))
    seen = set()
    for entry in manifest['files']:
        relative = PurePosixPath(entry['path'])
        if relative.is_absolute() or '..' in relative.parts or relative in seen:
            raise ValueError(f'套件檔案清單無效 {relative}')
        seen.add(relative)
        path = (ROOT / relative).resolve(strict=True)
        if not path.is_relative_to(ROOT) or path.stat().st_size != entry['bytes']:
            raise ValueError(f'套件檔案大小不符 {relative}')
        with path.open('rb') as stream:
            if file_digest(stream, 'sha256').hexdigest() != entry['sha256']:
                raise ValueError(f'套件檔案雜湊不符 {relative}')
    if any(PurePosixPath(name) not in seen for name in
           ('Client/Aevrynd-Mistwatch-Local-World.exe', 'Client/character/RiverbankExplorer.aeskin',
            'Server/MistwatchWorldServer.exe', 'Launcher/LaunchRiverbank.py')):
        raise ValueError('套件清單缺少啟動檔')
    print(f'套件完整性通過，共 {len(seen)} 個檔案。', flush=True)


def port_number(value):
    number = int(value)
    if not 1 <= number <= 65535:
        raise ValueError('連接埠須介於 1 和 65535')
    return number


def room_capacity(value):
    try:
        capacity = int(value)
    except ValueError:
        raise argparse.ArgumentTypeError('房間人數上限須為 1 到 32 的整數') from None
    if not 1 <= capacity <= 32:
        raise argparse.ArgumentTypeError('房間人數上限須為 1 到 32 的整數')
    return capacity


def launch(mode, host=None, port=None, capacity=None):
    if not CLIENT.is_file() or (mode != 'join' and not SERVER.is_file()):
        raise FileNotFoundError('找不到遊戲執行檔，請先解壓縮整個套件')
    command = [sys.executable, '-B', str(LAUNCHER), '--client', str(CLIENT), '--data', str(DATA)]
    if mode == 'join':
        host = address(str(IPv4Address(host)))
        command.extend(('--join-tailnet', host, '--port', str(port_number(port))))
    else:
        command.extend(('--server', str(SERVER)))
    if capacity is not None:
        command.extend(('--capacity', str(capacity)))
    if mode == 'two':
        command.append('--two')
    if mode in {'check', 'check-private'}:
        command.append('--check-server')
    if mode in {'host', 'check-private'}:
        node, _ = network_nodes()
        command.append('--share-tailnet')
        # 房主在私網看到實際連接埠後，才能把位址與埠交給另一位玩家。
        with subprocess.Popen(command, stdout=subprocess.PIPE, text=True,
                              encoding='utf-8', errors='replace') as session:
            for line in session.stdout:
                report = json.loads(line)
                print(f'房間位址 {node.address}    連接埠 {report["port"]}    人數上限 {report["room_capacity"]}', flush=True)
            return session.wait()
    return subprocess.call(command)


def menu():
    for key, (_, label) in CHOICES.items():
        print(f'{key}  {label}')
    mode = CHOICES.get(input('選擇操作 > ').strip())
    if mode is None:
        raise ValueError('請輸入選單上列出的數字')
    operation, _ = mode
    if operation == 'quit':
        return 0
    if operation == 'join':
        return launch(operation, input('開房者的 Tailscale IPv4 位址 > ').strip(),
                      input('房間連接埠 > ').strip())
    if operation == 'verify':
        verify()
        return 0
    if operation == 'host':
        value = input('房間人數上限 1–32，按 Enter 使用 16 > ').strip()
        return launch(operation, capacity=room_capacity(value) if value else None)
    return launch(operation)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', nargs='?', choices=('local', 'two', 'host', 'join', 'verify', 'check', 'check-private'))
    parser.add_argument('--address')
    parser.add_argument('--port')
    parser.add_argument('--capacity', type=room_capacity, metavar='1..32')
    args = parser.parse_args()
    if args.mode == 'join' and (not args.address or not args.port):
        parser.error('加入私網房間需要 --address 與 --port')
    if args.capacity is not None and args.mode not in {'host', 'check', 'check-private'}:
        parser.error('房間人數上限只能由開房者設定')
    try:
        if args.mode == 'verify':
            verify()
            return 0
        return menu() if args.mode is None else launch(args.mode, args.address, args.port, args.capacity)
    except (OSError, EOFError, ValueError, RuntimeError, argparse.ArgumentTypeError, subprocess.SubprocessError) as error:
        print(f'河岸試玩無法啟動或驗證。{error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
