# Animation runtime

`mgv::animation` is a renderer-independent runtime animation bounded context. It uses Ozz Animation
internally while exposing only project-owned types. Ozz headers and objects do not cross the public
API, so frontends and future Vulkan or Direct3D render adapters remain independent of the animation
library.

## Assets and ownership

An `AnimationAssetPaths` value identifies a runtime Ozz skeleton archive and animation archive.
`AnimationSystem::load()` validates that the skeleton contains joints and that its joint count
matches the clip's track count. Asset conversion is deliberately offline: applications load
compact `.ozz` files rather than importing authoring formats during a frame.

Loading returns a process-unique `AnimationClipId`. A clip can create any number of independently
controlled players, each identified by a process-unique `AnimationPlayerId`. Removing a player
never causes a stale identifier to alias another player.

`AnimationSystem` is a move-only Pimpl façade. It owns Ozz skeletons, clips, sampling contexts, and
pose buffers through RAII. Player storage is private, and callers cannot retain references that an
erase could invalidate.

## Playback contract

New players are stopped at time zero. `play`, `pause`, `stop`, `seek`, and playback-rate changes are
explicit operations:

- `stop` returns the player to its initial pose.
- `seek` clamps to the inclusive clip range, so seeking to the duration samples the end pose.
- Playing past the duration loops to the start.
- Playback rates must be finite and positive.
- Elapsed and seek durations must be finite and non-negative.

The engine exposes the same controls as typed `EngineCommand` alternatives. Frontends and network
decoders enqueue those commands; mutation occurs when `Engine::tick()` drains the queue. Playback
then advances using the engine's injectable monotonic `Clock`, which makes tests deterministic.

## Entity binding

`Engine::bind_animation()` associates one player with one entity. The sampled root-joint pose owns
that entity's complete render transform. Animation and physics bindings are mutually exclusive,
preventing tick order from silently deciding which subsystem wins.

## Skinning

Meshes are skinned. `GltfLoader` reads a rigged glTF -- OBJ cannot express a skeleton at all --
and produces an `ImportedSkin` beside the geometry, holding one inverse bind matrix and one
**name** per joint.

Names, not indices. A mesh and a skeleton are normally converted by different tools and have no
reason to order their joints alike; `Engine::bind_skin` resolves the skin's joints against the
skeleton's by name, once, at bind time, and refuses a skin naming a joint the skeleton lacks.
Matching by index instead gives a character that animates almost correctly, which is far harder to
diagnose than one that does not animate at all.

Each frame the engine multiplies every joint's model matrix by its inverse bind and hands the
result to the renderer as that renderable's palette. The product is the identity at the rest pose,
which is the single most useful invariant to assert when wiring skinning up: a character that is
mangled before it has been animated has this wrong.

Skinning influences live in a stream of their own rather than widening `Vertex`, because almost
nothing in a golf course is skinned and terrain, grass, trees and leaves would otherwise each pay
twenty bytes a vertex for data they never use. The backend binds them to attributes 5 and 6, and
only when the mesh has them.

The palette is a plain uniform array, so its size is bounded by the vertex stage's uniform budget.
The backend reports what it can take as `RenderBackendCapabilities::max_skinning_joints`,
reserving a quarter of the budget, which clears a humanoid rig comfortably.

Blending, layers, events, and inverse kinematics remain future work, as does attaching equipment
to a joint -- though the palette that needs is now published.

## Converting assets

`-DMGV_BUILD_ANIMATION_TOOLS=ON` builds Ozz's `gltf2ozz` from the dependency already fetched. It is
off by default: a runtime build has no use for an offline converter. See
[`docs/characters.md`](characters.md) for where to source a character and what its licence allows.

## Example

```cpp
const auto clip = engine.load_animation({
    .skeleton = "assets/golfer-skeleton.ozz",
    .animation = "assets/golfer-idle.ozz",
});
const auto player = engine.bind_animation(golfer_entity, clip);
engine.enqueue(mgv::PlayAnimationCommand{player});
```
