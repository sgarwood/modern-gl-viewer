# Characters: where to get a golfer, and what it takes to wire one in

Two separate questions, and the second is the larger one. Written
2026-10-08.

## The short version

The animation runtime plays Ozz clips and exposes **only the root
transform**. `docs/animation.md` says so plainly and has from the start:

> This first slice provides runtime playback and root-transform animation
> only. It does not yet skin mesh vertices.

So a golfer cannot currently be put on screen as a golfer. It can be put on
screen as a rigid object that moves where the root joint moves. Four pieces
are missing, listed at the end.

The assets themselves are the easy half, and are free.

## Where the assets come from

### The character

| Source | Licence | Notes |
|---|---|---|
| [Mixamo](https://www.mixamo.com) | Free for commercial use with an Adobe ID; **may not be redistributed as standalone assets** | Auto-rigs a humanoid and applies clips from a preset library. The path of least resistance. |
| [Quaternius](https://quaternius.com) | CC0 | Low-poly rigged characters, and the Universal Animation Library — 250-odd retargetable clips across UAL 1 and 2, in FBX, glTF and OBJ. |
| [Kenney](https://kenney.nl) | CC0 | Rigged low-poly characters with basic clips. Stylised. |

CC0 is worth preferring where the look allows: it has no attribution or
redistribution strings, which matters if this is ever packaged.

### The swing

This is the hard asset, and it is worth being clear why: **Mixamo's library
has not grown in years and a golf swing is a signature move, not a staple.**
Do not count on finding one there.

The [CMU Graphics Lab Motion Capture Database](http://mocap.cs.cmu.edu/) has
golf swings and putts, captured at 120 Hz. Its licence allows inclusion in
commercially sold products but **not reselling the data directly, even in
converted form** — so a converted `.ozz` must not ship as a standalone asset
pack. The raw data is BVH and C3D; the
[4TU FBX conversion](https://data.4tu.nl/datasets/0448aab2-3332-449f-a8e2-d208cb58c7df)
saves a retargeting step, and
[rancidmilk's pack](https://rancidmilk.itch.io/free-character-animations)
bundles CMU motions already retargeted onto a Quaternius rig in glTF.

**But consider whether the swing matters at all.** This is a launch-monitor
simulator: the real swing happens in the room, and the ball's flight comes
from measured data, not from an animation. The clips the GDD actually needs
are the walk between shots, the idle while addressing, the crouch to lift
the ball from the cup, and standing to use the range finder. Those are
staples, and Mixamo or Quaternius UAL have all of them. The swing is
cosmetic and can come last.

### The club

Do not source one. Generate it, as the trees, grass and leaves already are:
a tapered shaft and a head is a `make_cylinder` and a few revolutions, it
costs no licence, and the loft and lie can then come from the club the
player actually selected rather than from whatever a modeller happened to
build.

It attaches to a hand joint, which needs the joint palette exposed — see
below — so the club is blocked on the same work as the character.

## Converting an asset

Ozz is already a dependency, and its glTF importer builds from it. It is off
by default because a runtime build has no use for it:

```sh
cmake -S . -B build/tools -DMGV_BUILD_ANIMATION_TOOLS=ON
cmake --build build/tools --target gltf2ozz
./build/tools/_deps/ozz-build/src/animation/offline/gltf/gltf2ozz --file=golfer.glb
```

That writes a skeleton archive and one archive per animation, which is
exactly what `AnimationAssetPaths` takes. Mixamo exports FBX; Ozz's FBX
importer needs Autodesk's SDK, which cannot be fetched, so convert FBX to
glTF in Blender first.

`gltf2ozz --config_dump_reference=config.json` writes the full importer
configuration, which is how clips get named, sampled and optimised
individually.

## What is missing in this codebase

**Items 1, 2, 3 and 4 below are now done**, and so are textures. A rigged
glTF loads with its images, its influences reach the vertex stage, the
palette is published, and `skinned.vert` blends it and samples the base
colour. What remains of this list is the record of what it took.

Textures travel inside a `.glb` as buffer views, so there is no path to hand
a file-based decoder; `ImageLoader::decode` takes bytes. They are decoded as
**sRGB**, because base colour is authored for display -- decoding it as
linear is the classic way a textured character comes out washed out. The
model's sampler is honoured rather than defaulted, and a model that brought
no texture is given a white pixel, because an unbound sampler reads as black
and would turn it into a silhouette.

In the order they had to happen.

### 1. A skinned mesh importer

`ObjLoader` is the only importer, and OBJ cannot express a skeleton or skin
weights at all — there is no such thing as a rigged OBJ. A glTF importer
that reads `JOINTS_0` and `WEIGHTS_0`, plus the inverse bind matrices, is a
prerequisite for everything else.

### 2. Joint attributes in the vertex layout

`Vertex` carries position, normal and texture coordinate. Skinning needs
four joint indices and four weights per vertex. Attribute slots 0 to 2 are
the vertex, and 3 and 4 are the per-instance placement added for the grass,
so joints and weights become 5 and 6. OpenGL guarantees at least sixteen.

### 3. The joint palette, exposed

This one is nearly free. `AnimationSystem` already runs Ozz's
`LocalToModelJob` and keeps a full array of joint model matrices per player;
it simply does not publish them. `root_transform` returns one of them. The
work is an accessor returning the palette, and a way to upload it — a
uniform array is enough for the joint counts a golfer has.

Exposing it also unblocks attaching the club, which is a socket on a hand
joint and needs nothing else.

### 4. Skinning in the shader

A linear blend over four weights, applied before the model matrix, in a
`skinned.vert` sitting alongside the existing shaders. The fragment side is
unchanged: a skinned character lights exactly like anything else.

## Authoring a pose

A pose is an animation with one key, so the runtime needs nothing new to
play one. What was missing was a way to write one down.

`-DMGV_BUILD_ANIMATION_TOOLS=ON` builds `mgv_pose_author`, which reads a
skeleton and a description and writes a one-key clip:

```sh
./build/tools/mgv_pose_author skeleton.ozz assets/poses/follow_through.json finish.ozz
```

Each instruction aims one bone along a direction **in model space**, then
twists it about itself. Model space because a rig's local joint axes are
whatever its exporter felt like -- CesiumMan's spine runs along its parent's
-X -- and guessing them is how an afternoon disappears. Twist because aiming
a bone leaves rotation about its own length undetermined, and for a golf
finish that rotation is most of the pose: the separation between open hips
and a more open chest is the whole shape.

`mgv_pose_author <skeleton.ozz>` with no pose lists the joints and their
rest positions, and a run with a pose prints where that pose actually put
them. Aiming bones is open loop -- nothing checks that two hands meant to
share a grip end up in the same place -- so the report is how the author
closes the loop. For the shipped rig it says: up +Y, the character's own
left +X, front -Z. The pole targets sit *behind* the knees, so inferring
the facing from them gets it backwards; the shoulder line under a known
twist is the reliable test.

Two things that cost time, recorded so they do not again:

- **Directions are in the finished frame, not the rest frame.** The body has
  turned to face the target, so the golfer's own left is no longer the rig's
  left. Writing the arms in the rest frame put the hands in front of the
  face.
- **Accumulated rotations must include each joint's rest rotation**, not
  just the aim applied on top of it. Leaving it out gives every joint below
  the root the wrong parent frame, and the error compounds down the chain
  into what looks exactly like a slouch.

- **The twist sign is not what the right-hand rule suggests.** On this rig
  a *negative* twist about the spine turns the golfer towards a target at
  +X. Reasoning it out produced the opposite every time; rendering two
  poses with the sign flipped settled it in one go, and is the cheaper
  move.
- **Check the proportions before choosing a shape.** This character's arms
  reach 1.56 against a shoulder height of 3.76, so a hand anywhere near the
  head sits very close to its own shoulder and the elbow has to splay. The
  textbook finish -- both hands together above the lead shoulder -- is
  simply out of reach for the trail arm here, which is a fact about the
  asset and not something tuning will fix.

### Retargeting a capture instead

`tools/retarget/retarget_bvh.py` runs under Blender and puts a BVH capture
onto this character; the shipped follow-through is CMU subject 64's golf
swing rather than anything authored here. The two skeletons agree on
nothing but anatomy, so it copies how far each bone has turned *from its
own rest* and applies that to the target's rest, in world space:

    delta       = source_pose * source_rest^-1
    destination = delta * destination_rest

Four things it has to get right, each of which broke it first:

- **Orientations only, never whole matrices.** An imported armature
  usually carries a scale on its object; feed that through the pose
  matrices and every bone it touches stretches.
- **The capture skeleton must not reach the exported file.** CMU names
  `Hips`, `Neck` and `Head` exactly as this rig does, and a runtime that
  binds a skin to a skeleton by name then picks whichever the importer saw
  last -- which drags the head off into the sky while leaving everything
  else correct, so it does not look like a naming problem at all.
- **Keys are rebased to frame one.** Written at their source frame numbers,
  a trimmed clip keeps its whole lead-in as dead air and the game plays
  that instead of the motion.
- **Plant the feet, not the hips.** The subject has different proportions
  and stood on a different floor. Pinning the pelvis leaves the feet
  hovering through the finish, when the legs are straightest; dropping the
  root until the lower foot is back at its rest height survives the change
  of proportions.

Hand-authoring reaches a recognisable posture quickly and a convincing one
slowly. For a swing proper, retarget motion capture -- see the CMU database
above -- and keep this for the static poses a round needs: addressing,
crouching at the cup, standing at the range finder.

## The default player

`assets/characters/` holds Quaternius' CC0 `Male_Casual`, which the default
course session stands by the ball. `tools/prepare_quaternius_character.py`
rebuilds the GLB from the Blender source: the model carries seven flat
colour materials, and a skinned character here draws with one, so the
script bakes those colours into a small palette texture and gives every
face a constant UV in its slot. `assets/characters/README.md` has the
commands.

`CourseCharacter::facing_offset_degrees` turns the model's authored forward
axis to face the engine's. Blender's -Y becomes glTF +Z, so an asset out of
Blender wants 180 here; without it a character walks the hole backwards.
It is separate from `facing_degrees` because one is a property of the asset
and the other is where the player happens to be looking.

The rig is 31 joints named the way Blender names them -- `UpperArm.L`,
`Palm.R`, `Hips`, `Torso` -- which is easier to author a pose against than
the model it replaced.

## Socketing a club

A prop held in a hand reads the same animation the hand does, rather than a
copy of it. `Engine::joint_transform(player, name)` gives a named joint's
current place in the skeleton's own space; multiply it by whatever world
transform the character is drawn with and the club stays in the hand for
free. `Engine::joint_names(clip)` says what there is to socket to -- an
unknown name comes back empty rather than throwing, because swapping in a
differently rigged asset is ordinary rather than a fault.

`assets/golf_club.stl` is read by `load_binary_stl`. STL is what CAD and
sculpting tools export when asked for something simple: triangles and a
face normal each, no indices, no texture coordinates, no materials. It
carries less than the OBJ and glTF importers expect, which is why it is
read separately rather than folded into either. Because it duplicates every
corner, a mesh read literally is faceted; the loader welds corners and
averages their normals, keeping any edge sharper than a crease angle, which
is what stops a club face melting into its sole.

`CourseClub` says which joint holds it and how it sits there. The offset is
in the club's own units and is applied **before** the club is turned and
scaled, so it can be read straight off the model -- "put z = 3 in the hand"
-- instead of being a post-rotation nudge that has to be re-derived every
time the rotation changes. The committed defaults suit this asset: shaft
along +Z, head at -Z, origin mid-shaft.

## Suggested order

Do **3** first, on its own. It is small, it is self-contained, and it lets a
procedurally generated club be attached to a hand joint and swung by an
animation — which proves the whole chain end to end against an asset we
generate ourselves, with no importer and no skinning in the way.

Then 1, 2 and 4 together, since a skinned mesh is useless without all three.
