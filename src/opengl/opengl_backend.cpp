#include "mgv/opengl_backend.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace mgv {
namespace {

class Buffer final {
public:
    Buffer() { glGenBuffers(1, &name_); }
    ~Buffer() { glDeleteBuffers(1, &name_); }
    Buffer(Buffer&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    Buffer& operator=(Buffer&& other) noexcept {
        if (this != &other) {
            glDeleteBuffers(1, &name_);
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

class VertexArray final {
public:
    VertexArray() { glGenVertexArrays(1, &name_); }
    ~VertexArray() { glDeleteVertexArrays(1, &name_); }
    VertexArray(VertexArray&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    VertexArray& operator=(VertexArray&& other) noexcept {
        if (this != &other) {
            glDeleteVertexArrays(1, &name_);
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }
    VertexArray(const VertexArray&) = delete;
    VertexArray& operator=(const VertexArray&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

class Renderbuffer final {
public:
    Renderbuffer() { glGenRenderbuffers(1, &name_); }
    ~Renderbuffer() { glDeleteRenderbuffers(1, &name_); }
    Renderbuffer(Renderbuffer&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    Renderbuffer& operator=(Renderbuffer&& other) noexcept {
        if (this != &other) {
            glDeleteRenderbuffers(1, &name_);
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }
    Renderbuffer(const Renderbuffer&) = delete;
    Renderbuffer& operator=(const Renderbuffer&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

class Framebuffer final {
public:
    Framebuffer() { glGenFramebuffers(1, &name_); }
    ~Framebuffer() { glDeleteFramebuffers(1, &name_); }
    Framebuffer(Framebuffer&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    Framebuffer& operator=(Framebuffer&& other) noexcept {
        if (this != &other) {
            glDeleteFramebuffers(1, &name_);
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }
    Framebuffer(const Framebuffer&) = delete;
    Framebuffer& operator=(const Framebuffer&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

/// Restores whatever framebuffer was bound on entry.
///
/// The host owns the binding: QOpenGLWidget and the capture tool each render
/// into a framebuffer of their own, so any setup that binds one of ours has
/// to put theirs back rather than assuming framebuffer zero.
class BoundFramebufferScope final {
public:
    BoundFramebufferScope() {
        GLint bound{};
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound);
        previous_ = static_cast<GLuint>(bound);
    }
    ~BoundFramebufferScope() { glBindFramebuffer(GL_FRAMEBUFFER, previous_); }
    BoundFramebufferScope(const BoundFramebufferScope&) = delete;
    BoundFramebufferScope& operator=(const BoundFramebufferScope&) = delete;
    BoundFramebufferScope(BoundFramebufferScope&&) = delete;
    BoundFramebufferScope& operator=(BoundFramebufferScope&&) = delete;

private:
    GLuint previous_{};
};

class Shader final {
public:
    explicit Shader(GLenum type) : name_{glCreateShader(type)} {
        if (name_ == 0) {
            throw std::runtime_error{"OpenGL failed to create a shader"};
        }
    }
    ~Shader() { glDeleteShader(name_); }
    Shader(Shader&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

class Program final {
public:
    Program() : name_{glCreateProgram()} {
        if (name_ == 0) {
            throw std::runtime_error{"OpenGL failed to create a program"};
        }
    }
    ~Program() { glDeleteProgram(name_); }
    Program(Program&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    Program& operator=(Program&& other) noexcept {
        if (this != &other) {
            glDeleteProgram(name_);
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

class TextureName final {
public:
    TextureName() { glGenTextures(1, &name_); }
    ~TextureName() { glDeleteTextures(1, &name_); }
    TextureName(TextureName&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    TextureName& operator=(TextureName&& other) noexcept {
        if (this != &other) {
            glDeleteTextures(1, &name_);
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }
    TextureName(const TextureName&) = delete;
    TextureName& operator=(const TextureName&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

class SamplerName final {
public:
    SamplerName() { glGenSamplers(1, &name_); }
    ~SamplerName() { glDeleteSamplers(1, &name_); }
    SamplerName(SamplerName&& other) noexcept : name_{std::exchange(other.name_, 0)} {}
    SamplerName& operator=(SamplerName&& other) noexcept {
        if (this != &other) {
            glDeleteSamplers(1, &name_);
            name_ = std::exchange(other.name_, 0);
        }
        return *this;
    }
    SamplerName(const SamplerName&) = delete;
    SamplerName& operator=(const SamplerName&) = delete;
    [[nodiscard]] GLuint get() const noexcept { return name_; }

private:
    GLuint name_{};
};

[[nodiscard]] std::string shader_log(GLuint shader) {
    GLint length{};
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    return log;
}

[[nodiscard]] std::string program_log(GLuint program) {
    GLint length{};
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(length, 1)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    return log;
}

[[nodiscard]] Shader compile_shader(GLenum type, const std::string& source, std::string_view name) {
    Shader shader{type};
    const auto* data = source.c_str();
    const auto size = static_cast<GLint>(source.size());
    glShaderSource(shader.get(), 1, &data, &size);
    glCompileShader(shader.get());
    GLint compiled{};
    glGetShaderiv(shader.get(), GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        throw std::runtime_error{"Shader compilation failed for " + std::string{name} + ":\n" + shader_log(shader.get())};
    }
    return shader;
}

class OpenGlMesh final : public MeshResource {
public:
    explicit OpenGlMesh(const MeshData& mesh) : index_count_{checked_count(mesh.indices.size())} {
        static_assert(std::is_standard_layout_v<Vertex>);
        glBindVertexArray(vertex_array_.get());
        glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer_.get());
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh.vertices.size() * sizeof(Vertex)),
            mesh.vertices.data(),
            GL_STATIC_DRAW);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer_.get());
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(mesh.indices.size() * sizeof(std::uint32_t)),
            mesh.indices.data(),
            GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, position)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tex_coord)));

        // Every mesh carries an instance buffer, even an ordinary one, whose
        // single entry is the identity placement. That way one depth program
        // and one vertex layout serve both cases, instead of needing an
        // instanced variant of each.
        static_assert(std::is_standard_layout_v<MeshInstance>);
        static constexpr MeshInstance identity_instance{};
        const auto* instance_data =
            mesh.instances.empty() ? &identity_instance : mesh.instances.data();
        const auto instances = mesh.instance_count();
        instance_count_ = checked_count(instances);
        glBindBuffer(GL_ARRAY_BUFFER, instance_buffer_.get());
        glBufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(instances * sizeof(MeshInstance)),
            instance_data,
            GL_STATIC_DRAW);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(
            3, 4, GL_FLOAT, GL_FALSE, sizeof(MeshInstance),
            reinterpret_cast<void*>(offsetof(MeshInstance, position)));
        glVertexAttribDivisor(3, 1);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(
            4, 4, GL_FLOAT, GL_FALSE, sizeof(MeshInstance),
            reinterpret_cast<void*>(offsetof(MeshInstance, parameters)));
        glVertexAttribDivisor(4, 1);
        glBindVertexArray(0);
    }

    [[nodiscard]] GLuint vertex_array() const noexcept { return vertex_array_.get(); }
    [[nodiscard]] GLsizei index_count() const noexcept { return index_count_; }
    [[nodiscard]] GLsizei instance_count() const noexcept { return instance_count_; }

private:
    [[nodiscard]] static GLsizei checked_count(std::size_t count) {
        if (count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
            throw std::overflow_error{"Mesh has too many indices for OpenGL"};
        }
        return static_cast<GLsizei>(count);
    }

    VertexArray vertex_array_;
    Buffer vertex_buffer_;
    Buffer index_buffer_;
    Buffer instance_buffer_;
    GLsizei index_count_{};
    GLsizei instance_count_{1};
};

[[nodiscard]] GLenum primitive_topology(PrimitiveTopology topology) {
    switch (topology) {
    case PrimitiveTopology::triangle_list:
        return GL_TRIANGLES;
    case PrimitiveTopology::line_list:
        return GL_LINES;
    case PrimitiveTopology::point_list:
        return GL_POINTS;
    }
    throw std::invalid_argument{"Unsupported primitive topology"};
}

[[nodiscard]] GLenum cull_face(CullMode mode) {
    switch (mode) {
    case CullMode::front:
        return GL_FRONT;
    case CullMode::back:
        return GL_BACK;
    case CullMode::none:
        break;
    }
    throw std::invalid_argument{"Cull face requested for disabled culling"};
}

[[nodiscard]] GLenum front_face(FrontFace face) {
    switch (face) {
    case FrontFace::counter_clockwise:
        return GL_CCW;
    case FrontFace::clockwise:
        return GL_CW;
    }
    throw std::invalid_argument{"Unsupported front-face winding"};
}

[[nodiscard]] GLenum polygon_mode(PolygonMode mode) {
    switch (mode) {
    case PolygonMode::fill:
        return GL_FILL;
    case PolygonMode::line:
        return GL_LINE;
    }
    throw std::invalid_argument{"Unsupported polygon mode"};
}

[[nodiscard]] GLenum compare_operation(CompareOperation operation) {
    switch (operation) {
    case CompareOperation::never:
        return GL_NEVER;
    case CompareOperation::less:
        return GL_LESS;
    case CompareOperation::equal:
        return GL_EQUAL;
    case CompareOperation::less_or_equal:
        return GL_LEQUAL;
    case CompareOperation::greater:
        return GL_GREATER;
    case CompareOperation::not_equal:
        return GL_NOTEQUAL;
    case CompareOperation::greater_or_equal:
        return GL_GEQUAL;
    case CompareOperation::always:
        return GL_ALWAYS;
    }
    throw std::invalid_argument{"Unsupported comparison operation"};
}

[[nodiscard]] GLenum blend_factor(BlendFactor factor) {
    switch (factor) {
    case BlendFactor::zero:
        return GL_ZERO;
    case BlendFactor::one:
        return GL_ONE;
    case BlendFactor::source_alpha:
        return GL_SRC_ALPHA;
    case BlendFactor::one_minus_source_alpha:
        return GL_ONE_MINUS_SRC_ALPHA;
    }
    throw std::invalid_argument{"Unsupported blend factor"};
}

[[nodiscard]] GLenum texture_filter(TextureFilter filter) {
    switch (filter) {
    case TextureFilter::nearest:
        return GL_NEAREST;
    case TextureFilter::linear:
        return GL_LINEAR;
    }
    throw std::invalid_argument{"Unsupported texture filter"};
}

[[nodiscard]] GLenum texture_address_mode(TextureAddressMode mode) {
    switch (mode) {
    case TextureAddressMode::repeat:
        return GL_REPEAT;
    case TextureAddressMode::mirrored_repeat:
        return GL_MIRRORED_REPEAT;
    case TextureAddressMode::clamp_to_edge:
        return GL_CLAMP_TO_EDGE;
    }
    throw std::invalid_argument{"Unsupported texture address mode"};
}

[[nodiscard]] GLsizei texture_dimension(std::uint32_t dimension) {
    if (dimension > static_cast<std::uint32_t>(std::numeric_limits<GLsizei>::max())) {
        throw std::overflow_error{"Texture dimension exceeds the OpenGL limit"};
    }
    return static_cast<GLsizei>(dimension);
}

class OpenGlTexture final : public TextureResource {
public:
    explicit OpenGlTexture(const ImageData& image) {
        if (image.format() != PixelFormat::rgba8_unorm) {
            throw std::invalid_argument{"OpenGL backend received an unsupported pixel format"};
        }
        const auto internal_format = image.color_space() == ColorSpace::srgb ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        glBindTexture(GL_TEXTURE_2D, name_.get());
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            internal_format,
            texture_dimension(image.width()),
            texture_dimension(image.height()),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            image.pixels().data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    [[nodiscard]] GLuint name() const noexcept { return name_.get(); }

private:
    TextureName name_;
};

class OpenGlSampler final : public SamplerResource {
public:
    explicit OpenGlSampler(const SamplerDescriptor& descriptor) {
        glSamplerParameteri(name_.get(), GL_TEXTURE_MIN_FILTER, static_cast<GLint>(texture_filter(descriptor.min_filter)));
        glSamplerParameteri(name_.get(), GL_TEXTURE_MAG_FILTER, static_cast<GLint>(texture_filter(descriptor.mag_filter)));
        glSamplerParameteri(name_.get(), GL_TEXTURE_WRAP_S, static_cast<GLint>(texture_address_mode(descriptor.address_u)));
        glSamplerParameteri(name_.get(), GL_TEXTURE_WRAP_T, static_cast<GLint>(texture_address_mode(descriptor.address_v)));
    }

    [[nodiscard]] GLuint name() const noexcept { return name_.get(); }

private:
    SamplerName name_;
};

// ---------------------------------------------------------------------------
// High dynamic range scene target and tone-mapped composite
// ---------------------------------------------------------------------------

/// A floating-point colour target the scene is rendered into, so lighting can
/// work in absolute radiometric units and only the composite pass decides what
/// the display sees.
class HdrTarget final {
public:
    /// Recreates the attachments when the viewport changes. Returns false when
    /// the driver cannot provide a complete floating-point target, which lets
    /// the backend fall back to rendering straight to the output framebuffer.
    [[nodiscard]] bool ensure(GLsizei width, GLsizei height) {
        if (width == width_ && height == height_) {
            return complete_;
        }
        width_ = width;
        height_ = height;
        const BoundFramebufferScope restore;

        glBindTexture(GL_TEXTURE_2D, color_.name());
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, width, height, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindRenderbuffer(GL_RENDERBUFFER, depth_.get());
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.get());
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_.name(), 0);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_.get());
        complete_ = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        return complete_;
    }

    [[nodiscard]] GLuint framebuffer() const noexcept { return framebuffer_.get(); }
    [[nodiscard]] GLuint color_texture() const noexcept { return color_.name(); }

private:
    class ColorTexture final {
    public:
        ColorTexture() { glGenTextures(1, &name_); }
        ~ColorTexture() { glDeleteTextures(1, &name_); }
        ColorTexture(ColorTexture&&) = delete;
        ColorTexture& operator=(ColorTexture&&) = delete;
        ColorTexture(const ColorTexture&) = delete;
        ColorTexture& operator=(const ColorTexture&) = delete;
        [[nodiscard]] GLuint name() const noexcept { return name_; }

    private:
        GLuint name_{};
    };

    Framebuffer framebuffer_;
    ColorTexture color_;
    Renderbuffer depth_;
    GLsizei width_{-1};
    GLsizei height_{-1};
    bool complete_{};
};

constexpr std::string_view composite_vertex_source = R"(#version 410 core
out vec2 vUv;
void main() {
    // A single oversized triangle covering the viewport, so there is no seam
    // down the diagonal that a two-triangle quad would introduce.
    vec2 corner = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = corner;
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
)";

constexpr std::string_view composite_fragment_source = R"(#version 410 core
in vec2 vUv;
out vec4 fragColor;

uniform sampler2D uSceneColor;
uniform float uExposure;
uniform vec2 uViewportSize;

// ACES filmic tone mapping, using Stephen Hill's fit of the RRT and ODT.
const mat3 kAcesInput = mat3(
    0.59719, 0.07600, 0.02840,
    0.35458, 0.90834, 0.13383,
    0.04823, 0.01566, 0.83777);

const mat3 kAcesOutput = mat3(
     1.60475, -0.10208, -0.00327,
    -0.53108,  1.10813, -0.07276,
    -0.07367, -0.00605,  1.07602);

vec3 rrt_and_odt_fit(vec3 v) {
    vec3 a = v * (v + 0.0245786) - 0.000090537;
    vec3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return a / b;
}

vec3 tonemap_aces(vec3 color) {
    color = kAcesInput * color;
    color = rrt_and_odt_fit(color);
    return clamp(kAcesOutput * color, 0.0, 1.0);
}

vec3 encode_srgb(vec3 linear) {
    vec3 low = linear * 12.92;
    vec3 high = 1.055 * pow(max(linear, vec3(1e-5)), vec3(1.0 / 2.4)) - 0.055;
    return mix(low, high, step(vec3(0.0031308), linear));
}

// An ordered dither of well under one 8-bit step, which breaks up the banding
// that wide sky gradients otherwise show after quantisation.
float dither(vec2 position) {
    return fract(dot(position, vec2(0.0344827586, 0.0689655172)) + 0.5) - 0.5;
}

void main() {
    vec3 scene = texture(uSceneColor, vUv).rgb;
    vec3 mapped = tonemap_aces(scene * uExposure);
    vec3 display = encode_srgb(mapped);
    display += dither(gl_FragCoord.xy) * (1.0 / 255.0);
    fragColor = vec4(display, 1.0);
}
)";

/// Resolves the floating-point scene target to the output framebuffer,
/// applying exposure, the ACES curve, the sRGB transfer function, and a dither.
class CompositePass final {
public:
    CompositePass() {
        const auto vertex = compile_shader(
            GL_VERTEX_SHADER, std::string{composite_vertex_source}, "composite.vert");
        const auto fragment = compile_shader(
            GL_FRAGMENT_SHADER, std::string{composite_fragment_source}, "composite.frag");
        glAttachShader(program_.get(), vertex.get());
        glAttachShader(program_.get(), fragment.get());
        glLinkProgram(program_.get());
        GLint linked{};
        glGetProgramiv(program_.get(), GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            throw std::runtime_error{
                "Composite shader link failed:\n" + program_log(program_.get())};
        }
        scene_color_ = glGetUniformLocation(program_.get(), "uSceneColor");
        exposure_ = glGetUniformLocation(program_.get(), "uExposure");
        viewport_ = glGetUniformLocation(program_.get(), "uViewportSize");
    }

    void run(GLuint scene_texture, float exposure, GLsizei width, GLsizei height) const {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glUseProgram(program_.get());
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, scene_texture);
        glBindSampler(0, 0);
        if (scene_color_ >= 0) {
            glUniform1i(scene_color_, 0);
        }
        if (exposure_ >= 0) {
            glUniform1f(exposure_, exposure);
        }
        if (viewport_ >= 0) {
            glUniform2f(viewport_, static_cast<float>(width), static_cast<float>(height));
        }
        glBindVertexArray(vertex_array_.get());
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
    }

private:
    Program program_;
    VertexArray vertex_array_;
    GLint scene_color_{-1};
    GLint exposure_{-1};
    GLint viewport_{-1};
};

// ---------------------------------------------------------------------------
// Directional shadow map
// ---------------------------------------------------------------------------

constexpr GLsizei shadow_map_resolution = 2048;

constexpr std::string_view depth_vertex_source = R"(#version 410 core
layout(location = 0) in vec3 aPosition;
layout(location = 3) in vec4 aInstancePosition;   // xyz offset, w yaw
layout(location = 4) in vec4 aInstanceParameters; // x scale
uniform mat4 uMvp;
void main() {
    float sine = sin(aInstancePosition.w);
    float cosine = cos(aInstancePosition.w);
    vec3 local = aPosition * max(aInstanceParameters.x, 1e-4);
    vec3 placed = vec3(
        local.x * cosine + local.z * sine,
        local.y,
        -local.x * sine + local.z * cosine) + aInstancePosition.xyz;
    gl_Position = uMvp * vec4(placed, 1.0);
}
)";

constexpr std::string_view depth_fragment_source = R"(#version 410 core
void main() {}
)";

/// A depth-only target rendered from the sun's point of view.
///
/// Geometry is drawn with a single trivial program rather than each surface's
/// own: a shadow map records only where something is, and compiling a depth
/// variant of every material would multiply the pipeline count for no gain.
class ShadowMap final {
public:
    ShadowMap() {
        const BoundFramebufferScope restore;
        glGenTextures(1, &depth_texture_);
        glBindTexture(GL_TEXTURE_2D, depth_texture_);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24,
            shadow_map_resolution, shadow_map_resolution, 0,
            GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        // Anything outside the covered region is lit, not shadowed.
        constexpr std::array<GLfloat, 4> border{1.0F, 1.0F, 1.0F, 1.0F};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border.data());
        // Hardware comparison, so a bilinear fetch returns a filtered
        // occlusion fraction rather than a filtered depth.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glBindTexture(GL_TEXTURE_2D, 0);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.get());
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depth_texture_, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        complete_ = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

        const auto vertex = compile_shader(
            GL_VERTEX_SHADER, std::string{depth_vertex_source}, "shadow.vert");
        const auto fragment = compile_shader(
            GL_FRAGMENT_SHADER, std::string{depth_fragment_source}, "shadow.frag");
        glAttachShader(program_.get(), vertex.get());
        glAttachShader(program_.get(), fragment.get());
        glLinkProgram(program_.get());
        GLint linked{};
        glGetProgramiv(program_.get(), GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            throw std::runtime_error{"Shadow shader link failed:\n" + program_log(program_.get())};
        }
        mvp_location_ = glGetUniformLocation(program_.get(), "uMvp");
    }

    ~ShadowMap() { glDeleteTextures(1, &depth_texture_); }
    ShadowMap(const ShadowMap&) = delete;
    ShadowMap& operator=(const ShadowMap&) = delete;
    ShadowMap(ShadowMap&&) = delete;
    ShadowMap& operator=(ShadowMap&&) = delete;

    [[nodiscard]] bool complete() const noexcept { return complete_; }
    [[nodiscard]] GLuint depth_texture() const noexcept { return depth_texture_; }

    void begin() const {
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_.get());
        glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
        // Front-face culling moves acne onto surfaces the camera cannot see.
        // Combined with a slope-scaled offset it keeps thin geometry from
        // detaching from its own shadow.
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(2.4F, 4.0F);
        glClear(GL_DEPTH_BUFFER_BIT);
        glUseProgram(program_.get());
    }

    void draw(const OpenGlMesh& mesh, const Mat4& model_view_projection) const {
        if (mvp_location_ >= 0) {
            glUniformMatrix4fv(mvp_location_, 1, GL_FALSE, model_view_projection.data());
        }
        glBindVertexArray(mesh.vertex_array());
        glDrawElementsInstanced(
            GL_TRIANGLES, mesh.index_count(), GL_UNSIGNED_INT, nullptr, mesh.instance_count());
        glBindVertexArray(0);
    }

    static void end() noexcept {
        glDisable(GL_POLYGON_OFFSET_FILL);
        glCullFace(GL_BACK);
    }

private:
    Framebuffer framebuffer_;
    Program program_;
    GLuint depth_texture_{};
    GLint mvp_location_{-1};
    bool complete_{};
};

/// The uniforms every mgv shader may declare. A shader that omits one simply
/// does not receive it, which keeps the minimal viewer shaders valid.
enum class StandardUniform : std::size_t {
    model,
    normal_matrix,
    view,
    projection,
    view_projection,
    camera_position,
    time,
    sun_direction,
    sun_color,
    sun_illuminance,
    sky_zenith_color,
    sky_horizon_color,
    ground_albedo,
    sky_illuminance,
    turbidity,
    fog_density,
    fog_height_falloff,
    exposure,
    wind_speed,
    wind_direction,
    surface_wetness,
    viewport_size,
    sun_view_projection,
    shadow_map,
    shadow_texel_size,
    trail_count,
    trail_positions,
};

constexpr std::array standard_uniform_names{
    "uModel",
    "uNormalMatrix",
    "uView",
    "uProjection",
    "uViewProjection",
    "uCameraPosition",
    "uTime",
    "uSunDirection",
    "uSunColor",
    "uSunIlluminance",
    "uSkyZenithColor",
    "uSkyHorizonColor",
    "uGroundAlbedo",
    "uSkyIlluminance",
    "uTurbidity",
    "uFogDensity",
    "uFogHeightFalloff",
    "uExposure",
    "uWindSpeed",
    "uWindDirection",
    "uSurfaceWetness",
    "uViewportSize",
    "uSunViewProjection",
    "uShadowMap",
    "uShadowTexelSize",
    "uTrailCount",
    "uTrailPositions",
};

class OpenGlPipeline final : public RenderPipelineResource {
public:
    explicit OpenGlPipeline(const RenderPipelineDescriptor& descriptor)
        : topology_{descriptor.topology},
          rasterization_{descriptor.rasterization},
          depth_{descriptor.depth},
          blending_{descriptor.blending} {
        const auto& sources = descriptor.shaders;
        if (sources.language != ShaderSourceLanguage::glsl) {
            throw std::invalid_argument{"OpenGL backend requires GLSL shader source"};
        }
        const auto vertex = compile_shader(GL_VERTEX_SHADER, sources.vertex, sources.vertex_name);
        const auto fragment = compile_shader(GL_FRAGMENT_SHADER, sources.fragment, sources.fragment_name);
        glAttachShader(program_.get(), vertex.get());
        glAttachShader(program_.get(), fragment.get());
        glLinkProgram(program_.get());
        GLint linked{};
        glGetProgramiv(program_.get(), GL_LINK_STATUS, &linked);
        if (linked != GL_TRUE) {
            throw std::runtime_error{"Shader link failed:\n" + program_log(program_.get())};
        }
        mvp_location_ = glGetUniformLocation(program_.get(), "uMvp");
        for (std::size_t index = 0; index < standard_uniform_names.size(); ++index) {
            standard_uniforms_[index] =
                glGetUniformLocation(program_.get(), standard_uniform_names[index]);
        }
    }

    [[nodiscard]] GLuint program() const noexcept { return program_.get(); }
    [[nodiscard]] GLint mvp_location() const noexcept { return mvp_location_; }

    /// Returns the cached location of a standard uniform, or -1 when the shader
    /// does not declare it. Shaders are free to use any subset.
    [[nodiscard]] GLint standard_uniform(StandardUniform uniform) const noexcept {
        return standard_uniforms_[static_cast<std::size_t>(uniform)];
    }

    /// Resolves a material-supplied binding name, caching the lookup so a
    /// repeated texture or colour name costs one hash probe per draw.
    [[nodiscard]] GLint named_uniform(const std::string& name) const {
        const auto cached = named_uniforms_.find(name);
        if (cached != named_uniforms_.end()) {
            return cached->second;
        }
        const auto location = glGetUniformLocation(program_.get(), name.c_str());
        named_uniforms_.emplace(name, location);
        return location;
    }
    [[nodiscard]] PrimitiveTopology topology() const noexcept { return topology_; }
    [[nodiscard]] const RasterizationState& rasterization() const noexcept { return rasterization_; }
    [[nodiscard]] const DepthState& depth() const noexcept { return depth_; }
    [[nodiscard]] const BlendState& blending() const noexcept { return blending_; }

private:
    Program program_;
    GLint mvp_location_{-1};
    std::array<GLint, standard_uniform_names.size()> standard_uniforms_{};
    mutable std::unordered_map<std::string, GLint> named_uniforms_;
    PrimitiveTopology topology_;
    RasterizationState rasterization_;
    DepthState depth_;
    BlendState blending_;
};

template <typename Target>
[[nodiscard]] const Target& backend_resource(const auto& resource, std::string_view kind) {
    const auto* result = dynamic_cast<const Target*>(&resource);
    if (result == nullptr) {
        throw std::invalid_argument{"Attempted to use a " + std::string{kind} + " from another render backend"};
    }
    return *result;
}

class OpenGlBackend final : public RenderBackend {
public:
    OpenGlBackend() {
        GLint vertex_units{};
        GLint fragment_units{};
        glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &vertex_units);
        glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &fragment_units);
        const auto units = std::min(vertex_units, fragment_units);
        max_sampled_textures_ = units > 0 ? static_cast<std::uint32_t>(units) : 0;

        try {
            shadow_map_.emplace();
            shadows_available_ = shadow_map_->complete() && max_sampled_textures_ > 1;
        } catch (const std::exception&) {
            // A driver that cannot give us a depth target still renders, just
            // without shadows.
            shadow_map_.reset();
            shadows_available_ = false;
        }
    }

    [[nodiscard]] RenderBackendCapabilities capabilities() const noexcept override {
        return {
            .clip_space = {},
            .wireframe = true,
            // One unit is reserved for the shadow map.
            .max_sampled_textures = max_sampled_textures_ > 0 ? max_sampled_textures_ - 1 : 0,
            .directional_shadows = shadows_available_,
            .shadow_map_resolution = shadow_map_resolution,
        };
    }

    std::unique_ptr<MeshResource> create_mesh(const MeshData& mesh) override {
        return std::make_unique<OpenGlMesh>(mesh);
    }

    std::unique_ptr<RenderPipelineResource> create_pipeline(
        const RenderPipelineDescriptor& descriptor) override {
        return std::make_unique<OpenGlPipeline>(descriptor);
    }

    std::unique_ptr<TextureResource> create_texture(const ImageData& image) override {
        return std::make_unique<OpenGlTexture>(image);
    }

    std::unique_ptr<SamplerResource> create_sampler(const SamplerDescriptor& descriptor) override {
        return std::make_unique<OpenGlSampler>(descriptor);
    }

    void begin_frame(const Frame& frame) override {
        frame_ = frame;
        trail_.clear();
        trail_.reserve(frame.wet_trail.size() * 3);
        for (const auto& point : frame.wet_trail) {
            trail_.push_back(point.x);
            trail_.push_back(point.y);
            trail_.push_back(point.z);
        }

        const auto width = static_cast<GLsizei>(frame.framebuffer_width);
        const auto height = static_cast<GLsizei>(frame.framebuffer_height);

        // The host owns the framebuffer we were called with: QOpenGLWidget and
        // the capture tool both render into their own. Remember it so the
        // composite can resolve back into the right place.
        GLint bound{};
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound);
        output_framebuffer_ = static_cast<GLuint>(bound);

        scene_is_hdr_ = !hdr_disabled_ && hdr_target_.ensure(width, height);
        if (scene_is_hdr_) {
            glBindFramebuffer(GL_FRAMEBUFFER, hdr_target_.framebuffer());
        }

        glViewport(0, 0, width, height);
        glDepthMask(GL_TRUE);
        glClearColor(0.0F, 0.0F, 0.0F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void begin_shadow_pass(const Frame& frame) override {
        if (!shadows_available_) {
            return;
        }
        frame_ = frame;
        GLint bound{};
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &bound);
        output_framebuffer_ = static_cast<GLuint>(bound);
        in_shadow_pass_ = true;
        shadow_map_->begin();
    }

    void end_shadow_pass() noexcept override {
        if (!in_shadow_pass_) {
            return;
        }
        ShadowMap::end();
        in_shadow_pass_ = false;
        glBindFramebuffer(GL_FRAMEBUFFER, output_framebuffer_);
    }

    /// Uploads whichever standard uniforms `pipeline` declares. Called once per
    /// draw because the active program changes between pipelines.
    void upload_standard_uniforms(const OpenGlPipeline& pipeline, const DrawPacket& packet) const {
        const auto& environment = frame_.environment;
        const auto matrix = [&pipeline](StandardUniform uniform, const Mat4& value) {
            const auto location = pipeline.standard_uniform(uniform);
            if (location >= 0) {
                glUniformMatrix4fv(location, 1, GL_FALSE, value.data());
            }
        };
        const auto vector3 = [&pipeline](StandardUniform uniform, const Vec3& value) {
            const auto location = pipeline.standard_uniform(uniform);
            if (location >= 0) {
                glUniform3f(location, value.x, value.y, value.z);
            }
        };
        const auto scalar = [&pipeline](StandardUniform uniform, float value) {
            const auto location = pipeline.standard_uniform(uniform);
            if (location >= 0) {
                glUniform1f(location, value);
            }
        };

        matrix(StandardUniform::model, packet.model);
        matrix(StandardUniform::normal_matrix, packet.normal_matrix);
        matrix(StandardUniform::view, frame_.view);
        matrix(StandardUniform::projection, frame_.projection);
        matrix(StandardUniform::view_projection, frame_.view_projection);
        vector3(StandardUniform::camera_position, frame_.camera_position);
        scalar(StandardUniform::time, frame_.elapsed_seconds);
        vector3(StandardUniform::sun_direction, environment.sun.direction);
        vector3(StandardUniform::sun_color, environment.sun.color);
        scalar(StandardUniform::sun_illuminance, environment.sun.illuminance);
        vector3(StandardUniform::sky_zenith_color, environment.sky_zenith_color);
        vector3(StandardUniform::sky_horizon_color, environment.sky_horizon_color);
        vector3(StandardUniform::ground_albedo, environment.ground_albedo);
        scalar(StandardUniform::sky_illuminance, environment.sky_illuminance);
        scalar(StandardUniform::turbidity, environment.turbidity);
        scalar(StandardUniform::fog_density, environment.fog_density);
        scalar(StandardUniform::fog_height_falloff, environment.fog_height_falloff);
        scalar(StandardUniform::exposure, environment.exposure);
        scalar(StandardUniform::wind_speed, environment.wind_speed);
        scalar(StandardUniform::wind_direction, environment.wind_direction_radians);
        scalar(StandardUniform::surface_wetness, environment.surface_wetness);

        const auto viewport_location = pipeline.standard_uniform(StandardUniform::viewport_size);
        if (viewport_location >= 0) {
            glUniform2f(
                viewport_location,
                static_cast<float>(frame_.framebuffer_width),
                static_cast<float>(frame_.framebuffer_height));
        }

        matrix(StandardUniform::sun_view_projection, frame_.sun_view_projection);
        scalar(StandardUniform::shadow_texel_size, 1.0F / static_cast<float>(shadow_map_resolution));
        const auto shadow_location = pipeline.standard_uniform(StandardUniform::shadow_map);
        if (shadow_location >= 0 && shadows_available_) {
            const auto unit = shadow_map_unit();
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, shadow_map_->depth_texture());
            glBindSampler(unit, 0);
            glUniform1i(shadow_location, static_cast<GLint>(unit));
            glActiveTexture(GL_TEXTURE0);
        }

        const auto count_location = pipeline.standard_uniform(StandardUniform::trail_count);
        const auto point_count = static_cast<GLsizei>(trail_.size() / 3);
        if (count_location >= 0) {
            glUniform1i(count_location, point_count);
        }
        const auto trail_location = pipeline.standard_uniform(StandardUniform::trail_positions);
        if (trail_location >= 0 && point_count > 0) {
            glUniform3fv(trail_location, point_count, trail_.data());
        }
    }

    void draw(const DrawPacket& packet) override {
        const auto& gl_mesh = backend_resource<OpenGlMesh>(packet.mesh, "mesh");
        if (in_shadow_pass_) {
            shadow_map_->draw(gl_mesh, packet.model_view_projection);
            return;
        }
        const auto& pipeline = backend_resource<OpenGlPipeline>(packet.pipeline, "pipeline");

        if (pipeline.depth().test_enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        glDepthMask(pipeline.depth().write_enabled ? GL_TRUE : GL_FALSE);
        glDepthFunc(compare_operation(pipeline.depth().compare));

        if (pipeline.rasterization().cull_mode == CullMode::none) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
            glCullFace(cull_face(pipeline.rasterization().cull_mode));
        }
        glFrontFace(front_face(pipeline.rasterization().front_face));
        glPolygonMode(GL_FRONT_AND_BACK, polygon_mode(pipeline.rasterization().polygon_mode));

        if (pipeline.blending().enabled) {
            glEnable(GL_BLEND);
            glBlendFunc(
                blend_factor(pipeline.blending().source),
                blend_factor(pipeline.blending().destination));
        } else {
            glDisable(GL_BLEND);
        }

        glUseProgram(pipeline.program());
        if (pipeline.mvp_location() >= 0) {
            glUniformMatrix4fv(
                pipeline.mvp_location(),
                1,
                GL_FALSE,
                packet.model_view_projection.data());
        }
        upload_standard_uniforms(pipeline, packet);
        for (const auto& binding : packet.colors) {
            const auto location = pipeline.named_uniform(binding.name);
            if (location >= 0) {
                glUniform4f(location, binding.value.x, binding.value.y, binding.value.z, binding.value.w);
            }
        }
        if (packet.textures.size() > max_sampled_textures_) {
            throw std::runtime_error{"Draw packet exceeds the OpenGL sampled-texture limit"};
        }
        for (std::size_t index = 0; index < packet.textures.size(); ++index) {
            const auto& binding = packet.textures[index];
            if (binding.texture == nullptr || binding.sampler == nullptr) {
                throw std::invalid_argument{"Draw packet contains an empty texture binding"};
            }
            const auto& texture = backend_resource<OpenGlTexture>(*binding.texture, "texture");
            const auto& sampler = backend_resource<OpenGlSampler>(*binding.sampler, "sampler");
            const auto unit = static_cast<GLenum>(index);
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, texture.name());
            glBindSampler(static_cast<GLuint>(index), sampler.name());
            const auto location = pipeline.named_uniform(binding.name);
            if (location >= 0) {
                glUniform1i(location, static_cast<GLint>(index));
            }
        }
        glBindVertexArray(gl_mesh.vertex_array());
        glDrawElementsInstanced(
            primitive_topology(pipeline.topology()),
            gl_mesh.index_count(),
            GL_UNSIGNED_INT,
            nullptr,
            gl_mesh.instance_count());
        glBindVertexArray(0);
        for (std::size_t index = 0; index < packet.textures.size(); ++index) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
            glBindSampler(static_cast<GLuint>(index), 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
    }

    void end_frame() noexcept override {
        if (!scene_is_hdr_) {
            return;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, output_framebuffer_);
        glViewport(0, 0, frame_.framebuffer_width, frame_.framebuffer_height);
        try {
            if (!composite_) {
                composite_.emplace();
            }
            composite_->run(
                hdr_target_.color_texture(),
                frame_.environment.exposure,
                static_cast<GLsizei>(frame_.framebuffer_width),
                static_cast<GLsizei>(frame_.framebuffer_height));
        } catch (const std::exception&) {
            // A driver that cannot compile the composite shader still gets a
            // picture: subsequent frames skip the HDR path entirely.
            scene_is_hdr_ = false;
            hdr_disabled_ = true;
        }
    }

private:
    /// The shadow map lives on the last unit the driver offers, above every
    /// unit a material is allowed to claim.
    [[nodiscard]] GLuint shadow_map_unit() const noexcept {
        return max_sampled_textures_ > 0 ? max_sampled_textures_ - 1 : 0;
    }

    std::uint32_t max_sampled_textures_{};
    std::optional<ShadowMap> shadow_map_;
    bool shadows_available_{};
    bool in_shadow_pass_{};
    Frame frame_;
    std::vector<float> trail_;
    HdrTarget hdr_target_;
    std::optional<CompositePass> composite_;
    GLuint output_framebuffer_{};
    bool scene_is_hdr_{};
    bool hdr_disabled_{};
};

} // namespace

std::unique_ptr<RenderBackend> make_opengl_backend(GlProcLoader load) {
    if (!load) {
        throw std::invalid_argument{"An OpenGL procedure loader is required"};
    }
    const auto load_with_context = [](void* context, const char* name) -> GLADapiproc {
        return (*static_cast<GlProcLoader*>(context))(name);
    };
    const auto version = gladLoadGLUserPtr(load_with_context, &load);
    if (version == 0) {
        throw std::runtime_error{"Unable to load OpenGL functions"};
    }
    return std::make_unique<OpenGlBackend>();
}

} // namespace mgv
