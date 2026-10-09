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

## Aerodynamics

Drag and lift coefficients are functions of the two quantities dimensional
analysis says matter -- the spin ratio `S = omega r / v` and the Reynolds
number -- rather than the constants they were. `aerodynamics.hpp` holds them
as pure functions so they can be checked against the published numbers
without standing up a world, which
`tests/physics/aerodynamic_coefficients_test.cpp` does.

The fits are Smits and Smith's, measured over `40,000 < Re < 250,000` and
spin ratios from 0.04 to 1.4 -- a driver through a wedge:

    C_D = 0.24 + 0.18 S + 0.06 sin(pi (Re - 90,000) / 200,000)
    C_L = 0.54 S^0.4

The lift exponent is the interesting one. Well under one, it means the first
few hundred revolutions buy most of the lift a ball will ever get. A constant
0.2 was wrong in both directions at once: too much lift for a driver turning
at 2700, far too little for a wedge turning at 9000, which is why a short
iron used to fly like a long one.

Reynolds number is computed from the kinematic viscosity the *air density*
implies, so altitude moves it along with everything else density touches.

### What it reproduces, and what it does not

Launched at TrackMan's published PGA Tour averages, the model puts a 7 iron
and a pitching wedge within a few per cent of their measured carry, apex and
descent angle. A driver reaches the right height and carries about ten per
cent short, which says its drag is too high late in the flight rather than
its lift being wrong -- and a driver is the one club that spends its whole
flight above `Re = 200,000`, where Smits and Smith reported a second fall in
drag that this fit does not carry. Closing that is the next thing worth doing
to the flight model. `tests/physics/flight_test.cpp` pins all three clubs
against the tour figures so that a change has to be re-measured rather than
quietly moving the yardstick.

The sign of the transition term is the one number here with real doubt
attached, and it was settled by measurement rather than by reading: carried
at the published `+0.06` all three clubs land near their measured apex, while
negating it or dropping it sends a driver's apex to 49 and 40 metres against
a measured 31.

## Spin

A ball's spin is a dynamic quantity, not a decoration on its launch. Three
things read or change it.

**Lift.** The Magnus term, along `omega x v`, with a coefficient that now
rises with the spin ratio -- see below.

**Decay in flight.** The boundary layer a spinning ball drags round with it
exerts a retarding moment, so spin bleeds off as it flies. It is applied as a
rate proportional to airspeed rather than as a fixed moment, because a moment
that does not vanish with the spin drives a lightly spinning ball through
zero and out the other side. Smits and Smith's measured rate puts a drive
launched at 2700 rpm at about 93 per cent of it six seconds later.

**Contact.** This is the one that changes how the game plays. Friction is
solved on the velocity of the ball's *surface* where it meets the ground, not
on the velocity of its centre: a ball with backspin is dragging its underside
forward across the turf faster than it is travelling, which is why a struck
approach shot stops where it lands, and why one with enough spin left walks
backwards. The impulse is applied to both the centre and the spin, which for
a sphere splits a slide five sevenths into the one and two sevenths into the
other -- a ball launched sliding without spin rolls away on exactly five
sevenths of its speed, and `tests/physics/spin_test.cpp` pins that number.

Coulomb friction and rolling resistance are separate, because they act at
different times: friction while the contact slides, resistance once it does
not. Summing them into one coefficient made a putt and a landing drive the
same event. Resistance is a couple opposing the spin, with the static
friction that maintains the roll passing the deceleration to the centre --
five sevenths of `c * g` for a sphere, which is what makes a green's speed
measurable rather than a tuning knob. `rolling_resistance_from_stimp` turns a
stimpmeter reading into that coefficient, and the solver is checked against
it: a ball released at 1.83 m/s on a green that stimps at ten runs ten feet.

A rolling ball's spin is not a free variable, so it is re-imposed from the
roll each step. Spin about the contact normal is left alone: a ball can
rotate like a top while it rolls, and that component is what cut spin on a
putt becomes.

`inverse_inertia()` is a scalar, which is only honest for a sphere. It is
zero for a static body and for any dynamic body that is not a sphere; a box
needs a tensor, and one number would make it spin wrongly rather than not at
all. The only dynamic body in this project is a ball.

## What the ball can hit

