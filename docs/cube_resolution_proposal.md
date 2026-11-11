# Cube resolution proposal

Current sizes:
- `assets/textures/npc_cube.png` : 512 x 512 (cube NPC)
- `assets/textures/ebuc.png`, `ebuc_punch.png`, `ebuc_elf.png`, `ebuc_elf_punch.png` : 50 x 50 (fighters)

Proposed new resolution (~1.5 - 2 times smaller):

| Asset | Current | 1.5x smaller | 2x smaller (recommended) | Reason |
|---|---|---|---|---|
| `npc_cube.png` | 512x512 | 341x341 | **256x256** | Keeps readability, halves memory and load time |
| Fighter sprites (`ebuc*`) | 50x50 | 33x33 | **25x25** | Still visible on 540x960 screen, reduces texture load |

The 2x reduction is preferred: it aligns to clean pixel boundaries,
keeps the square proportion exactly, and is easy to generate with any
image tool (`python3 -m PIL Image.open(in).resize((256,256))`).

Implementation note: no code changes needed besides asset generation;
the `cube_scale` variable in `battle_fighter_look.inc` and `battle_render.inc`
scales the texture at draw time, so a smaller PNG just renders sharper.
