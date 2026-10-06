#include "qt_viewer_window.hpp"

#include "qt_viewer_widget.hpp"

#include <QAction>
#include <QFileDialog>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>

#include <filesystem>
#include <utility>

namespace {

[[nodiscard]] std::filesystem::path path_from(const QString& value) {
    return std::filesystem::path{value.toStdString()};
}

} // namespace

QtViewerWindow::QtViewerWindow(mgv::AssetPaths assets, QWidget* parent)
    : QMainWindow{parent}, assets_{std::move(assets)} {
    setWindowTitle("Modern GL Viewer — Engine");
    viewer_ = new QtViewerWidget{assets_, this};
    setCentralWidget(viewer_);
    create_actions();
    statusBar()->showMessage("Arrow keys: orbit  |  +/−: zoom  |  Home/R: reset");
    resize(viewer_->sizeHint());
}

void QtViewerWindow::load_assets(mgv::AssetPaths assets) {
    assets_ = std::move(assets);
    viewer_->load_assets(assets_);
}

void QtViewerWindow::create_actions() {
    auto* file_menu = menuBar()->addMenu("&File");
    auto* open_model = file_menu->addAction("Open &OBJ…");
    connect(open_model, &QAction::triggered, this, [this] {
        const auto selected = QFileDialog::getOpenFileName(
            this, "Open OBJ", {}, "Wavefront OBJ (*.obj)");
        if (!selected.isEmpty()) {
            assets_.model = path_from(selected);
            viewer_->load_assets(assets_);
        }
    });

    auto* open_vertex = file_menu->addAction("Open &vertex shader…");
    connect(open_vertex, &QAction::triggered, this, [this] {
        const auto selected = QFileDialog::getOpenFileName(
            this, "Open vertex shader", {}, "GLSL (*.vert *.glsl);;All files (*)");
        if (!selected.isEmpty()) {
            assets_.vertex_shader = path_from(selected);
            viewer_->load_assets(assets_);
        }
    });

    auto* open_fragment = file_menu->addAction("Open &fragment shader…");
    connect(open_fragment, &QAction::triggered, this, [this] {
        const auto selected = QFileDialog::getOpenFileName(
            this, "Open fragment shader", {}, "GLSL (*.frag *.glsl);;All files (*)");
        if (!selected.isEmpty()) {
            assets_.fragment_shader = path_from(selected);
            viewer_->load_assets(assets_);
        }
    });

    file_menu->addSeparator();
    auto* reload = file_menu->addAction("&Reload assets");
    connect(reload, &QAction::triggered, viewer_, [this] {
        viewer_->load_assets(assets_);
    });
    auto* close = file_menu->addAction("&Close engine");
    connect(close, &QAction::triggered, this, &QWidget::close);

    auto* controls_menu = menuBar()->addMenu("&Controls");
    const auto add_control = [this, controls_menu](
                                 const QString& label,
                                 mgv::InputAction input) {
        auto* action = controls_menu->addAction(label);
        connect(action, &QAction::triggered, viewer_, [this, input] {
            viewer_->handle_input(input);
        });
    };
    add_control("Orbit &left", mgv::InputAction::orbit_left);
    add_control("Orbit &right", mgv::InputAction::orbit_right);
    add_control("Orbit &up", mgv::InputAction::orbit_up);
    add_control("Orbit &down", mgv::InputAction::orbit_down);
    controls_menu->addSeparator();
    add_control("Zoom &in", mgv::InputAction::zoom_in);
    add_control("Zoom &out", mgv::InputAction::zoom_out);
    add_control("&Reset view", mgv::InputAction::reset_view);
    add_control("Fire &Test Shot", mgv::InputAction::fire_test_shot);
}
