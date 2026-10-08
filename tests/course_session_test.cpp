#include "mgv/animation/animation_system.hpp"
#include "mgv/course_session.hpp"
#include "mgv/gltf_loader.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

TEST_CASE("the default course bundles a renderable Quaternius player") {
    const std::filesystem::path assets{MGV_TEST_ASSETS};
    const auto description = mgv::default_course_session(assets);

    REQUIRE(description.character.has_value());
    const auto& character = *description.character;
    CHECK(character.model.filename() == "quaternius_male_casual.glb");
    CHECK(character.scale > 0.0F);

    const auto model = mgv::GltfLoader{}.load(character.model);
    REQUIRE(model.primitives.size() == 1);
    CHECK(model.primitives.front().mesh.skinned());
    REQUIRE(model.skin.has_value());
    REQUIRE(model.materials.size() == 1);
    CHECK(model.materials.front().diffuse_image.has_value());

    mgv::animation::AnimationSystem animations;
    const auto clip = animations.load({
        .skeleton = character.skeleton,
        .animation = character.animation,
    });
    const auto skeleton_joints = animations.joint_names(clip);
    // Control bones may live in the animation skeleton without influencing
    // a vertex. Every skin joint must exist; exact counts need not match.
    CHECK(skeleton_joints.size() >= model.skin->joint_names.size());
    for (const auto& joint : model.skin->joint_names) {
        CHECK(std::ranges::find(skeleton_joints, joint) != skeleton_joints.end());
    }
}
