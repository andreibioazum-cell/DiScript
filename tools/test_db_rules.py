#!/usr/bin/env python3
"""Validation checks for firebase.rules.json event bounds, gift keys, and unknown fields."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RULES = ROOT / "firebase.rules.json"


def main() -> int:
    rules = json.loads(RULES.read_text(encoding="utf-8"))["rules"]
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
    if "dimasi4ek229@cb4.game" not in hue.get(".write", "") or "qwertyuiopaj1234@cb4.game" not in hue.get(".write", ""):
        print(f"emerald_hue must be written by admins only: {hue}", file=sys.stderr)
        return 1
    profile = rules["users"]["$nick"]
    # The CodeCracker keeps a level branch of its own, so the profile has to
    # accept its two fields - and it has to accept the packed skin flags
    # (azum_skin carries two bits, 0..3), or a save with a skin is rejected.
    for key in ("codecracker_level", "codecracker_levels"):
        rule = profile.get(key, {}).get(".validate")
        if not rule or "<= 4" not in rule:
            print(f"profile rule for {key} is missing or too narrow: {rule}", file=sys.stderr)
            return 1
    # The top asks the database for the cup leaders instead of downloading every
    # profile, which only stays cheap while "cups" is indexed.
    index = rules["users"].get(".indexOn")
    if index != ["cups"]:
        print(f"users must stay indexed on cups for the top: {index}", file=sys.stderr)
        return 1
    skin_rule = profile.get("azum_skin", {}).get(".validate", "")
    if "<= 3" not in skin_rule:
        print(f"azum_skin rule rejects the packed skin flags: {skin_rule}", file=sys.stderr)
        return 1
    print("db rules: event 0..5, gift keys poop/cake/bear/rocket/emerald, "
          "emerald hue 0..7 for admins, codecracker levels, packed skins and the cups index OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
