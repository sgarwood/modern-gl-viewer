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

## The follow-through

`quaternius_male_casual_finish.ozz` is motion capture, not hand animation:
take `64_01` from the Carnegie Mellon Graphics Lab Motion Capture Database,
subject 64, a golf swing. CMU's data is free for all uses; the BVH
conversion is Bruce Hahne's, released for free use worldwide.

  - Database: https://mocap.cs.cmu.edu/
  - BVH conversion: https://sites.google.com/a/cgspeed.com/cgspeed/motion-capture
  - Mirror the file was fetched from: https://github.com/una-dinosauria/cmu-mocap

The capture is retargeted onto this character by
`tools/retarget/retarget_bvh.py`, and trimmed to the stretch from impact to
the finish, because the round plays it on `long_shot_struck` and the ball
has already gone by then:

```sh
blender --background --python tools/retarget/retarget_bvh.py -- \
  assets/characters/quaternius_male_casual.glb 64_01.bvh finish.glb 1 322:450 180
gltf2ozz --file=finish.glb
mv GolfSwing.ozz assets/characters/quaternius_male_casual_finish.ozz
```

The arguments after the paths are the frame step, the trim, and a yaw in
degrees. Run without a trim and the script reports where the finish and
impact fall, which is how the numbers above were found: the hands are
highest at the finish and dip just before it, so impact is the low point in
the second preceding the global maximum. Searching the whole take instead
finds the address, where the hands are just as low and nothing has
happened. The yaw turns the capture to face this course's target -- which
way the subject happened to hit is a property of the recording session.
