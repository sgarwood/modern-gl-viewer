#include "qt_viewer_widget.hpp"

#include "mgv/renderer.hpp"

#include <QAction>
#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFileDialog>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QSurfaceFormat>

#include <filesystem>

namespace {

[[nodiscard]] std::filesystem::path path_from(const QString& value) {
    return std::filesystem::path{value.toStdString()};
}

[[nodiscard]] QString qstring_from(const std::filesystem::path& value) {
    return QString::fromStdString(value.string());
}

} // namespace

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication application{argc, argv};
    QApplication::setApplicationName("Modern GL Viewer");
    QApplication::setApplicationVersion("0.2.0");

    const auto asset_directory = std::filesystem::path{MGV_DEFAULT_ASSET_DIR};
    QCommandLineParser parser;
    parser.setApplicationDescription("A backend-neutral C++ OBJ viewer with a Qt 6 shell.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("model", "OBJ model to load.", "[model.obj]");
    const QCommandLineOption vertex_option{
        "vertex", "Runtime vertex shader.", "shader.vert", qstring_from(asset_directory / "shaders/default.vert")};
    const QCommandLineOption fragment_option{
        "fragment", "Runtime fragment shader.", "shader.frag", qstring_from(asset_directory / "shaders/default.frag")};
    parser.addOption(vertex_option);
    parser.addOption(fragment_option);
    parser.process(application);

    const auto positional = parser.positionalArguments();
    mgv::AssetPaths assets{
        .model = positional.empty() ? asset_directory / "cube.obj" : path_from(positional.front()),
        .vertex_shader = path_from(parser.value(vertex_option)),
        .fragment_shader = path_from(parser.value(fragment_option)),
    };

    QMainWindow window;
    window.setWindowTitle("Modern GL Viewer — Qt");
    auto* viewer = new QtViewerWidget{assets, &window};
    window.setCentralWidget(viewer);

    auto* file_menu = window.menuBar()->addMenu("&File");
    auto* open_model = file_menu->addAction("Open &OBJ…");
    QObject::connect(open_model, &QAction::triggered, &window, [&] {
        const auto selected = QFileDialog::getOpenFileName(&window, "Open OBJ", {}, "Wavefront OBJ (*.obj)");
        if (!selected.isEmpty()) {
            assets.model = path_from(selected);
            viewer->load_assets(assets);
        }
    });

    auto* open_vertex = file_menu->addAction("Open &vertex shader…");
    QObject::connect(open_vertex, &QAction::triggered, &window, [&] {
        const auto selected = QFileDialog::getOpenFileName(&window, "Open vertex shader", {}, "GLSL (*.vert *.glsl);;All files (*)");
        if (!selected.isEmpty()) {
            assets.vertex_shader = path_from(selected);
            viewer->load_assets(assets);
        }
    });

    auto* open_fragment = file_menu->addAction("Open &fragment shader…");
    QObject::connect(open_fragment, &QAction::triggered, &window, [&] {
        const auto selected = QFileDialog::getOpenFileName(&window, "Open fragment shader", {}, "GLSL (*.frag *.glsl);;All files (*)");
        if (!selected.isEmpty()) {
            assets.fragment_shader = path_from(selected);
            viewer->load_assets(assets);
        }
    });

    file_menu->addSeparator();
    auto* reload = file_menu->addAction("&Reload assets");
    QObject::connect(reload, &QAction::triggered, viewer, [&, viewer] { viewer->load_assets(assets); });
    auto* quit = file_menu->addAction("E&xit");
    QObject::connect(quit, &QAction::triggered, &application, &QApplication::quit);

    auto* controls_menu = window.menuBar()->addMenu("&Controls");
    const auto add_control = [controls_menu, viewer](const QString& label, mgv::InputAction input) {
        auto* action = controls_menu->addAction(label);
        QObject::connect(action, &QAction::triggered, viewer, [viewer, input] { viewer->handle_input(input); });
    };
    add_control("Orbit &left", mgv::InputAction::orbit_left);
    add_control("Orbit &right", mgv::InputAction::orbit_right);
    add_control("Orbit &up", mgv::InputAction::orbit_up);
    add_control("Orbit &down", mgv::InputAction::orbit_down);
    controls_menu->addSeparator();
    add_control("Zoom &in", mgv::InputAction::zoom_in);
    add_control("Zoom &out", mgv::InputAction::zoom_out);
    add_control("&Reset view", mgv::InputAction::reset_view);

    window.statusBar()->showMessage("Arrow keys: orbit  |  +/−: zoom  |  Home/R: reset");

    window.resize(viewer->sizeHint());
    window.show();
    return application.exec();
}
