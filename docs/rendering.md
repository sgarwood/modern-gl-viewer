# Rendering

The renderer is a forward pipeline that lights in world space, in absolute
radiometric units, and resolves to the display once at the end of the frame.

## Frame contract

`Frame` carries everything constant for a frame: the view, projection, and
view-projection matrices, the camera position, elapsed time, the sun's shadow
matrix, and an `Environment`. Frontends fill in only the viewport;
`Renderer::render` derives the rest and hands the completed frame to
`RenderBackend::begin_frame`. No shader recovers a world-space quantity from a
model-view-projection matrix.

`DrawPacket` carries the world matrix and its inverse transpose alongside the
model-view-projection, so normals survive non-uniform scale.

Shaders declare whatever subset of the standard uniforms they need. The OpenGL
backend caches each location at link time and uploads only what a program
declares, which keeps the minimal viewer shaders valid.

| Uniform | Meaning |
|---|---|
| `uModel`, `uNormalMatrix` | object to world, and its inverse transpose |
| `uView`, `uProjection`, `uViewProjection` | camera matrices |
| `uCameraPosition` | eye position, world space |
| `uTime` | seconds since the session started |
| `uSunDirection` | unit vector **towards** the sun |
| `uSunColor`, `uSunIlluminance` | sun colour and lux |
| `uSkyZenithColor`, `uSkyHorizonColor`, `uGroundAlbedo`, `uSkyIlluminance` | ambient hemisphere |
| `uTurbidity` | atmospheric turbidity, 1.7 to 10 |
| `uFogDensity`, `uFogHeightFalloff` | aerial perspective |
| `uWindSpeed`, `uWindDirection` | wind, for foliage |
| `uSurfaceWetness` | 0 dry to 1 saturated |
| `uSunViewProjection`, `uShadowMap`, `uShadowTexelSize` | shadow lookup |
| `uTrailCount`, `uTrailPositions` | where the ball has rolled |
| `uViewportSize` | framebuffer size in pixels |

## Units and exposure

Lighting is absolute: `uSunIlluminance` is lux, and shaders emit radiance in
cd/m². The scene renders into an RGBA16F target; the composite pass applies
`Environment::exposure`, Stephen Hill's ACES RRT/ODT fit, the sRGB transfer
function, and a sub-quantisation-step ordered dither.

`exposure_from_ev100` converts a photographic exposure value to the multiplier.
Daylight sits near EV 15; the bundled session uses EV 14, which puts a sunlit
fairway around mid grey.

## Passes

1. **Shadow.** Casters are drawn into a 2048² depth target from the sun's
   point of view, through one trivial depth program rather than a depth
   variant of every material. A renderable can decline to cast; the sky dome
   does, since it surrounds the camera.
2. **Colour.** The scene is drawn into the floating-point target. Surfaces
   sample the shadow map through a hardware comparison sampler with a rotated
   eight-tap Poisson kernel.
3. **Composite.** Exposure, tone map, encode, dither, resolve to whatever
   framebuffer the host had bound.

A backend that cannot create a floating-point target or a depth target falls
back: it reports no shadow support and never has a shadow pass driven against
it, and it draws straight to the output rather than producing a blank window.

The host owns the output framebuffer. `QOpenGLWidget` and `mgv_capture` each
render into one of their own, so the backend captures the binding at
`begin_frame` and restores it for the resolve.

## Shader modules

`ShaderLoader` resolves `#include "..."` relative to the including file,
expands each module once, and emits `#line` directives so driver diagnostics
still name the right file. Circular includes are rejected.

- `lib/uniforms.glsl` — the standard uniform block
- `lib/brdf.glsl` — GGX/Smith metallic-roughness, hemisphere ambient
- `lib/sky.glsl` — Preetham daylight and the sun disc
- `lib/shadow.glsl` — filtered sun visibility
- `lib/fog.glsl` — closed-form height-falloff aerial perspective
- `lib/color.glsl`, `lib/noise.glsl`

## Instancing

`MeshData::instances` carries placements of a mesh as per-instance vertex
attributes: a world offset and yaw at location 3, and four free parameters at
location 4. Every mesh has an instance buffer, including an ordinary one,
whose single entry is the identity placement; that way one depth program and
one vertex layout serve both cases.

Bounds cover every placement. Without that a field of grass is culled on the
strength of one blade sitting at the origin.

## Grass

Individual blades are drawn only near the player. Beyond a few metres a blade
is smaller than a pixel, and the turf shader's filtered surface is both
cheaper and steadier.

