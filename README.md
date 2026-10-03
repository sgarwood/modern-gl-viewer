# Modern GL Viewer

A compact C++20 OBJ viewer with a replaceable rendering backend. The OpenGL 4.1 renderer can run
inside either the Qt 6 or GLFW application shell; the portable core contains no toolkit or OpenGL
types.

## Features

- Qt 6 `QOpenGLWidget` entrypoint designed for WSLg/Wayland, with file pickers for OBJ and GLSL.
- GLFW entrypoint for lightweight native use.
- Runtime OBJ loading with `v`, `vt`, `vn`, positive/negative indices, polygon triangulation,
  vertex deduplication, generated normals, and material-preserving submeshes.
- Wavefront MTL diffuse colours, opacity, and `map_Kd` textures, with paths resolved relative to
  their MTL file and shared images decoded once per model load.
- Runtime PNG, JPEG, TGA, BMP, PSD, GIF, HDR, PIC, PNM, and ASCII PPM image loading behind a
  Pimpl façade; translucent materials select an alpha-blended, depth-read-only pipeline.
- Runtime GLSL loading with useful file, compiler, and linker diagnostics.
- Backend-neutral `Scene`, `Renderable`, `Transform`, `Material`, and `Camera` APIs.
- Frontend-neutral orbit/zoom input through dependency-inverted `InputSink` and `CameraTarget` ports.
- Shared CPU mesh/material identity mapped to deduplicated backend resources.
- Backend-neutral `RenderBackend`, `MeshResource`, and `RenderPipelineResource` APIs.
- Explicit topology, rasterization, depth, and blending state in immutable pipeline descriptors.
- Backend-reported clip-space conventions, including OpenGL and zero-to-one depth ranges.
- Mesh AABBs and bounding spheres, backend-neutral frustum culling, and deterministic render queues.
- Opaque front-to-back and translucent back-to-front submission with per-frame render statistics.
- Optional `mgv::physics` domain with strong SI units, fixed timesteps, gravity, and rigid bodies.
- Injectable sphere/AABB collision detection with penetration correction and restitution impulses.
- Validated RGBA8 image data, linear/sRGB colour spaces, sampler descriptions, and RAII texture
  resources.
- `MaterialInstance` keeps named texture bindings separate from immutable material pipelines.
- RAII ownership for windows, GL buffers, vertex arrays, shaders, and programs.
- Pimpl façades for `Application`, `Renderer`, and `ObjLoader`.
- Headless Catch2 tests for camera math, asset loading, physics, and render orchestration.

The research and resulting design decisions are recorded in
[`docs/research.md`](docs/research.md).

The physics domain model, unit contract, and current MVP boundaries are documented in
[`docs/physics.md`](docs/physics.md).

## WSL + Qt quick start

Install Qt 6 and its Wayland platform plugin in Ubuntu 24.04:

```sh
sudo apt install build-essential cmake qt6-base-dev qt6-wayland libxkbcommon-dev
```

Configure, build, test, and run using the checked-in preset:

```sh
cmake --preset wsl-qt
cmake --build --preset wsl-qt --parallel
ctest --preset wsl-qt
QT_QPA_PLATFORM=wayland ./build/wsl-qt/mgv_qt
```

The included cube and default shaders load when no arguments are supplied. A model and custom
shaders can also be selected from the File menu or passed on the command line:

```sh
QT_QPA_PLATFORM=wayland ./build/wsl-qt/mgv_qt model.obj \
  --vertex shaders/custom.vert \
  --fragment shaders/custom.frag
```

If WSLg's accelerated Mesa path is unavailable, software rendering is a useful diagnostic:

```sh
QT_QPA_PLATFORM=wayland LIBGL_ALWAYS_SOFTWARE=1 ./build/wsl-qt/mgv_qt
```

Use the arrow keys to orbit, `+`/`-` to zoom, and `Home` or `R` to reset the camera. The Qt
Controls menu exposes the same commands as clickable test controls. GLFW uses the same keys.

The bundled cube and default shader exercise material texture binding with a generated checkerboard.
Custom GLSL can declare `uniform sampler2D uBaseColorTexture` and
`uniform vec4 uBaseColorFactor`; shaders that omit either binding remain valid.

## Other build configurations

The default configuration builds the GLFW shell. On Linux it requires the X11 development
packages used by GLFW:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/mgv
```

Use `-DMGV_GLFW_X11=OFF -DMGV_GLFW_WAYLAND=ON` for a GLFW/Wayland build. In a CI container with no
window-system headers, `-DMGV_HEADLESS_GLFW=ON` compiles the full GLFW application against its null
platform, but that binary cannot create an interactive OpenGL window.

For the core library and tests only:

```sh
cmake --preset headless-tests
cmake --build --preset headless-tests --parallel
ctest --preset headless-tests
```

## Coordinates and camera

The renderer's canonical world is right-handed and Y-up:

- +X points right, +Y points up, and the default camera sits on +Z looking toward the origin.
- `Camera` uses a right-handed look-at view and the active backend's projection convention.
- Matrices are column-major; OpenGL selects normalized device depth of −1 to +1, while a future
  backend can select zero-to-one depth and inverted clip-space Y.
- OBJ positions, normals, units, and UVs are preserved as authored. OBJ files contain no standard
  metadata declaring handedness, up-axis, units, or UV origin.

Consequently, right-handed Y-up OBJ assets are directly supported. Z-up or left-handed assets are
accepted but are not automatically converted yet; they need an import transform. See
[`docs/coordinates.md`](docs/coordinates.md) for the exact contract and extension point.

## Custom shader contract

| Location | Type   | Meaning |
|---------:|--------|---------|
| 0        | `vec3` | position |
| 1        | `vec3` | normal |
| 2        | `vec2` | texture coordinate |

If declared, `uniform mat4 uMvp` receives the current model-view-projection matrix. A shader may
omit it. Asset loads also provide optional `sampler2D uBaseColorTexture` and
`vec4 uBaseColorFactor` bindings.

## Architecture

```text
Qt / GLFW adapter -> InputSink <- OrbitCameraController -> CameraTarget <- Renderer -> Camera
Qt / GLFW host    -> OpenGL backend -> RenderBackend <------------------- Renderer
Scene -> Renderable -> Transform / MeshData / MaterialInstance --------> Renderer
                                      |-> Material -> Pipeline
                                      |-> Texture -> Image / Sampler
Input / gameplay -> PhysicsWorld -> RigidBody / CollisionDetector
                                      |-> Position -> Renderer transform update
```

The CMake targets mirror these boundaries: `mgv::core`, `mgv::physics`, `mgv::opengl`, and the
optional Qt/GLFW entrypoints. A future Vulkan or Direct3D adapter can replace `mgv::opengl` without
changing OBJ, camera, shader-file, or scene orchestration code.

`Scene` objects use shared immutable meshes, material pipelines, and textures. A `MaterialInstance`
contains per-material named bindings without duplicating pipeline state. When a scene is submitted,
`Renderer` deduplicates backend mesh, pipeline, texture, and sampler resources by shared identity and
keeps per-renderable transforms and visibility separate. `RenderPipelineDescriptor` captures
primitive topology, rasterization, depth, and blending state; the backend owns compilation and
native pipeline resources.

## Scope

This is intentionally a focused viewer, not a complete Wavefront implementation. It supports
`mtllib`, `usemtl`, `newmtl`, `Kd`, `d`, `Tr`, and plain `map_Kd` paths. Advanced MTL texture-map
options, smoothing groups, illumination models, and specular/normal maps are not implemented.
Faces and geometry attributes are supported, and absent normals are generated.
