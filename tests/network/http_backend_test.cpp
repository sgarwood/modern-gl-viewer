#include "mgv/network/http_backend_client.hpp"
#include "mgv/course_site.hpp"
#include "mgv/network/site_conditions.hpp"
#include "mgv/network/httplib.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

class LocalWeatherServer final {
public:
    LocalWeatherServer() {
        server_.Get("/courses/1/weather", [](const auto&, auto& response) {
            response.set_content(
                R"({"temperatureC":12.5,"windSpeedMps":4.0,"windDirectionDeg":225.0,"isRaining":true,"turfWetness":0.75})",
                "application/json");
        });
        // The payloads below are what the C# backend emits for real upstream
        // data: the club list is the real Nominatim answer for
        // "[golf_course] Surrey", and the conditions are the real Open-Meteo
        // answer for St Andrews Major Golf Club on 9 October 2026, both put
        // through the same conversions the providers apply -- hectopascals to
        // pascals, humidity to a fraction, and the wind turned through 180
        // from the direction it blows from to the direction it blows towards.
        server_.Get("/clubs", [](const auto& request, auto& response) {
            if (request.get_param_value("query") != "Surrey") {
                response.set_content("[]", "application/json");
                return;
            }
            response.set_content(
                R"([{"id":280455297,"name":"Royal Mid-Surrey Golf Club",)"
                R"("address":"Royal Mid-Surrey Golf Club, Thames Towpath, St Margarets, Greater London, England, TW7 6XE, United Kingdom",)"
                R"("latitude":51.4690089,"longitude":-0.307877},)"
                R"({"id":279667862,"name":"Surrey Downs Golf Club",)"
                R"("address":"Surrey Downs Golf Club, Larch Close, Chipstead, Surrey, England, KT20 6JF, United Kingdom",)"
                R"("latitude":51.2918928,"longitude":-0.1902278}])",
                "application/json");
        });

        server_.Get("/conditions", [](const auto& request, auto& response) {
            latitude_seen = request.get_param_value("latitude");
            response.set_content(
                R"({"name":"St Andrews Major Golf Club","latitude":51.42574,"longitude":-3.249649,)"
                R"("elevationMetres":10.0,"dayOfYear":282,"utcHours":6.75,)"
                R"("sunriseUtcHours":6.4333333,"sunsetUtcHours":17.5333333,)"
                R"("weather":{"temperatureC":16.0,"windSpeedMps":8.5,"windDirectionDeg":79.0,)"
                R"("isRaining":false,"turfWetness":0.3875,"pressurePascals":101220.0,)"
                R"("relativeHumidity":0.91}})",
                "application/json");
        });

        server_.Get("/conditions-unavailable", [](const auto&, auto& response) {
            response.status = 503;
            response.set_content(R"({"detail":"The weather service did not answer."})", "application/json");
        });

        port_ = server_.bind_to_any_port("127.0.0.1");
        if (port_ <= 0) {
            throw std::runtime_error{"Unable to bind local weather test server"};
        }
        thread_ = std::jthread{[this] { static_cast<void>(server_.listen_after_bind()); }};
        for (int attempt = 0; attempt < 100 && !server_.is_running(); ++attempt) {
            std::this_thread::sleep_for(std::chrono::milliseconds{1});
        }
        if (!server_.is_running()) {
            server_.stop();
            throw std::runtime_error{"Local weather test server did not start"};
        }
    }

    ~LocalWeatherServer() { server_.stop(); }

    LocalWeatherServer(const LocalWeatherServer&) = delete;
    LocalWeatherServer& operator=(const LocalWeatherServer&) = delete;

    [[nodiscard]] std::string base_url() const {
        return "http://127.0.0.1:" + std::to_string(port_);
    }

    /// What the server was asked for, so the query string can be checked.
    static std::string latitude_seen;

private:
    httplib::Server server_;
    int port_{};
    std::jthread thread_;
};

std::string LocalWeatherServer::latitude_seen;

} // namespace

TEST_CASE("HttpBackendClient fetches and parses course weather over HTTP") {
    const LocalWeatherServer server;
    mgv::network::HttpBackendClient client{server.base_url()};

    const auto result = client.fetch_course_weather(1);

    REQUIRE(result.has_value());
    CHECK(result->temperature_c == Catch::Approx(12.5F));
    CHECK(result->wind_speed_mps == Catch::Approx(4.0F));
    CHECK(result->wind_direction_deg == Catch::Approx(225.0F));
    CHECK(result->is_raining);
    CHECK(result->turf_wetness == Catch::Approx(0.75F));
}

