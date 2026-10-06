# Golf shot execution

The golf slice joins the existing launch-monitor, physics, renderer, and input boundaries without
making any one adapter authoritative. `mgv::golf::Shot` is the domain event shared by hardware
adapters. `ShotModel` is the replaceable policy that converts that event into strongly typed linear
and angular velocity. `StandardShotModel` is the default implementation.

## Coordinate and measurement contract

- World coordinates remain right-handed and Y-up.
- A zero-degree shot travels along −Z; positive launch direction turns toward +X.
- Launch angle is measured above the horizontal plane.
- A zero-degree spin axis represents pure backspin around the shot-local right axis.
- Full-swing speed is already metres per second and total spin is converted from RPM to radians per
  second.
- A putt uses `putter_speed_mps * putting_smash_factor` and face angle for its initial direction.
  It starts with the no-slip rolling angular speed `ball_speed / ball_radius`. The remaining putter
  measurements are retained on the event for later equipment/contact models; the MVP does not
  invent effects for them.

Invalid, negative, non-finite, or out-of-range measurements are rejected before they enter physics.
Applications can inject another `ShotModel` when a calibrated launch model is available.

## Runtime flow and ownership

```text
MLM2PRO / Vertex adapter -> LaunchMonitor callback -> queued LaunchBallCommand
Space / Qt test control ---------------------------> queued CameraInputCommand
                                                       |
                                                       v (next Engine::tick)
                                            ShotModel -> PhysicsWorld
                                                      -> bound Renderable
```

`Engine` owns an attached `LaunchMonitor`, starts it after an active ball has been bound, and stops
it before teardown or scene replacement. Adapter callbacks only copy a shot into the thread-safe
engine command queue. Shot validation, model evaluation, velocity changes, physics stepping, and
render synchronization all happen on the engine thread at a deterministic tick boundary.

`bind_golf_ball` enforces one active, dynamic sphere. `add_static_collider` adds terrain or world
geometry without requiring a dummy rendered entity. Qt and GLFW currently create a normalized ball
proxy and an invisible ground box; Space and **Controls → Fire Test Shot** use the same execution
path as hardware input.

## Current boundary

This slice deliberately stops at launch execution. It does not yet provide club fitting, calibrated
ball-contact coefficients, shot history/scoring, a rendered course plus separately rendered ball,
or a ball-follow camera. Those can build on `ShotModel`, the active-ball binding, and the existing
stable entity handles without changing launch-monitor adapters.
