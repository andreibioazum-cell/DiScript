# In-game gifts

Gifts work through the Realtime Database directly — there is no Cloud Function
(`giftAction`) involved. The catalogue lives in the client
(`src/engine/network/gifts.inc`; the UI reads the price via `net_gift_price`):

The catalogue has five stickers; the emerald is the most expensive one
(500 cups).

| ID | Sticker | Cost |
| --- | --- | ---: |
| `poop` | Какашка / Poop | 25 |
| `cake` | Торт / Cake | 100 |
| `bear` | Мишка / Teddy Bear | 250 |
| `rocket` | Ракета / Rocket | 400 |
| `emerald` | Изумруд / Emerald | 500 |

Для двух админских аккаунтов (тот же список, что в `admin_slot`) покупка
стикера бесплатна: `gifts.inc` пишет только счётчик стикера и не трогает
`cups`, а кнопка «Купить» для админа никогда не серая.

The first sticker used to be `rose`; it is now `poop`. Old `rose` counters may
still sit in profiles — the rules keep accepting them, but the game neither
shows nor spends them. The same applies to `gifts/emerald_hue`: the emerald is
always rainbow now (`net_gift_hue()` returns 0), so the admin-chosen hue is no
longer read. Both are obsolete fields; the web admin panel lists them in
`LEGACY_FIELDS` and removes them from the database with the dedicated
«Устаревшие поля» filter and the «Удалить устаревшие поля» button.

## Data layout

```
users/<nick>/cups          cup balance
users/<nick>/gifts/poop    0..999 owned stickers (cake, bear, rocket likewise)
```

* **Buy** — one authenticated PATCH to `users/<nick>.json` that writes the new
  cup balance and the incremented sticker together, so a purchase cannot take
  the cups without delivering the sticker.
* **Send** — `-1` to the sender's `gifts/<id>` first, then `+1` to the
  recipient's. Each step is a single-child write, so the other counters are
  never rewritten. If the second write fails, the client puts the sticker back
  into the sender's counter.
* **Sell** — one authenticated PATCH to `users/<nick>.json` that takes one
  sticker from the player's own counter and adds **half of the shop price** to
  the cups (12 / 50 / 125 / 200 / 250). The shop stays the cheaper way to get a
  sticker, while a duplicate from an event is still worth something.
* **Admin grant** — the CCP panel's Gifts tab performs the same `+1` write; it
  does not debit cups. It only needs the recipient's nickname: nicknames are
  unique, so the nickname already points at exactly one account.

The sticker counter in the buy/sell PATCH is written as a **path**, not as a
nested object:

```
{"cups":137,"gifts/poop":0,"w":"<secret>"}
```

A REST PATCH replaces the named subtree as a whole, so the nested form
(`{"cups":137,"gifts":{"poop":0}}`) wiped every other counter of the
collection — selling one poop also "sold" the emerald. The path form writes
only `gifts/poop` and leaves `gifts/cake`, `gifts/emerald` and the rest
untouched, while the balance and the sticker still change in one atomic
request.

All requests are normal Firebase REST calls with the signed-in player's ID
token (`fb_http`), exactly like progress saves. No callable function is needed.

## Database rules

`firebase.rules.json` allows a signed-in `@cb4.game` password player to read
profiles, as before. For each gift counter:

* the owner may set it to any value `0..999`;
* another player may only move it from `N` to `N + 1` (one sticker per write),
  and may create a missing counter only with `1`;
* deletes are denied (`newData.exists()` in the `.write` rule), so nobody can
  wipe somebody else's collection;
* the rest of the profile (cups, levels, classes) stays owner-only, so gifting
  cannot change anything besides the gift counter.

Deploy the rules with:

```
firebase deploy --only database
```

If an older build had the `giftAction` callable deployed, remove it (nothing in
the game calls it anymore):

```
firebase functions:delete giftAction
```

The cup balance is client-written, same trust model as the rest of the
progress save, so a modified client could still fake its own cups — but not
another player's profile. The rules now also cap how far one write may raise a
balance (`newData.val() <= data.val() + 1000`), so a hand-made request cannot
jump straight to millions; administrators are exempt.

The shop never computes the new balance from the local copy: buying and
selling write the balance together with the sticker and the client adopts the
number the database returned (`net_gift_cups`). A stale local wallet can no
longer overwrite the account with a smaller value.

### Why a balance could turn into zero

A sign-out zeroes the device profile and saves it locally. That local-only
save used to set the "waiting for the cloud" marker, so the next login merged
the zeroed device over the account and pushed zeros to Firebase. Fixed in two
places: a save without a session no longer marks the profile dirty, and a login
merge ignores a device with no progress at all (and never lets a local cup
count lower the cloud balance, since cups are only spent through the shop,
which writes them itself).

## UI

* Every sticker is a PNG sprite (`assets/textures/gift_poop.png`,
  `gift_cake.png`, `gift_bear.png`, `gift_rocket.png`, `gift_emerald.png`).
  They are generated by `tools/gen_gift_textures.py` (flat cartoon art,
  192x192 with transparency) and drawn by `gift_icon()` in
  `src/game/ui/gift_icons.inc`; the pixel font is never asked for a symbol.
  A card shows its picture once - there is no second sprite or badge.
* The emerald shimmers. The stone itself is drawn white
  (`tools/gen_gift_textures.py`), and on top of it the engine draws a second,
  translucent copy of the sprite in the current hue, so a coloured veil flows
  over the white facets (a tint reads only on a white base - on a coloured
  sprite the hue would turn to mud); the hue drifts slowly
  (`ds_frame_seconds() * 0.08`) and the veil's strength breathes with a sine.
  The base hue is fixed to 0 (plain rainbow) for every account: `net_gift_hue()`
  always returns 0. `gifts/emerald_hue` is a legacy field kept only in the
  database rules for compatibility with old profiles.
* Each card also names the sticker and its rarity in the rarity colour:
  **common** white, **rare** light blue (`#5CC8FF`), **epic** violet
  (`#C08CFF`) - and the **exclusive** emerald is signed plain white too: the
  colour belongs to the stone itself, which drifts through the hue wheel. The
  catalogue is poop common, cake rare, teddy bear and rocket epic, emerald
  exclusive.
* The page has two columns: the shop on the left and the player's own
  stickers on the right. The whole catalogue fits the left column at once;
  the collection lists **one card per owned sticker** - two poops are two
  cards - and scrolls smoothly with inertia, without snapping to rows, and
  with a gentle glide (the fling is capped, so the list no longer flies past
  the sticker under the finger). Every card is clipped by hand to the list
  window, so nothing spills over the header or the field below.
* Cards carry no buttons. A tap selects a card (shop or collection) and the
  action row under the columns changes: **Buy** for a shop card,
  **Send** + **Sell** for an owned one, with the half-price refund on the
  Sell label. The recipient nickname field sits there too and is only needed
  for Send.
* An empty collection says so **under the "MY GIFTS" heading**, in its own
  column - in the middle of the screen the line looked like a message about
  the whole page and landed on top of the shop.
* The collection on screen is never wiped by a refresh: the last known
  counters stay visible until the fresh ones arrive, and a bought, sold or
  sent sticker changes its own counter at once (`net_gift_count_add`). Only
  that one sticker leaves the list - the rest of the collection (the emerald,
  for one) stays where it was.

The signed-in Account screen contains that two-column page; the in-game CCP
panel has a Gifts tab for admin grants.
