# Third-party notices

Alone in the Dark (2008), its original code, artwork, characters, media and marks
belong to their respective rights holders. This unofficial project does not
supply or license those materials. Legitimate ownership and normal game
activation are required; no alternative activation or DRM-removal method is
provided.

## Platform and local preparation dependencies

Windows, DirectX, Windows fonts, .NET Framework and the Windows SDK are external
dependencies. Their files are not redistributed. Own HLSL and compiled project
shader headers are included; original game shader bytecode is excluded. The
original `ShaderCache/Offline.sha` remains a local game prerequisite, not a
Release file. The keycap recipe checks the supported local Segoe UI Bold version
and does not bundle font files.

Pillow 13.0.0 and NumPy 2.5.4 are installed separately for local asset generation;
they are not relicensed under this project's MIT grant. Pillow's own license
is documented in its [official license notice](https://pillow.readthedocs.io/en/stable/about.html#license).
NumPy's license applies to NumPy. The pinned [NumPy 2.5.4 release](https://pypi.org/project/numpy/2.5.4/)
requires Python 3.12+, while [Pillow 13.0.0](https://pypi.org/project/pillow/13.0.0/)
requires Python 3.11+; the complete setup therefore requires 64-bit Python 3.12+.

The bounded LZO reader cites the Linux kernel format description and is a
project implementation rather than a port of Linux or MiniLZO source.

## Generated UI sound effects — combined use only

Three UI cues were generated through fal.ai using ElevenLabs Sound Effects V2.
The [official fal model page](https://fal.ai/models/fal-ai/elevenlabs/sound-effects/v2)
labels the model for commercial use. The reviewed
[ElevenCreative terms](https://elevenlabs.io/creative-terms), updated
2026-10-09, Exhibit 4 section 2, describe sound-effect use combined with other
material, including games/apps, and restrict isolated audio-file or audio-library
distribution. Applicable provider, account and reseller conditions must also be
retained; the model-page label alone is not an unrestricted sample license.

This project's distribution scope is the three cues embedded in its own renderer
DLL as part of the mod, not standalone audio files, sample packs or sound-effect
libraries. Embedded resource IDs 301, 302 and 303 provide hover, confirm and back.
The public source contains no audio samples or standalone WAV/resource objects.
Do not publish the external build-input WAVs or resource objects. Do not extract
and repackage the cues as separately reusable audio. The cues are outside MIT,
and this project grants no independent sample-reuse license.

The Release defaults enable `ModernSounds`; source builds without the separately
supplied licensed resources do not acquire sample content from this repository.
The notice records the selected release scope and reviewed terms, not legal
approval or a guarantee against third-party claims.

## Project media and development disclosure

The project owner supplied additional artwork/video and confirmed distribution
rights for the listed logo, episode images, video and metallic cursor. On
2026-11-03 the owner also approved the two supplied map sheets and their six
derived icon PNGs for project distribution, outside regular source Git history; see
[ASSET_RIGHTS.md](ASSET_RIGHTS.md). Project media are outside the MIT code grant.

OpenAI Codex/ChatGPT agents assisted code development, including GPT-6.1 Sol and
Terra during technical QA. Project slate/root artwork and some graphical
materials were generated using OpenAI ImageGen. Model-generated output, provider
labels and owner declarations do not by themselves certify third-party rights.
