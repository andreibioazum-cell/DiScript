# Legal audit for Cubic Battle 4 (RU), 2026

Not a lawyer's opinion. This file tracks how the code and documents match Russian
law and where the simplest fixes are.

## Short verdict (after simplification)

- Only a nickname and a password are stored; no real names, e-mail or phone.
- Firebase is used worldwide; the user considers this acceptable because no
  real personal identifiers are collected. The registration screen and terms
  explicitly warn not to put a real name, surname, phone or e-mail into the nick.
- The `pumpkin_cube` gift has been removed; gift list is now 5 items.
- User-generated chat is filtered; complaints are filed with a text command in chat, and an admin web panel exists.
- Age label 12+ is shown; parental PIN works locally (not transmitted).
- White vignette, blue ice blocks, Santa letter (bottom-right, "Be good in 2026
  and 2027! Love, Santa"), light random-cube-skin codecreeper and English-only
  code comments are implemented.

## What is done

- Nick + password only (fictitious `@cb4.game`-style allowed).
- Terms and registration screen warn against putting real personal data in nick.
- Account settings: change password, switch account, delete, second password,
  gifts; nick is never editable.
- Legal text (terms, privacy, consent) is present inside the game (`legal_text.inc`).
- Complaints are sent only from chat with `/жалоба Ник причина` (or `/report Nick reason`); there are no complaint buttons.
- `docs/gifts.md` reflects the reduced gift list (pumpkin removed).
- `docs/pc-version.md` notes headless build (`build/pc/cb4_headless`) works.

## What remains if the project grows

- If real names/e-mail/phone are added later, a Russian-hosted database and a
  public privacy URL become required again.
- If free chat stays, an ORI notification and Russian-hosted chat logs may be
  needed under 149-FZ.
- A public operator e-mail and privacy page URL are needed for store listings.
