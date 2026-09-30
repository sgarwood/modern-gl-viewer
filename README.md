# Modern GL Viewer

A compact C++20 OBJ viewer with a replaceable rendering backend. The OpenGL 4.1 renderer can run
inside either the Qt 6 or GLFW application shell; the portable core contains no toolkit or OpenGL
types.

## Features

- Qt 6 `QOpenGLWidget` entrypoint designed for WSLg/Wayland, with file pickers for OBJ and GLSL.
- GLFW entrypoint for lightweight native use.
- Runtime OBJ loading with `v`, `vt`, `vn`, positive/negative indices, polygon triangulation,
  vertex deduplication, and generated normals.
- Runtime GLSL loading with useful file, compiler, and linker diagnostics.
- Backend-neutral `Camera`, `RenderBackend`, `MeshResource`, and `ShaderResource` APIs.
- RAII ownership for windows, GL buffers, vertex arrays, shaders, and programs.
- Pimpl façades for `Application`, `Renderer`, and `ObjLoader`.
- Headless Catch2 tests for camera math, asset loading, and render orchestration.

The research and resulting design decisions are recorded in
[`docs/research.md`](docs/research.md).

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
- `Camera` uses a right-handed look-at view and an OpenGL perspective projection.
- Matrices are column-major; OpenGL normalized device depth is −1 to +1.
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
omit it.

## Architecture

```text
Qt shell ---------\
                   -> OpenGL backend -> RenderBackend interface
GLFW shell -------/                         |
                                             -> Renderer facade -> Camera
ObjLoader ------> MeshData -----------------|
ShaderLoader ---> ShaderSources ------------|
```

The CMake targets mirror these boundaries: `mgv::core`, `mgv::opengl`, and the optional Qt/GLFW
entrypoints. A future Vulkan or Direct3D adapter can replace `mgv::opengl` without changing OBJ,
camera, shader-file, or scene orchestration code.

## Scope

This is intentionally an OBJ geometry viewer, not a full Wavefront material implementation.
`mtllib`, `usemtl`, smoothing groups, and texture image loading are ignored. Faces and geometry
attributes are supported, and absent normals are generated.
