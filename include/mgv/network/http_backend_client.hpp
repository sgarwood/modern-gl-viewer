#pragma once

#include "mgv/network/backend_client.hpp"
#include <string>

namespace mgv::network {

class HttpBackendClient : public IBackendClient {
public:
    explicit HttpBackendClient(std::string base_url);

    std::optional<WeatherCondition> fetch_course_weather(int course_id) override;
    std::vector<Club> search_clubs(const std::string& query) override;
    std::optional<SiteConditions> fetch_conditions(
        double latitude,
        double longitude,
        const std::string& name) override;

private:
    std::string base_url_;
};

} // namespace mgv::network
