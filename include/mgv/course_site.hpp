#pragma once

#include "mgv/lighting.hpp"
#include "mgv/solar.hpp"

#include <string>

namespace mgv {

/// Where a round is played, and when.
///
/// The sun used to be a direction typed into a course description. A site is
/// the fact underneath it: a point on the earth and a moment, from which the
/// sun's position follows, and an elevation, from which the air's density
/// follows. Both are things a player can feel -- long shadows, a ball that
/// carries further up a hill -- and neither can be authored per hole.
///
/// The defaults are the Royal Observatory at Greenwich, at sea level, at
/// midday. Every build that cannot ask a backend where it is plays from
/// there: the GLFW shell, the capture tool, and the Qt launcher before a club
/// has been chosen. It is the one place on earth with no case to answer about
/// its longitude, and a sun due south at noon is the least surprising light
/// to render an unconfigured scene under.
///
/// The date defaults to the March equinox for the same reason: the sun's
/// declination is zero there, so the altitude is simply ninety degrees less
/// the latitude, with no season leaning on it either way.
struct CourseSite final {
    std::string name{"Greenwich"};
    /// Degrees north.
    float latitude_degrees{51.4779F};
    /// Degrees east, so Britain is a little negative.
    float longitude_degrees{-0.0015F};
    float elevation_metres{};
    /// 1 on the first of January; 80 is the March equinox.
    int day_of_year{80};
    float utc_hours{12.0F};

    friend bool operator==(const CourseSite&, const CourseSite&) = default;
};

/// The sun's position as this site sees it at its own moment.
[[nodiscard]] SunPosition sun_at(const CourseSite& site) noexcept;

/// When the sun rises and sets over this site, in hours UTC.
[[nodiscard]] Daylight daylight_at(const CourseSite& site) noexcept;

/// Points an environment's sun where the site's sky puts it.
///
/// Only the direction: how bright the sun is and what colour it is are
/// matters of weather and of the look wanted, and a clear noon in Surrey and
/// an overcast noon in Surrey put the sun in the same place.
void apply_site(Environment& environment, const CourseSite& site);

} // namespace mgv
