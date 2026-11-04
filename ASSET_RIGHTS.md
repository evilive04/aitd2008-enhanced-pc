# Asset rights and provenance

On 2026-11-03 the project owner stated that the supplied 4K menu video, logo and
episode artwork are theirs and may be publicly distributed. The owner separately
confirmed distribution rights for the metallic cursor. These statements record
the owner's representation, not an independent title search or legal opinion.

On the same date the owner explicitly answered “Ano, můžeme je šířit” for the
supplied `map_icons_alpha.png` and `map_circular_noise_alpha.png`. This approval
covers project distribution of their six derived `new-*.png` map sheets listed
below. It is recorded as the owner's declaration, not independent legal review.

The approved project media may accompany this project's distribution. They are
**outside the MIT source-code grant**. This document grants no separate license
to reuse them outside this project; contact the maintainers before such reuse.
Original game names and marks remain their holders' property. Supplied artwork
does not imply official endorsement or transfer the underlying game marks.

## Included project artwork

- `src/launcher/assets/background.jpg`, `logo.png`, `launcher.ico` and the
  documented A-icon source/generation records: supplied project launcher art.
- `assets/ui/aitd_logo.png`: owner-approved runtime logo.
- `assets/ui/episode_cards.png`: owner-approved episode artwork.
- `assets/ui/cursor_metal.png` and `cursor_metal.cur`: approved metallic cursor
  for software rendering and the Windows system cursor.
- `assets/ui/slate.png` and `roots-corner.png`: project ImageGen materials
  documented in `assets/ui/PROVENANCE.md`.

## Separate video and approved map artwork

The approved `menu-loop.mp4` is 121,912,755 bytes, above GitHub's regular
100 MiB file limit. It stays outside source Git history and is an explicit
runtime Release input, installed at `enhanced-ui/menu-loop.mp4`.
`MEDIA_MANIFEST.json` in the local approved-media staging folder records selected
media hashes; it is an integrity record, not a title certificate.

The six approved derived map-icon sheets are:
`new-530.png`, `new-534.png`, `new-536.png`, `new-538.png`, `new-541.png` and
`new-545.png`. They may accompany this project's Release under the owner's
declaration above. They are outside MIT and regular source Git history. Only
their metadata and hashes are present in the source recipes. No separate
general reuse license is granted by this document.

The explicit packager expects these six PNGs under an approved media directory's
`map-icons` folder. Release staging keeps
them in `assets/map-icons`, outside the installed payload. Setup reads the
user's original texture headers locally to create the matching map resources;
no original game header or completed XMT is distributed.

## Embedded generated UI cues

Three cues generated through fal.ai / ElevenLabs Sound Effects V2 are selected
for combined use with this mod. They are embedded only in the own renderer DLL,
using resource IDs 301, 302 and 303 for hover, confirm and back. The source tree
contains the integration code, not the sample bytes, external generated resource
objects or standalone WAVs. Release defaults set `ModernSounds=true`.

The cues are outside MIT. Their release scope is combined use in this mod's
application/DLL, not isolated audio files, an audio library or separate sample
reuse. Preserve the provider notices and restrictions in
`THIRD_PARTY_NOTICES.md`. Commercial-use labeling on a model page does not
supersede applicable account, provider or reseller terms. This project does not
grant independent reuse rights in the generated samples.

## Original-game and locally derived materials

Original game executables, archives, shaders, movies, credits, captured fonts,
full modified VMF/XMT files and native icon images are not supplied or licensed
by this project. Setup reads a supported owner's game and generates the required
resources locally. Four native UI PNGs, enhanced interface/texture output and
compiled archive caches remain local-only and must not be uploaded.

Source semantic recipes contain authoring operations and references; preserved
original content is read from the local original resources. Correct hashes,
clean file selection and successful tests are not independent rights clearance.
No document here guarantees immunity from copyright, trademark, contract or
other third-party claims.
