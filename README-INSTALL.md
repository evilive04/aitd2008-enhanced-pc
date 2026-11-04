# Enhanced PC Edition — preview installation

Version **0.1.0-rc1** is a compatibility preview awaiting user gameplay checks.
The installer integration prepared 91 mod files; six override packs with 63
replacements compiled successfully. A controlled original-Steam startup opened
the compiled A/C archive overrides and ran near 117 FPS. Full gameplay, all
interface layouts and parity with the private development build have not been
established. Read `RELEASE_STATUS.md` for the evidence and limits. This document
describes the packaged preview workflow. Download the runtime ZIP from the
[preview release](https://github.com/evilive04/aitd2008-enhanced-pc/releases/tag/v0.1.0-rc1),
not GitHub's automatically generated source-code archives.

On 2026-11-03 the owner explicitly approved project distribution of
`map_icons_alpha.png` and `map_circular_noise_alpha.png`, including their six
derived icon sheets. They are outside the MIT source license and regular Git
history. The source contains only recipe metadata and hashes; the approved
PNGs are separate Release build inputs. See `ASSET_RIGHTS.md`.

## Requirements

- Windows and your own working, normally activated Steam installation of
  Alone in the Dark (2008).
- The supported original `Alone.exe`: SHA-256
  `d8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818`.
- 64-bit Python 3.12 or newer, Pillow 13.0.0 and NumPy 2.5.4. Install these
  dependencies into the Python environment selected in Setup:

  ```text
  python -m pip install -r requirements-install.txt
  ```

Keep a normal backup of your game and saves. Close the game before installing.
Use a clean installation: Setup refuses to overwrite existing mod files.
The game folder must retain its 107 required original files: six archive files,
100 movie files and `ShaderCache/Offline.sha`. Setup checks their presence;
exact original resource checks run during local preparation. A game manual PDF
is not required. Use a local path: UNC paths and paths exceeding the supported
Windows 260-character limit are rejected.

## Install the packaged Release

1. Extract the complete Release archive to a separate folder. Keep `Setup.exe`,
   `PAYLOAD_MANIFEST.json`, `tools`, `release`, `configs`, `assets` and `payload`
   together.
2. Start `Setup.exe`, choose the game folder containing the original
   `Alone.exe`, and select your installed 64-bit Python interpreter.
3. Choose **Install mod**. The installer reads the original archives to prepare
   the interface and texture changes locally. It also checks the exact original
   resources and approved generated outputs. Do not upload these locally
   generated game-derived resources.
4. When installation finishes, open **`AITD Enhanced Launcher.exe` in the game
   folder** to choose your settings and start the game.

The setup program does not download dependencies. Unsupported original game
files, missing dependencies or an unsupported Windows UI font cause an explicit
failure rather than silently substituting different artwork. The tutorial-keycap
recipe currently requires the exact supported Segoe UI Bold font version.

The command-line equivalent, using the same selected Python, is:

```text
python tools/install_mod.py GAME_DIR --distribution RELEASE_DIR
```

No original game executable, archive or activation component is distributed or
replaced. Existing saved games and game settings are preserved. Normal game
licensing and activation remain required. This preview does not provide DRM
removal, an executable patcher or activation assistance.

## Settings and removal

Use the English custom launcher for the supported settings. Installed defaults
are in `aitd-enhanced.ini`. The installed `.enhanced-install.json` receipt lists
the mod files; preserve it for troubleshooting. No automatic uninstaller is
included in this preview. To remove the mod manually, close the game and remove
only the mod files listed in that receipt, preserving saves and original files.
Ask for help if you have added or changed files since installation.

The supplied defaults enable `ModernSounds`. The three UI cues are embedded in
the project's renderer DLL for combined use with this mod; they are not separate
audio downloads and are outside the MIT license. Preserve the restrictions in
`THIRD_PARTY_NOTICES.md` when redistributing a packaged preview.

## Maintainer packaging

The source tree by itself is not an installable Release. A maintainer builds the
four project components, records their hashes and source hashes in a local
build-provenance manifest, and packages only the explicit runtime allowlist:

```text
python tools/package_runtime_release.py --source-root SOURCE_DIR --media-root APPROVED_MEDIA_DIR --provenance SOURCE_DIR/build/release-provenance.json --output-dir NEW_OUTPUT_DIR
```

The source provenance contains `schema: 1`, `kind: "local-source-build"`, the
supported `target_exe_sha256`, and a `sources` object mapping every public
`src/` file plus the four build scripts to `bytes` and `sha256`. The `components`
object contains exactly `renderer`, `input`, `launcher` and `setup`, each with
its fixed build-relative `path`, `bytes` and `sha256`. This records a selected
local build; it is not a digital signature or proof of authorship. The packager
also checks component PE metadata and project exports or managed identities.

The new output contains `Release` and its ZIP. `PAYLOAD_MANIFEST.json` lists
installed files, while `RELEASE_MANIFEST.json` covers the shipped content. These
hashes provide integrity checks, not a legal clearance certificate or evidence
that every feature runs correctly. Publish neither a failed staging folder nor
any private build history, original archive, capture, crash dump or locally
generated game-derived asset.

The approved media directory must contain `menu-loop.mp4` and the six approved
derived map sheets: `map-icons/new-530.png`, `new-534.png`,
`new-536.png`, `new-538.png`, `new-541.png` and `new-545.png`. All seven inputs
are pinned to exact reviewed hashes. The six PNGs are staged under
`Release/assets/map-icons`, outside the installed payload; Setup uses them to
prepare map textures locally. No generated XMT is distributed. Missing or
different artwork causes packaging to stop instead of selecting other files.

The renderer may use the externally supplied licensed WAV resources only at
build time. The public source contains no samples, generated resource object or
standalone WAV. Distribute the reviewed combined-use DLL and notices rather
than publishing those audio inputs.

## Rights

This is an unofficial mod, not an endorsed game edition. Original project code
uses the MIT license. Owner-approved artwork and video are outside the MIT
grant; see `ASSET_RIGHTS.md` and `THIRD_PARTY_NOTICES.md`. Original game names
and marks remain their holders' property. No legal guarantee is offered.
