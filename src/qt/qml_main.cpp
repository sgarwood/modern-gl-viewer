#include "main_menu_controller.hpp"
#include "qt_engine_launcher.hpp"

#include "mgv/runtime_paths.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QSurfaceFormat>
#include <QTimer>
#include <QUrl>

#include <filesystem>

namespace {

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

    QCommandLineParser parser;
    parser.setApplicationDescription("Modern GL Viewer QML launcher.");
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption launch_option{
        "launch", "Launch the bundled engine immediately."};
    parser.addOption(launch_option);
    parser.process(application);

    const auto asset_directory = mgv::locate_asset_directory(
        MGV_DEFAULT_ASSET_DIR,
        std::filesystem::path{QApplication::applicationDirPath().toStdString()});
    QtEngineLauncher launcher;
    MainMenuController controller{
        launcher,
        {
            .model = asset_directory / "cube.obj",
            .vertex_shader = asset_directory / "shaders/default.vert",
            .fragment_shader = asset_directory / "shaders/default.frag",
        },
    };

    QQmlApplicationEngine qml_engine;
    qml_engine.rootContext()->setContextProperty("menuController", &controller);
    qml_engine.load(QUrl{QStringLiteral("qrc:/MgvMenu/Main.qml")});
    if (qml_engine.rootObjects().isEmpty()) {
        return 1;
    }
    if (parser.isSet(launch_option)) {
        QTimer::singleShot(0, &controller, &MainMenuController::launchEngine);
    }
    return application.exec();
}
