# Installer revision 0.1.0-rc3

This revision adds uninstall mod.exe and explicitly forwards generator stderr
into the setup log. The reported texture-preparation failure on another PC is not
diagnosed by its generic child-exit traceback: its underlying cause remains
unknown until an affected-machine log with these diagnostics is available.
An exact Windows-font mismatch is only a candidate explanation. Validation and
artwork design are not relaxed as a substitute for finding the actual cause.

The game DLLs and launcher remain unchanged. The same rebuilt Setup binary is
installed under uninstall mod.exe; its filename selects the removal UI. Removal
uses the bounded installed-file receipt, preserves modified files by default,
removes only recognized own cache generations and cleans up itself after exit.
Unknown files, original game data, saves and shared outside-game prerequisites
are preserved.

<details>
<summary>Targeted installer/uninstaller checks (rc3)</summary>

The final production Setup/uninstaller built with warnings treated as errors.
Real successful/failing Python subprocesses verified merged stdout/stderr,
UTF-8 diagnostics, exit codes and a preparation failure before game copying.
Exact font identity, mismatch reporting and missing-font reporting passed.

Production compiled removal methods passed actual file-handle deletion,
receipt/path/device-name validation, modified-file preservation and opt-in
removal, original/custom/other-mod preservation, known cache cleanup and
rejection of a real test junction. An actual Python installation commit in a
synthetic fixture generated a receipt consumed by the production remover.

The first final-helper test found a missing PowerShell command dependency.
After replacement with direct .NET operations, repeated actual helper-process
checks passed with an empty PSModulePath: wait for the parent, remove own files,
remove the temporary helper, and preserve files if receipt or EXE changed.

All checks used isolated synthetic fixtures. The fixture's EXE verification was
substituted with a sentinel identity; the real game was not launched or changed.
Actual uninstaller GUI clicking, its running-EXE image-lock lifecycle, and a
complete install/remove flow on the affected computer were not tested. The
texture failure's underlying cause remains unknown pending its new log.

</details>

<details>
<summary>Earlier Setup checks and limitations (rc2)</summary>

# Setup revision 0.1.0-rc2

This revision changes Setup and player documentation. The game DLLs and launcher
are unchanged from rc1. Setup now requests consent for official dependency
downloads, creates isolated package environments and uses an automatic layout
instead of fixed-height controls. Complete gameplay acceptance remains outstanding.

<details>
<summary>Setup revision checks and limits (rc2)</summary>

The production Setup built with warnings treated as errors. Environment parsing
and six real Windows argument round trips passed. The production downloader
retrieved the official Python 3.14.10 x64 installer and accepted its trusted PSF
signature; unsigned data and a valid signature from another publisher were
rejected. Actual pinned Pillow/NumPy wheels installed and imported in an isolated
QA environment, with the base interpreter's recorded package presence unchanged.

A native WinForms harness instantiated the actual form and checked control
bounds and preferred text sizes with proportional geometry and fonts at 1x,
1.5x and 2x, including the additional post-install button. This is measured
layout evidence, not separate sessions at changed Windows DPI settings.

The consent-refusal automation timed out twice. Actual interactive No behavior
and runtime network inactivity on that path remain unverified. Source inspection
confirms that No exits before downloader/pip calls. The full affirmative GUI
flow and execution of the real Python installer on a clean Windows profile were
not tested; no Python installer was run into the user's profile and no game
session was launched for this revision. The earlier rc1 backend integration
results below do not prove this new complete UI workflow.

</details>

<details>
<summary>Baseline technical checks and known limits (rc1)</summary>

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


</details>

</details>
