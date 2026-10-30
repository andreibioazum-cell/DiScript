# In-game gifts

Gifts work through the Realtime Database directly — there is no Cloud Function
(`giftAction`) involved. The catalogue lives in the client
(`src/engine/network/gifts.inc`; the UI reads the price via `net_gift_price`):

The shop sells five ordinary stickers; the emerald is the most expensive
purchasable item (500 cups). The catalogue also shows **Тыква-куб / Pumpkin Cube**
as a read-only exclusive card with a 555-cup value. Players cannot buy, send, or
sell it; only an administrator can grant it. Its card uses the ordinary Pumpkin
card illustration.

| ID | Sticker | Cost |
| --- | --- | ---: |
| `poop` | Какашка / Poop | 25 |
| `cake` | Торт / Cake | 100 |
| `bear` | Мишка / Teddy Bear | 250 |
| `rocket` | Ракета / Rocket | 400 |
| `emerald` | Изумруд / Emerald | 500 |

Read-only exclusive catalogue card (not purchasable in the player shop):

| ID | Sticker | Catalog cost | Availability |
| --- | --- | ---: | --- |
| `pumpkin_cube` | Тыква-куб / Pumpkin Cube | 555 | Admin grant only; no player purchase, send, or sale |

Для основного админского аккаунта (`Dimasi4ek229`, как в `admin_slot`)
обычные стикеры бесплатны: `gifts.inc` пишет только счётчик и не трогает
`cups`, а кнопка «Купить» для него никогда не серая. Это не открывает покупку
эксклюзивного `pumpkin_cube`.

The first sticker used to be `rose`; it is now `poop`. Old `rose` counters may
still sit in profiles — the rules keep accepting them, but the game neither
shows nor spends them. The old `gifts/emerald_hue` field has been removed from
the Firebase rules. The emerald now cycles smoothly through rainbow hues using
a local animation clock; no per-player hue is stored. The admin panel still
finds both old fields in `LEGACY_FIELDS` so they can be removed from existing
profiles.

## Data layout

```
users/<nick>/cups          cup balance
users/<nick>/gifts/poop          0..999 owned stickers
users/<nick>/gifts/pumpkin_cube  0..999 admin-granted exclusive stickers
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
  unique, so the nickname already points at exactly one account. The exclusive
  `pumpkin_cube` is available only through this admin action.

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

* for ordinary gift types, the owner may set the counter to any value `0..999`;
* for `gifts/pumpkin_cube`, only an admin may write; an owner writing their
  whole profile must leave that counter unchanged, so an ordinary profile save
  cannot bypass the special gift rule;
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
  A card shows its picture once - there is no second sprite or badge. Pumpkin
  Cube uses its own centered, sticker-sized crop of the cube artwork.
* The emerald texture is a white faceted stone tinted with a smoothly cycling
  rainbow hue. Its animation uses the local shimmer clock and does not read a
  per-player color field.
* Each card also names the sticker and its rarity in the rarity colour:
  **common** white, **rare** light blue (`#5CC8FF`), **epic** violet
  (`#C08CFF`), and **exclusive** white. The emerald artwork itself stays one
  steady green. The catalogue is poop common, cake rare, teddy bear and rocket
  epic, emerald and Pumpkin Cube exclusive.
* The page has two independently scrollable columns: the shop on the left and
  the player's own stickers on the right. Each list is clipped to its viewport;
  the collection lists **one card per owned sticker** - two poops are two
  cards - and both lists scroll smoothly with inertia, without snapping to rows, and
  with a gentle glide (the fling is capped, so the list no longer flies past
  the sticker under the finger). Every card is clipped by hand to the list
  window, so nothing spills over the header or the field below.
* Cards carry no buttons. A tap selects a card (shop or collection) and the
  action row under the columns changes: **Buy** for a shop card,
  **Send** + **Sell** for an owned one, with the half-price refund on the
  Sell label. The recipient nickname field sits there too and is only needed
  for Send. “Inspect” is shown only for owned gifts, never for shop cards.
  The shop remains limited to the five ordinary gifts; admin-granted
  `pumpkin_cube` appears only in the collection after it is granted.
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
