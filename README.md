# Modern GL Viewer

A compact C++20 OBJ viewer with a replaceable rendering backend. The OpenGL 4.1 renderer can run
inside either the Qt 6 or GLFW application shell; the portable core contains no toolkit or OpenGL
types.

## Features

- Qt Quick/QML main menu for choosing OBJ/GLSL assets and launching the native engine window.
- Qt 6 `QOpenGLWidget` engine shell designed for WSLg/Wayland, with file pickers for OBJ and GLSL.
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
- Replaceable golf `ShotModel` for validated full-swing and putting measurements, with queued
  launch-monitor-to-ball execution on the engine thread.
- Optional `mgv::network` domain with a RAII `std::jthread` service and real IPv4 UDP transport.
- Dependency-inverted network I/O, bounded message queues, backpressure, and failure events.
- Pimpl `Engine` runtime with stable `EntityId`/`RenderableId` handles and queued typed commands.
- Injectable monotonic clock, physics-to-render bindings, and main-thread network-event decoding.
- Ozz-backed animation clips and players with stable handles, deterministic playback commands,
  and exclusive animation-to-render transform bindings.
- Validated RGBA8 image data, linear/sRGB colour spaces, sampler descriptions, and RAII texture
  resources.
- `MaterialInstance` keeps named texture bindings separate from immutable material pipelines.
- RAII ownership for windows, GL buffers, vertex arrays, shaders, and programs.
- Pimpl façades for `Application`, `Renderer`, and `ObjLoader`.
- World-space forward rendering with per-frame camera, sun, sky, fog, and wind state.
- HDR RGBA16F scene target resolved through exposure, an ACES fit, sRGB, and a dither.
- Preetham analytic daylight, a GGX/Smith metallic-roughness BRDF, and height-falloff
  aerial perspective, all in absolute radiometric units.
- Three camera-fitted directional shadow cascades with comparison-sampled,
  Poisson-filtered lookups and texel-snapped light volumes.
- Generated course terrain that reproduces the physics heightmap across the green and
  runs out to the horizon, with per-vertex surface classes and normal-tilted mower stripes.
- GLSL `#include` with relative resolution, include-once, and cycle detection.
- `mgv_capture`, an offscreen renderer that writes a PNG of the live course session.
- Headless Catch2 tests for camera math, asset loading, physics, lighting, and render
  orchestration.

The rendering pipeline, shader contract, units, and the offscreen capture tool are
documented in [`docs/rendering.md`](docs/rendering.md).

Where the project is and what is next is kept in [`docs/roadmap.md`](docs/roadmap.md).

The patterns this codebase is built from, judged against the ones the field has
settled on, and what is worth changing, are in
[`docs/architecture.md`](docs/architecture.md).

Where to source a golfer and a club, and what the animation runtime still needs
before one can be put on screen, are in [`docs/characters.md`](docs/characters.md).

The round's state machine, its ports, and why it does not live in the engine are
documented in [`docs/round.md`](docs/round.md).

Research into how the industry solves the vegetation and terrain problems this
renderer has hit, what was adopted and what was deliberately not, is recorded in
[`docs/research-vegetation-and-terrain.md`](docs/research-vegetation-and-terrain.md).

The research and resulting design decisions are recorded in
[`docs/research.md`](docs/research.md).

The physics domain model, unit contract, and current MVP boundaries are documented in
[`docs/physics.md`](docs/physics.md).

The networking ownership, threading, and shutdown contracts are documented in
[`docs/networking.md`](docs/networking.md).

Runtime orchestration, stable handles, tick ordering, and frontend boundaries are documented in
[`docs/runtime.md`](docs/runtime.md).

Animation archive loading, playback, transform ownership, and the current root-motion boundary are
documented in [`docs/animation.md`](docs/animation.md).

Golf-shot coordinates, launch-monitor threading, model policy, and the current gameplay boundary are
documented in [`docs/golf.md`](docs/golf.md).

Cross-platform CircleCI builds and the manually triggered Debian/MSI prerelease process are
documented in [`docs/releasing.md`](docs/releasing.md).

## WSL + Qt quick start

Install Qt 6 and its Wayland platform plugin in Ubuntu 24.04:

```sh
sudo apt install build-essential cmake qt6-base-dev qt6-wayland libxkbcommon-dev
```

For the QML main menu, install Qt Declarative and its runtime imports as well:

```sh
sudo apt install qt6-declarative-dev qt6-declarative-dev-tools \
  qml6-module-qtquick qml6-module-qtquick-controls \
  qml6-module-qtquick-dialogs qml6-module-qtquick-layouts \
  qml6-module-qtquick-templates qml6-module-qtquick-window \
  qml6-module-qtqml-models qml6-module-qtqml-workerscript
```

Configure, build, test, and launch the QML menu with its checked-in preset:

```sh
cmake --preset wsl-qml
cmake --build --preset wsl-qml --parallel
ctest --preset wsl-qml
QT_QPA_PLATFORM=wayland ./build/wsl-qml/mgv_qml
```

The menu selects the model and runtime shaders through QML file dialogs. `MainMenuController`
validates those URLs and invokes an abstract `EngineLauncher`; the Qt adapter then opens the same
tested `QtViewerWindow` used by the widgets entrypoint. The menu remains open for rapid relaunches.

To launch the widgets entrypoint directly without the QML menu:

```sh
cmake --preset wsl-qt
cmake --build --preset wsl-qt --parallel
ctest --preset wsl-qt
QT_QPA_PLATFORM=wayland ./build/wsl-qt/mgv_qt
```

The included ball and default shaders load when no arguments are supplied. A model and custom
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

Use the arrow keys to orbit, `+`/`-` to zoom, `Home` or `R` to reset the camera, and Space to fire
the deterministic test shot. The Qt Controls menu exposes the same commands as clickable test
controls. GLFW uses the same keys.

The bundled ball and default shader exercise material texture binding with a generated checkerboard.
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
Qt / GLFW adapter -> queued EngineCommand -> Engine -> OrbitCameraController -> Renderer
QML menu -> MainMenuController -> EngineLauncher -> QtViewerWindow -> Engine
Qt / GLFW host    -> OpenGL backend -> RenderBackend <----------------------- Renderer
Scene -> Renderable -> Transform / MeshData / MaterialInstance --------> Renderer
                                      |-> Material -> Pipeline
                                      |-> Texture -> Image / Sampler
Engine -> PhysicsWorld -> RigidBody / CollisionDetector
          |-> BodyId bound to EntityId/RenderableId -> Renderer transform update
Engine -> AnimationSystem -> AnimationPlayerId -> EntityId/RenderableId transform update
LaunchMonitor -> queued Shot -> ShotModel -> active physics ball -> Renderer transform update
NetworkService -> NetworkEventSource -> NetworkEventDecoder -> queued EngineCommand
```

The CMake targets mirror these boundaries: `mgv::core`, `mgv::animation`, `mgv::golf`, `mgv::physics`,
`mgv::network`, `mgv::game`, `mgv::runtime`, `mgv::opengl`, `mgv::qt_frontend`, and the optional QML/Qt/GLFW
entrypoints. Ozz is private to `mgv::animation`; public engine and frontend headers expose no Ozz
types. A future Vulkan or Direct3D adapter can replace `mgv::opengl` without changing OBJ, camera,
animation, golf-shot, shader-file, networking, or scene orchestration code.

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
