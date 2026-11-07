# Disco Medusae — source provenance

- **Title:** Disco Medusae
- **Author / recording artist:** Kevin MacLeod (incompetech.com)
- **License:** [CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)
- **Official track:** https://incompetech.com/music/royalty-free/index.html?isrc=USUAN1500041
- **Official MP3:** https://incompetech.com/music/royalty-free/mp3-royaltyfree/Disco%20Medusae.mp3
- **ISRC:** USUAN1500041
- **Verified:** 2026-11-07. The official track page identifies the genre as Disco,
  length as 3:41, tempo as 115 BPM, and supplies the CC BY 4.0 attribution notice.

## Downloaded file

`Disco Medusae - Kevin MacLeod.mp3` is the complete recording obtained from the
public GitHub mirror `gelius7/sample-vue`, path
`public/audio/disco-medusae-complete.mp3`, pinned to commit
`3eeb305ca771d4fa3c4f6277c6f69a7d305e7e69` (Git blob
`7a97d3cee90d46669f963bc4191e7ab6e23d8b02`). The mirror's `public/audio/CREDITS.md`
identifies the official Incompetech source and the prior processing.

- Size: **3,541,898 bytes**
- SHA-256: `0957c02613a361cd89b3f95ab8d0e708ded2849cc28787202e7a46931ff4bace`
- Decoded duration: **221.283265 s**, 9,758,592 frames at 44,100 Hz
- Prior changes by the mirror: complete source progression/ending, constant gain
  ×0.91, 10 ms fade-in, 500 ms fade-out, MP3 re-encode at 128 kbps/44.1 kHz stereo.
  No looping, tempo change or pitch change in the source file.

The download was checksum-verified against the mirror's credits. This project
did not independently compare the mirror bytes with the official MP3 (the
official host was not directly downloadable in the build environment).

## Game conversion

`python3 tools/import_disco_music.py` verifies the source checksum and uses
FFmpeg only to decode/resample to **22,050 Hz stereo 16-bit PCM WAV**, because
the game's mixer does not decode MP3. It does not synthesize notes or generate
music. The complete recording is retained; the mixer loops it during event 1.

Output: `assets/audio/disco_medusae_kevin_macleod.wav`

- Size: **19,517,262 bytes**
- SHA-256: `70f4cf819003b19faa8d1d87745c7b2ca5a29d11792626be98f40e0a00e69b68`

Attribution travels with the game in `assets/audio/Disco-Medusae-NOTICE.txt`,
the full CC license in `assets/audio/licenses/`, the battle HUD, and page 9 of
the in-game legal viewer. The recording remains CC BY 4.0; the repository's
proprietary license does not replace that permission.
