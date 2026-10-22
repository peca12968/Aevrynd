"""Strict, read-only qualification of a same-executable water component capture pair.

This is a diagnostic comparator, never a realistic-water quality/energy score.
Requires existing full mission/readback receipts, exact actual raster and source
geometry. The report is new; existing files are never overwritten.
"""
from pathlib import Path
from datetime import datetime, timezone
import argparse, hashlib, json, math, struct
from PIL import Image

SOURCE_FILES = (
    'bearing-bridge-source.tsv', 'bearing-world-after.f32',
    'mature-world-before.f32', 'mature-world-after.f32',
    'riverbank-guards.tsv', 'riverbank-heightfield.tsv',
    'storm-river-current-actual-mesh.bin', 'storm-river-old-actual-mesh.bin',
    'indexed-gameplay-commits.tsv',
)
REGIONS = {
    'protected_top_forest_hud': (0, 0, 1280, 280),
    'protected_lower_terrain_actor': (0, 430, 1280, 720),
    'left_water_interior': (30, 355, 450, 370),
    'right_water_interior': (1105, 312, 1270, 335),
}

def pin(path):
    path = Path(path)
    raw = path.read_bytes()
    return dict(path=path.resolve().as_posix(), bytes=len(raw),
                sha256=hashlib.sha256(raw).hexdigest().upper())

def fields(path):
    rows = Path(path).read_text(encoding='utf-8-sig').splitlines()
    result = {}
    for row in rows:
        if not row:
            continue
        key, value = row.split('=', 1)
        if key in result:
            raise ValueError('Duplicate capture metadata key: ' + key)
        result[key] = value
    return result

def raster_equal(left, right):
    required = ('uploaded_vp_bits', 'uploaded_unjittered_vp_bits', 'jitter', 'window')
    def valid(row):
        try:
            for key in required[:2]:
                values = row[key].rstrip(',').split(',')
                if len(values) != 16:
                    return False
                for value in values:
                    bits = int(value)
                    if not 0 <= bits <= 0xffffffff or not math.isfinite(struct.unpack('<f', struct.pack('<I', bits))[0]):
                        return False
            for key, count in (('jitter', 2), ('window', 8)):
                values = row[key].rstrip(',').split(',')
                if len(values) != count or not all(math.isfinite(float(value)) for value in values):
                    return False
            return True
        except (KeyError, ValueError, OverflowError):
            return False
    if not valid(left) or not valid(right):
        return False
    return all(left[key] == right[key] for key in required)

def qualify(baseline, candidate):
    a, b = Path(baseline).resolve(), Path(candidate).resolve()
    failures, rasters, source = [], [], []
    receipts = []
    for root in (a, b):
        receipt = json.loads((root.parent / 'receipt.json').read_bytes())
        process = receipt['process']
        last = process['job_drain']['observations'][-1]
        valid = (process['terminal'] and process['exit_code'] == 0
                 and not process['timeout'] and not process['own_job_terminated']
                 and process['job_drain']['settled'] and not last['job_pids']
                 and last['active_processes'] == last['assigned_processes'] == 0)
        if not valid:
            failures.append('NATIVE_JOB_NOT_NATURALLY_COMPLETE')
        readback = json.loads((root.parent / 'readback.json').read_bytes())
        if readback['pngs_fully_decoded'] != 229:
            failures.append('FULL_MISSION_FRAME_READBACK_MISSING')
        receipts.append(receipt)
    same_executable = receipts[0]['game']['sha256'] == receipts[1]['game']['sha256']
    frames_a = {p.name for p in a.glob('*.png')}
    frames_b = {p.name for p in b.glob('*.png')}
    if frames_a != frames_b or len(frames_a) != 229:
        failures.append('FULL_FRAME_SET_MISMATCH')
    for name in sorted(frames_a & frames_b):
        left = fields(Path(str(a / name) + '.uploaded-view.txt'))
        right = fields(Path(str(b / name) + '.uploaded-view.txt'))
        rasters.append(dict(file=name, actual_raster_equal=raster_equal(left, right),
                            unjittered_equal=left['uploaded_unjittered_vp_bits'] == right['uploaded_unjittered_vp_bits'],
                            before_jitter=left['jitter'], after_jitter=right['jitter']))
    if not rasters or not all(row['actual_raster_equal'] for row in rasters):
        failures.insert(0, 'ACTUAL_UPLOADED_RASTER_MISMATCH')
    if not same_executable:
        failures.append('EXECUTABLE_CONTENT_MISMATCH')
    for name in SOURCE_FILES:
        left, right = pin(a / name), pin(b / name)
        equal = left['sha256'] == right['sha256'] and left['bytes'] == right['bytes']
        source.append(dict(file=name, equal=equal, baseline=left, candidate=right))
        if not equal:
            failures.append('GEOMETRY_OR_PHYSICS_SOURCE_MISMATCH:' + name)
    name = '48-repair-bridge-verify.png'
    with Image.open(a / name) as im:
        left = im.convert('RGB'); left.load()
    with Image.open(b / name) as im:
        right = im.convert('RGB'); right.load()
    if left.size != right.size or left.size != (1280, 720):
        raise ValueError('Expected complete native1280x720 controls, not resized pictures')
    regions = {}
    for label, box in REGIONS.items():
        ac = list(left.crop(box).getdata()); bc = list(right.crop(box).getdata())
        changed = sum(x != y for x, y in zip(ac, bc))
        region = dict(box=box, pixels=len(bc), changed_pixels=changed,
                      maximum_channel_difference=max(abs(x[k]-y[k]) for x, y in zip(ac, bc) for k in range(3)),
                      postprocessed_diagnostic_means_rgb=[sum(p[k] for p in bc)/len(bc) for k in range(3)],
                      green_dominant_pixels=sum(p[1] > p[0] and p[1] > p[2] for p in bc))
        regions[label] = region
        if label.startswith('protected_') and changed:
            failures.append('NONWATER_PIXEL_CONTROL_MISMATCH:' + label)
        if label.endswith('interior') and region['green_dominant_pixels'] < len(bc)*.95:
            failures.append('SCOPED_REFLECTION_SIGNAL_NOT_DOMINANT:' + label)
    return dict(schema='aevrynd-water-component-pair-v1',
                status='QUALIFIED_SCOPED_WATER_COMPONENT_COMPARISON' if not failures else 'UNQUALIFIED_CAPTURE_PAIR',
                first_divergence=failures[0] if failures else None, failures=failures,
                recorded_utc=datetime.now(timezone.utc).isoformat(),
                same_executable=same_executable, actual_raster_equal_frames=sum(row['actual_raster_equal'] for row in rasters),
                frames=len(rasters), rasters=rasters, source=source, regions=regions,
                baseline=pin(a / name), diagnostic=pin(b / name),
                scope='Exact same executable, actual uploaded VP/jitter/window, source geometry/physics and scoped non-water pixels. RGB after postprocess are diagnostic signals, not linear energy fractions, appearance acceptance, a visual score or complete production TAA equivalence.')

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    if args.out.exists():
        parser.error('Report already exists; preserve it and use a distinct explicit report path')
    report = qualify(args.baseline, args.candidate)
    report['reader'] = pin(Path(__file__))
    args.out.write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({k: report[k] for k in ('status', 'first_divergence', 'same_executable', 'actual_raster_equal_frames', 'frames')}))
    return 0 if not report['failures'] else 20

if __name__ == '__main__':
    raise SystemExit(main())
