#include "mgv/game/round.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>

namespace {

/// A hill running up towards -Z, so a camera that interpolates straight
/// through the ground instead of walking over it is caught.
class Hillside final : public mgv::game::GroundHeights {
public:
    [[nodiscard]] float height_at(float, float z) const override { return -0.08F * z; }
};

class Flat final : public mgv::game::GroundHeights {
public:
    [[nodiscard]] float height_at(float, float) const override { return 0.0F; }
};

[[nodiscard]] mgv::game::RoundRules rules_with_hole_at(mgv::Vec2 hole) {
    mgv::game::RoundRules rules;
    rules.hole = hole;
    return rules;
}

[[nodiscard]] mgv::game::RoundObservation resting_at(mgv::Vec3 ball) {
    return {.elapsed_seconds = 1.0F / 60.0F, .ball_position = ball, .ball_speed = 0.0F};
}

} // namespace

TEST_CASE("a round starts addressing the ball from behind it, on the line to the hole") {
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -40.0F}), ground};

    const auto update = round.update(resting_at({0.0F, 0.02F, 0.0F}));

    CHECK(update.state == mgv::game::RoundState::addressing);
    SECTION("the camera stands behind the ball, away from the hole") {
        CHECK(update.eye.z > 0.0F);
        CHECK(update.eye.z == Catch::Approx(round.rules().stance_distance));
    }
    SECTION("at eye height above the ground") {
        CHECK(update.eye.y == Catch::Approx(round.rules().eye_height));
    }
    SECTION("looking at the ball") {
        CHECK(update.look_at.z == Catch::Approx(0.0F));
    }
}

TEST_CASE("striking the ball puts the round in flight, and resting ends it") {
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -40.0F}), ground};
    static_cast<void>(round.update(resting_at({0.0F, 0.02F, 0.0F})));

    const auto struck = round.update(
        {.elapsed_seconds = 0.016F, .ball_position = {0.0F, 1.0F, -5.0F}, .ball_speed = 40.0F});
    CHECK(struck.state == mgv::game::RoundState::in_flight);
    CHECK(struck.event == mgv::game::RoundEvent::ball_struck);

    const auto landed = round.update(resting_at({1.0F, 0.02F, -120.0F}));
    CHECK(landed.state == mgv::game::RoundState::walking);
    CHECK(landed.event == mgv::game::RoundEvent::ball_came_to_rest);
}

TEST_CASE("a ball that stops in the cup is holed, not walked to") {
    const Flat ground;
    auto rules = rules_with_hole_at({3.0F, -40.0F});
    mgv::game::Round round{rules, ground};
    static_cast<void>(round.update(
        {.elapsed_seconds = 0.016F, .ball_position = {3.0F, 0.02F, -20.0F}, .ball_speed = 9.0F}));

    const auto holed = round.update(resting_at({3.02F, 0.02F, -40.01F}));

    CHECK(holed.event == mgv::game::RoundEvent::ball_holed);
    CHECK(holed.state == mgv::game::RoundState::holing_out);

    SECTION("the cup is wherever the rules say, not a hardcoded point") {
        mgv::game::Round elsewhere{rules_with_hole_at({-88.0F, 150.0F}), ground};
        static_cast<void>(elsewhere.update(
            {.elapsed_seconds = 0.016F, .ball_position = {}, .ball_speed = 9.0F}));
        // The same resting place is nowhere near this hole.
        const auto missed = elsewhere.update(resting_at({3.02F, 0.02F, -40.01F}));
        CHECK(missed.event == mgv::game::RoundEvent::ball_came_to_rest);
    }
}

TEST_CASE("the walk ends at the ball, in the time the rules allow") {
    const Flat ground;
    auto rules = rules_with_hole_at({0.0F, -60.0F});
    rules.walk_duration = 2.0F;
    mgv::game::Round round{rules, ground};

    static_cast<void>(round.update(resting_at({0.0F, 0.02F, 0.0F})));
    static_cast<void>(round.update(
        {.elapsed_seconds = 0.016F, .ball_position = {0.0F, 2.0F, -10.0F}, .ball_speed = 30.0F}));
    const mgv::Vec3 lie{4.0F, 0.02F, -30.0F};
    static_cast<void>(round.update(resting_at(lie)));

    mgv::game::RoundUpdate last{};
    auto reached = 0;
    for (int step = 0; step < 240; ++step) {   // four seconds at sixty hertz
        last = round.update(resting_at(lie));
        reached += last.event == mgv::game::RoundEvent::reached_ball ? 1 : 0;
    }

    CHECK(reached == 1);
    CHECK(round.state() == mgv::game::RoundState::addressing);
    SECTION("and the camera finishes standing over the ball") {
        const auto across = std::sqrt(
            (last.eye.x - lie.x) * (last.eye.x - lie.x) +
            (last.eye.z - lie.z) * (last.eye.z - lie.z));
        CHECK(across == Catch::Approx(rules.stance_distance).margin(0.2F));
    }
}

TEST_CASE("the camera walks over a hill rather than through it") {
    const Hillside ground;
    auto rules = rules_with_hole_at({0.0F, -200.0F});
    rules.walk_duration = 3.0F;
    mgv::game::Round round{rules, ground};

    static_cast<void>(round.update(resting_at({0.0F, 0.0F, 0.0F})));
    static_cast<void>(round.update(
        {.elapsed_seconds = 0.016F, .ball_position = {0.0F, 4.0F, -40.0F}, .ball_speed = 40.0F}));
    const mgv::Vec3 lie{0.0F, 6.4F, -80.0F};
    static_cast<void>(round.update(resting_at(lie)));

    // Straight interpolation between the two ends would cut below a ground
    // that rises six metres across the walk.
    auto lowest_clearance = 1.0e9F;
    for (int step = 0; step < 200; ++step) {
        const auto update = round.update(resting_at(lie));
        if (update.state != mgv::game::RoundState::walking) {
            break;
        }
        lowest_clearance = std::min(
            lowest_clearance, update.eye.y - ground.height_at(update.eye.x, update.eye.z));
    }

    CHECK(lowest_clearance > rules.eye_height - rules.walk_bob - 1.0e-3F);
}

