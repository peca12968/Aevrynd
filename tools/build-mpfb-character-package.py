"""建立 candidate7 檔案身份清單；不改模型、不放寬 Gameplay 九材質契約。"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import tempfile


def identity(path):
    path = path.resolve(strict=True)
    return dict(path=str(path), bytes=path.stat().st_size,
                sha256=hashlib.sha256(path.read_bytes()).hexdigest())


def skeleton(path):
    doc = json.loads(path.read_text(encoding="utf-8"))
    if len(doc.get("skins", [])) != 1:
        raise ValueError("expected one skin")
    skin = doc["skins"][0]
    parents = {c: i for i, n in enumerate(doc["nodes"]) for c in n.get("children", [])}
    joints = skin["joints"]
    nodes = []
    for j in joints:
        n = doc["nodes"][j]
        nodes.append(dict(name=n["name"], parent=joints.index(parents[j]) if parents.get(j) in joints else -1,
                          translation=n.get("translation", [0, 0, 0]), rotation=n.get("rotation", [0, 0, 0, 1]),
                          scale=n.get("scale", [1, 1, 1]), matrix=n.get("matrix")))
    a = doc["accessors"][skin["inverseBindMatrices"]]
    if a["type"] != "MAT4" or a["componentType"] != 5126 or a["count"] != len(joints) or "sparse" in a:
        raise ValueError("unsupported inverse binds")
    v = doc["bufferViews"][a["bufferView"]]
    buffer = path.parent / doc["buffers"][v["buffer"]]["uri"]
    raw = buffer.read_bytes()
    offset = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    matrices = [struct.unpack_from("<16f", raw, offset + i * v.get("byteStride", 64)) for i in range(len(joints))]
    return dict(nodes=nodes, inverseBinds=matrices), doc


def build(root):
    specs = [
        ("body", "face-detail-59/candidate7/body", "face-detail-59/body-materials.json"),
        ("upper", "garment-shell-followup-94/bounded-shell/upper", "face-detail-59/upper-iris-readable.json"),
        ("lower", "character-split-lower-17/character-dense", "face-detail-59/upper-materials.json"),
        ("hair", "hair-backfill-92/combined", "hair-backfill-92/combined.materials.json"),
    ]
    entries = []
    canonical = None
    for role, stem, sidecar in specs:
        source = root / (stem + ".gltf")
        rig, doc = skeleton(source)
        if len(rig["nodes"]) != 53:
            raise ValueError(role + ": expected 53 joints")
        if canonical is None:
            canonical = rig
        if rig != canonical:
            raise ValueError(role + ": source skeleton/bind mismatch")
        material_path = root / sidecar
        materials = json.loads(material_path.read_text(encoding="utf-8"))["root"]["materials"]
        textures = {}
        for name in [m["name"] for m in doc["materials"]]:
            if name not in materials:
                raise ValueError(role + ": missing material " + name)
            for key, texture in materials[name].get("textures", {}).items():
                if key == "maximumResolution":
                    continue
                item = identity(material_path.parent / texture["path"])
                if item["bytes"] != texture["expectedBytes"] or item["sha256"].lower() != texture["expectedSha256"].lower():
                    raise ValueError(role + ": texture identity mismatch")
                textures[item["path"]] = item
        runtime = source.with_suffix(".aeskin") if role != "lower" else source.parent / "character.aeskin"
        entries.append(dict(role=role, source=identity(source), runtime=identity(runtime),
                            buffers=[identity(source.parent / b["uri"]) for b in doc["buffers"]],
                            materials=identity(material_path), textures=list(textures.values()),
                            materialNames=[m["name"] for m in doc["materials"]]))
    return dict(schema="aevrynd.mpfb-character-package", version=1, character="candidate7",
                scope="inventory-and-source-bind-identity", gameplayReady=False,
                legacyNineMaterialGateUnchanged=True, jointCount=53,
                sourceSkeletonSha256=hashlib.sha256(json.dumps(canonical, sort_keys=True, allow_nan=False).encode()).hexdigest(),
                components=entries,
                morph=identity(root / "face-detail-59/candidate7/body.acms"),
                provenance=identity(root / "heroine-mpfb-v1/provenance.json"),
                pending=["native component loader/bind verification", "posed geometry and bind-axis validation",
                         "per-component license inventory", "Gameplay dedicated package entry", "fresh native visual review"])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--artifact-root", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    # 所有輸入通過後才建立暫存輸出；失敗不留下成功清單或半成品。
    result = build(args.artifact_root.resolve(strict=True))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix="mpfb-package-", dir=args.output.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(result, stream, ensure_ascii=False, indent=2, allow_nan=False)
        os.replace(temporary, args.output)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)
    print("MPFB_PACKAGE_INVENTORY=PASS gameplayReady=false")


if __name__ == "__main__":
    main()
