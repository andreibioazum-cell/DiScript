#!/usr/bin/env python3
"""Validation checks for firebase.rules.json event bounds, gift keys, and unknown fields."""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "test"))
import fake_firebase

ROOT = Path(__file__).resolve().parents[1]
RULES = ROOT / "firebase.rules.json"


def main() -> int:
    rules = json.loads(RULES.read_text(encoding="utf-8"))["rules"]
    # Christmas root flag: public read, admin-only write, strict 0/1.
    xmas = rules.get("Christmas", {})
    if xmas.get(".read") is not True:
        print("Christmas must be publicly readable", file=sys.stderr)
        return 1
    if "dimasi4ek229@cb4.game" not in xmas.get(".write", ""):
        print("Christmas must be writable by admins only", file=sys.stderr)
        return 1
    if "newData.val() == 0" not in xmas.get(".validate", ""):
        print("Christmas must validate 0 or 1", file=sys.stderr)
        return 1
    slots = rules["rooms"]["$room"]["players"]["$slot"]
    for key in ("xbx", "xby", "xbdx", "xbdy", "xball"):
        if key not in slots:
            print(f"slot rule is missing Christmas field: {key}", file=sys.stderr)
            return 1
    event = rules["event"][".validate"]
    if "<= 5" not in event:
        print(f"event rule rejects event 5: {event}", file=sys.stderr)
        return 1
    gifts = rules["users"]["$nick"]["gifts"]
    for key in ("poop", "cake", "bear", "rocket", "emerald", "pumpkin_cube"):
        if key not in gifts:
            print(f"gift rule is missing: {key}", file=sys.stderr)
            return 1
        for rule in (".write", ".validate"):
            if rule not in gifts[key]:
                print(f"gift {key} has no {rule} rule", file=sys.stderr)
                return 1
    pumpkin = gifts["pumpkin_cube"]
    if "dimasi4ek229@cb4.game" not in pumpkin.get(".write", ""):
        print("pumpkin_cube must be writable by the admin only", file=sys.stderr)
        return 1
    if "<= 999" not in pumpkin.get(".validate", ""):
        print("pumpkin_cube counter must be capped at 999", file=sys.stderr)
        return 1
    owner_write = rules["users"]["$nick"][".write"]
    if "gifts').child('pumpkin_cube" not in owner_write or "newData.child('gifts').child('pumpkin_cube').val() == data.child('gifts').child('pumpkin_cube').val()" not in owner_write:
        print("owner profile writes must preserve the admin-only pumpkin_cube count", file=sys.stderr)
        return 1
    if gifts["$other"][".validate"] is not False:
        print("unknown gift keys must be rejected", file=sys.stderr)
        return 1
    # The emerald's hue is chosen by an admin and stored next to the counters:
    # the rule has to allow 0..7 and to let only the admin write it.
    hue = gifts.get("emerald_hue", {})
    if "<= 7" not in hue.get(".validate", ""):
        print(f"emerald_hue rule is missing or too narrow: {hue}", file=sys.stderr)
        return 1
    if "dimasi4ek229@cb4.game" not in hue.get(".write", ""):
        print(f"emerald_hue must be written by an admin only: {hue}", file=sys.stderr)
        return 1
    profile = rules["users"]["$nick"]
    # The CodeCracker keeps a level branch of its own, so the profile has to
    # accept its two fields - and it has to accept the packed skin flags
    # (azum_skin carries the existing and ordinary-cube skin bits, 0..63), or a save with a skin is rejected.
    for key in ("codecracker_level", "codecracker_levels"):
        rule = profile.get(key, {}).get(".validate")
        if not rule or "<= 3" not in rule:
            print(f"profile rule for {key} is missing or too narrow: {rule}", file=sys.stderr)
            return 1
    skin_rule = profile.get("azum_skin", {}).get(".validate", "")
    if "<= 63" not in skin_rule:
        print(f"azum_skin rule rejects the packed skin flags: {skin_rule}", file=sys.stderr)
        return 1
    for key, maximum in (("wheel_spins", "999"), ("wheel_wins", "5")):
        rule = profile.get(key, {}).get(".validate", "")
        if f"<= {maximum}" not in rule:
            print(f"{key} rule is missing or too narrow: {rule}", file=sys.stderr)
            return 1
    # The fake RTDB must enforce the same parent-profile protection as Firebase.
    fake = fake_firebase
    player = "Player2"
    existing = fake.DB["users"].get(player)
    fake.DB["users"][player] = {"cups": 100, "gifts": {"poop": 1, "pumpkin_cube": 2}}
    player_email = "player2@cb4.game"
    changed_profile = {"cups": 99, "gifts": {"poop": 1, "pumpkin_cube": 3}}
    kept_profile = {"cups": 99, "gifts": {"poop": 1, "pumpkin_cube": 2}}
    if fake.profile_write_preserves_pumpkin(["users", player], player_email, "PUT",
                                            json.dumps(changed_profile).encode()):
        print("fake Firebase allowed owner to alter pumpkin_cube through whole-profile PUT", file=sys.stderr)
        return 1
    if not fake.profile_write_preserves_pumpkin(["users", player], player_email, "PUT",
                                                json.dumps(kept_profile).encode()):
        print("fake Firebase rejected owner profile save preserving pumpkin_cube", file=sys.stderr)
        return 1
    if fake.can_write(["users", player, "gifts", "pumpkin_cube"], player_email):
        print("fake Firebase allowed direct ordinary-player pumpkin_cube write", file=sys.stderr)
        return 1
    if not fake.can_write(["users", player, "gifts", "pumpkin_cube"], "dimasi4ek229@cb4.game"):
        print("fake Firebase rejected admin pumpkin_cube write", file=sys.stderr)
        return 1
    if existing is None:
        del fake.DB["users"][player]
    else:
        fake.DB["users"][player] = existing
    print("db rules: event 0..5, Christmas flag + snowball slot fields, "
          "gift keys poop/cake/bear/rocket/emerald plus admin-only pumpkin_cube, "
          "emerald hue 0..7 for admins, "
          "wheel progress and packed skins OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
