#include "main_menu_controller.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QUrl>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

class FakeEngineLauncher final : public EngineLauncher {
public:
    void launch(mgv::AssetPaths assets) override {
        if (failure) {
            throw std::runtime_error{"engine launch failed"};
        }
        launched = std::move(assets);
    }

    bool failure{};
    std::optional<mgv::AssetPaths> launched;
};

[[nodiscard]] mgv::AssetPaths default_assets() {
    return {
        .model = "/assets/cube.obj",
        .vertex_shader = "/assets/default.vert",
        .fragment_shader = "/assets/default.frag",
    };
}

} // namespace

TEST_CASE("QML menu controller launches the engine through its port") {
    FakeEngineLauncher launcher;
    MainMenuController controller{launcher, default_assets()};
    controller.setModelSource(QUrl::fromLocalFile("/models/ship.obj"));
    controller.setVertexShaderSource(QUrl::fromLocalFile("/shaders/ship.vert"));
    controller.setFragmentShaderSource(QUrl::fromLocalFile("/shaders/ship.frag"));

    controller.launchEngine();

    REQUIRE(launcher.launched.has_value());
    CHECK(launcher.launched->model == std::filesystem::path{"/models/ship.obj"});
    CHECK(launcher.launched->vertex_shader == std::filesystem::path{"/shaders/ship.vert"});
    CHECK(launcher.launched->fragment_shader == std::filesystem::path{"/shaders/ship.frag"});
    CHECK(controller.status() == "Engine loaded");
    CHECK_FALSE(controller.hasError());
}

TEST_CASE("QML menu controller restores bundled assets") {
    FakeEngineLauncher launcher;
    const auto defaults = default_assets();
    MainMenuController controller{launcher, defaults};
    controller.setModelSource(QUrl::fromLocalFile("/models/other.obj"));

    controller.restoreDefaults();

    CHECK(controller.modelSource() == QUrl::fromLocalFile(
          QString::fromStdString(defaults.model.string())));
}

TEST_CASE("QML menu controller contains engine launch failures") {
    FakeEngineLauncher launcher;
    launcher.failure = true;
    MainMenuController controller{launcher, default_assets()};

    controller.launchEngine();

    CHECK_FALSE(launcher.launched.has_value());
    CHECK(controller.hasError());
    CHECK(controller.status() == "engine launch failed");
}

TEST_CASE("QML menu controller rejects non-local asset URLs") {
    FakeEngineLauncher launcher;
    MainMenuController controller{launcher, default_assets()};
    controller.setModelSource(QUrl{"https://example.com/model.obj"});

    controller.launchEngine();

    CHECK_FALSE(launcher.launched.has_value());
    CHECK(controller.hasError());
    CHECK(controller.status() == "Model must be a local file");
}
