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
