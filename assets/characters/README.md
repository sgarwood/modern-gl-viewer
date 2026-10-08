# Quaternius Male Casual

The default player is the `Male_Casual` character from Quaternius' CC0
LowPoly Models pack. The checked-in GLB has the original rig and `Man_Idle`
animation. Its seven flat-colour materials are represented by one embedded
palette texture so the runtime can draw the skinned character in one pass.

To rebuild the GLB from the source Blender file:

```sh
blender --background Male_Casual.blend \
  --python tools/prepare_quaternius_character.py -- \
  assets/characters/quaternius_male_casual.glb
```

Build the Ozz glTF importer as described in `docs/characters.md`, then run it
from this directory:

```sh
gltf2ozz --file=quaternius_male_casual.glb
mv skeleton.ozz quaternius_male_casual_skeleton.ozz
mv Animation.ozz quaternius_male_casual_idle.ozz
```

See `LICENSE.txt` for the source and CC0 dedication.
