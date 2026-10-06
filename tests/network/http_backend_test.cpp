#include "mgv/network/http_backend_client.hpp"
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

private:
    httplib::Server server_;
    int port_{};
    std::jthread thread_;
};

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
