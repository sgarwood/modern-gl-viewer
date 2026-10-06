#include "mgv/application.hpp"

#include "mgv/engine.hpp"
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
        engine = std::make_unique<Engine>(make_opengl_backend([](const char* name) {
            return glfwGetProcAddress(name);
        }));
        engine->load(config.assets);
        glfwSetWindowUserPointer(window.get(), this);
        glfwSetKeyCallback(window.get(), [](GLFWwindow* native_window, int key, int, int action, int) {
            if (action != GLFW_PRESS && action != GLFW_REPEAT) {
                return;
            }
            auto& application = *static_cast<Impl*>(glfwGetWindowUserPointer(native_window));
            switch (key) {
            case GLFW_KEY_LEFT:
                application.engine->enqueue(InputAction::orbit_left);
                break;
            case GLFW_KEY_RIGHT:
                application.engine->enqueue(InputAction::orbit_right);
                break;
            case GLFW_KEY_UP:
                application.engine->enqueue(InputAction::orbit_up);
                break;
            case GLFW_KEY_DOWN:
                application.engine->enqueue(InputAction::orbit_down);
                break;
            case GLFW_KEY_SPACE:
                application.engine->enqueue(InputAction::fire_test_shot);
                break;
            case GLFW_KEY_EQUAL:
            case GLFW_KEY_KP_ADD:
                application.engine->enqueue(InputAction::zoom_in);
                break;
            case GLFW_KEY_MINUS:
            case GLFW_KEY_KP_SUBTRACT:
                application.engine->enqueue(InputAction::zoom_out);
                break;
            case GLFW_KEY_HOME:
            case GLFW_KEY_R:
                application.engine->enqueue(InputAction::reset_view);
                break;
            default:
                break;
            }
        });
    }

    // Declaration order is intentional: engine GL resources die before the context and GLFW runtime.
    GlfwRuntime runtime;
    Window window;
    std::unique_ptr<Engine> engine;
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
        impl_->engine->tick({
            .framebuffer_width = std::max(width, 1),
            .framebuffer_height = std::max(height, 1),
        });
        
        static int frame_count = 0;
        if (frame_count == 1) {
            impl_->engine->enqueue(InputAction::fire_test_shot);
        }
        
        glfwSwapBuffers(impl_->window.get());
        if (++frame_count == 60) {
            extern void save_framebuffer_to_png(int, int, const std::string&);
            save_framebuffer_to_png(width, height, "/home/sgarwood/.gemini/antigravity-cli/brain/66790bf0-eae1-40b0-8b7f-3241d4cd3b8a/aerodynamics_render.png");
            break;
        }

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
