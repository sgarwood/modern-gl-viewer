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

This first slice provides runtime playback and root-transform animation only. It does not yet skin
mesh vertices. A later skeletal-rendering slice will add weighted joint attributes, inverse bind
poses, joint palettes, GPU buffers, and skinned shader support. Blending, layers, events, and an
offline glTF/FBX conversion workflow also remain future work.

## Example

```cpp
const auto clip = engine.load_animation({
    .skeleton = "assets/golfer-skeleton.ozz",
    .animation = "assets/golfer-idle.ozz",
});
const auto player = engine.bind_animation(golfer_entity, clip);
engine.enqueue(mgv::PlayAnimationCommand{player});
```
