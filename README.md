# Alone in the Dark (2008) — Enhanced PC Edition

**Preview: 0.1.0-rc1.** The runtime package is awaiting a complete gameplay check
of this public build; it is not a verified stable release.

[Download the preview installer](https://github.com/evilive04/aitd2008-enhanced-pc/releases/tag/v0.1.0-rc1)
and extract `AITD-Enhanced-PC-0.1.0-rc1.zip`. Run `Setup.exe`, then use the installed
`AITD Enhanced Launcher.exe`. The source-code archives are for development.
Read [installation requirements](README-INSTALL.md) first.

This unofficial PC enhancement project adds modern mouse/combat controls, free
cameras, an updated interface, SSS/GTAO/contact shadows, compatibility fixes and
an English custom launcher. It requires an owner's working, normally activated
original game. Its source and packaged preview omit original game executables,
archives, activation components, DRM-removal tools and extracted game content.

The public installation pipeline now prepares the original-derived interface
and texture changes locally. A complete installer integration prepared 91 mod
files, and the archive compiler produced six packs with 63 replacements. A
controlled stock startup with those compiled overrides opened all four redirected
A/C archive files successfully and ran near 117 FPS. This establishes that
bounded integration path; it does not establish every feature, gameplay physics,
all UI layouts or parity with the private development build. See
[release status](RELEASE_STATUS.md) for the exact evidence and remaining checks.

## Supported game and tools

Windows x64; original Steam application 259170 / build 132104, English executable
08.19.14. The supported original `Alone.exe` SHA-256 is
`d8a7370a1f4f2418cfeb2f22c0d2e9dafa59f606ff83fbb6fc8311ba84df4818`.
Normal licensing must already work. Other executable builds are rejected.

Build with Visual Studio 2022 C++ Build Tools and a Windows SDK. The launcher
uses Windows .NET Framework 4.8 / WPF. `tools/build-performance.cmd` builds the
project's x86 proxies, runs its included non-game contract checks and builds the
launcher; `tools/build-installer.cmd` builds the setup program. Own HLSL and
compiled project shader headers are included; original game shader bytecode and
Windows/DirectX binaries are not supplied.

Local asset preparation requires **64-bit Python 3.12+**, Pillow 13.0.0 and
NumPy 2.5.4. Install the pinned dependencies using
`python -m pip install -r requirements-install.txt`. The tutorial-keycap recipe
currently requires the exact supported Windows Segoe UI Bold font version;
different input files fail clearly rather than silently changing the artwork.

## Source and packaged Release

This repository contains source, semantic authoring recipes and approved project
artwork. It is not an installable Release folder. The runtime packager stages
only explicitly listed own build outputs, installer tools, recipes and separately
approved media. It neither copies an existing game directory nor publishes the
generated game-derived resources.

The packaged preview
uses **`Setup.exe`** to prepare and install local resources. Start the installed
game through **`AITD Enhanced Launcher.exe` in the selected game folder**.
[Preview installation instructions](README-INSTALL.md) describe requirements,
the setup flow, integrity records and current limits. Keep the existing private
development installation separate.

The installer checks for 107 required original filenames: six archive files,
100 movie files and `ShaderCache/Offline.sha`. It preserves original game files,
saves and settings, and refuses to overwrite existing mod files. It does not
download dependencies. Use a clean local game directory; UNC paths and paths
exceeding the supported Windows 260-character limit are rejected.

## Artwork, audio and locally generated content

The source includes the supplied logo, episode artwork, metallic cursor in PNG
and CUR formats, launcher artwork, and documented project slate/root materials.
Their rights records are in [ASSET_RIGHTS.md](ASSET_RIGHTS.md); these media are
outside the MIT source-code grant.

The owner-approved 4K menu video is 121,912,755 bytes and stays outside regular
Git history. The runtime package installs it as `enhanced-ui/menu-loop.mp4`.
The six approved derived map-icon sheets are separate Release inputs, kept
outside regular Git history and outside the installed payload. Their approved
source sheets are `map_icons_alpha.png` and `map_circular_noise_alpha.png`.
Source recipe metadata contains no map pixels; see `ASSET_RIGHTS.md` for the
owner's declaration and the six derived filenames.

Four native mouse/ammunition PNGs, enhanced VMFs and texture resources are read
or generated from the user's original installation during setup. Do not commit
or redistribute that local output, compiled archive caches or original fonts.
`tools/extract_runtime_ui.py`, `generate_texture_mods.py`, `generate_map_mods.py`
and `generate_ui_mods.py` implement the separate preparation stages with exact
source/output checks.

Three generated UI cues are embedded only in the project's renderer DLL for
use with the mod. The source contains no audio samples or standalone WAVs.
The Release defaults enable `ModernSounds`; a source build without supplied
licensed resources has no embedded cue samples. Provider restrictions apply;
see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Rights and credits

Original project code uses the MIT license. Original game content, names and
project/third-party media are outside that grant. This project is not affiliated
with or endorsed by the game's rights holders. OpenAI Codex/ChatGPT agents and
ImageGen assisted development; the owner supplied additional artwork/video.
Rights declarations, hashes and successful checks are not legal clearance or a
guarantee against third-party claims. Complete gameplay and visual verification
remain outstanding.
