#include "main_menu_controller.hpp"

#include <catch2/catch_test_macros.hpp>

#include <QUrl>

#include <filesystem>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <utility>

namespace {

class FakeEngineLauncher final : public EngineLauncher {
public:
    void launch(mgv::AssetPaths assets, mgv::CourseConditions conditions) override {
        if (failure) {
            throw std::runtime_error{"engine launch failed"};
        }
        launched = std::move(assets);
        played_at = conditions;
    }

    bool failure{};
    std::optional<mgv::AssetPaths> launched;
    std::optional<mgv::CourseConditions> played_at;
};

/// A directory with one Welsh club in it, answering with the conditions
/// Open-Meteo really reported over it on 9 October 2026.
class FakeClubDirectory final : public ClubDirectory {
public:
    [[nodiscard]] std::vector<mgv::network::Club> search(const std::string& query) override {
        searched = query;
        if (query != "Surrey" && query != "St Andrews") {
            return {};
        }
        return {mgv::network::Club{
            .id = 1,
            .name = "St Andrews Major Golf Club",
            .address = "St Andrews Major Golf Club, Dinas Powys, Vale of Glamorgan",
            .latitude = 51.4258297,
            .longitude = -3.2395940,
        }};
    }

    [[nodiscard]] std::optional<mgv::network::SiteConditions> conditions(
        const mgv::network::Club& club) override {
        if (silent) {
            return std::nullopt;
        }
        return mgv::network::SiteConditions{
            .name = club.name,
            .latitude = club.latitude,
            .longitude = club.longitude,
            .elevation_metres = 10.0F,
            .day_of_year = 282,
            .utc_hours = 6.75F,
            .sunrise_utc_hours = 6.4333F,
            .sunset_utc_hours = 17.5333F,
            .weather = {
                .temperature_c = 16.0F,
                .wind_speed_mps = 8.5F,
                .wind_direction_deg = 79.0F,
                .is_raining = false,
                .turf_wetness = 0.3875F,
                .pressure_pascals = 101'220.0F,
                .relative_humidity = 0.91F,
            },
        };
    }

    bool silent{};
    std::string searched;
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

TEST_CASE("a launch with no club chosen is played at Greenwich at midday") {
    FakeEngineLauncher launcher;
    MainMenuController controller{launcher, default_assets()};

    CHECK_FALSE(controller.hasClubDirectory());
    controller.launchEngine();

    REQUIRE(launcher.played_at.has_value());
    CHECK(launcher.played_at->site == mgv::CourseSite{});
    CHECK(launcher.played_at->site.name == "Greenwich");
    CHECK(launcher.played_at->site.elevation_metres == 0.0F);
    CHECK(launcher.played_at->site.utc_hours == 12.0F);
    // Said in words, because a player reading "0.0 m/s" cannot tell still air
    // from no answer.
    CHECK(controller.weatherSummary().contains("No live weather"));
}

TEST_CASE("the menu searches for a club and plays the conditions over it") {
    FakeEngineLauncher launcher;
    FakeClubDirectory clubs;
    MainMenuController controller{launcher, default_assets(), &clubs};

    controller.setClubQuery("Surrey");
    controller.searchClubs();

    REQUIRE(controller.clubResults().size() == 1);
    CHECK(controller.clubResults().front() == "St Andrews Major Golf Club");
    CHECK(controller.clubAddresses().front().contains("Dinas Powys"));
    CHECK(controller.status() == "1 club found");

    controller.selectClub(0);

    CHECK(controller.siteName() == "St Andrews Major Golf Club");
    CHECK(controller.siteSummary().contains("51.426"));
    // West of the meridian, which a signed number makes a player work out.
    CHECK(controller.siteSummary().contains("W"));
    CHECK(controller.siteSummary().contains("10 m"));
    CHECK(controller.weatherSummary().contains("16.0"));
    CHECK(controller.weatherSummary().contains("8.5"));
    // A quarter past six on an October morning in Wales: just risen.
    CHECK(controller.sunSummary().contains("06:2"));
    CHECK(controller.sunSummary().contains("17:3"));

    controller.launchEngine();

    REQUIRE(launcher.played_at.has_value());
    const auto& played = *launcher.played_at;
    CHECK(played.site.name == "St Andrews Major Golf Club");
    CHECK(played.site.elevation_metres == 10.0F);
    CHECK(played.site.day_of_year == 282);
    CHECK(played.site.utc_hours == 6.75F);
    CHECK(played.weather.pressure_pascals == 101'220.0F);
    CHECK(played.weather.wind_speed_mps == 8.5F);
}

TEST_CASE("a search that matches nothing says so and changes nothing") {
    FakeEngineLauncher launcher;
    FakeClubDirectory clubs;
    MainMenuController controller{launcher, default_assets(), &clubs};

    controller.setClubQuery("Narnia");
    controller.searchClubs();

    CHECK(controller.clubResults().isEmpty());
    CHECK(controller.hasError());
    CHECK(controller.siteName() == "Greenwich");
}

TEST_CASE("a club with no live conditions keeps its place and says the weather is standard") {
    FakeEngineLauncher launcher;
    FakeClubDirectory clubs;
    clubs.silent = true;
    MainMenuController controller{launcher, default_assets(), &clubs};

    controller.setClubQuery("Surrey");
    controller.searchClubs();
    controller.selectClub(0);

    // The club is where it is whatever the weather service is doing, so the
    // round is played there -- in standard air, and the player is told.
    CHECK(controller.siteName() == "St Andrews Major Golf Club");
    CHECK(controller.siteSummary().contains("51.426"));
    CHECK(controller.hasError());
    CHECK(controller.weatherSummary().contains("No live weather"));
    // And the sun is still the real sun over the real place, because that
    // needs no service at all.
    CHECK(controller.sunSummary().contains("Sunrise"));
}

TEST_CASE("a short query is refused before it reaches the directory") {
    FakeEngineLauncher launcher;
    FakeClubDirectory clubs;
    MainMenuController controller{launcher, default_assets(), &clubs};

    controller.setClubQuery("St");
    controller.searchClubs();

    CHECK(clubs.searched.empty());
    CHECK(controller.hasError());
}

TEST_CASE("the menu can be sent back to Greenwich") {
    FakeEngineLauncher launcher;
    FakeClubDirectory clubs;
    MainMenuController controller{launcher, default_assets(), &clubs};
    controller.setClubQuery("Surrey");
    controller.searchClubs();
    controller.selectClub(0);
    REQUIRE(controller.siteName() == "St Andrews Major Golf Club");

    controller.useGreenwich();

    CHECK(controller.conditions().site == mgv::CourseSite{});
    CHECK(controller.siteName() == "Greenwich");
}
