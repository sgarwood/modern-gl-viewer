#include "main_menu_controller.hpp"

#include <stdexcept>
#include <string>
#include <utility>

MainMenuController::MainMenuController(
    EngineLauncher& launcher,
    mgv::AssetPaths default_assets,
    QObject* parent)
    : QObject{parent},
      launcher_{launcher},
      default_assets_{std::move(default_assets)},
      model_source_{url_from(default_assets_.model)},
      vertex_shader_source_{url_from(default_assets_.vertex_shader)},
      fragment_shader_source_{url_from(default_assets_.fragment_shader)} {}

QUrl MainMenuController::modelSource() const { return model_source_; }
QUrl MainMenuController::vertexShaderSource() const { return vertex_shader_source_; }
QUrl MainMenuController::fragmentShaderSource() const { return fragment_shader_source_; }
const QString& MainMenuController::status() const noexcept { return status_; }
bool MainMenuController::hasError() const noexcept { return has_error_; }

void MainMenuController::setModelSource(const QUrl& value) {
    if (model_source_ == value) {
        return;
    }
    model_source_ = value;
    emit assetSourcesChanged();
}

void MainMenuController::setVertexShaderSource(const QUrl& value) {
    if (vertex_shader_source_ == value) {
        return;
    }
    vertex_shader_source_ = value;
    emit assetSourcesChanged();
}

void MainMenuController::setFragmentShaderSource(const QUrl& value) {
    if (fragment_shader_source_ == value) {
        return;
    }
    fragment_shader_source_ = value;
    emit assetSourcesChanged();
}

void MainMenuController::launchEngine() {
    try {
        launcher_.launch({
            .model = path_from(model_source_, "Model"),
            .vertex_shader = path_from(vertex_shader_source_, "Vertex shader"),
            .fragment_shader = path_from(fragment_shader_source_, "Fragment shader"),
        });
        set_status("Engine loaded", false);
    } catch (const std::exception& error) {
        set_status(QString::fromUtf8(error.what()), true);
    }
}

void MainMenuController::restoreDefaults() {
    model_source_ = url_from(default_assets_.model);
    vertex_shader_source_ = url_from(default_assets_.vertex_shader);
    fragment_shader_source_ = url_from(default_assets_.fragment_shader);
    emit assetSourcesChanged();
    set_status("Bundled assets restored", false);
}

QUrl MainMenuController::url_from(const std::filesystem::path& value) {
    return QUrl::fromLocalFile(QString::fromStdString(value.string()));
}

std::filesystem::path MainMenuController::path_from(
    const QUrl& value,
    const char* label) {
    if (!value.isLocalFile() || value.toLocalFile().isEmpty()) {
        throw std::invalid_argument{std::string{label} + " must be a local file"};
    }
    return std::filesystem::path{value.toLocalFile().toStdString()};
}

void MainMenuController::set_status(QString status, bool error) {
    status_ = std::move(status);
    has_error_ = error;
    emit statusChanged();
}
