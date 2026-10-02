"""分析固定場景單株草的 on/off/repeat PNG 證據，無須執行渲染器。"""

import argparse
import hashlib
import json
import math
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image


ROI = (0, 76, 1280, 597)
MAX_REPEAT_NOISE = 8
CASE_PATTERN = re.compile(r"image-(\d+(?:\.\d+)?)-(on|off|repeat)$")
MATCH_FIELDS = (
    "grass_fixture_index", "grass_fixture_variant", "grass_fixture_root",
    "grass_fixture_scale", "grass_fixture_yaw", "camera_position",
    "camera_target", "camera_up", "camera_fov", "water_phase_seconds",
    "grass_fixture_player_distance",
)
REQUIRED_FLAGS = {
    "multisample_count": 4,
    "quality_shadows": 1, "quality_ao": 1, "quality_post_aa": 1,
    "ground_textured": 1, "diagnostic_no_shadows": 0,
    "diagnostic_no_msaa": 0, "diagnostic_no_bloom": 0,
    "diagnostic_hdr_stage": 0, "debug_errors": 0, "debug_warnings": 0,
    "grass_fixture_gate_selected": 1,
}
EXPLANATION = (
    "Changed pixels measure the total RGB image contribution of toggling the "
    "selected grass fixture, including shadows, ambient occlusion, and post "
    "effects. They are not direct visible grass pixels. Different distance "
    "groups are never subtracted. Absence of detected contribution is not "
    "proof that the fixture is invisible."
)


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def valid_digest(value):
    return isinstance(value, str) and re.fullmatch(r"[0-9a-fA-F]{64}", value)


def numeric(value, field):
    try:
        result = float(value)
    except (TypeError, ValueError):
        raise ValueError(f"{field}: expected a finite number") from None
    if not math.isfinite(result):
        raise ValueError(f"{field}: expected a finite number")
    return result


def canonical(value, field):
    # 收據可能來自 PowerShell 的字串字典或已轉換的 JSON 數值。
    if field in ("grass_fixture_root", "camera_position", "camera_target", "camera_up"):
        values = value.split(",") if isinstance(value, str) else value
        if not isinstance(values, (list, tuple)) or len(values) != 3:
            raise ValueError(f"{field}: expected three coordinates")
        return tuple(numeric(item, field) for item in values)
    return numeric(value, field)


def resolve_capture(evidence, value):
    if not isinstance(value, str) or not value:
        raise ValueError("capture: expected a PNG path")
    path = Path(value)
    if not path.is_absolute():
        path = evidence / path
    path = path.resolve()
    # 僅讀取本次證據目錄內的輸入，避免混用其他測試的圖片。
    if not path.is_relative_to(evidence):
        raise ValueError("capture must be inside the evidence directory")
    return path


def load_case(evidence, entry, mode, executable_digest):
    receipt = entry.get("receipt")
    if not isinstance(receipt, dict):
        raise ValueError("receipt: expected an object")
    for field, expected in REQUIRED_FLAGS.items():
        if field not in receipt or numeric(receipt[field], field) != expected:
            raise ValueError(f"{field}: required value is {expected}")
    enabled = 0 if mode == "off" else 1
    for field in ("grass_fixture_enabled", "grass_fixture_opaque_draw_instances"):
        if field not in receipt or numeric(receipt[field], field) != enabled:
            raise ValueError(f"{field}: required value for {mode} is {enabled}")
    state = {}
    for field in MATCH_FIELDS:
        if field not in receipt:
            raise ValueError(f"receipt missing {field}")
        state[field] = canonical(receipt[field], field)
    for field in ("grass_fixture_index", "grass_fixture_variant"):
        if state[field] < 0 or state[field] != int(state[field]):
            raise ValueError(f"{field}: expected a nonnegative integer")
    if state["grass_fixture_scale"] <= 0 or state["grass_fixture_player_distance"] < 0:
        raise ValueError("fixture scale must be positive and player distance nonnegative")
    if not 0 < state["camera_fov"] < math.pi:
        raise ValueError("camera_fov: expected radians between zero and pi")
    if "executable_sha256" in receipt:
        if str(receipt["executable_sha256"]).lower() != executable_digest:
            raise ValueError("receipt executable_sha256 does not match manifest")
    capture = resolve_capture(evidence, entry.get("capture"))
    expected_digest = entry.get("capture_sha256")
    if not valid_digest(expected_digest):
        raise ValueError("capture_sha256: expected a SHA256 hex digest")
    actual_digest = sha256(capture)
    if actual_digest != expected_digest.lower():
        raise ValueError("capture_sha256 does not match PNG bytes")
    with Image.open(capture) as image:
        if image.format != "PNG":
            raise ValueError("capture is not PNG")
        if image.size != (1280, 720):
            raise ValueError("production capture dimensions must be exactly 1280x720")
        pixels = np.asarray(image.convert("RGB"), dtype=np.int16)
    return pixels, state, {
        "case": entry["case"], "capture": str(capture),
        "capture_sha256": actual_digest, "capture_sha256_verified": True,
    }


