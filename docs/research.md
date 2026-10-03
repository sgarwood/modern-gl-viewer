# Open-source design research

Research was performed on 30 September–3 October 2026 with GitHub CLI, using repository search,
metadata, source-tree inspection, and shallow clones. Representative commands:

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
header leaks into `mgv_core`. Immutable pipeline descriptors carry topology, rasterization, depth,
and blending state, while `DrawPacket` is the backend-neutral submission unit.

### [Diligent Engine / DiligentCore](https://github.com/DiligentGraphics/DiligentCore)

Diligent exposes an explicit
[`IRenderDevice`](https://github.com/DiligentGraphics/DiligentCore/blob/master/Graphics/GraphicsEngine/interface/RenderDevice.h)
and backend-owned
[`IDeviceObject`](https://github.com/DiligentGraphics/DiligentCore/blob/master/Graphics/GraphicsEngine/interface/DeviceObject.h)
resources. It demonstrates the value of separating resource creation from command submission and
of making resources polymorphic at the API boundary.

Applied here: mesh/pipeline creation is separate from `begin_frame`/`draw`/`end_frame`, and resources
have abstract owning base classes. Backends report clip-space capabilities, and an RAII frame scope
guarantees that every successfully begun frame ends. A new backend cannot accidentally receive
another backend's native object; the OpenGL adapter checks that boundary.

The same boundary now covers distinct texture and sampler resources. `MaterialInstance` owns named
bindings while `Material` remains immutable pipeline state, matching the material/material-instance
split used by larger renderers without importing their entity or resource-manager machinery.

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
`ShaderSources` declares its source language and travels inside a backend-neutral pipeline
descriptor. The OpenGL adapter explicitly accepts GLSL and rejects incompatible source languages.

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

The QML launcher follows the same adapter boundary. Declarative code sees only URL/status
properties and commands on `MainMenuController`; it never receives a renderer or native OpenGL
object. The controller depends on the abstract `EngineLauncher`, while `QtEngineLauncher` adapts
that port to the reusable widgets engine window. `qt_add_qml_module` embeds and compiles the menu,
and the feature remains an explicit CMake option so headless and non-Qt builds do not acquire QML
dependencies.

### [WSLg GUI support](https://learn.microsoft.com/en-us/windows/wsl/tutorials/gui-apps)

WSLg supports integrated Linux GUI applications through Wayland and X11. The inspected machine has
`WAYLAND_DISPLAY=wayland-0` and no `DISPLAY`, so the `wsl-qt` preset disables the GLFW shell and the
documented launch command explicitly selects Qt's Wayland platform plugin.

### [Asio](https://github.com/chriskohlhoff/asio)

Asio's current chat server examples serialize each socket's asynchronous read/write workflow and
keep pending messages in owned queues. This demonstrates the useful invariant that one execution
context owns transport state, while callers exchange messages rather than manipulating sockets.

Applied here: `NetworkService` confines the injected `DatagramTransport` to one worker and uses
owned queues at the thread boundary. The initial UDP adapter stays deliberately small instead of
adding Asio as another project dependency, but the transport port can accept an Asio adapter later.

### [GameNetworkingSockets](https://github.com/ValveSoftware/GameNetworkingSockets) and
[ENet](https://github.com/lsalzman/enet)

GameNetworkingSockets' example chat loop polls incoming messages and connection callbacks from one
owning loop, passing console input across threads through a synchronized queue. ENet similarly
centres network progress around repeated `enet_host_service()` calls. Both reinforce explicit
service-loop ownership and pulling events into application code at a controlled point.

Applied here: frontends pull `NetworkEvent` values; the worker never invokes gameplay or rendering
callbacks. Queues are bounded, shutdown is joined, and transport failures cross the thread boundary
as data rather than uncaught exceptions.

## Deliberate choices

- The project adopts ideas, not source code; no researched implementation is copied.
- The public API uses `std::unique_ptr` ownership instead of integer handles. It is smaller and
  harder to misuse at this scale while preserving backend substitution.
- GLSL is loaded and compiled at startup because the requested backend is OpenGL. Shader source
  language is explicit, so another backend can accept HLSL or evolve the neutral descriptor to
  carry SPIR-V/DXIL without exposing native API types.
- The OBJ parser is local and narrowly tested rather than adding a large scene-import dependency.
  For glTF, materials, animation, or production asset conversion, Assimp or a dedicated offline
  pipeline would be more appropriate.
- Each optional frontend has its own CMake option and target. `mgv::opengl` accepts an injected
  procedure loader, allowing Qt and GLFW to share the exact backend without either depending on
  the other.
- The scene layer separates immutable shared assets (`MeshData`, `Material`) from per-instance
  state (`MaterialInstance`, `Transform`, visibility). Renderer preparation deduplicates mesh,
  pipeline, texture, and sampler GPU resources by shared asset identity while retaining one draw
  submission per visible `Renderable`.
- FetchContent dependency sources are pinned to full commits, warnings live in a project-only
  interface target, Qt is found before downloads begin, and checked-in presets make WSL and
  headless configurations repeatable.
