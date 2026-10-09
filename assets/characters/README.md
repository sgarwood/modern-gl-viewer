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

## The golf swing

`quaternius_male_casual_swing.ozz` is motion capture, not hand animation:
take `64_08` from the Carnegie Mellon Graphics Lab Motion Capture Database,
subject 64, a golf swing. CMU's data is free for all uses; the BVH
conversion is Bruce Hahne's, released for free use worldwide.

  - Database: https://mocap.cs.cmu.edu/
  - BVH conversion: https://sites.google.com/a/cgspeed.com/cgspeed/motion-capture
  - Mirror the file was fetched from: https://github.com/una-dinosauria/cmu-mocap

The capture is retargeted onto this character by
`tools/retarget/retarget_bvh.py`. Keep the whole take from address through
the held finish. The launch-monitor session seeks to impact at runtime;
tools and other modes can therefore show the complete swing without carrying
two subtly different animation assets:

```sh
blender --background --python tools/retarget/retarget_bvh.py -- \
  assets/characters/quaternius_male_casual.glb 64_08.bvh swing.glb \
  1 150:250 180 in-place 220
gltf2ozz --file=swing.glb
mv GolfSwing.ozz assets/characters/quaternius_male_casual_swing.ozz
```

The arguments after the paths are the frame step, trim, yaw in degrees, root
mode, and impact frame. `in-place` removes the recording's global drift because
the course owns where the golfer stands. The script also plants the feet,
bakes a two-hand grip constraint, and reports the `0.5833` second runtime entry
used by `CourseCharacter::follow_through_start`. The trim keeps the stable
address through the balanced finish while dropping the capture's lead-in and
recovery step. The yaw turns the capture to face this course's target -- which
way the subject happened to hit is a property of the recording session.
