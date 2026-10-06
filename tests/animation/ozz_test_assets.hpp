#pragma once

#include "mgv/animation/animation_system.hpp"

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/base/maths/vec_float.h>

#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>

class TestAnimationAssets final {
public:
    TestAnimationAssets() : directory_{make_directory()} {
        ozz::animation::offline::RawSkeleton raw_skeleton;
        raw_skeleton.roots.resize(1);
        raw_skeleton.roots.front().name = "root";
        ozz::animation::offline::SkeletonBuilder skeleton_builder;
        const auto skeleton = skeleton_builder(raw_skeleton);
        if (!skeleton) {
            throw std::runtime_error{"Unable to build test skeleton"};
        }

        ozz::animation::offline::RawAnimation raw_animation;
        raw_animation.name = "test-translation";
        raw_animation.duration = 1.0F;
        raw_animation.tracks.resize(1);
        raw_animation.tracks.front().translations.push_back(
            {0.0F, ozz::math::Float3{0.0F, 0.0F, 0.0F}});
        raw_animation.tracks.front().translations.push_back(
            {1.0F, ozz::math::Float3{2.0F, 0.0F, 0.0F}});
        ozz::animation::offline::AnimationBuilder animation_builder;
        const auto animation = animation_builder(raw_animation);
        if (!animation) {
            throw std::runtime_error{"Unable to build test animation"};
        }

        save(directory_ / "skeleton.ozz", *skeleton);
        save(directory_ / "animation.ozz", *animation);
    }

    ~TestAnimationAssets() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }

    TestAnimationAssets(const TestAnimationAssets&) = delete;
    TestAnimationAssets& operator=(const TestAnimationAssets&) = delete;
    TestAnimationAssets(TestAnimationAssets&&) = delete;
    TestAnimationAssets& operator=(TestAnimationAssets&&) = delete;

    [[nodiscard]] mgv::animation::AnimationAssetPaths paths() const {
        return {
            .skeleton = directory_ / "skeleton.ozz",
            .animation = directory_ / "animation.ozz",
        };
    }

private:
    [[nodiscard]] static std::filesystem::path make_directory() {
        std::random_device random;
        for (int attempt = 0; attempt < 64; ++attempt) {
            const auto candidate = std::filesystem::temp_directory_path() /
                                   ("mgv-animation-test-" +
                                    std::to_string(random()));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) {
                return candidate;
            }
            if (error) {
                throw std::runtime_error{"Unable to create animation test directory"};
            }
        }
        throw std::runtime_error{"Unable to allocate a unique animation test directory"};
    }

    template <typename Value>
    static void save(const std::filesystem::path& path, const Value& value) {
        const auto native_path = path.string();
        ozz::io::File file{native_path.c_str(), "wb"};
        if (!file.opened()) {
            throw std::runtime_error{"Unable to open animation test archive"};
        }
        ozz::io::OArchive archive{&file};
        archive << value;
    }

    std::filesystem::path directory_;
};
