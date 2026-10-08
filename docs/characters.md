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

Two things that cost time, recorded so they do not again:

- **Directions are in the finished frame, not the rest frame.** The body has
  turned to face the target, so the golfer's own left is no longer the rig's
  left. Writing the arms in the rest frame put the hands in front of the
  face.
- **Accumulated rotations must include each joint's rest rotation**, not
  just the aim applied on top of it. Leaving it out gives every joint below
  the root the wrong parent frame, and the error compounds down the chain
  into what looks exactly like a slouch.

Hand-authoring reaches a recognisable posture quickly and a convincing one
slowly. For a swing proper, retarget motion capture -- see the CMU database
above -- and keep this for the static poses a round needs: addressing,
crouching at the cup, standing at the range finder.

## Suggested order

Do **3** first, on its own. It is small, it is self-contained, and it lets a
procedurally generated club be attached to a hand joint and swung by an
animation — which proves the whole chain end to end against an asset we
generate ourselves, with no importer and no skinning in the way.

Then 1, 2 and 4 together, since a skinned mesh is useless without all three.
