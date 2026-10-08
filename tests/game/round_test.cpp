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
    return {.elapsed_seconds = 1.0F / 60.0F, .ball_position = ball, .ball_velocity = {}};
}

/// A strike too weak to be worth a camera move: a chip, not a drive.
[[nodiscard]] mgv::game::RoundObservation chipped_from(mgv::Vec3 ball) {
    return {.elapsed_seconds = 0.016F, .ball_position = ball,
            .ball_velocity = {0.0F, 6.0F, -8.0F}};
}

/// A full shot, launched down -Z at fifteen degrees.
[[nodiscard]] mgv::game::RoundObservation driven_from(mgv::Vec3 ball) {
    return {.elapsed_seconds = 0.016F, .ball_position = ball,
            .ball_velocity = {0.0F, 14.2F, -53.1F}};
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
        {.elapsed_seconds = 0.016F, .ball_position = {0.0F, 1.0F, -5.0F}, .ball_velocity = {0.0F, 0.0F, -40.0F}});
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
        {.elapsed_seconds = 0.016F, .ball_position = {3.0F, 0.02F, -20.0F}, .ball_velocity = {0.0F, 0.0F, -9.0F}}));

    const auto holed = round.update(resting_at({3.02F, 0.02F, -40.01F}));

    CHECK(holed.event == mgv::game::RoundEvent::ball_holed);
    CHECK(holed.state == mgv::game::RoundState::holing_out);

    SECTION("the cup is wherever the rules say, not a hardcoded point") {
        mgv::game::Round elsewhere{rules_with_hole_at({-88.0F, 150.0F}), ground};
        static_cast<void>(elsewhere.update(
            {.elapsed_seconds = 0.016F, .ball_position = {}, .ball_velocity = {0.0F, 0.0F, -9.0F}}));
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
        {.elapsed_seconds = 0.016F, .ball_position = {0.0F, 2.0F, -10.0F}, .ball_velocity = {0.0F, 0.0F, -30.0F}}));
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
        {.elapsed_seconds = 0.016F, .ball_position = {0.0F, 4.0F, -40.0F}, .ball_velocity = {0.0F, 0.0F, -40.0F}}));
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
        {.elapsed_seconds = 0.016F, .ball_position = {}, .ball_velocity = {0.0F, 0.0F, -30.0F}}));
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
            {.elapsed_seconds = 0.016F, .ball_position = {}, .ball_velocity = {0.0F, 0.0F, -30.0F}});
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

TEST_CASE("carry is estimated from the whole launch velocity, not its speed alone") {
    // Forty-five degrees carries furthest in a vacuum, and a ball driven
    // flat or popped straight up carries less at the same speed. A round
    // deciding on speed alone would treat all three the same.
    const auto flat = mgv::game::Round::estimated_carry({0.0F, 5.0F, -50.0F});
    const auto best = mgv::game::Round::estimated_carry({0.0F, 35.6F, -35.6F});
    const auto popped = mgv::game::Round::estimated_carry({0.0F, 50.0F, -5.0F});

    CHECK(best > flat);
    CHECK(best > popped);
    SECTION("the optimum matches the closed form") {
        CHECK(best == Catch::Approx(50.0F * 50.0F / 9.81F).epsilon(0.02F));
    }
    SECTION("a ball going nowhere carries nothing") {
        CHECK(mgv::game::Round::estimated_carry({}) == Catch::Approx(0.0F));
    }
    SECTION("a ball driven into the ground does not carry backwards") {
        CHECK(mgv::game::Round::estimated_carry({0.0F, -20.0F, -40.0F}) >= 0.0F);
    }
}

TEST_CASE("a long shot runs follow-through, then tracing, then the walk") {
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -220.0F}), ground};
    static_cast<void>(round.update(resting_at({0.0F, 0.02F, 0.0F})));

    const auto struck = round.update(driven_from({0.0F, 0.1F, 0.0F}));
    CHECK(struck.event == mgv::game::RoundEvent::long_shot_struck);
    CHECK(struck.state == mgv::game::RoundState::following_through);

    SECTION("the camera holds on the player while they finish") {
        // Not on the ball: the ball is already fifty metres away and the
        // point of the shot is watching the swing end.
        const auto eye = struck.eye;
        const auto held = round.update(driven_from({0.0F, 12.0F, -50.0F}));
        CHECK(held.state == mgv::game::RoundState::following_through);
        CHECK(held.eye.x == Catch::Approx(eye.x).margin(0.5F));
        CHECK(held.eye.z == Catch::Approx(eye.z).margin(0.5F));
    }

    SECTION("then it pulls back and traces the ball") {
        auto flying = struck;
        for (int step = 0; step < 90 && flying.state != mgv::game::RoundState::tracing; ++step) {
            flying = round.update(driven_from({0.0F, 20.0F, -80.0F}));
        }
        REQUIRE(flying.state == mgv::game::RoundState::tracing);
        CHECK(flying.directs_camera);

        // Once the pull-back has run its course, tracing looks at the
        // ball, wherever it is. Getting there is eased rather than cut, so
        // the aim is only on the ball at the end of the move.
        mgv::game::RoundUpdate high{};
        for (int step = 0; step < 200; ++step) {
            high = round.update(driven_from({0.0F, 30.0F, -140.0F}));
        }
        CHECK(high.look_at.z == Catch::Approx(-140.0F).margin(0.5F));
        CHECK(high.look_at.y == Catch::Approx(30.0F).margin(0.5F));
    }

    SECTION("and the camera really does pull out, not just follow") {
        mgv::game::RoundUpdate update{};
        auto nearest = 1.0e9F;
        auto furthest = 0.0F;
        for (int step = 0; step < 300; ++step) {
            update = round.update(driven_from({0.0F, 20.0F, -80.0F}));
            if (update.state != mgv::game::RoundState::tracing) {
                continue;
            }
            // Measured from where the swing was played, which is itself a
            // stance behind the ball -- not from the world origin.
            const auto stance_z = round.rules().stance_distance;
            const auto back = std::sqrt(
                update.eye.x * update.eye.x +
                (update.eye.z - stance_z) * (update.eye.z - stance_z));
            nearest = std::min(nearest, back);
            furthest = std::max(furthest, back);
        }
        CHECK(furthest > nearest + 5.0F);
        CHECK(furthest == Catch::Approx(round.rules().trace_distance).margin(1.0F));
    }
}