def delta_statistics(rgb_delta, noise_max):
    per_pixel = rgb_delta.max(axis=2)
    changed = per_pixel > noise_max
    count = int(changed.sum())
    return {
        "pixel_count": int(per_pixel.size),
        "changed_pixels": count,
        "changed_fraction": count / int(per_pixel.size),
        "max_rgb_delta": int(per_pixel.max()),
        "mean_max_rgb_delta": float(per_pixel.mean()),
        "mean_rgb_channel_delta": float(rgb_delta.mean()),
        "changed_mean_max_rgb_delta": float(per_pixel[changed].mean()) if count else 0.0,
        "changed_max_rgb_delta": int(per_pixel[changed].max()) if count else 0,
    }


def analyze_group(evidence, output, group, entries, executable_digest):
    missing = set(("on", "off", "repeat")) - entries.keys()
    if missing:
        raise ValueError(f"missing cases: {', '.join(sorted(missing))}")
    loaded = {
        mode: load_case(evidence, entries[mode], mode, executable_digest)
        for mode in ("on", "off", "repeat")
    }
    on, off, repeat = (loaded[mode][0] for mode in ("on", "off", "repeat"))
    if on.shape != off.shape or on.shape != repeat.shape:
        raise ValueError("on/off/repeat dimensions do not match")
    baseline = loaded["on"][1]
    for mode in ("off", "repeat"):
        for field in MATCH_FIELDS:
            if loaded[mode][1][field] != baseline[field]:
                raise ValueError(f"{mode} receipt differs from on: {field}")
        # 動態新增的品質旗標也必須一致，避免忽略未來增加的品質設定。
        for field in set(loaded_receipt_key for entry in entries.values()
                         for loaded_receipt_key in entry["receipt"]
                         if loaded_receipt_key.startswith("quality_")):
            if field not in entries["on"]["receipt"] or field not in entries[mode]["receipt"]:
                raise ValueError(f"quality receipt missing {field}")
            if numeric(entries[mode]["receipt"][field], field) != numeric(entries["on"]["receipt"][field], field):
                raise ValueError(f"{mode} receipt differs from on: {field}")
    x0, y0, x1, y1 = ROI
    crop = np.s_[y0:y1, x0:x1, :]
    noise = np.abs(on - repeat)
    difference = np.abs(on - off)
    noise_max = int(noise.max())
    full_stats = delta_statistics(difference, noise_max)
    roi_stats = delta_statistics(difference[crop], noise_max)
    if noise_max > MAX_REPEAT_NOISE:
        status = "inconclusive"
        reason = "Global repeat noise exceeds the declared conservative noise limit."
    elif roi_stats["changed_pixels"]:
        status = "measurable_contribution"
        reason = "Content ROI contains RGB differences at least one code value above global repeat noise."
    elif noise_max:
        status = "inconclusive"
        reason = "No content ROI difference exceeds nonzero repeat noise; it may conceal fixture contribution."
    else:
        status = "no_detectable_contribution"
        reason = "The repeat is identical and no RGB contribution was detected in the content ROI."
    artifacts = {}
    for label, pixels in (("on", on[crop]), ("off", off[crop]), ("difference", difference[crop])):
        target = output / f"image-{group}-{label}-crop.png"
        Image.fromarray(pixels.astype(np.uint8)).save(target)
        artifacts[label] = {"path": str(target), "sha256": sha256(target)}
    return {
        "group": f"image-{group}", "status": status, "reason": reason,
        "paired_receipt_state": baseline,
        "inputs": [loaded[mode][2] for mode in ("on", "off", "repeat")],
        "image_dimensions": {"width": int(on.shape[1]), "height": int(on.shape[0])},
        "repeat_noise_max_rgb_delta": noise_max,
        "repeat_noise_roi_max_rgb_delta": int(noise[crop].max()),
        "repeat_noise_mean_max_rgb_delta": float(noise.max(axis=2).mean()),
        "repeat_noise_changed_pixels": int(np.count_nonzero(noise.max(axis=2))),
        "changed_pixel_min_rgb_delta": noise_max + 1,
        "on_off_full_image": full_stats, "on_off_content_roi": roi_stats,
        "artifacts": artifacts,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", required=True, type=Path,
                        help="Directory containing grass-distance-validation.json and PNG captures")
    args = parser.parse_args()
    evidence = args.evidence.resolve()
    output = evidence / "grass-image-contribution"
    try:
        # 已存在的結果屬於證據，不得在重新分析時覆蓋。
        output.mkdir(parents=True, exist_ok=False)
    except OSError as error:
        print(f"Cannot create fresh output directory {output}: {error}", file=sys.stderr)
        return 1
    report = {
        "schema_version": 1,
        "status": "invalid", "explanation": EXPLANATION,
        "metric": "8-bit RGB absolute difference; per-pixel delta is the maximum RGB channel delta",
        "content_roi": {"x_min": 0, "x_max_inclusive": 1279,
                        "y_min": 76, "y_max_inclusive": 596},
        "noise_policy": {
            "threshold": "global on/repeat maximum RGB delta + 1",
            "max_accepted_repeat_noise_rgb_delta": MAX_REPEAT_NOISE,
            "description": "Conservative fixed limit of 8/255; one repeat bounds observed noise only, not all possible temporal variation.",
        },
        "difference_crop_encoding": "Unamplified absolute RGB channel differences (0..255)",
        "groups": [], "errors": [],
    }
    try:
        manifest_path = evidence / "grass-distance-validation.json"
        manifest = json.loads(manifest_path.read_text(encoding="utf-8-sig"))
        report["manifest_sha256"] = sha256(manifest_path)
        executable_digest = manifest.get("executable_sha256")
        if not valid_digest(executable_digest):
            raise ValueError("executable_sha256: expected a SHA256 hex digest")
        report["executable_sha256"] = executable_digest.lower()
        report["executable_sha256_verification"] = "Manifest identity recorded; executable bytes are not supplied by this schema."
        cases = manifest.get("cases")
        if not isinstance(cases, list):
            raise ValueError("cases: expected an array")
        groups = {}
        for entry in cases:
            if not isinstance(entry, dict) or not isinstance(entry.get("case"), str):
                raise ValueError("every case requires an object with a string case name")
            name = entry["case"]
            if not name.startswith("image-"):
                continue
            match = CASE_PATTERN.fullmatch(name)
            if not match:
                raise ValueError(f"unsupported image case name: {name}")
            group, mode = match.groups()
            modes = groups.setdefault(group, {})
            if mode in modes:
                raise ValueError(f"duplicate case: {name}")
            modes[mode] = entry
        if not groups:
            raise ValueError("no image on/off/repeat groups found")
        for group, entries in sorted(groups.items(), key=lambda item: float(item[0])):
            try:
                report["groups"].append(analyze_group(evidence, output, group, entries, executable_digest.lower()))
            except (ValueError, OSError, KeyError, TypeError) as error:
                report["groups"].append({"group": f"image-{group}", "status": "invalid", "reason": str(error)})
        statuses = {group["status"] for group in report["groups"]}
        report["status"] = ("invalid" if "invalid" in statuses else
                            "inconclusive" if "inconclusive" in statuses else "analyzed")
    except (ValueError, OSError, KeyError, TypeError, AttributeError) as error:
        report["errors"].append(str(error))
    try:
        report_path = output / "report.json"
        report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
    except OSError as error:
        print(f"Cannot write report: {error}", file=sys.stderr)
        return 1
    print(json.dumps({"status": report["status"], "report": str(report_path),
                      "groups": [{"group": group["group"], "status": group["status"]}
                                 for group in report["groups"]]}, indent=2))
    return {"analyzed": 0, "invalid": 1, "inconclusive": 2}[report["status"]]


if __name__ == "__main__":
    sys.exit(main())
