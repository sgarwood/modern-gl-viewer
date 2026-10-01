#pragma once

#include "mgv/shader_loader.hpp"

namespace mgv {

enum class PrimitiveTopology {
    triangle_list,
    line_list,
    point_list,
};

enum class CullMode {
    none,
    front,
    back,
};

enum class FrontFace {
    counter_clockwise,
    clockwise,
};

enum class PolygonMode {
    fill,
    line,
};

enum class CompareOperation {
    never,
    less,
    equal,
    less_or_equal,
    greater,
    not_equal,
    greater_or_equal,
    always,
};

enum class BlendFactor {
    zero,
    one,
    source_alpha,
    one_minus_source_alpha,
};

struct RasterizationState final {
    CullMode cull_mode{CullMode::none};
    FrontFace front_face{FrontFace::counter_clockwise};
    PolygonMode polygon_mode{PolygonMode::fill};

    friend bool operator==(const RasterizationState&, const RasterizationState&) = default;
};

struct DepthState final {
    bool test_enabled{true};
    bool write_enabled{true};
    CompareOperation compare{CompareOperation::less};

    friend bool operator==(const DepthState&, const DepthState&) = default;
};

struct BlendState final {
    bool enabled{};
    BlendFactor source{BlendFactor::source_alpha};
    BlendFactor destination{BlendFactor::one_minus_source_alpha};

    friend bool operator==(const BlendState&, const BlendState&) = default;
};

struct RenderPipelineDescriptor final {
    ShaderSources shaders;
    PrimitiveTopology topology{PrimitiveTopology::triangle_list};
    RasterizationState rasterization;
    DepthState depth;
    BlendState blending;

    friend bool operator==(const RenderPipelineDescriptor&, const RenderPipelineDescriptor&) = default;
};

} // namespace mgv
