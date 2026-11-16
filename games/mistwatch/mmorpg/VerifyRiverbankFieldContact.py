"""重現守衛直走撞上活野豬，再從接觸面後退。"""
from contextlib import ExitStack
from hashlib import file_digest
import json
from math import dist
from pathlib import Path
import sys
from tempfile import mkdtemp

from DriveRiverbankParty import Player
from VerifyRiverbankCoopJourney import actors_read, walk
from VerifyRiverbankFieldHunt import Hunter
from VerifyRiverbankProfiles import start


def verify(server, work):
    output = Path(mkdtemp(prefix='field-contact-', dir=work))
    closest = float('inf')
    warnings = 0
    with start(server, output / 'Profiles.sqlite3') as (port, epoch), ExitStack() as sockets:
        profile = Hunter(port, epoch, sockets, 1)
        guardian = Player(profile.peer, profile.id, 1)
        walk([guardian], [guardian], epoch, (10.7, 15.0))
        for _ in range(65):
            actors_read([guardian], epoch)
            field = profile.field()
            if field['hp'] != 84:
                raise AssertionError('接觸測試需要一直存活的野豬')
            closest = min(closest, dist(guardian.position, field['position']))
            warnings += field['phase'] == 2
            guardian.move_towards(epoch, field['position'])
        if not 1.74 <= closest <= 1.83 or not warnings:
            raise AssertionError(f'接觸距離或野豬蓄力不符，最近={closest:.5f} 公尺，警告樣本={warnings}')

        before = guardian.position
        walk([guardian], [guardian], epoch, (before[0] - 2.5, before[1]))
        after = guardian.position
        if not after[0] < before[0] - 1.8:
            raise AssertionError(f'接觸後無法後退：{before} → {after}')
    with server.open('rb') as stream:
        identity = file_digest(stream, 'sha256').hexdigest()
    report = dict(status='RIVERBANK_FIELD_CONTACT_PASSED', minimum_separation_m=closest,
                  warning_samples=warnings, retreat_from=before, retreat_to=after, server_sha256=identity)
    path = output / 'FieldContactVerification.json'
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    print(path, flush=True)


if __name__ == '__main__':
    verify(Path(sys.argv[1]).resolve(), Path(sys.argv[2]).resolve())