TEST_CASE("the range finder raises, wanders, and stows back to where it was") {
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -150.0F}), ground};
    const auto resting = resting_at({0.0F, 0.02F, 0.0F});
    static_cast<void>(round.update(resting));

    CHECK_FALSE(round.sight_direction().has_value());
    CHECK(round.toggle_range_finder(resting) == mgv::game::RoundEvent::range_finder_deployed);
    CHECK(round.state() == mgv::game::RoundState::sighting);

    const auto aim = round.sight_direction();
    REQUIRE(aim.has_value());
    SECTION("it points down the hole") {
        CHECK(aim->z < -0.9F);
    }

    SECTION("the aim wanders, but stays on the target") {
        auto widest = 0.0F;
        mgv::Vec3 first{};
        for (int step = 0; step < 300; ++step) {
            const auto update = round.update(resting);
            if (step == 0) {
                first = update.look_at;
            }
            widest = std::max(widest, std::abs(update.look_at.x - first.x));
            CHECK(update.state == mgv::game::RoundState::sighting);
        }
        // It has to move, or it is a tripod, and it has to stay within a
        // hand's worth of the target, or it is not aimed at anything.
        CHECK(widest > 1.0e-3F);
        CHECK(widest < 1.0F);
    }

    SECTION("stowing returns to what was happening before") {
        CHECK(round.toggle_range_finder(resting) == mgv::game::RoundEvent::range_finder_stowed);
        CHECK(round.state() == mgv::game::RoundState::addressing);
        CHECK_FALSE(round.sight_direction().has_value());
    }
}

TEST_CASE("the range finder can be raised mid-walk and gives the walk back") {
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -90.0F}), ground};
    const auto resting = resting_at({0.0F, 0.02F, -40.0F});
    static_cast<void>(round.update(resting_at({0.0F, 0.02F, 0.0F})));
    static_cast<void>(round.update(
        {.elapsed_seconds = 0.016F, .ball_position = {}, .ball_speed = 30.0F}));
    static_cast<void>(round.update(resting));
    REQUIRE(round.state() == mgv::game::RoundState::walking);

    static_cast<void>(round.toggle_range_finder(resting));
    CHECK(round.state() == mgv::game::RoundState::sighting);
    static_cast<void>(round.toggle_range_finder(resting));
    CHECK(round.state() == mgv::game::RoundState::walking);
}

TEST_CASE("the round takes the camera only when it is staging something") {
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -50.0F}), ground};
    const auto resting = resting_at({0.0F, 0.02F, 0.0F});

    SECTION("addressing leaves the view to the player") {
        const auto update = round.update(resting);
        CHECK(update.state == mgv::game::RoundState::addressing);
        CHECK_FALSE(update.directs_camera);
        // It still offers a stance, for a caller that wants one.
        CHECK(update.eye.y == Catch::Approx(round.rules().eye_height));
    }

    SECTION("a struck ball, the walk after it, and the range finder do not") {
        const auto struck = round.update(
            {.elapsed_seconds = 0.016F, .ball_position = {}, .ball_speed = 30.0F});
        CHECK(struck.directs_camera);

        const auto landed = round.update(resting_at({0.0F, 0.02F, -70.0F}));
        CHECK(landed.directs_camera);

        static_cast<void>(round.toggle_range_finder(resting));
        CHECK(round.update(resting).directs_camera);
    }
}

TEST_CASE("the round knows which way the green lies") {
    const Flat ground;

    SECTION("a hole straight ahead is a bearing of zero") {
        const mgv::game::Round round{rules_with_hole_at({0.0F, -100.0F}), ground};
        CHECK(round.aim_bearing_degrees({0.0F, 0.0F, 0.0F}) == Catch::Approx(0.0F).margin(1.0e-3F));
    }
    SECTION("a hole behind is half a turn, in the shot model's convention") {
        const mgv::game::Round round{rules_with_hole_at({0.0F, 100.0F}), ground};
        CHECK(std::abs(round.aim_bearing_degrees({0.0F, 0.0F, 0.0F})) ==
              Catch::Approx(180.0F).margin(1.0e-3F));
    }
    SECTION("a hole to the right is a quarter turn clockwise") {
        const mgv::game::Round round{rules_with_hole_at({100.0F, 0.0F}), ground};
        CHECK(round.aim_bearing_degrees({0.0F, 0.0F, 0.0F}) == Catch::Approx(90.0F).margin(1.0e-3F));
    }
    SECTION("the bearing is taken from the ball, not from the origin") {
        const mgv::game::Round round{rules_with_hole_at({0.0F, 0.0F}), ground};
        // Standing past the hole, it is behind you.
        CHECK(std::abs(round.aim_bearing_degrees({0.0F, 0.0F, -50.0F})) ==
              Catch::Approx(180.0F).margin(1.0e-3F));
    }
    SECTION("a bearing is always one the shot model will accept") {
        const mgv::game::Round round{rules_with_hole_at({12.0F, -8.0F}), ground};
        for (const auto x : {-200.0F, -1.0F, 0.0F, 7.5F, 410.0F}) {
            const auto bearing = round.aim_bearing_degrees({x, 0.0F, x * 0.3F});
            CHECK(bearing >= -180.0F);
            CHECK(bearing <= 180.0F);
        }
    }
}