TEST_CASE("a short shot skips the finish and the pull-back entirely") {
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -40.0F}), ground};
    static_cast<void>(round.update(resting_at({0.0F, 0.02F, 0.0F})));

    const auto struck = round.update(chipped_from({0.0F, 0.1F, 0.0F}));

    // A chip is over before a camera could finish moving, and cutting away
    // from the player to watch it would be worse than not cutting at all.
    CHECK(struck.event == mgv::game::RoundEvent::ball_struck);
    CHECK(struck.state == mgv::game::RoundState::in_flight);
}

TEST_CASE("a traced shot still ends in the walk to the ball") {
    const Flat ground;
    auto rules = rules_with_hole_at({0.0F, -300.0F});
    rules.walk_duration = 1.0F;
    mgv::game::Round round{rules, ground};
    static_cast<void>(round.update(resting_at({0.0F, 0.02F, 0.0F})));
    static_cast<void>(round.update(driven_from({0.0F, 0.1F, 0.0F})));

    for (int step = 0; step < 300; ++step) {
        static_cast<void>(round.update(driven_from({0.0F, 20.0F, -90.0F})));
    }
    REQUIRE(round.state() == mgv::game::RoundState::tracing);

    const auto landed = round.update(resting_at({1.0F, 0.02F, -180.0F}));
    CHECK(landed.event == mgv::game::RoundEvent::ball_came_to_rest);
    CHECK(landed.state == mgv::game::RoundState::walking);
}

TEST_CASE("the player stands on the ground, not at their own eye height") {
    // stance_for() answers in eye positions because that is what the camera
    // wants; a character stood on one hovers a head above the turf.
    const Flat ground;   // y = 0 everywhere
    mgv::game::Round round{rules_with_hole_at({0.0F, -150.0F}), ground};

    const auto addressing = round.update(resting_at({0.0F, 0.02F, 0.0F}));
    CHECK(addressing.player_position.y == Catch::Approx(0.0F).margin(1.0e-4F));

    SECTION("and keeps their feet down through the swing and the trace") {
        auto update = round.update(driven_from({0.0F, 0.1F, 0.0F}));
        for (int step = 0; step < 200; ++step) {
            CHECK(update.player_position.y == Catch::Approx(0.0F).margin(1.0e-4F));
            update = round.update(driven_from({0.0F, 20.0F, -70.0F}));
        }
    }
    SECTION("the camera, which does want eye height, is still above them") {
        CHECK(addressing.eye.y > addressing.player_position.y + 1.0F);
    }
}

TEST_CASE("the pull-back eases its aim as well as its position") {
    // A camera that keeps its position but snaps its aim to a ball already
    // a hundred metres away has cut, not panned, and thrown the player out
    // of frame on the way.
    const Flat ground;
    mgv::game::Round round{rules_with_hole_at({0.0F, -260.0F}), ground};
    static_cast<void>(round.update(resting_at({0.0F, 0.02F, 0.0F})));
    const auto struck = round.update(driven_from({0.0F, 0.1F, 0.0F}));
    const auto watching = struck.look_at;

    mgv::game::RoundUpdate first_trace{};
    for (int step = 0; step < 300; ++step) {
        const auto update = round.update(driven_from({0.0F, 25.0F, -120.0F}));
        if (update.state == mgv::game::RoundState::tracing) {
            first_trace = update;
            break;
        }
    }
    REQUIRE(first_trace.state == mgv::game::RoundState::tracing);

    SECTION("the first traced frame still looks near the player") {
        CHECK(first_trace.look_at.z == Catch::Approx(watching.z).margin(8.0F));
    }
    SECTION("and by the end it is looking at the ball itself") {
        mgv::game::RoundUpdate settled{};
        for (int step = 0; step < 300; ++step) {
            settled = round.update(driven_from({0.0F, 25.0F, -120.0F}));
        }
        CHECK(settled.look_at.z == Catch::Approx(-120.0F).margin(0.5F));
        CHECK(settled.look_at.y == Catch::Approx(25.0F).margin(0.5F));
    }
}
