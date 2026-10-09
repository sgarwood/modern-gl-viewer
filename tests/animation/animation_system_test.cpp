#include "mgv/animation/animation_system.hpp"

#include "ozz_test_assets.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <limits>

TEST_CASE("animation players advance deterministically and honour playback controls") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem animations;
    const auto clip = animations.load(assets.paths());
    const auto player = animations.create_player(clip);

    animations.play(player);
    animations.advance(std::chrono::duration<float>{0.25F});
    CHECK(animations.root_transform(player).position().x == Catch::Approx(0.5F).margin(0.001F));

    animations.pause(player);
    animations.advance(std::chrono::duration<float>{0.25F});
    CHECK(animations.root_transform(player).position().x == Catch::Approx(0.5F).margin(0.001F));

    animations.seek(player, std::chrono::duration<float>{0.75F});
    CHECK(animations.root_transform(player).position().x == Catch::Approx(1.5F).margin(0.001F));

    animations.set_playback_rate(player, 2.0F);
    animations.play(player);
    animations.advance(std::chrono::duration<float>{0.25F});
    CHECK(animations.root_transform(player).position().x == Catch::Approx(0.5F).margin(0.001F));

    animations.stop(player);
    CHECK(animations.root_transform(player).position().x == Catch::Approx(0.0F));
}

TEST_CASE("animation player handles remain stable across removal") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem animations;
    const auto clip = animations.load(assets.paths());
    const auto removed = animations.create_player(clip);
    const auto retained = animations.create_player(clip);

    CHECK(animations.remove_player(removed));
    CHECK_FALSE(animations.contains(removed));
    CHECK(animations.contains(retained));
    CHECK_THROWS_AS(animations.play(removed), std::out_of_range);

    const auto replacement = animations.create_player(clip);
    CHECK(replacement != removed);
}

TEST_CASE("animation seeking clamps to the end pose") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem animations;
    const auto player = animations.create_player(animations.load(assets.paths()));

    animations.seek(player, mgv::animation::AnimationDuration{1.0F});
    CHECK(animations.root_transform(player).position().x == Catch::Approx(2.0F).margin(0.001F));

    animations.play(player);
    animations.advance(mgv::animation::AnimationDuration{0.0F});
    CHECK(animations.root_transform(player).position().x == Catch::Approx(2.0F).margin(0.001F));

    animations.seek(player, mgv::animation::AnimationDuration{10.0F});
    CHECK(animations.root_transform(player).position().x == Catch::Approx(2.0F).margin(0.001F));
}

TEST_CASE("one-shot animation holds its finish instead of wrapping") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem animations;
    const auto player = animations.create_player(animations.load(assets.paths()));

    animations.set_playback_mode(player, mgv::animation::PlaybackMode::once);
    animations.seek(player, mgv::animation::AnimationDuration{0.75F});
    animations.play(player);
    animations.advance(mgv::animation::AnimationDuration{0.5F});

    CHECK(animations.root_transform(player).position().x ==
          Catch::Approx(2.0F).margin(0.001F));
    CHECK(animations.state(player) == mgv::animation::PlaybackState::paused);
}

TEST_CASE("animation reports its authored duration") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem animations;
    const auto clip = animations.load(assets.paths());

    CHECK(animations.duration(clip).count() == Catch::Approx(1.0F));
}

TEST_CASE("a short one-shot tail reaches the exact finish despite cross-fading") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem animations;
    const auto first = animations.load(assets.paths());
    const auto second = animations.load(assets.paths());
    const auto player = animations.create_player(first);

    animations.seek(player, mgv::animation::AnimationDuration{0.4F});
    animations.set_clip(player, second);
    animations.seek(player, mgv::animation::AnimationDuration{0.95F});
    animations.set_playback_mode(player, mgv::animation::PlaybackMode::once);
    animations.play(player);
    animations.advance(mgv::animation::AnimationDuration{0.1F});

    CHECK(animations.root_transform(player).position().x ==
          Catch::Approx(2.0F).margin(0.001F));
    CHECK(animations.state(player) == mgv::animation::PlaybackState::paused);
}

