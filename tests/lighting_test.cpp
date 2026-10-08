#include "mgv/lighting.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

[[nodiscard]] float length(const mgv::Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

TEST_CASE("light directions are normalized") {
    const auto direction = mgv::normalized_light_direction({0.0F, 4.0F, 3.0F});

    CHECK(length(direction) == Catch::Approx(1.0F));
    CHECK(direction.y == Catch::Approx(0.8F));
    CHECK(direction.z == Catch::Approx(0.6F));
}

TEST_CASE("a degenerate light direction falls back to the default sun") {
    const auto direction = mgv::normalized_light_direction({0.0F, 0.0F, 0.0F});

    CHECK(length(direction) == Catch::Approx(1.0F));
    CHECK(direction.y > 0.0F);
}

TEST_CASE("a sun directly overhead points straight up") {
    const auto direction = mgv::sun_direction_from_angles(0.0F, 90.0F);

    CHECK(direction.x == Catch::Approx(0.0F).margin(1.0e-6F));
    CHECK(direction.y == Catch::Approx(1.0F));
    CHECK(direction.z == Catch::Approx(0.0F).margin(1.0e-6F));
}

TEST_CASE("sun azimuth is measured clockwise from north") {
    const auto north = mgv::sun_direction_from_angles(0.0F, 0.0F);
    CHECK(north.z == Catch::Approx(-1.0F));

    const auto east = mgv::sun_direction_from_angles(90.0F, 0.0F);
    CHECK(east.x == Catch::Approx(1.0F));
    CHECK(east.z == Catch::Approx(0.0F).margin(1.0e-6F));
}

TEST_CASE("a sun at altitude keeps a unit length") {
    const auto direction = mgv::sun_direction_from_angles(135.0F, 40.0F);

    CHECK(length(direction) == Catch::Approx(1.0F));
    CHECK(direction.y == Catch::Approx(std::sin(40.0F * 3.14159265F / 180.0F)).epsilon(1.0e-4F));
}

namespace {

[[nodiscard]] mgv::Vec3 project(const mgv::Mat4& m, const mgv::Vec3& p) {
    const auto x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
    const auto y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
    const auto z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
    const auto w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
    return {x / w, y / w, z / w};
}

} // namespace

TEST_CASE("a directional light's shadow volume contains everything it covers") {
    mgv::ShadowVolume volume;
    volume.centre = {12.0F, 0.0F, -30.0F};
    volume.radius = 40.0F;
    const auto matrix = mgv::directional_light_view_projection(
        mgv::sun_direction_from_angles(140.0F, 45.0F), volume);

    // Sample the corners of the covered region at ground level and overhead.
    for (const auto dx : {-1.0F, 1.0F}) {
        for (const auto dz : {-1.0F, 1.0F}) {
            for (const auto height : {0.0F, 15.0F}) {
                const mgv::Vec3 point{
                    volume.centre.x + dx * volume.radius * 0.7F,
                    height,
                    volume.centre.z + dz * volume.radius * 0.7F,
                };
                const auto clip = project(matrix, point);
                CHECK(std::abs(clip.x) <= 1.0F);
                CHECK(std::abs(clip.y) <= 1.0F);
                CHECK(clip.z >= -1.0F);
                CHECK(clip.z <= 1.0F);
            }
        }
    }
}

TEST_CASE("an orthographic light projection preserves parallel spacing") {
    mgv::ShadowVolume volume;
    volume.radius = 50.0F;
    const auto matrix = mgv::directional_light_view_projection(
        mgv::Vec3{0.0F, 1.0F, 0.0F}, volume);

    // Under an orthographic projection, equal world steps give equal clip
    // steps no matter how far away they are.
    const auto near_step =
        project(matrix, {10.0F, 0.0F, 0.0F}).x - project(matrix, {0.0F, 0.0F, 0.0F}).x;
    const auto far_step =
        project(matrix, {10.0F, 40.0F, 0.0F}).x - project(matrix, {0.0F, 40.0F, 0.0F}).x;

    CHECK(near_step == Catch::Approx(far_step).epsilon(1.0e-5F));
}

TEST_CASE("the shadow volume snaps to whole texels so its edges do not crawl") {
    mgv::ShadowVolume volume;
    volume.radius = 32.0F;
    volume.resolution = 1024;
    const auto direction = mgv::sun_direction_from_angles(135.0F, 50.0F);

    // Moving the volume may only ever shift the shadow map by a whole number
    // of texels. Any fractional shift is what makes shadow edges crawl, and a
    // sub-texel camera movement is the case that exposes it: the quantised
    // offset is then either zero or exactly one texel, never in between.
    const auto texel = 2.0F * volume.radius / static_cast<float>(volume.resolution);
    const auto texels_per_clip_unit = static_cast<float>(volume.resolution) * 0.5F;
    const mgv::Vec3 probe{5.0F, 1.0F, -7.0F};

    const auto before = project(
        mgv::directional_light_view_projection(direction, volume), probe);
    for (const auto fraction : {0.1F, 0.3F, 0.5F, 0.9F, 1.4F}) {
        auto nudged = volume;
        nudged.centre = {texel * fraction, 0.0F, texel * fraction * 0.5F};
        const auto after = project(
            mgv::directional_light_view_projection(direction, nudged), probe);

        for (const auto shift : {(after.x - before.x) * texels_per_clip_unit,
                                 (after.y - before.y) * texels_per_clip_unit}) {
            CHECK(shift == Catch::Approx(std::round(shift)).margin(1.0e-3F));
        }
    }
}

TEST_CASE("a light pointing straight down still produces a usable basis") {
    mgv::ShadowVolume volume;
    const auto matrix = mgv::directional_light_view_projection({0.0F, 1.0F, 0.0F}, volume);

    const auto clip = project(matrix, {0.0F, 0.0F, 0.0F});
    CHECK(std::abs(clip.x) <= 1.0F);
    CHECK(std::abs(clip.y) <= 1.0F);
}

TEST_CASE("a point nearer the sun records nearer in the shadow map") {
    mgv::ShadowVolume volume;
    volume.radius = 50.0F;
    const auto direction = mgv::sun_direction_from_angles(150.0F, 35.0F);
    const auto matrix = mgv::directional_light_view_projection(direction, volume);

    // Walking from the ground towards the sun must decrease the recorded
    // depth monotonically. If it increases, the depth test can never find an
    // occluder and nothing in the scene ever casts a shadow -- a failure that
    // containment and orthographic-spacing checks both pass straight through.
    auto previous = project(matrix, {0.0F, 0.0F, 0.0F}).z;
    for (const auto step : {2.0F, 6.0F, 12.0F, 25.0F}) {
        const mgv::Vec3 towards_sun{
            direction.x * step,
            direction.y * step,
            direction.z * step,
        };
        const auto depth = project(matrix, towards_sun).z;
        CHECK(depth < previous);
        previous = depth;
    }
}

TEST_CASE("the shadow volume reaches above its own ground plane") {
    mgv::ShadowVolume volume;
    volume.radius = 60.0F;
    const auto matrix = mgv::directional_light_view_projection(
        mgv::sun_direction_from_angles(120.0F, 20.0F), volume);

    // A tall tree standing at the centre has to fit, or its crown is clipped
    // out of the map and casts nothing.
    for (const auto height : {0.0F, 5.0F, 12.0F, 24.0F}) {
        const auto clip = project(matrix, {0.0F, height, 0.0F});
        CHECK(clip.z >= -1.0F);
        CHECK(clip.z <= 1.0F);
    }
}

TEST_CASE("directional shadow cascades fit ordered slices of the camera frustum") {
    mgv::Camera camera;
    camera.look_at({8.0F, 6.0F, 12.0F}, {0.0F, 0.5F, -20.0F});
    camera.set_perspective(50.0F, 0.1F, 300.0F);
    mgv::ShadowVolume settings;
    settings.cascade_count = 3;
    settings.maximum_distance = 120.0F;
    settings.split_lambda = 0.7F;
    settings.resolution = 2048;

    const auto cascades = mgv::directional_light_cascades(
        camera,
        16.0F / 9.0F,
        mgv::sun_direction_from_angles(140.0F, 45.0F),
        settings);

    REQUIRE(cascades.size() == 3);
    CHECK(cascades[0].split_depth > 0.1F);
    CHECK(cascades[0].split_depth < cascades[1].split_depth);
    CHECK(cascades[1].split_depth < cascades[2].split_depth);
    CHECK(cascades[2].split_depth == Catch::Approx(120.0F));
    for (const auto& cascade : cascades) {
        for (const auto value : cascade.view_projection) {
            CHECK(std::isfinite(value));
        }
    }

    CHECK(mgv::directional_light_cascades(
              camera,
              16.0F / 9.0F,
              mgv::sun_direction_from_angles(140.0F, 45.0F),
              settings) == cascades);
}

TEST_CASE("directional shadow cascades reject invalid policy") {
    const mgv::Camera camera;
    mgv::ShadowVolume settings;

    settings.cascade_count = 0;
    CHECK_THROWS_AS(
        mgv::directional_light_cascades(camera, 1.0F, {0.0F, 1.0F, 0.0F}, settings),
        std::invalid_argument);

    settings.cascade_count = mgv::maximum_shadow_cascades + 1U;
    CHECK_THROWS_AS(
        mgv::directional_light_cascades(camera, 1.0F, {0.0F, 1.0F, 0.0F}, settings),
        std::invalid_argument);

    settings.cascade_count = 3;
    settings.split_lambda = 1.1F;
    CHECK_THROWS_AS(
        mgv::directional_light_cascades(camera, 1.0F, {0.0F, 1.0F, 0.0F}, settings),
        std::invalid_argument);
}
