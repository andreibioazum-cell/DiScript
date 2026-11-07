# Firebase Realtime Database rules

`firebase.rules.json` is the deployable policy. Firebase rules are JSON and do not
support comments, so this page explains the permissions in readable terms. Publish
the whole file from the Realtime Database **Rules** tab; do not copy only a
single child rule unless you also understand the inherited parent permissions.

## Who can access what

| Path | Read | Write |
| --- | --- | --- |
| `/AzumBuff`, `/FlashSale`, `/Christmas` | Anyone | `Dimasi4ek229` only; values must be `0` or `1` |
| `/event`, `/banner`, `/bans` | Signed-in `@cb4.game` password accounts | `Dimasi4ek229` only |
| `/users/<nick>` | Signed-in `@cb4.game` password accounts | The matching account, with the game's `w` secret; or `Dimasi4ek229` |
| `/rooms/<room>` | Signed-in `@cb4.game` password accounts | Admin or the specific owner/slot rules in the file |
| Unknown root paths | Admin only | Nobody |

The root `.write: false` is intentional: every writable root is explicitly
listed. It prevents a new, unreviewed node from becoming writable by accident.

## Gift counters

The ordinary gift counters (`poop`, `cake`, `bear`, `rocket`, `emerald`) are
capped at `0..999`. An owner can update their own counts; another signed-in
player can only add one to an existing counter (or create it with `1`).
Deleting another player's gift counter is rejected.

`pumpkin_cube` was removed; the rule remains for backward compatibility.
save their profile; the `pumpkin_cube` counter no longer exists in the game.
extra profile-level check is necessary because Firebase parent `.write` rules
also cover writes to descendants.

`rose` is an old gift key retained for legacy profiles. `emerald_hue` is not a
rule key anymore, and `$other: { ".validate": false }` rejects new unknown gift
fields. Existing `emerald_hue` values can be removed with the admin panel's
legacy-field cleaner.

## Profile balance limits

Profile writes require a valid game nickname, required progress fields and the
write secret `w`. Cups and candies are capped at ten million; a normal update
cannot raise either balance by more than `1000` in one write. The primary admin
is exempt from that increase cap. Each progress field has its own range validator
so malformed values are rejected before they reach game state.

## Updating safely

1. Edit the source file `firebase.rules.json` in Git, not the deployed console
   copy.
2. Run `python3 -m json.tool firebase.rules.json` and
   `python3 tools/test_db_rules.py`.
3. Review this page when changing a permission boundary.
4. Publish the complete JSON file in Firebase Console or with
   `firebase deploy --only database`.

The static rule test checks the Christmas flag, event bounds, slot fields,
required gift permissions and the owner-profile
invariant. The admin-panel schema test checks that the panel and Firebase rules
agree about live fields.
