#include "mgv/network/http_backend_client.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

using namespace mgv::network;

TEST_CASE("HttpBackendClient can fetch weather from mock backend", "[integration]") {
    // Assuming the mock backend is running on localhost:5000 in CI
    HttpBackendClient client("http://localhost:5000");
    
    // Stoplight Prism mock server will return valid dummy data based on the openapi schema
    auto result = client.fetch_course_weather(1);
    
    REQUIRE(result.has_value());
    // Since it's a mock, it will just return some numbers matching the schema types
    // We just verify it successfully parsed the schema
    CHECK(result->temperature_c >= -1000.0f); 
}