TEST_CASE("HttpBackendClient searches for golf clubs over HTTP") {
    const LocalWeatherServer server;
    mgv::network::HttpBackendClient client{server.base_url()};

    const auto clubs = client.search_clubs("Surrey");

    REQUIRE(clubs.size() == 2);
    CHECK(clubs.front().name == "Royal Mid-Surrey Golf Club");
    CHECK(clubs.front().id == 280'455'297);
    CHECK(clubs.front().latitude == Catch::Approx(51.4690089));
    CHECK(clubs.front().longitude == Catch::Approx(-0.307877));
    // The address is what tells two Royal somethings apart.
    CHECK(clubs.front().address.find("TW7 6XE") != std::string::npos);
    CHECK(clubs.back().name == "Surrey Downs Golf Club");
}

TEST_CASE("a club search that matches nothing is an empty list, not a failure") {
    const LocalWeatherServer server;
    mgv::network::HttpBackendClient client{server.base_url()};

    CHECK(client.search_clubs("Narnia").empty());
}

TEST_CASE("HttpBackendClient fetches the conditions over a club") {
    const LocalWeatherServer server;
    mgv::network::HttpBackendClient client{server.base_url()};

    const auto conditions = client.fetch_conditions(51.4257, -3.2496, "St Andrews Major");

    REQUIRE(conditions.has_value());
    CHECK(conditions->name == "St Andrews Major Golf Club");
    CHECK(conditions->elevation_metres == Catch::Approx(10.0F));
    CHECK(conditions->day_of_year == 282);
    CHECK(conditions->utc_hours == Catch::Approx(6.75F));
    CHECK(conditions->sunrise_utc_hours == Catch::Approx(6.4333F).margin(0.001F));
    CHECK(conditions->sunset_utc_hours == Catch::Approx(17.5333F).margin(0.001F));

    // Pascals, not the hectopascals the service reports, and a humidity
    // fraction rather than a percentage. Both are conversions the backend
    // owns, and both are silent failures if they are missed: the pressure
    // would be a hundredth of an atmosphere and the humidity ninety-one
    // times too high.
    CHECK(conditions->weather.pressure_pascals == Catch::Approx(101'220.0F));
    CHECK(conditions->weather.relative_humidity == Catch::Approx(0.91F));
    CHECK(conditions->weather.temperature_c == Catch::Approx(16.0F));
    CHECK(conditions->weather.wind_speed_mps == Catch::Approx(8.5F));
    // Open-Meteo said the wind came from 259 degrees; the engine wants where
    // it is going.
    CHECK(conditions->weather.wind_direction_deg == Catch::Approx(79.0F));
    CHECK_FALSE(conditions->weather.is_raining);

    // The coordinates must reach the query as decimals with a dot, whatever
    // locale the machine is running under.
    CHECK(LocalWeatherServer::latitude_seen.find(',') == std::string::npos);
    CHECK(LocalWeatherServer::latitude_seen.find("51.4") == 0);
}

TEST_CASE("conditions from a club become the site a round is played at") {
    const LocalWeatherServer server;
    mgv::network::HttpBackendClient client{server.base_url()};

    const auto conditions = client.fetch_conditions(51.4257, -3.2496, "St Andrews Major");
    REQUIRE(conditions.has_value());
    const auto site = mgv::network::course_site_of(*conditions);

    CHECK(site.name == "St Andrews Major Golf Club");
    CHECK(site.latitude_degrees == Catch::Approx(51.4257F).margin(0.001F));
    CHECK(site.elevation_metres == Catch::Approx(10.0F));
    CHECK(site.day_of_year == 282);
    CHECK(site.utc_hours == Catch::Approx(6.75F));

    // Quarter past six on an October morning in south Wales: the sun is just
    // up and barely off the horizon, in the east.
    const auto sun = mgv::sun_at(site);
    CHECK(sun.altitude_degrees > 0.0F);
    CHECK(sun.altitude_degrees < 10.0F);
    CHECK(sun.azimuth_degrees > 90.0F);
    CHECK(sun.azimuth_degrees < 125.0F);
}

TEST_CASE("a backend that is not there yields nothing, not a guess") {
    // Nothing listening on this port. The launcher falls back to Greenwich,
    // which it can see it has done.
    mgv::network::HttpBackendClient client{"http://127.0.0.1:1"};

    CHECK_FALSE(client.fetch_conditions(51.4, -3.2, "Anywhere").has_value());
    CHECK(client.search_clubs("Surrey").empty());
    CHECK_FALSE(client.fetch_course_weather(1).has_value());
}
