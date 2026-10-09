#include "main_menu_controller.hpp"
#include "qt_engine_launcher.hpp"

#include "mgv/network/http_backend_client.hpp"
#include "mgv/network/site_conditions.hpp"
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

/// The club directory, over the C# backend.
///
/// Both calls block the menu for up to the client's five second timeout.
/// That is visible to a player as a stalled window, and the port exists so
/// that it can be moved off this thread without the menu changing.
class BackendClubDirectory final : public ClubDirectory {
public:
    explicit BackendClubDirectory(std::string base_url) : client_{std::move(base_url)} {}

    [[nodiscard]] std::vector<mgv::network::Club> search(const std::string& query) override {
        return client_.search_clubs(query);
    }

    [[nodiscard]] std::optional<mgv::network::SiteConditions> conditions(
        const mgv::network::Club& club) override {
        return client_.fetch_conditions(club.latitude, club.longitude, club.name);
    }

private:
    mgv::network::HttpBackendClient client_;
};

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
    const QCommandLineOption backend_option{
        "backend",
        "Base URL of the course backend. Without one the round is played at "
        "Greenwich, at sea level, at midday.",
        "url",
        "http://127.0.0.1:5000"};
    parser.addOption(backend_option);
    parser.process(application);

    const auto asset_directory = mgv::locate_asset_directory(
        MGV_DEFAULT_ASSET_DIR,
        std::filesystem::path{QApplication::applicationDirPath().toStdString()});
    QtEngineLauncher launcher;
    BackendClubDirectory clubs{parser.value(backend_option).toStdString()};
    MainMenuController controller{
        launcher,
        {
            .model = asset_directory / "ball.obj",
            .vertex_shader = asset_directory / "shaders/default.vert",
            .fragment_shader = asset_directory / "shaders/default.frag",
        },
        &clubs,
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
