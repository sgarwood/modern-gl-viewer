#pragma once

#include "mgv/course_site.hpp"
#include "mgv/network/weather_condition.hpp"

namespace mgv {

/// Where a round will be played and what the weather is doing there,
/// resolved before the engine opens.
///
/// Defaulted, this is the fallback: Greenwich, at sea level, at midday, in
/// still dry air at the standard pressure. Every build that cannot ask a
/// backend gets exactly this, and so does the Qt launcher until a player
/// chooses a club.
struct CourseConditions final {
    CourseSite site{};
    network::WeatherCondition weather{};
};

} // namespace mgv
