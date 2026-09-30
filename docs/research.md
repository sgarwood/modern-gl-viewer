# Open-source design research

Research was performed on 30 September 2026 with GitHub CLI, using repository search, metadata,
source-tree inspection, and shallow clones. Representative commands:

```sh
gh search repos opengl --language C++ --sort stars --limit 20
gh repo view bkaradzic/bgfx --json description,stargazerCount,licenseInfo,url
gh repo clone cginternals/globjects /tmp/globjects -- --depth 1
gh api repos/DiligentGraphics/DiligentCore/contents/Graphics/GraphicsEngine/interface/RenderDevice.h
gh search code 'QOpenGLWidget initializeGL paintGL language:C++'
```

## Projects studied

### [bgfx](https://github.com/bkaradzic/bgfx)

An established, backend-agnostic rendering library. Its internal
[`RendererContextI`](https://github.com/bkaradzic/bgfx/blob/master/src/bgfx_p.h) is implemented by
separate OpenGL, Vulkan, Metal, and Direct3D translation units. The useful lesson here is that the
frontend should traffic in backend-neutral descriptions and resources, while native handles stay
inside an implementation.

Applied here: `RenderBackend` is the substitution boundary; no `GLenum`, GLuint, GLFW, or GLAD
header leaks into `mgv_core`.

### [Diligent Engine / DiligentCore](https://github.com/DiligentGraphics/DiligentCore)

Diligent exposes an explicit
[`IRenderDevice`](https://github.com/DiligentGraphics/DiligentCore/blob/master/Graphics/GraphicsEngine/interface/RenderDevice.h)
and backend-owned
[`IDeviceObject`](https://github.com/DiligentGraphics/DiligentCore/blob/master/Graphics/GraphicsEngine/interface/DeviceObject.h)
resources. It demonstrates the value of separating resource creation from command submission and
of making resources polymorphic at the API boundary.

Applied here: mesh/shader creation is separate from `begin_frame`/`draw`/`end_frame`, and resources
have abstract owning base classes. A new backend cannot accidentally receive another backend's
native object; the OpenGL adapter checks that boundary.

### [globjects](https://github.com/cginternals/globjects)

A focused C++ wrapper over OpenGL. Its
[`Object`](https://github.com/cginternals/globjects/blob/master/source/globjects/include/globjects/Object.h),
[`Shader`](https://github.com/cginternals/globjects/blob/master/source/globjects/include/globjects/Shader.h),
and [`Program`](https://github.com/cginternals/globjects/blob/master/source/globjects/include/globjects/Program.h)
classes bind native object lifetime to C++ object lifetime.

Applied here: every OpenGL name is held by a non-copyable RAII class. Partially constructed shader
programs clean themselves up on compilation or link failure, and application member order ensures
GL resources are destroyed while the context is still alive.

### [threepp](https://github.com/markaren/threepp)

A modern C++20 3D library with a concise
[`OBJLoader`](https://github.com/markaren/threepp/blob/master/include/threepp/loaders/OBJLoader.hpp)
Pimpl API and runtime-configurable
[`ShaderMaterial`](https://github.com/markaren/threepp/blob/master/include/threepp/materials/ShaderMaterial.hpp).
It is a useful precedent for keeping parsing internals private and making custom shader source a
first-class input.

Applied here: `ObjLoader` presents a small filesystem/stream API backed by Pimpl, while
`ShaderSources` is passed through the renderer without OpenGL-specific compilation concerns.

### [Magnum](https://github.com/mosra/magnum)

Magnum's modularity and extensive unit/GL test split reinforce testing format handling independently
of a live graphics context. This matters in build agents where a GPU or display server is absent.

Applied here: all OBJ, shader-file, and façade behavior is tested through `mgv_core`; the fake
backend verifies orchestration without initializing OpenGL.

### [Qt 6 QOpenGLWidget](https://doc.qt.io/qt-6/qopenglwidget.html)

Qt guarantees that the widget's context is current in `initializeGL()` and `paintGL()`, recommends
deferring all resource creation until `initializeGL()`, and calls for explicit resource cleanup
with the context current. It also recommends setting a shared core-profile `QSurfaceFormat` before
constructing `QApplication`.

Applied here: the Qt shell creates the backend and uploads assets only in `initializeGL()`, renders
only in `paintGL()`, and destroys RAII GL resources in both its destructor and the context's
`aboutToBeDestroyed` path. The default surface requests OpenGL 4.1, 24-bit depth, 8-bit stencil,
and multisampling before `QApplication` exists.

The GitHub CLI search also found established implementations in
[`nCine`](https://github.com/nCine/nCine/blob/master/src/QtWidget.cpp) and
[`fstl`](https://github.com/fstl-app/fstl/blob/master/src/canvas.cpp). Both reinforce the same
deferred initialization and context-current destruction pattern.

### [Qt CMake guidance](https://doc.qt.io/qt-6/cmake-get-started.html)

Qt recommends imported targets and `qt_add_executable`. `qt_standard_project_setup` is useful for
Qt-centric projects because it globally enables AUTOMOC/AUTOUIC; this mixed project deliberately
does not call it because no class uses `Q_OBJECT` and its global behavior needlessly processes
Catch2 and non-Qt targets.

Applied here: Qt is a system dependency discovered with `find_package(Qt6 6.4 REQUIRED COMPONENTS
Widgets OpenGLWidgets)`, linked only through imported targets, and finalized with
`qt_finalize_executable`. Qt is not fetched or leaked into the portable library.

### [WSLg GUI support](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps)

WSLg supports integrated Linux GUI applications through Wayland and X11. The inspected machine has
`WAYLAND_DISPLAY=wayland-0` and no `DISPLAY`, so the `wsl-qt` preset disables the GLFW shell and the
documented launch command explicitly selects Qt's Wayland platform plugin.

## Deliberate choices

- The project adopts ideas, not source code; no researched implementation is copied.
- The public API uses `std::unique_ptr` ownership instead of integer handles. It is smaller and
  harder to misuse at this scale while preserving backend substitution.
- GLSL is loaded and compiled at startup because the requested backend is OpenGL. A Vulkan or
  Direct3D backend can interpret `ShaderSources` differently or evolve the neutral descriptor to
  accept SPIR-V/DXIL without exposing native API types.
- The OBJ parser is local and narrowly tested rather than adding a large scene-import dependency.
  For glTF, materials, animation, or production asset conversion, Assimp or a dedicated offline
  pipeline would be more appropriate.
- Each optional frontend has its own CMake option and target. `mgv::opengl` accepts an injected
  procedure loader, allowing Qt and GLFW to share the exact backend without either depending on
  the other.
- FetchContent dependency sources are pinned to full commits, warnings live in a project-only
  interface target, Qt is found before downloads begin, and checked-in presets make WSL and
  headless configurations repeatable.
