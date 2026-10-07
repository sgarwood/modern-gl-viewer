# Research: vegetation, terrain, and the golf hardware path

Notes from reading around how the industry and the open-source world solve
the problems this renderer has hit. Recorded so the reasoning survives, and
so the items not taken up can be picked up later without repeating the
search.

Dated 2026-10-08.

---

## Adopted

### Spherical canopy normals

**Problem.** Foliage built from separate masses shades as separate masses.
Every lobe of a crown was lit as its own sphere, so a tree read as a heap of
balloons and flickered lobe by lobe as the sun moved.

**What the industry does.** Transfer the vertex normals from a single sphere
around the whole crown, so the canopy receives light as one volume. This is
long-standing vegetation-art practice — studios have done it for years with
tools like Normal Thief in 3ds Max, and the Blender addons that sell under
names like "Foliage Normals" exist to do exactly this. It is usually exposed
with a blend slider rather than as an all-or-nothing replacement.

**What we did.** `spherify_canopy_normals` in `src/primitives.cpp`, blended at
0.78 for broadleaves and 0.42 for conifers, which are cones whose whorls are
meant to read as separate tiers. The bole is excluded: a trunk genuinely is a
cylinder, and spherifying it lights it from inside.

This was the single largest improvement available to a crown built from
geometry, and it costs nothing at runtime.

### Quadratic Bezier grass blade

**Problem.** The blade was bent by rotating it about its root. A real blade
bends along its length. The expression that produced the curve also contained
a divide and a multiply that cancelled.

**What the industry does.** Sucker Punch's *Ghost of Tsushima* generates
blades on the GPU whose shape is a cubic Bezier, with wind driven by scrolling
Perlin noise feeding a sine-based function that modulates the curve. The
community reimplementation `ItsMeNiV/GhostOfTsushima-Foliage` shows the
pattern plainly: a `toBezier(t)` for the spine and a `toBezierDerivative(t)`
for the tangent, the tangent then used as the blade's local up so the normal
follows the curve.

**What we did.** A quadratic Bezier spine in `assets/shaders/grass.vert`, with
the tangent taken in closed form rather than differenced from two samples.
Quadratic rather than cubic: with the base upright and one lean angle at the
tip there is nothing for a second control point to say. Blades also align
downwind as wind rises, so a gust crosses the field instead of every blade
pulsing in place — the same idea as `2Retr0/GodotGrass`, which interpolates
each blade's resting direction towards the wind by the wind's strength.

---

## Worth doing, not yet done

### 1. Centre the terrain clipmap on the camera

The terrain is concentric square loops that coarsen outwards. That is the
structure of a geometry clipmap, and consecutive loops carry the same vertex
count, so it is watertight by construction with no T-junctions to stitch.

What it is not is camera-centred. The rings are nailed to the green, so
detail does not follow the player. A clipmap proper translates with the
viewer and snaps to whole grid steps, which is what stops the sampling grid
swimming underfoot as the camera moves.

This is the highest-value remaining item, and the GDD's auto-walk cannot ship
without it. References worth reading: Hoppe's geometry clipmaps, and CDLOD as
implemented in `bsbgreenfield/CDLOD_wgpu` and `sduenasg/terrain-sandbox`.

It needs one thing the renderer does not have: a way to replace a mesh's
vertex data without rebuilding the scene.

### 2. Grass patches that follow the camera

The same problem, same fix. The blade field is a static disc centred on the
ball. It wants to be a ring of chunks around the camera, each frustum-culled
and with density falling off by distance, regenerated as the player walks.

Ghost of Tsushima does this on the GPU with a compute shader, which an OpenGL
4.1 core profile cannot. CPU-side chunk regeneration on a background thread is
the equivalent here, and the instancing to draw it already exists.

### 3. Recursive branching

`dgreenheck/ez-tree` exposes the parameter set the Weber-Penn lineage settled
on: `levels`, `children`, `angle`, `length`, `radius`, `taper`, `gnarliness`,
`twist`, and `start` — where along the parent a child branch begins.

Our trees have one level of straight limbs. Gnarliness and a second level is
where the remaining silhouette realism is, and silhouette is what identifies a
tree at the distances a golf hole is seen from.

### 4. Tree level of detail and impostors

ez-tree's `generateLODs()` shares geometry across tiers and switches on camera
distance, with `sectionStride`, `segmentFactor`, `leafStride`, `leafScale`, and
a billboard mode per tier. With twelve tree meshes instanced around ninety
times, trees are already the dominant vertex cost in a wide shot.

---

## Considered and rejected

### Leaf cards on branches, in place of canopy lobes

The textbook way to build a crown is to scatter alpha-tested leaf cards along
recursive branches. It is also what makes a tree hold up in close-up.

A golf hole is seen from thirty to two hundred metres. At those distances an
individual leaf is well under a pixel, so the cards would cost a great deal of
geometry to deliver something the filtering immediately throws away, while
aliasing badly in motion. Silhouette and canopy lighting carry the read
instead, which is why spherical normals paid off so much more than geometry
would have.

Worth revisiting only if the game ever puts the camera inside a tree.

---

## Golf hardware path

`OpenSkyPlus` confirms **GSPro OpenConnect** as the de facto protocol for
getting launch-monitor data into simulator software; it exists to bridge
C#-based monitors to it. This project already has a GSPro JSON parser, so
Phase 2 of the GDD is aimed at the right target.

Not yet verified: the wire format. The OpenSkyPlus README states neither port
nor message schema, so the GSPro4OSP plugin source needs reading before
anything is built against it.

---

## Sources

- Polycount, [tree lighting and normals problems in UE4](https://polycount.com/discussion/202402/tree-lighting-and-normals-problems-in-ue4)
- [Foliage Normals](https://superhivemarket.com/products/foliage-normals), a Blender addon for the spherify-normals workflow
- GDC 2021, [Advanced Graphics Summit: Procedural Grass in 'Ghost of Tsushima'](https://gdcvault.com/play/1027214/Advanced-Graphics-Summit-Procedural-Grass)
- GDC, [Samurai Landscapes: Building and Rendering Ghost of Tsushima](https://gdcvault.com/play/1027352/Samurai-Landscapes-Building-and-Rendering)
- [ItsMeNiV/GhostOfTsushima-Foliage](https://github.com/ItsMeNiV/GhostOfTsushima-Foliage)
- [2Retr0/GodotGrass](https://github.com/2Retr0/GodotGrass)
- [dgreenheck/ez-tree](https://github.com/dgreenheck/ez-tree)
- [bsbgreenfield/CDLOD_wgpu](https://github.com/bsbgreenfield/CDLOD_wgpu), [sduenasg/terrain-sandbox](https://github.com/sduenasg/terrain-sandbox)
- [OpenSkyPlus](https://github.com/OpenSkyPlus/OpenSkyPlus)
- 80.lv, [Building Procedural Grass With GPU Compute](https://80.lv/articles/building-procedural-2d-grass-with-gpu-compute)
