# In-game gifts

Gifts work through the Realtime Database directly - there is no Cloud Function
(`giftAction`) involved. The catalogue lives in the client
(`src/engine/network/gifts.inc`):

The shop sells five ordinary stickers; the emerald is the most expensive
(500 cups). The old `pumpkin_cube` gift has been removed (user request).

| ID | Sticker | Cost |
| --- | --- | ---: |
| `poop` | Poop | 25 |
| `cake` | Cake | 100 |
| `bear` | Teddy Bear | 250 |
| `rocket` | Rocket | 400 |
| `emerald` | Emerald | 500 |

For the gift admin account (`Dimasi4ek229`, the only `admin_gifts_allowed()`)
ordinary stickers are free: `gifts.inc` writes only the counter and does not
touch `cups`; the Buy button is never grey for this account. That does not
open the removed `pumpkin_cube` gift. The second root admin
(`qwertyuiopaj1234`) pays full price like any player and cannot hand out
gifts at all: the rules exclude him from the `+1` branch and the game hides
the CCP Gifts tab from him.

The first sticker used to be `rose`; it is now `poop`. Old `rose` counters may
still sit in profiles - the rules keep accepting them, but the game neither
shows nor spends them. The old `gifts/emerald_hue` field has been removed from
the Firebase rules. The emerald now cycles smoothly through rainbow hues using
a local animation clock; no per-player hue is stored. The admin panel still
finds both old fields in `LEGACY_FIELDS` so they can be cleaned from profiles.

## Data layout

```
users/<nick>/cups          cup balance
users/<nick>/gifts/poop          0..999 owned stickers
... (cake, bear, rocket, emerald)
```

* **Buy** - one authenticated PATCH to `users/<nick>.json` that writes the new
  cup balance and the incremented sticker together, so a purchase cannot take
  the cups without delivering the sticker.
* **Send** - `-1` to the sender's `gifts/<id>` first, then `+1` to the recipient's.
  Each step is a single-child write, so the other counters are never rewritten.
  If the second write fails, the client puts the sticker back into the sender's
  counter.
* **Sell** - one authenticated PATCH to `users/<nick>.json` that takes one
  sticker from the player's own counter and adds **half of the shop price**
  (12 / 50 / 125 / 200 / 250). The shop stays the cheaper way to get a sticker,
  while a duplicate from an event is still worth something.
* **Admin grant** - the CCP panel's Gifts tab performs the same `+1` write;
  it does not debit cups. It only needs the recipient's nickname.

The sticker counter in the buy/sell PATCH is written as a **path**, not as a
nested object:

```
{"cups":137,"gifts/poop":0,"w":"<secret>"}
```

A REST PATCH replaces the named subtree as a whole, so the nested form
(`{"cups":137,"gifts":{"poop":0}}`) wiped every other counter of the
collection - selling one poop also "sold" the emerald. The path form writes
only `gifts/poop` and leaves the rest untouched, while the balance and the
sticker change in one atomic request.

All requests are normal Firebase REST calls with the signed-in player's ID
token (`fb_http`), exactly like progress saves.

The cup balance is client-written, same trust model as the rest of the progress
save, so a modified client could still fake its own cups - but not another
player's profile. The rules now also cap how far one write may raise a balance
(`newData.val() <= data.val() + 1000`), so a hand-made request cannot jump
straight to millions; administrators are exempt.

The shop never computes the new balance from the local copy: buying and
selling write the balance together with the sticker and the client adopts the
number the database returned (`net_gift_cups`). A stale local wallet can no
longer overwrite the account with a smaller value.

## UI

* Each sticker is a PNG sprite (`assets/textures/gift_poop.png`,
  `gift_cake.png`, `gift_bear.png`, `gift_rocket.png`, `gift_emerald.png`).
  They are drawn by `gift_icon()` in `src/game/ui/gift_icons.inc`.
* The emerald texture is a white faceted stone tinted with a smoothly cycling
  rainbow hue. Its animation uses the local shimmer clock and does not read a
  per-player color field.
* Each card names the sticker and its rarity in the rarity colour: common,
  rare, epic. There are no exclusive cards left after the removal of
  `pumpkin_cube`.
* The page has two independently scrollable columns: the shop on the left and
  the player's own stickers on the right. Every card is clipped to its viewport.
* Cards carry no buttons. A tap selects a card and the action row changes:
  **Buy** for a shop card, **Send** + **Sell** for an owned one, with the
  half-price refund shown on the Sell label. The recipient nickname field is
  only needed for Send.
* The collection on screen is never wiped by a refresh: the last known counters
  stay visible until the fresh ones arrive, and a bought, sold or sent sticker
  changes its own counter at once (`net_gift_count_add`). Only that sticker
  leaves the list.