One unit-height blade mesh serves the whole course; height, width, lean, and
colour all arrive as instance parameters, taken from the height of cut where
each blade stands. A putting green grows none worth drawing. The field thins
in number and in height towards its rim, or it ends in a visible ring.

Blades do not cast into the shadow map: a hundred thousand of them would spend
the whole map on detail finer than one of its texels.

The field is static and centred on the ball. Patches that follow the camera,
regenerated as the player walks, are the next step and are what this needs to
become before the hike is playable.

## Trees and litter

Four species: oak, beech, maple and pine. What separates them is proportion,
not colour. Crown half-width and the height at which foliage starts identify a
tree across a fairway long before any difference in hue does, so each species
is a set of proportions -- bole height, trunk radius, crown width, how far up
the crown begins, whether it is taller or wider than it is round -- and the
palette comes second.

A broadleaf crown is limbs rising out of the bole with masses of foliage
filling the ellipsoid they reach into, biased outwards because foliage grows
where the light is. A trunk that rises and stops with a ball of leaves on top
is the single thing that makes a generated tree look generated. A pine is
built instead from drooping whorls narrowing to a leader.

Each mass is displaced by a smooth function of direction. Overlapping smooth
spheres still read as spheres, because the silhouette stays circular and the
eye reads silhouette first. The displacement has to be coherent: hashing each
vertex of a low-poly sphere turns it into visible flat facets, which looks
worse than the sphere did.

Each species is one draw with every tree of that kind as an instance, and its
colours ride on the material instance, since they are the same for every tree
of a species. Trees are planted in stands, because trees of a kind grow
together and a perfectly mixed wood reads as scattered props.

Leaves drift against the broadleaves that shed them, carried a little
downwind; pines are skipped, since they drop needles. Leaves fill the volume
of each mound rather than tiling its surface, so a ball dropped into a drift
is genuinely among them. Each leaf carries how deep it lies, which is what
lets the shader darken the ones underneath, and the count thins towards the
rim as well as the depth, or the drift ends on a hard circle.

## Filtering

Surface detail is faded against the screen-space pixel footprint, taken from
the derivative of world position so it accounts for distance and grazing angle
together. A feature smaller than a pixel is removed rather than sampled, which
is the difference between turf and crawling static.

Features at different scales need their own thresholds. Mower stripes are
metres wide and blade detail is centimetres; filtering both at one scale
suppresses the stripes entirely.

## The course

Terrain is generated, not loaded. `course_terrain_height` reproduces the
analytic surface the physics heightmap samples exactly across the green, then
fades into rolling landscape and runs out to 420 m in rings that coarsen with
distance. The landscape blend may not begin before the green's diagonal, or the
drawn surface diverges from the collision surface at its corners.

Surface class — putting surface, mown approach, rough — is authored per vertex
rather than inferred from slope, because a green and a bunker can both be
level. It travels in the texture coordinate, since the turf shader derives
everything else from world position.

Mower stripes are a change in which way the grass is lying, not a change of
colour: the shading normal tilts across each band. The bands therefore swap
light and dark as the camera or the sun moves.

## Reviewing a change

`mgv_capture` renders the same course session the interactive window builds,
offscreen, to a PNG. Build it with `-DMGV_BUILD_CAPTURE_TOOL=ON` (on in the
`wsl-qt` preset):

```sh
QT_QPA_PLATFORM=wayland ./build/wsl-qt/mgv_capture --out shot.png \
  --width 1100 --height 620 --sun-azimuth 42 --sun-elevation 15 --exposure 13.2
```

`--exposure` takes an EV100 value. `--camera` and `--target` take a ground
position and a height above terrain, as `x,height,z`. Also available:
`--turbidity`, `--wetness`, `--wind`, `--fov`, `--frames`, `--ball`,
`--grass`, `--grass-radius`, and `--shaders`. The last points at an
alternative shader directory, which is how a debug variant of a surface
shader is rendered without disturbing the real one.

Each run reports the draw statistics and where the ball came to rest against
the terrain height beneath it, which is the quickest way to see that it is
resting on the ground rather than falling past it.

## Hardware note

This workstation has no GPU available to WSL: `/dev/dxg` exists but Mesa cannot
bind it, and OpenGL runs on the `llvmpipe` software rasteriser. That is fine for
correctness and for captures, and useless for judging frame rate. Nothing in the
pipeline depends on software rendering, but none of it has been timed on real
hardware.
