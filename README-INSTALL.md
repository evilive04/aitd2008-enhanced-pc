# Enhanced PC Edition — installation

1. Extract the runtime ZIP and run **Setup.exe**.
2. Select your original, activated Steam game folder.
3. Click **Install mod** and approve missing requirements if needed.
4. Use **AITD Enhanced Launcher.exe** in the game folder to play.

Version **0.1.0-rc4** is a preview. Complete gameplay acceptance remains pending.
The requirements, removal instructions and developer details are below.

## Manual requirements

- Windows and your own working, normally activated Steam installation of
  Alone in the Dark (2008).
- The supported original `Alone.exe`: SHA-256
  `d8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818`.
- [64-bit Python 3.12+](https://www.python.org/downloads/windows/),
  [Pillow 13.0.0](https://pypi.org/project/Pillow/13.0.0/) and
  [NumPy 2.5.4](https://pypi.org/project/numpy/2.5.4/).
  You may approve automatic setup instead. For manual/offline use, install
  the packages in the Python environment selected in Setup:

  ```text
  python -m venv aitd-setup-env
  aitd-setup-env\Scripts\python.exe -m pip install -r requirements-install.txt
  ```

  Select `aitd-setup-env\Scripts\python.exe` in Setup and keep that environment
  while using the mod. This avoids changing packages in your normal Python.

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
   `Alone.exe`, and optionally select your installed 64-bit Python interpreter.
   Leave Python empty to let Setup offer to install it.
3. Choose **Install mod**. The installer reads the original archives to prepare
   the interface and texture changes locally. It also checks the exact original
   resources and approved generated outputs. Do not upload these locally
   generated game-derived resources.
4. When installation finishes, open **`AITD Enhanced Launcher.exe` in the game
   folder** to choose your settings and start the game.

If requirements are missing, Setup asks before downloading them. Declining
stops this step without downloads; use the Manual downloads button instead.
Python 3.14.10 x64 comes directly from python.org and must have a trusted Python
Software Foundation signature before execution. It is installed for the current
Windows user without changing PATH. Pillow and NumPy are installed from PyPI
as prebuilt wheels in a new isolated environment under
`%LOCALAPPDATA%/AITD2008Enhanced/setup/environments`; your existing packages are
not upgraded or replaced. Keep this environment while using the mod: the
installed resource compiler records its interpreter path. Downloads need an
Internet connection and disk space. Failed downloads can be retried; no game
files are copied until requirements pass. Downloaded Python installers are
retained under the adjacent `downloads` folder and may be removed afterward.

 Unsupported original game
files, missing dependencies or damaged project graphics cause an explicit
failure. Tutorial keys use the included project graphics, with no dependence
on the version of Segoe UI installed in Windows. The graphics use OFL Noto Sans
Bold and preserve the key layout, native mouse cells and texture mapping.

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
the mod files; preserve it for troubleshooting. Run **uninstall mod.exe** in the game folder to remove the mod. Close the game,
launcher and Setup first. The uninstaller reads this receipt, preserves original
game files and saved games, and removes its own recognized generated archive
cache. Modified mod files are kept by default; explicitly select the option to
remove them too if you want to discard your mod settings. Unknown files and
other mods are preserved. If files are kept or locked, the receipt and uninstaller
remain available for review and retry. After successful removal the uninstaller
and receipt remove themselves using a bounded temporary Windows helper.

Python, its package environments, downloaded installers and diagnostic logs
outside the game folder are preserved, because they may be shared with another
installation. This does not reset activation or delete Windows fonts. Keep the
receipt until removal completes.

The included uninstaller is installed starting with rc3 and requires that
version's receipt. Do not copy it into an older installation or delete its
receipt manually; preserve the older receipt and ask for migration/removal help.

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
