#include "qt_viewer_window.hpp"

#include "mgv/renderer.hpp"
#include "mgv/runtime_paths.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QString>
#include <QSurfaceFormat>

#include <filesystem>

namespace {

[[nodiscard]] std::filesystem::path path_from(const QString& value) {
    return std::filesystem::path{value.toStdString()};
}

[[nodiscard]] QString qstring_from(const std::filesystem::path& value) {
    return QString::fromStdString(value.string());
}

void configure_surface_format() {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    format.setSwapInterval(1);
    QSurfaceFormat::setDefaultFormat(format);
}

} // namespace

int main(int argc, char** argv) {
    configure_surface_format();

    QApplication application{argc, argv};
    QApplication::setApplicationName("Modern GL Viewer");
    QApplication::setApplicationVersion("0.2.0");

    const auto asset_directory = mgv::locate_asset_directory(
        MGV_DEFAULT_ASSET_DIR,
        std::filesystem::path{QApplication::applicationDirPath().toStdString()});
    QCommandLineParser parser;
    parser.setApplicationDescription("A backend-neutral C++ OBJ viewer with a Qt 6 shell.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("model", "OBJ model to load.", "[model.obj]");
    const QCommandLineOption vertex_option{
        "vertex",
        "Runtime vertex shader.",
        "shader.vert",
        qstring_from(asset_directory / "shaders/default.vert")};
    const QCommandLineOption fragment_option{
        "fragment",
        "Runtime fragment shader.",
        "shader.frag",
        qstring_from(asset_directory / "shaders/default.frag")};
    parser.addOption(vertex_option);
    parser.addOption(fragment_option);
    parser.process(application);

    const auto positional = parser.positionalArguments();
    QtViewerWindow window{{
        .model = positional.empty()
            ? asset_directory / "cube.obj"
            : path_from(positional.front()),
        .vertex_shader = path_from(parser.value(vertex_option)),
        .fragment_shader = path_from(parser.value(fragment_option)),
    }};
    window.show();
    return application.exec();
}