TEST_CASE("clip changes cross-fade instead of snapping between poses") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem animations;
    const auto first = animations.load(assets.paths());
    const auto second = animations.load(assets.paths());
    const auto player = animations.create_player(first);

    animations.seek(player, mgv::animation::AnimationDuration{0.4F});
    animations.set_clip(player, second);
    animations.seek(player, mgv::animation::AnimationDuration{0.5F});
    CHECK(animations.root_transform(player).position().x ==
          Catch::Approx(0.8F).margin(0.001F));

    animations.play(player);
    animations.advance(mgv::animation::AnimationDuration{0.05F});
    const auto halfway = animations.root_transform(player).position().x;
    CHECK(halfway > 0.8F);
    CHECK(halfway < 1.1F);

    animations.advance(mgv::animation::AnimationDuration{0.05F});
    CHECK(animations.root_transform(player).position().x ==
          Catch::Approx(1.2F).margin(0.001F));
}

TEST_CASE("animation rejects invalid assets, handles, and timing values") {
    mgv::animation::AnimationSystem animations;
    const mgv::animation::AnimationAssetPaths missing{
        .skeleton = "missing-skeleton.ozz",
        .animation = "missing-animation.ozz",
    };
    CHECK_THROWS_AS(animations.load(missing), std::runtime_error);
    CHECK_THROWS_AS(
        animations.create_player(mgv::animation::AnimationClipId{999}),
        std::out_of_range);

    const TestAnimationAssets assets;
    const auto clip = animations.load(assets.paths());
    const auto player = animations.create_player(clip);
    CHECK(animations.contains(clip));
    CHECK_THROWS_AS(animations.set_playback_rate(player, 0.0F), std::invalid_argument);
    CHECK_THROWS_AS(
        animations.set_playback_rate(player, std::numeric_limits<float>::infinity()),
        std::invalid_argument);
    CHECK_THROWS_AS(
        animations.seek(player, mgv::animation::AnimationDuration{-0.1F}),
        std::invalid_argument);
    CHECK_THROWS_AS(
        animations.advance(mgv::animation::AnimationDuration{-0.1F}),
        std::invalid_argument);
}

TEST_CASE("a player can be switched to another clip on the same skeleton") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem system;
    const auto first = system.load(assets.paths());
    const auto second = system.load(assets.paths());
    const auto player = system.create_player(first);

    system.play(player);
    system.advance(mgv::animation::AnimationDuration{0.4F});
    REQUIRE(system.clip_of(player) == first);

    system.set_clip(player, second);

    SECTION("the player now reports the clip it was given") {
        CHECK(system.clip_of(player) == second);
    }
    SECTION("and starts it from the beginning") {
        // A swing that began halfway through would look like a twitch. A
        // player freshly created on the same clip is by definition at the
        // start, so after both have run for the same time they must agree.
        // The comparison happens after an advance because a pose is sampled
        // on the tick, not on the switch.
        const auto reference = system.create_player(second);
        system.play(reference);
        system.advance(mgv::animation::AnimationDuration{0.1F});
        CHECK(system.root_transform(player).position().x ==
              Catch::Approx(system.root_transform(reference).position().x).margin(1.0e-3F));
    }
    SECTION("the binding that says which mesh it deforms survives") {
        CHECK(system.contains(player));
        CHECK(system.state(player) == mgv::animation::PlaybackState::playing);
    }
}

TEST_CASE("switching to an unknown clip or player is an error") {
    const TestAnimationAssets assets;
    mgv::animation::AnimationSystem system;
    const auto clip = system.load(assets.paths());
    const auto player = system.create_player(clip);

    CHECK_THROWS_AS(
        system.set_clip(player, mgv::animation::AnimationClipId{9999}), std::out_of_range);
    CHECK_THROWS_AS(
        system.set_clip(mgv::animation::AnimationPlayerId{9999}, clip), std::out_of_range);
}
