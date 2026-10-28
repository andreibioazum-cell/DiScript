#!/usr/bin/env python3
"""Validation checks for firebase.rules.json event bounds, gift keys, and unknown fields."""
import json
import sys
from pathlib import Path

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
    for key in ("poop", "cake", "bear", "rocket", "emerald"):
        if key not in gifts:
            print(f"gift rule is missing: {key}", file=sys.stderr)
            return 1
        for rule in (".write", ".validate"):
            if rule not in gifts[key]:
                print(f"gift {key} has no {rule} rule", file=sys.stderr)
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
    for key, maximum in (("wheel_spins", "999"), ("wheel_wins", "9")):
        rule = profile.get(key, {}).get(".validate", "")
        if f"<= {maximum}" not in rule:
            print(f"{key} rule is missing or too narrow: {rule}", file=sys.stderr)
            return 1
    print("db rules: event 0..5, Christmas flag + snowball slot fields, "
          "gift keys poop/cake/bear/rocket/emerald, emerald hue 0..7 for admins, "
          "wheel progress and packed skins OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
