"""核對同一套試玩包在不同 Tailscale 裝置上的開房與重連收據。"""
import argparse
from hashlib import sha256
from ipaddress import IPv4Address, IPv4Network
import json
from math import isfinite
from pathlib import Path
import re

HEX = re.compile(r"[0-9a-f]{64}\Z")
TAILNET = IPv4Network("100.64.0.0/10")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def counter(value):
    return type(value) is int and value > 0


def point(value):
    return (isinstance(value, list) and len(value) == 2 and
            all(type(n) in (int, float) and isfinite(n) for n in value))


def advance(first, second):
    return (point(first) and point(second) and abs(second[0] - first[0]) <= .001 and
            .098 <= second[1] - first[1] <= .102)


def audit(host, guests, manifest_raw):
    require(guests, "至少需要一台來賓電腦的收據")
    manifest_id = sha256(manifest_raw).hexdigest()
    manifest = json.loads(manifest_raw)
    server_entries = [row for row in manifest["files"] if row["path"] == "Server/MistwatchWorldServer.exe"]
    if len(server_entries) != 1:
        raise ValueError("套件清單缺少唯一的專用伺服器")
    server_hash = server_entries[0]["sha256"]
    require(HEX.fullmatch(server_hash) and host.get("server_binary_sha256") == server_hash,
            "房主執行檔雜湊和指定套件不一致")
    endpoint = IPv4Address(host["tailnet_address"])
    port, epoch = host["port"], host["epoch"]
    require(endpoint in TAILNET and counter(port) and port <= 65535 and counter(epoch),
            "房間位址、連接埠或世界識別無效")
    require(host.get("mode") == "dedicated" and counter(host.get("server_pid")) and
            counter(host.get("room_capacity")) and host["room_capacity"] <= 32,
            "房主收據缺少專用房間資料")
    room_scope = host.get("host_device_scope", "")
    require(HEX.fullmatch(room_scope) and room_scope == host.get("local_device_scope"),
            "房主收據缺少一致的裝置身分")
    require(host.get("package_manifest_sha256") == manifest_id, "房主套件清單雜湊不符")

    seen = {room_scope}
    evidence = []
    for guest in guests:
        require("server_pid" in guest and guest["server_pid"] is None and
                (guest.get("tailnet_address"), guest.get("port"), guest.get("epoch")) == (str(endpoint), port, epoch),
                "來賓未連入相同的房間與世界")
        require(guest.get("package_manifest_sha256") == manifest_id, "來賓使用不同的套件清單")
        require(room_scope == guest.get("host_device_scope"), "來賓收據中的房主裝置不同")
        scope = guest.get("local_device_scope", "")
        require(HEX.fullmatch(scope) and guest.get("separate_tailnet_device") is True and scope not in seen,
                "房主與來賓必須使用不同的 Tailscale 裝置，且來賓收據不可重複")
        seen.add(scope)
        movement, reconnect = guest["movement"], guest["reconnect"]
        player, returning = guest["player"], reconnect["player"]
        ticks = [guest["snapshot_tick"], movement["tick"], reconnect["tick"], reconnect["movement_tick"]]
        require(all(counter(tick) for tick in ticks) and all(a < b for a,b in zip(ticks,ticks[1:])),
                "快照、移動與重連時間順序不一致")
        require(counter(player) and counter(returning) and player != returning and reconnect.get("role") == 1,
                "來賓未以原測試職業在另一連線上重新登入")
        require(advance(movement["from"],movement["to"]) and movement["to"] == reconnect["position"] and
                advance(reconnect["position"],reconnect["movement_position"]),
                "伺服器未確認接續相同位置移動")
        evidence.append({"initial_connection":player,"reconnected_as":returning,"ticks":ticks})
    return {"stage":"PRIVATE_TRIAL_DISTINCT_DEVICE_RECEIPTS_MATCH","device_count":len(seen),
            "world_epoch":epoch,"manifest_sha256":manifest_id,"server_sha256":server_hash,"guests":evidence}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest",type=Path,metavar="PackageManifest.json")
    parser.add_argument("host",type=Path,metavar="Host-launch.json")
    parser.add_argument("guests",type=Path,nargs="+",metavar="Guest-launch.json")
    parameters=parser.parse_args()
    try:
        host=json.loads(parameters.host.read_bytes())
        guests=[json.loads(path.read_bytes()) for path in parameters.guests]
        result=audit(host,guests,parameters.manifest.read_bytes())
    except (OSError, LookupError, TypeError, OverflowError, ValueError) as error:
        parser.exit(1,f"驗收未通過。{error}\n")
    print(json.dumps(result,ensure_ascii=False,indent=2))


if __name__=="__main__":
    main()
