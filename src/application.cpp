#include "mgv/application.hpp"

#include "mgv/camera_controller.hpp"
#include "mgv/opengl_backend.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace mgv {

struct Application::Config final {
    std::string title{"Modern GL Viewer"};
    int width{1280};
    int height{720};
    AssetPaths assets;
};

namespace {

class GlfwRuntime final {
public:
    GlfwRuntime() {
        glfwSetErrorCallback([](int, const char* description) {
            last_error_ = description != nullptr ? description : "unknown GLFW error";
        });
        if (glfwInit() != GLFW_TRUE) {
            throw std::runtime_error{"Unable to initialize GLFW: " + last_error_};
        }
    }
    ~GlfwRuntime() { glfwTerminate(); }

    GlfwRuntime(const GlfwRuntime&) = delete;
    GlfwRuntime& operator=(const GlfwRuntime&) = delete;

private:
    static inline std::string last_error_;
};

struct WindowDeleter final {
    void operator()(GLFWwindow* window) const noexcept { glfwDestroyWindow(window); }
};

using Window = std::unique_ptr<GLFWwindow, WindowDeleter>;

[[nodiscard]] Window create_window(int width, int height, const std::string& title) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    Window window{glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr)};
    if (!window) {
        throw std::runtime_error{"Unable to create an OpenGL 4.1 window"};
    }
    return window;
}

} // namespace

struct Application::Impl final {
    explicit Impl(Config config)
        : window{create_window(config.width, config.height, config.title)} {
        glfwMakeContextCurrent(window.get());
        glfwSwapInterval(1);
        renderer = std::make_unique<Renderer>(make_opengl_backend([](const char* name) {
            return glfwGetProcAddress(name);
        }));
        renderer->load(config.assets);
        input = make_orbit_camera_controller(*renderer);
        glfwSetWindowUserPointer(window.get(), this);
        glfwSetKeyCallback(window.get(), [](GLFWwindow* native_window, int key, int, int action, int) {
            if (action != GLFW_PRESS && action != GLFW_REPEAT) {
                return;
            }
            auto& application = *static_cast<Impl*>(glfwGetWindowUserPointer(native_window));
            switch (key) {
            case GLFW_KEY_LEFT:
                application.input->handle(InputAction::orbit_left);
                break;
            case GLFW_KEY_RIGHT:
                application.input->handle(InputAction::orbit_right);
                break;
            case GLFW_KEY_UP:
                application.input->handle(InputAction::orbit_up);
                break;
            case GLFW_KEY_DOWN:
                application.input->handle(InputAction::orbit_down);
                break;
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:
                application.input->handle(InputAction::zoom_in);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:
                application.input->handle(InputAction::zoom_out);
                break;
            case GLFW_KEY_HOME:
            case GLFW_KEY_R:
                application.input->handle(InputAction::reset_view);
                break;
            default:
                break;
            }
        });
    }

    // Declaration order is intentional: input and GL resources die before the context and GLFW runtime.
    GlfwRuntime runtime;
    Window window;
    std::unique_ptr<Renderer> renderer;
    std::unique_ptr<InputSink> input;
};

Application::Application(Config config) : impl_{std::make_unique<Impl>(std::move(config))} {}
Application::~Application() = default;
Application::Application(Application&&) noexcept = default;
Application& Application::operator=(Application&&) noexcept = default;

int Application::run() {
    while (glfwWindowShouldClose(impl_->window.get()) == GLFW_FALSE) {
        glfwPollEvents();
        if (glfwGetKey(impl_->window.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(impl_->window.get(), GLFW_TRUE);
        }

        int width{};
        int height{};
        glfwGetFramebufferSize(impl_->window.get(), &width, &height);
        impl_->renderer->render({
            .framebuffer_width = std::max(width, 1),
            .framebuffer_height = std::max(height, 1),
            .elapsed_seconds = static_cast<float>(glfwGetTime()),
        });
        glfwSwapBuffers(impl_->window.get());
    }
    return 0;
}

Application::Builder::Builder() : config_{std::make_unique<Config>()} {}
Application::Builder::~Builder() = default;
Application::Builder::Builder(Builder&&) noexcept = default;
Application::Builder& Application::Builder::operator=(Builder&&) noexcept = default;

Application::Builder& Application::Builder::title(std::string value) {
    if (value.empty()) {
        throw std::invalid_argument{"Window title must not be empty"};
    }
    config_->title = std::move(value);
    return *this;
}

Application::Builder& Application::Builder::size(int width, int height) {
    if (width <= 0 || height <= 0) {
        throw std::invalid_argument{"Window dimensions must be positive"};
    }
    config_->width = width;
    config_->height = height;
    return *this;
}

Application::Builder& Application::Builder::model(std::filesystem::path path) {
    config_->assets.model = std::move(path);
    return *this;
}

Application::Builder& Application::Builder::shaders(
    std::filesystem::path vertex,
    std::filesystem::path fragment) {
    config_->assets.vertex_shader = std::move(vertex);
    config_->assets.fragment_shader = std::move(fragment);
    return *this;
}

Application Application::Builder::build() {
    if (config_->assets.model.empty()) {
        throw std::invalid_argument{"An OBJ model path is required"};
    }
    if (config_->assets.vertex_shader.empty() || config_->assets.fragment_shader.empty()) {
        throw std::invalid_argument{"Vertex and fragment shader paths are required"};
    }
    return Application{std::move(*config_)};
}

} // namespace mgv