Sphere, box, heightmap and an upright capsule. The capsule is what a trunk
and a flagstick are; it stands on the Y axis rather than taking a free one,
because nothing on this course leans and an arbitrary axis would cost a
rotation on every contact. Knowing the axis makes the closest point on it
the sphere's own height clamped to the segment, so the contact normal comes
out horizontal wherever up the trunk a ball strikes.

Every tree on the hole carries one around its bole. The bole only: a
broadleaf divides into limbs a ball flies between as often as not, and what
a crown does is the wind's business below. Its radius and height come from
`tree_trunk`, which reads the same species profile the mesh is built from --
derived separately the two would drift, and a ball would pass through one
trunk and bounce off thin air beside the next. `primitives_test.cpp` checks
the capsule stands inside the bark's own taper, which is the most that can
be asked of one radius against a bole that swells at the base and narrows
towards the crown.

Bark's restitution, 0.35, is a judgement rather than a measurement. So is
the drag a canopy adds. Both are written down where they are chosen.

### Only the pairs that can touch

Two pieces of scenery cannot move relative to one another, so only pairs
holding a dynamic body are considered. Ninety trees make four thousand pairs
and all but ninety of them are tree against tree; walking them all cost 5%
of a 60 Hz budget with nothing happening, and 55% at three hundred trees.

This is not a spatial broad phase -- a dynamic body is still compared
against every static one -- but the quadratic term is gone, and one ball
against a course's furniture is linear. A course with thousands of trees
would want real partitioning.

## Wind and foliage

Wind is a uniform vector, reduced inside a foliage volume and in its wake.
Downwind is measured along the wind, which it was not: the wake used to be
tested against the volume's Z bounds, so it worked for a wind along +Z and
for no other.

A canopy also adds drag in proportion to its density, so a ball through the
middle of a crown drops rather than carrying. That replaced a probabilistic
branch strike drawn from the sine of the ball's own z coordinate, which
printed to standard output when it fired.

The course plants no foliage volumes yet -- only the GLFW demo does, one, by
hand -- so the crowns do nothing until they are planted from the trees. That
is the obvious next job here.

## Coming to rest

A body that stays under `sleep_linear_speed` and `sleep_angular_speed` for
`sleep_delay` stops integrating, and its velocities are zeroed. It is woken by
an impulse, by a velocity or position set on it, by a moving body arriving
against it, or by a static collider being replaced -- which the streamed
collision patch does as the player walks up the hole.

This is not an optimisation. A resting body re-penetrates the ground by
`g dt^2` every step, and the correction that pushes it back out runs along the
contact normal; on a slope that normal is tilted, so each push moves the body
a fraction of a millimetre downhill. A ball left on a five per cent slope used
to walk 2.7 cm in three seconds with its velocity reading exactly zero
throughout, which is why friction could not answer it -- there was no velocity
to oppose. It now drifts 4 mm while it settles and then nothing at all,
bounded by the sleep delay rather than growing without limit.

The thresholds are set so that a ball a slope can still move never qualifies:
a green at stimp ten cannot hold a ball on a fifteen per cent fall, and such a
ball is through `sleep_linear_speed` inside a fiftieth of a second, far short
of the delay. That is the condition worth preserving if these numbers are ever
retuned, and `tests/physics/resting_test.cpp` states it.

## Still excluded

- **Orientation.** Bodies carry an angular velocity but no rotation. Nothing
  needs one yet -- a ball is a sphere and its spin is invisible -- but a
  logo, a dimple pattern or a non-spherical body would.
- **Constraints and continuous collision detection.** A step covers 1.2 m at
  driver speed, which is fine against a heightmap and would not be against a
  thin collider -- a cup, a pin, a fence.
- **A spatial broad phase.** Collision pairs are evaluated in quadratic time,
  which is appropriate for a scene holding a ball and a patch of ground, and
  provides a clean seam for a later broad phase strategy.
- **The second drag fall above Re = 200,000.** See above: it is why a driver
  carries short. Everything else in the flight model is inside a few per cent
  of measured tour trajectories.
- **Wind gradients, gusts and shelter from terrain.** Wind is one vector for
  the whole course, modified only by foliage volumes.
- **Canopies on the course.** The trunks are solid; the crowns are not
  planted. See above.
