# Release status

**Source status: compatibility-in-progress.** The curated source is buildable.
The intended `0.1.0-rc1` runtime distribution is a preview awaiting user gameplay
checks, not a verified stable release or legal clearance certificate. The owner
approved the map artwork and its six derived PNGs for project distribution on
2026-11-03. The runtime preview has been built and content-checked locally: 64 explicit
release files, 18 installed payload inputs accepted by the packaged installer,
and valid ZIP checksums. No original game resources or standalone audio samples
are included. Complete gameplay checks remain outstanding.

## Technical checks performed by the project maintainer

- Original-Steam-only input and renderer x86 builds passed with `/W4 /WX`.
  Normal activation remains required; the supported original executable is not
  replaced or patched by the installer.
- Stock-ported archive compiler safety: 11 checks passed, including the latest
  isolated-tool run.
- Public source packager fixture: eight checks passed in its earlier batch.
  Final publication-candidate review remains separate from that fixture.
- Native backend: 33 checks passed, plus the executable-handle lock check.
- Launcher: 45 checks passed.
- Native frame-wait adapter: 42 compiled/emulated cases passed. Stock execution
  was separately observed in the startup integration described below.
- Installer preflight/safety: eight checks passed, including rejection of a
  missing original `ShaderCache/Offline.sha`.
- Complete local installer integration: 91 mod files prepared/installed in the
  owned integration fixture; original game files were preserved.
- Archive override compilation: six packs and 63 resource replacements passed.
- Texture generation: 49 water/title/lens/keycap resources and six map textures
  matched their accepted output SHA-256 values exactly.
- Native UI generation: seven VMFs matched accepted bytes exactly. The eighth,
  inventory, has the intentional independent structural correction described
  below.
- Four locally extracted native PNGs matched the references exactly after RGBA
  decoding. These original-derived PNGs remain local-only.
- Embedded UI cue decoding: three valid resources and two rejection cases
  passed. These checks cover the bounded decoder, not subjective audio quality.

These are technical results for the named checks. No visual or gameplay result
is inferred from a successful build, exact resource hash or fixture check.

## Inventory resource correction

The inventory movie retains all 102 frames and non-header records. Independent
canonical normalization fixes 289 stale header bytes at absolute offsets
88–501. The corrected output's expected SHA-256 is recorded in the semantic
recipe and was confirmed against the independent normalizer. This is an
intentional structural difference from the accepted private legacy pack, not
an unreported byte mismatch. Final appearance remains a user check.

## Original-game integration observations

Early loading independently verified the supported original executable before
native input initialization. The renderer and input proxy both admitted their
enhancements; native frame-wait hook installation and its owned-process branch
were observed. An earlier bounded probe became ready after about 5.3 seconds.

The latest complete stock/cache startup ran for 22.3 seconds under observation.
Four original A/C FAT/BIG opens were successfully redirected to the locally
compiled overrides, and the render loop ran near 117 FPS in that scene. The
maintainer then deliberately terminated that owned test. The original
`Alone.exe` SHA-256 remained unchanged and the test profiles were restored.

An earlier incomplete private test fixture crashed with an access violation
while missing shipped support data, including `ShaderCache/Offline.sha`.
Startup succeeded after restoring the missing original support files. Installer
preflight now requires 107 original filenames: six archives, 100 movies and
that shader cache. A manual PDF is not an installation requirement. This does
not establish that every possible crash is resolved.

These are bounded startup and archive-routing observations. They do not prove
all hook execution, core gameplay, 120 FPS physics in every chapter, camera and
combat feel, every UI layout, or complete parity with the private build. The
existing private development installation was left unchanged. Private logs,
captures, original support files and generated caches are not public artifacts.

## Publication and remaining checks

- Map distribution permission is recorded in `ASSET_RIGHTS.md`. Keep the six
  approved derived PNGs outside source Git history, as separate Release inputs.
- Fresh-source review passed for 403 explicitly selected files (about 16.2 MB). All four components compiled from that clean source tree; launcher self-tests passed 45 checks. Runtime PE policy accepted the four own components and rejected the original game for all four component roles. Final runtime packaging and artifact paths are tracked separately. Do not upload private history, original files, local VMF/XMT/cache output, standalone audio or captures.
- Hand off concise gameplay and visual checks of the combined public install:
  camera/control behavior, inventory and combining, shooting/throwing,
  progression and saving, high-frame-rate puzzles/cloth, and intended effects.
  The public build has not received that complete pass.
- Retain the source/build and media rights records, including the restrictions
  on the three embedded ElevenLabs UI cues. These records are not a legal
  opinion or guarantee against third-party claims.
- Public releases are marked as previews until the remaining gameplay checks
  are complete. See the repository's Releases page for downloadable artifacts.

Setup requires a clean local supported game folder, 64-bit Python 3.12+, pinned
dependencies, and the supported Segoe UI Bold font version. UNC paths and paths
past the supported Windows 260-character limit are rejected. These are known
preview constraints, not complete platform coverage.

The unchanged original executable remains a 32-bit process without the private
LAA change. Its ordinary 2 GiB address-space limit is retained. The bounded menu
probe is not evidence of memory stability throughout the story at high graphics
settings; longer gameplay and memory pressure remain unverified.

