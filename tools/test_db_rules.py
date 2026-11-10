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
    if "<= 7" not in event:
        print(f"event rule rejects event 7: {event}", file=sys.stderr)
        return 1

    # The second root admin: qwertyuiopaj1234 may do everything except hand
    # out gifts - every admin clause lists him, every gift counter blocks him.
    dmail = "dimasi4ek229@cb4.game"
    qmail = "qwertyuiopaj1234@cb4.game"
    for node in ("event", "banner", "AzumBuff", "FlashSale", "Christmas"):
        write = rules[node][".write"]
        if dmail not in write or qmail not in write:
            print(f"{node} must be writable by both root admins", file=sys.stderr)
            return 1
    ban_write = rules["bans"]["$nick"][".write"]
    if qmail not in ban_write or "'qwertyuiopaj1234'" not in ban_write:
        print("the giftless admin must ban others and stay unbannable himself", file=sys.stderr)
        return 1
    if qmail not in rules["users"]["$nick"][".write"]:
        print("the giftless admin must reach the player profiles", file=sys.stderr)
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
        if qmail not in gifts[key][".validate"]:
            print(f"gift {key} must block the giftless admin from handing it out", file=sys.stderr)
            return 1
    rose_validate = gifts["rose"][".validate"]
    if qmail in rose_validate or dmail not in rose_validate:
        print("legacy rose stays own-profile or gift-admin only", file=sys.stderr)
        return 1
    if "pumpkin_cube" in gifts:
        print("the pumpkin cube gift must be removed from the rules", file=sys.stderr)
        return 1
    owner_write = rules["users"]["$nick"][".write"]
    if "newData.exists()" not in owner_write:
        print("owner profile writes must still require an existing profile", file=sys.stderr)
        return 1
    if gifts["$other"][".validate"] is not False:
        print("unknown gift keys must be rejected", file=sys.stderr)
        return 1
    if "emerald_hue" in gifts or gifts["$other"][".validate"] is not False:
        print("emerald_hue must be removed and unknown gift fields rejected", file=sys.stderr)
        return 1
    profile = rules["users"]["$nick"]
    # The CodeCracker keeps a level branch of its own, so the profile has to
    # accept its two fields - and it has to accept the packed skin flags
    # (azum_skin carries all packed skin bits, 0..127), or a save with a skin is rejected.
    # The game allows levels up to max_level = 4, so the profile rules must too,
    # otherwise a level 4 save is rejected and the cloud keeps the old value.
    for key in ("codecracker_level", "codecracker_levels", "level", "levels",
                "ordinary_level", "azum_level", "santa_level", "ebuc_level"):
        rule = profile.get(key, {}).get(".validate", "")
        cap = rule.split("<=")[-1].strip() if "<=" in rule else ""
        if not rule or not cap or int(float(cap)) < 4:
            print(f"profile rule for {key} is missing or too narrow: {rule}", file=sys.stderr)
            return 1
    skin_rule = profile.get("azum_skin", {}).get(".validate", "")
    if "<= 127" not in skin_rule:
        print(f"azum_skin rule rejects the packed skin flags: {skin_rule}", file=sys.stderr)
        return 1
    for key in ("candies", "santa", "bp", "bpx"):
        rule = profile.get(key, {}).get(".validate", "")
        if "newData.val() == data.val()" not in rule or "newData.val() == 0" not in rule:
            print(f"{key} rule does not keep new rewards admin-only: {rule}", file=sys.stderr)
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
    fake.DB["users"][player] = {"cups": 100, "w": fake.SECRET, "gifts": {"poop": 1}}
    player_email = "player2@cb4.game"
    kept_profile = {"cups": 99, "w": fake.SECRET, "gifts": {"poop": 2}}
    lost_secret = {"cups": 99, "w": "x", "gifts": {"poop": 2}}
    if not fake.owner_profile_write_ok(["users", player], player_email, "PUT",
                                       json.dumps(kept_profile).encode()):
        print("fake Firebase rejected a valid owner profile save", file=sys.stderr)
        return 1
    if fake.owner_profile_write_ok(["users", player], player_email, "PUT",
                                   json.dumps(lost_secret).encode()):
        print("fake Firebase allowed an owner profile save that drops the password hash", file=sys.stderr)
        return 1
    if fake.can_write(["users", player, "gifts", "poop"], "other@cb4.game"):
        print("fake Firebase allowed a foreign player to write someone else's gift", file=sys.stderr)
        return 1
    if not fake.can_write(["event"], qmail) or not fake.can_write(["bans", "Spammer"], qmail):
        print("fake Firebase must let the giftless admin run events and bans", file=sys.stderr)
        return 1
    if not fake.can_write(["users", player, "cups"], qmail):
        print("fake Firebase blocked the giftless admin from the profiles", file=sys.stderr)
        return 1
    if fake.can_write(["users", player, "gifts", "poop"], qmail):
        print("fake Firebase let the giftless admin hand out a gift", file=sys.stderr)
        return 1
    if not fake.can_write(["users", "qwertyuiopaj1234", "gifts", "poop"], qmail):
        print("fake Firebase blocked the giftless admin's own gift purchase", file=sys.stderr)
        return 1
    if not fake.can_write(["users", player, "gifts", "poop"], dmail):
        print("fake Firebase blocked the gift admin from handing out a gift", file=sys.stderr)
        return 1
    if fake.can_write(["bans", "qwertyuiopaj1234"], dmail) or fake.can_write(["bans", "dimasi4ek229"], qmail):
        print("root admins must stay unbannable", file=sys.stderr)
        return 1
    if existing is None:
        del fake.DB["users"][player]
    else:
        fake.DB["users"][player] = existing
    print("db rules: event 0..7, Christmas flag + snowball slot fields, "
          "gift keys poop/cake/bear/rocket/emerald, "
          "rainbow-rendered emerald without emerald_hue, "
          "wheel progress and packed skins OK, "
          "second root admin qwertyuiopaj1234 without gift handing OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
