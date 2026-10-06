#include "mgv/opengl_backend.hpp"

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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
        glBindVertexArray(0);
    }

    [[nodiscard]] GLuint vertex_array() const noexcept { return vertex_array_.get(); }
    [[nodiscard]] GLsizei index_count() const noexcept { return index_count_; }

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
    GLsizei index_count_{};
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
    }

    [[nodiscard]] GLuint program() const noexcept { return program_.get(); }
    [[nodiscard]] GLint mvp_location() const noexcept { return mvp_location_; }
    [[nodiscard]] PrimitiveTopology topology() const noexcept { return topology_; }
    [[nodiscard]] const RasterizationState& rasterization() const noexcept { return rasterization_; }
    [[nodiscard]] const DepthState& depth() const noexcept { return depth_; }
    [[nodiscard]] const BlendState& blending() const noexcept { return blending_; }

private:
    Program program_;
    GLint mvp_location_{-1};
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
    }

    [[nodiscard]] RenderBackendCapabilities capabilities() const noexcept override {
        return {
            .clip_space = {},
            .wireframe = true,
            .max_sampled_textures = max_sampled_textures_,
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
        glViewport(0, 0, frame.framebuffer_width, frame.framebuffer_height);
        glDepthMask(GL_TRUE);
        glClearColor(0.025F, 0.035F, 0.055F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void draw(const DrawPacket& packet) override {
        const auto& gl_mesh = backend_resource<OpenGlMesh>(packet.mesh, "mesh");
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
        for (const auto& binding : packet.colors) {
            const auto location = glGetUniformLocation(pipeline.program(), binding.name.c_str());
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
            const auto location = glGetUniformLocation(pipeline.program(), binding.name.c_str());
            if (location >= 0) {
                glUniform1i(location, static_cast<GLint>(index));
            }
        }
        glBindVertexArray(gl_mesh.vertex_array());
        glDrawElements(
            primitive_topology(pipeline.topology()),
            gl_mesh.index_count(),
            GL_UNSIGNED_INT,
            nullptr);
        glBindVertexArray(0);
        for (std::size_t index = 0; index < packet.textures.size(); ++index) {
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(index));
            glBindSampler(static_cast<GLuint>(index), 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        glActiveTexture(GL_TEXTURE0);
    }

    void end_frame() noexcept override {}

private:
    std::uint32_t max_sampled_textures_{};
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
