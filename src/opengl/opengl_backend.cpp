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

class OpenGlShader final : public ShaderResource {
public:
    explicit OpenGlShader(const ShaderSources& sources) {
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

private:
    Program program_;
    GLint mvp_location_{-1};
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
    std::unique_ptr<MeshResource> create_mesh(const MeshData& mesh) override {
        return std::make_unique<OpenGlMesh>(mesh);
    }

    std::unique_ptr<ShaderResource> create_shader(const ShaderSources& sources) override {
        return std::make_unique<OpenGlShader>(sources);
    }

    void begin_frame(const Frame& frame) override {
        glViewport(0, 0, frame.framebuffer_width, frame.framebuffer_height);
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.025F, 0.035F, 0.055F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void draw(const MeshResource& mesh, const ShaderResource& shader, const Mat4& transform) override {
        const auto& gl_mesh = backend_resource<OpenGlMesh>(mesh, "mesh");
        const auto& gl_shader = backend_resource<OpenGlShader>(shader, "shader");
        glUseProgram(gl_shader.program());
        if (gl_shader.mvp_location() >= 0) {
            glUniformMatrix4fv(gl_shader.mvp_location(), 1, GL_FALSE, transform.data());
        }
        glBindVertexArray(gl_mesh.vertex_array());
        glDrawElements(GL_TRIANGLES, gl_mesh.index_count(), GL_UNSIGNED_INT, nullptr);
        glBindVertexArray(0);
    }

    void end_frame() override {}
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
