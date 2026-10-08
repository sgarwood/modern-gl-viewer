# Roadmap

Where the project is and what is next, as of 2026-10-08. Kept short: this is
a list of what to pick up, not a plan to follow to the letter.

## Done

The renderer and the physics beneath it now hold up as a golf hole.

- World-space forward rendering with per-frame camera, sun, sky, fog and wind
- HDR scene target resolved through exposure, an ACES fit, sRGB and a dither
- Preetham analytic daylight, GGX/Smith materials, height-falloff aerial
  perspective, all in absolute radiometric units
- Camera-fitted directional shadow cascades
- Generated terrain agreeing exactly with the physics heightmap across the
  green, surfaces separated by height of cut, normal-tilted mower stripes
- Instanced per-blade grass on a Bezier spine; four tree species with
  spherical canopy normals; leaf drifts against the broadleaves
- Terrain, blade field and collidable ground all follow the player
- `mgv::game::Round`: the state machine for a round, outside the engine and
  tested without a GL context

## Next

### 1. Rebuilds off the main thread

A full blade-field rebuild is around a tenth of a second. It disappears into
the frame time on a software rasteriser and would be the entire frame budget
on a GPU. It wants a worker thread, or splitting across frames.

Blocked on nothing. Should land before anyone plays this on real hardware.

### 2. The hybrid camera controller

The GDD asks for third person that drops into first person for the range
finder and for retrieving the ball from the cup. `OrbitCameraController` is
still what the player gets while addressing, and it orbits a point rather
than standing behind a golfer.

`Round` already decides when it is directing; this is the other half.

### 3. The range finder's first-person pass

The sway and the ranging work. What is missing is the look: a sniper-clip
mask over the view and depth of field behind the reticle. The depth of field
is wanted anyway, so this and the post-processing below are one job.

### 4. Post-processing

Bloom, then ambient occlusion, then depth of field. The HDR target and the
composite pass they hook into already exist.

### 5. The hike

The walk between shots is a straight line over the ground. The GDD asks for
a path that avoids hazards, which means a navigable representation of the
course and a search over it.

## Known limits

- **No GPU here.** `/dev/dxg` exists but Mesa cannot bind it; OpenGL runs on
  `llvmpipe`. Correctness and captures are fine; nothing has been timed on
  real hardware.
- **`InputAction::fire_test_shot` handled inside the engine fires down -Z**,
  which on this course plays away from the green. The played shot is
  `play_test_shot`, which aims at the hole. The engine's is a viewer debug
  command and should probably go once the GLFW shell has a session.
- **A ball at rest on a slope creeps**, about six centimetres over two
  hundred frames, from the penetration correction nudging along the contact
  normal. Real grass would hold it. Wants a resting-contact threshold.

## Reference

- [`docs/rendering.md`](rendering.md) — pipeline, shader contract, units, streaming
- [`docs/round.md`](round.md) — the round state machine and its ports
- [`docs/research-vegetation-and-terrain.md`](research-vegetation-and-terrain.md)
