#include "mgv/primitives.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

namespace {

[[nodiscard]] float length(const mgv::Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

TEST_CASE("a sky dome sits on a sphere of the requested radius") {
    const auto dome = mgv::make_sky_dome(500.0F, 8, 12);

    REQUIRE_FALSE(dome.empty());
    for (const auto& vertex : dome.vertices) {
        CHECK(length(vertex.position) == Catch::Approx(500.0F).epsilon(1.0e-4F));
    }
    CHECK(dome.indices.size() % 3 == 0);
}

TEST_CASE("sky dome normals face inwards towards the eye at its centre") {
    const auto dome = mgv::make_sky_dome(100.0F, 6, 10);

    for (const auto& vertex : dome.vertices) {
        const auto outward = vertex.position;
        const auto alignment = vertex.normal.x * outward.x + vertex.normal.y * outward.y +
                               vertex.normal.z * outward.z;
        CHECK(alignment < 0.0F);
        CHECK(length(vertex.normal) == Catch::Approx(1.0F).epsilon(1.0e-4F));
    }
}

TEST_CASE("a sky dome reaches the zenith and continues below the horizon") {
    const auto dome = mgv::make_sky_dome(10.0F, 16, 16);

    auto lowest = dome.vertices.front().position.y;
    auto highest = lowest;
    for (const auto& vertex : dome.vertices) {
        lowest = std::min(lowest, vertex.position.y);
        highest = std::max(highest, vertex.position.y);
    }
    CHECK(highest == Catch::Approx(10.0F).epsilon(1.0e-4F));
    CHECK(lowest < 0.0F);
}

TEST_CASE("sky dome construction rejects degenerate parameters") {
    CHECK_THROWS(mgv::make_sky_dome(0.0F, 8, 8));
    CHECK_THROWS(mgv::make_sky_dome(10.0F, 1, 8));
    CHECK_THROWS(mgv::make_sky_dome(10.0F, 8, 2));
}

TEST_CASE("a ground plane is centred, level, and fully subdivided") {
    const auto plane = mgv::make_ground_plane(8.0F, 4);

    CHECK(plane.vertices.size() == 25);
    CHECK(plane.indices.size() == 4 * 4 * 6);
    for (const auto& vertex : plane.vertices) {
        CHECK(vertex.position.y == 0.0F);
        CHECK(std::abs(vertex.position.x) <= 4.0F);
        CHECK(std::abs(vertex.position.z) <= 4.0F);
        CHECK(vertex.normal == mgv::Vec3{0.0F, 1.0F, 0.0F});
    }
}

TEST_CASE("ground plane construction rejects degenerate parameters") {
    CHECK_THROWS(mgv::make_ground_plane(0.0F, 4));
    CHECK_THROWS(mgv::make_ground_plane(4.0F, 0));
}

TEST_CASE("course terrain reproduces the physics surface across the green") {
    const mgv::CourseTerrainDescription description;

    // Inside the green the drawn surface must equal the analytic surface the
    // physics heightmap samples, or the ball rolls on something the player
    // cannot see.
    const auto analytic = [](float x, float z) {
        return -0.05F * z + 0.1F * std::sin(0.4F * z) * std::cos(0.4F * x);
    };
    for (float z = -7.0F; z <= 7.0F; z += 1.75F) {
        for (float x = -7.0F; x <= 7.0F; x += 1.75F) {
            CHECK(mgv::course_terrain_height(x, z, description) ==
                  Catch::Approx(analytic(x, z)).margin(1.0e-5F));
        }
    }
}

TEST_CASE("course terrain is continuous across the edge of the green") {
    const mgv::CourseTerrainDescription description;
    const auto edge = description.green_half_extent;

    const auto inside = mgv::course_terrain_height(0.0F, -edge + 0.01F, description);
    const auto outside = mgv::course_terrain_height(0.0F, -edge - 0.01F, description);

    CHECK(std::abs(inside - outside) < 0.01F);
}

TEST_CASE("course terrain rolls beyond the green") {
    const mgv::CourseTerrainDescription description;

    auto lowest = 0.0F;
    auto highest = 0.0F;
    for (float z = -300.0F; z <= 300.0F; z += 7.0F) {
        for (float x = -300.0F; x <= 300.0F; x += 7.0F) {
            const auto relief =
                mgv::course_terrain_height(x, z, description) + 0.05F * z;
            lowest = std::min(lowest, relief);
            highest = std::max(highest, relief);
        }
    }
    CHECK(highest - lowest > 1.0F);
}

TEST_CASE("the surface class cuts the green, its collar, the approach, and the rough") {
    const mgv::CourseTerrainDescription description;
    const auto half = description.green_half_extent;

    const auto putting = mgv::course_surface_class(0.0F, 0.0F, description);
    const auto collar = mgv::course_surface_class(0.0F, -half * 0.95F, description);
    const auto approach = mgv::course_surface_class(0.0F, -60.0F, description);
    const auto rough = mgv::course_surface_class(90.0F, -60.0F, description);

    SECTION("each surface is cut shorter than the one outside it") {
        CHECK(putting.cut_height < collar.cut_height);
        CHECK(collar.cut_height <= approach.cut_height);
        CHECK(approach.cut_height < rough.cut_height);
    }
    SECTION("a putting surface is a few millimetres and rough is centimetres") {
        CHECK(putting.cut_height < 0.005F);
        CHECK(rough.cut_height > 0.04F);
        // The eye reads cut height, so the two have to differ by an order of
        // magnitude, not by a tint.
        CHECK(rough.cut_height / putting.cut_height > 10.0F);
    }
    SECTION("the green and the approach are mown across each other") {
        CHECK(putting.mow < -0.9F);
        CHECK(approach.mow > 0.9F);
    }
    SECTION("an unmown collar separates the two patterns") {
        // Walking out from the middle of the green to the approach, the mow
        // signal has to pass through zero somewhere: that unmown ring is what
        // gives a green its crisp edge instead of letting the two striping
        // patterns run into each other.
        auto quietest = 1.0F;
        auto quietest_cut = 0.0F;
        for (float radius = half * 0.5F; radius <= half * 1.4F; radius += half * 0.01F) {
            const auto sample = mgv::course_surface_class(0.0F, -radius, description);
            if (std::abs(sample.mow) < quietest) {
                quietest = std::abs(sample.mow);
                quietest_cut = sample.cut_height;
            }
        }
        CHECK(quietest < 0.1F);
        SECTION("and it is cut between the two") {
            CHECK(quietest_cut > putting.cut_height);
            CHECK(quietest_cut < approach.cut_height);
        }
    }
    SECTION("rough carries no stripes at all") {
        CHECK(std::abs(rough.mow) < 1.0e-4F);
    }
}

TEST_CASE("the surface class stays within its declared ranges") {
    const mgv::CourseTerrainDescription description;

    for (float z = -400.0F; z <= 400.0F; z += 11.0F) {
        for (float x = -400.0F; x <= 400.0F; x += 11.0F) {
            const auto surface = mgv::course_surface_class(x, z, description);
            CHECK(surface.cut_height > 0.0F);
            CHECK(surface.cut_height <= 0.07F);
            CHECK(surface.mow >= -1.0F);
            CHECK(surface.mow <= 1.0F);
        }
    }
}

TEST_CASE("course terrain geometry is watertight where the rings meet the green") {
    mgv::CourseTerrainDescription description;
    description.green_resolution = 8;
    description.rings = 4;
    description.outer_extent = 80.0F;

    const auto terrain = mgv::make_course_terrain(description);

    REQUIRE_FALSE(terrain.empty());
    CHECK(terrain.indices.size() % 3 == 0);
    for (const auto index : terrain.indices) {
        CHECK(index < terrain.vertices.size());
    }

    SECTION("every vertex sits on the analytic surface") {
        for (const auto& vertex : terrain.vertices) {
            CHECK(vertex.position.y ==
                  Catch::Approx(mgv::course_terrain_height(
                                    vertex.position.x, vertex.position.z, description))
                      .margin(1.0e-4F));
        }
    }
    SECTION("the terrain reaches its outer extent") {
        auto furthest = 0.0F;
        for (const auto& vertex : terrain.vertices) {
            furthest = std::max(furthest, std::abs(vertex.position.x));
        }
        CHECK(furthest == Catch::Approx(80.0F).epsilon(1.0e-4F));
    }
}

TEST_CASE("course terrain construction rejects degenerate descriptions") {
    mgv::CourseTerrainDescription description;
    description.green_resolution = 4;
    description.rings = 2;

    auto no_green = description;
    no_green.green_half_extent = 0.0F;
    CHECK_THROWS(mgv::make_course_terrain(no_green));

    auto inverted = description;
    inverted.outer_extent = description.green_half_extent * 0.5F;
    CHECK_THROWS(mgv::make_course_terrain(inverted));

    auto no_rings = description;
    no_rings.rings = 0;
    CHECK_THROWS(mgv::make_course_terrain(no_rings));
}

TEST_CASE("a grass blade tapers to a single vertex at its tip") {
    const auto blade = mgv::make_grass_blade(4);

    REQUIRE_FALSE(blade.empty());
    CHECK(blade.vertices.size() == 9);   // four pairs plus the tip
    CHECK(blade.indices.size() == 3 * 7);

    const auto& tip = blade.vertices.back();
    CHECK(tip.position.x == 0.0F);
    CHECK(tip.position.y == Catch::Approx(1.0F));

    SECTION("the blade is of unit height, so instance scale is its length") {
        auto tallest = 0.0F;
        for (const auto& vertex : blade.vertices) {
            tallest = std::max(tallest, vertex.position.y);
            CHECK(vertex.position.y >= 0.0F);
        }
        CHECK(tallest == Catch::Approx(1.0F));
    }
    SECTION("it narrows from base to tip") {
        CHECK(std::abs(blade.vertices[0].position.x) >
              std::abs(blade.vertices[6].position.x));
    }
}

TEST_CASE("a grass blade rejects a degenerate segment count") {
    CHECK_THROWS(mgv::make_grass_blade(0));
}

TEST_CASE("a grass field takes each blade from the cut where it stands") {
    const mgv::CourseTerrainDescription terrain;
    mgv::GrassFieldDescription field;
    field.centre = {40.0F, -60.0F};   // deep in the rough, beside the approach
    field.radius = 6.0F;
    field.density = 120.0F;

    const auto grass = mgv::make_grass_field(field, terrain);

    REQUIRE(grass.instances.size() > 50);
    for (const auto& instance : grass.instances) {
        const auto height = instance.parameters.x;
        const auto width = instance.parameters.y;
        CHECK(height > 0.0F);
        CHECK(height < 0.12F);
        CHECK(width > 0.0F);
        CHECK(width < height);

        SECTION("every blade stands on the terrain") {
            CHECK(instance.position.y == Catch::Approx(mgv::course_terrain_height(
                      instance.position.x, instance.position.z, terrain)).margin(1.0e-4F));
        }
        SECTION("and inside the field") {
            const auto dx = instance.position.x - field.centre.x;
            const auto dz = instance.position.z - field.centre.y;
            CHECK(std::sqrt(dx * dx + dz * dz) <= field.radius + 1.0e-3F);
        }
    }
}

TEST_CASE("rough grows taller blades than a fairway does") {
    const mgv::CourseTerrainDescription terrain;
    const auto average_height = [&terrain](mgv::Vec2 centre) {
        mgv::GrassFieldDescription field;
        field.centre = centre;
        field.radius = 4.0F;
        field.density = 300.0F;
        const auto grass = mgv::make_grass_field(field, terrain);
        auto total = 0.0F;
        for (const auto& instance : grass.instances) {
            total += instance.parameters.x;
        }
        return total / static_cast<float>(grass.instances.size());
    };

    const auto fairway = average_height({0.0F, -60.0F});
    const auto rough = average_height({45.0F, -60.0F});

    CHECK(rough > fairway * 3.0F);
}

TEST_CASE("a putting surface grows no blades worth drawing") {
    const mgv::CourseTerrainDescription terrain;
    mgv::GrassFieldDescription field;
    field.centre = {0.0F, 0.0F};
    field.radius = 3.0F;
    field.density = 400.0F;

    const auto grass = mgv::make_grass_field(field, terrain);

    // Three millimetres of bentgrass is below a pixel from any stance, so the
    // field degenerates to the single placeholder instance.
    REQUIRE(grass.instances.size() == 1);
    CHECK(grass.instances.front().parameters.x == 0.0F);
}

TEST_CASE("a grass field rejects a degenerate description") {
    const mgv::CourseTerrainDescription terrain;
    mgv::GrassFieldDescription field;

    auto no_radius = field;
    no_radius.radius = 0.0F;
    CHECK_THROWS(mgv::make_grass_field(no_radius, terrain));

    auto no_density = field;
    no_density.density = 0.0F;
    CHECK_THROWS(mgv::make_grass_field(no_density, terrain));
}
