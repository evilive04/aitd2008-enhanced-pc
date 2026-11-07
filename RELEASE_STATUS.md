# Preview verification — 0.1.0-rc5

This preview addresses fresh-install startup, chapter-loading memory pressure,
and a phone help-text mismatch. It remains a preview, not a completed story
playthrough or a guarantee that every original-game crash has been eliminated.

## Changes

- Generate the complete original menu credits locally during installation.
- Keep the native display probe intact and enable the guarded display correction
  that retained the selected resolution in the supported original Steam game.
- Select different physical CPU cores for the startup compatibility limit when
  the allowed processor topology permits.
- Match enhanced UI switches to their locally generated archive resources.
- Decode the unchanged 4K menu video in an owned, isolated x64 helper. The game
  maps the resulting NV12 frame instead of holding the decoder's large pools in
  its limited 32-bit address space. Stop it before native loading actions.
- Preserve usable navigation if video decoding fails, with a complete last frame
  or a clean backdrop; do not stretch or lower the resolution of video artwork.
- Display Q in the phone's Flashlight Tips when modern controls are enabled.
- Record both runtime helpers for safe removal by the uninstaller.

Original executables, archives, normal activation and saved games are not
replaced by the installer. Windows x64 and a working activated supported game
are required. The game itself remains 32-bit.

<details>
<summary>Technical evidence</summary>

All six own components were compiled with warnings treated as errors. The
source/build records, native x86/x64 component checks and positive Release
allowlist were checked. Installer fixtures cover the required video helper,
old removal receipts, changed-file preservation and path restrictions.

A real x86 client and x64 video worker passed 4K playback, pause/resume, restart,
parent-exit cleanup, missing-helper and terminated-worker checks. A deliberately
stalled owned worker was stopped at the bounded deadline and reported a timeout.
Abandoned-mutex tests reject incomplete frames persistently, including after a
diagnostic query. A sampled decoded frame was byte-identical to direct decoding
at the same timestamp. This is not an exhaustive comparison of every frame.

The client used about 14 MB of private memory in that isolated run; earlier
in-process decoder snapshots used approximately 268–299 MB. Shared frame memory
and the separate helper are additional memory. These runs used different
sampling and are not a controlled system-memory or performance benchmark.

A hidden D3D9 HAL test confirmed that a missing helper leaves an authenticated
menu drawable and ready for input. This does not replace game integration or
subjective visual acceptance.

The protected original game retained an actual 3840×2160 backbuffer and the
selected two-core mask on this PC. Earlier tested chapter entries include Fire
Escape, Down the Fissure, Parking and Call 911. The populated inventory, cabinet
F/hold-F, E preparation and Q cancellation, a third-person throw, GPS/M closing,
empty Sarah messages/back, Favorites cancellation and key-conflict responses
were exercised. A 30 FPS launcher setting produced about 29.8 FPS in an idle
4K Down the Fissure scene; complete frame-time percentiles were not recorded.

Launcher settings were also exercised at 60 and 120 FPS in the same idle 4K
Down the Fissure scene: approximately 59.0–59.2 FPS and 116.7–117.3 FPS,
respectively. These are short interval samples, not a full-game benchmark or
frame-time percentiles. The 30 FPS run used the native menu; the 60/120 FPS runs
used the isolated video player. Both enhanced-menu runs returned from the chapter
to an interactive video menu, then closed the game and both helpers normally.
The final 120 FPS run also cancelled New Game and Quit confirmations, with the
menu video resuming after each No. Phone Flashlight Tips visibly displayed Q.

</details>

## Remaining coverage

A complete story playthrough, every item/combination, high-speed driving,
all-resolution visual acceptance, subjective sound quality, all physics puzzles
and indefinite memory stability remain unverified in this public build.

Earlier long-session chapter loads failed at native executable offset
0x0057420a with fragmented address space. Releasing the old decoder before loading
allowed subsequent tested chapters to load. Its fault address alone did not
prove a complete crash cause. The newer isolated decoder addresses the mod's
memory cost; it does not enlarge the original game's address-space limit.

Use the issue tracker for reproducible problems. Include the episode, steps and
relevant logs after removing personal paths; do not upload original game files
or full process-memory dumps.
