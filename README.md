# Alone in the Dark (2008) — Enhanced PC Edition

## Install

1. [Download the mod](https://github.com/evilive04/aitd2008-enhanced-pc/releases/tag/v0.1.0-rc3), choose **AITD-Enhanced-PC-0.1.0-rc3.zip**, and extract the whole archive.
2. Close the game and run **Setup.exe**. Select your original Steam game folder.
3. If requirements are missing, approve their automatic download and setup. **Manual downloads** are also available.
4. Start **AITD Enhanced Launcher.exe** from the game folder, choose your settings, and play.

You need your own working, activated Steam copy and Windows x64. Use a clean
game folder without another installed mod. This is a **preview**; a complete
playthrough of this public build is still pending.

## What the mod adds

A PC-focused update that keeps the original game's atmosphere and mechanics.

- Modern mouse and keyboard controls for combat, throwing, inventory and vehicles.
- Free cameras, optional fixed-camera overrides and better first-person controls.
- A clearer interface with authentic item icons and updated menus.
- Subsurface scattering, ambient occlusion and contact shadows, without ReShade.
- Improved 120 FPS compatibility and fixes for high-frame-rate physics.
- An English launcher, optional intro skipping and continuous episode playback.

<details>
<summary>Requirements and manual installation</summary>

Setup asks before downloading anything. Python comes from python.org; Pillow
and NumPy come from PyPI. Packages go into a separate environment for the mod,
leaving your other Python packages unchanged. The Python installer is checked
for a trusted Python Software Foundation signature before it runs.

See [manual requirements](README-INSTALL.md#manual-requirements) for offline setup.
The supported game is the original English Steam executable 08.19.14.
Other executable versions are rejected. Setup also checks the required Windows font.
To remove the mod, run **uninstall mod.exe** in the game folder. Original game
files and saved games are kept; modified mod files require separate consent.

</details>

<details>
<summary>Troubleshooting and compatibility</summary>

See [installation and removal](README-INSTALL.md) and [technical verification](RELEASE_STATUS.md).
The game remains 32-bit; long-session memory stability and every story sequence
have not yet been verified. Technical checks do not replace a complete playthrough.

[Report a bug](https://github.com/evilive04/aitd2008-enhanced-pc/issues) with the
episode, what you did and the relevant log. Remove personal paths before sharing
logs. Do not attach original game files or full memory dumps.

</details>

<details>
<summary>Source code, licenses and credits</summary>

This unofficial mod is not endorsed by the game's rights holders. Original game
files, activation tools and DRM bypasses are not included. Required modified
game resources are generated locally from your own installation.

Own code is [MIT licensed](LICENSE). Artwork, video and embedded audio have
separate terms in [asset rights](ASSET_RIGHTS.md) and [third-party notices](THIRD_PARTY_NOTICES.md).
The owner supplied and approved the artwork and video. OpenAI Codex/ChatGPT
agents and ImageGen assisted development. These records are not legal clearance.

Developers: use Visual Studio C++ Build Tools and the Windows SDK; build scripts
are in `tools/`. The source archives are for development. The large menu video
and map-art build inputs are kept outside regular Git history.

</details>
