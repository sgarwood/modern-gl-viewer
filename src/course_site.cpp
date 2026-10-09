#include "mgv/course_site.hpp"

namespace mgv {

SunPosition sun_at(const CourseSite& site) noexcept {
    return sun_position(
        site.latitude_degrees, site.longitude_degrees, site.day_of_year, site.utc_hours);
}

Daylight daylight_at(const CourseSite& site) noexcept {
    return daylight(site.latitude_degrees, site.longitude_degrees, site.day_of_year);
}

void apply_site(Environment& environment, const CourseSite& site) {
    const auto sun = sun_at(site);
    environment.sun.direction = sun_direction_from_angles(
        sun.azimuth_degrees, sun.altitude_degrees);
}

} // namespace mgv
