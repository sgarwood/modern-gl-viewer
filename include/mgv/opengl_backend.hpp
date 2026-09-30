#pragma once

#include "mgv/render_backend.hpp"

#include <functional>
#include <memory>

namespace mgv {

using GlProcAddress = void (*)();
using GlProcLoader = std::function<GlProcAddress(const char*)>;

// Requires a current OpenGL context. The returned backend owns every GL object it creates.
[[nodiscard]] std::unique_ptr<RenderBackend> make_opengl_backend(GlProcLoader load);

} // namespace mgv
