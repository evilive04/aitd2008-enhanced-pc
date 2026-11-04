# Native UI authoring recipes

These eight recipes describe the mod's vector interface with numeric triangle
coordinates, colors, named field operations, and references to the user's own
original resources. They contain no complete VMF, raster pixels, font records,
or native action bytecode. Original fill styles and identical triangle runs
are read from the authenticated original movie during local generation.
The 85 newly cropped native textured triangles are source-basis references with
exact rational geometry edit weights. Their original vertex coordinates and
UV fill styles are read locally. Remaining newly authored geometry is solid
vector chrome; no native bitmap pixels are represented as numeric art.

The codec deliberately provides no opaque byte insertion or binary delta
operation. Unchanged native data remains local. Mainmenu recap adapters append
our five labels and a Stop instruction; the handlers themselves come from the
user's original button records. Sprite frame offsets and extents are derived
from the generated structure.

Generate locally with:

```text
python tools/generate_ui_mods.py GAME_DIR OUTPUT_NEW_DIR
```

The new output contains game-derived resources and must not be published.
These recipes cover eight movies. Marker, lens, tutorial and water/title texture
generation are separate parts of the installation pipeline.

`derive_semantic_ui_recipes.py` is a maintainer authoring utility. It refuses
changes outside the supported semantic operations rather than inserting
unexplained native data. Final source/output hashes bind the exact reviewed
resource version. Hash checks do not establish copyright ownership; the own
geometry and style provenance must also be maintained when updating recipes.

The inventory output intentionally corrects a stale root sprite extent/frame
table in the legacy private pack. Independent canonical normalization changes
289 header bytes at absolute offsets88–501, retaining all102 frames and every
non-header record. The legacy and corrected hashes are recorded in the recipe.
This structural correction has technical evidence; no runtime/visual result is
claimed by these recipes.
