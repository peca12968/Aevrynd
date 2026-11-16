"""使用正式啟動器連線實際伺服器，核對啟動收據與隔離存檔。"""
import json
from pathlib import Path
import subprocess
import sys
from tempfile import mkdtemp


def check_room(server, evidence_dir, capacity=None):
    data = Path(mkdtemp(prefix='launcher-receipt-', dir=evidence_dir))
    launcher = Path(__file__).with_name('LaunchRiverbank.py')
    command = [sys.executable, '-B', str(launcher), '--server', str(server), '--data', str(data), '--check-server']
    if capacity is not None:
        command.extend(('--capacity', str(capacity)))
    result = subprocess.run(command, capture_output=True, check=True, text=True, timeout=8)
    (receipt,) = (data / 'sessions').glob('riverbank-*/launch.json')
    persisted = json.loads(receipt.read_text(encoding='utf-8'))
    if persisted != json.loads(result.stdout) or persisted['room_capacity'] != (capacity or 16):
        raise AssertionError('主控台與啟動收據不一致')
    if not (0 < persisted['port'] <= 65535 and persisted['epoch'] > 0
            and persisted['server_pid'] > 0 and persisted['player'] == 1
            and persisted['snapshot_tick'] > 0):
        raise AssertionError('啟動收據缺少實際連線證明')
    if (data / 'RiverbankProfiles.sqlite3').exists() or not (receipt.parent / 'RiverbankProfiles.sqlite3').is_file():
        raise AssertionError('檢查模式未隔離角色存檔')
    if receipt.with_suffix('.json.tmp').exists():
        raise AssertionError('原子寫入留下暫存收據')
    print(f'RIVERBANK_LAUNCH_RECEIPT_VERIFIED {receipt}')


if __name__ == '__main__':
    server, evidence_dir = map(Path, sys.argv[1:])
    check_room(server, evidence_dir)
    check_room(server, evidence_dir, capacity=32)
