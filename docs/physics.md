# Physics domain

The optional `mgv::physics` target is a renderer-independent bounded context. Its public API uses
SI value objects rather than bare scalar arguments:

- `Duration` is measured in seconds.
- `Length`, `Position`, and `Dimensions` are measured in metres.
- `Mass` is measured in kilograms.
- `LinearVelocity` and `Acceleration` state their derived units explicitly.
- `Impulse` is measured in newton-seconds and is applied through the world aggregate.

`PhysicsWorld` is the aggregate root. It owns `RigidBody` entities, assigns stable `BodyId` values,
runs a fixed timestep, and publishes the collision manifolds from the last completed substep.
`RigidBodyBuilder` is the only way to construct a body definition, keeping mass, collider, and
restitution invariants at the domain boundary.

Collision detection follows the Strategy pattern. `PhysicsWorld` depends on the abstract
`CollisionDetector` port and uses `DiscreteCollisionDetector` by default. Tests or future engines
can inject another implementation without changing the world. The built-in detector supports
sphere-sphere, axis-aligned box-box, and sphere-box pairs.

The MVP uses semi-implicit Euler integration, positional penetration correction, and a normal
restitution impulse. Only dynamic bodies integrate gravity; static bodies have zero inverse mass.
An accumulator makes results independent of frontend frame chunking, while `maximum_substeps`
prevents an unbounded catch-up loop.

Example:

```cpp
const auto ball_entity = engine.set_scene(std::move(scene)).front();
const auto ball_body = engine.bind_physics(
    ball_entity,
    mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::sphere(mgv::physics::Length{0.5F})}
        .at(mgv::physics::Position{{0.0F, 5.0F, 0.0F}})
        .mass(mgv::physics::Mass{1.0F})
        .restitution(0.7F)
        .build());

engine.tick({framebuffer_width, framebuffer_height});
```

`Engine` retains the `BodyId`-to-`RenderableId` binding and copies the simulated position to the
render transform after each fixed physics step. The returned `ball_body` handle remains available
to domain code that needs to identify the body.

This first slice deliberately excludes angular motion, friction, constraints, continuous collision
detection, sleeping, and a spatial broad phase. Collision pairs are currently evaluated in
quadratic time, which is appropriate for small scenes and provides a clean seam for a later broad
phase strategy.
