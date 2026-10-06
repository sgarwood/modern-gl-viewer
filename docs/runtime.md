# Engine runtime

`mgv::runtime` is the application-service layer that composes rendering, input, physics, and
networking without moving those policies into a frontend. `Engine` is a move-only Pimpl façade;
Qt and GLFW create the platform render backend, enqueue inputs, and call `tick()` with the current
framebuffer size. They do not own or directly update a `Renderer`, camera controller, or physics
world.

## Stable identity

`RenderableId`, `EntityId`, `BodyId`, `AnimationClipId`, and `AnimationPlayerId` are distinct value
types so identifiers from different domains cannot be mixed accidentally.

- `Scene::add()` assigns a process-unique `RenderableId` and never reuses it.
- Renderer updates and removal use `RenderableId`, not a vector offset.
- `Engine` associates each loaded renderable with an `EntityId` that is never reused by that
  engine instance.
- An entity can optionally bind one `BodyId`; removing the entity removes both bindings.
- An entity can instead bind one `AnimationPlayerId`; physics and animation cannot both own the
  same transform.
- Stale or foreign handles are rejected rather than silently addressing a different object.

Dense vectors remain an implementation detail for cache-friendly traversal. Erasing one object may
move another internally, but its public handle remains unchanged.

## Deterministic tick

`Clock` is a dependency-inversion port. Production uses a steady monotonic clock; tests inject a
clock whose time advances explicitly. A tick executes in this order:

1. Poll up to the per-tick network-event budget.
2. Decode each event into typed `EngineCommand` values on the calling thread.
3. Drain queued input, transform, impulse, and animation playback commands.
4. Advance animation players using elapsed clock time and copy their root poses to bound entities.
5. Advance `PhysicsWorld` using elapsed clock time and its fixed-step accumulator.
6. Copy bound rigid-body positions into their entity transforms.
7. Render once using total clock time and the supplied viewport.

Input callbacks only enqueue commands. State therefore changes at a tick boundary rather than
inside a toolkit callback. `NetworkService` can continue owning socket I/O on its worker, while the
`NetworkEventDecoder` and all engine mutation execute on the main/render thread.

The runtime is currently single-thread-affine: `enqueue()`, `set_scene()`, and `tick()` are expected
on the owning frontend thread. Cross-thread traffic must enter through the bounded queues owned by
`NetworkService` or another adapter implementing `NetworkEventSource`.
