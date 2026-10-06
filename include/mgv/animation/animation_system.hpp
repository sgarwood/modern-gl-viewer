#pragma once

#include "mgv/transform.hpp"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>

namespace mgv::animation {

struct AnimationClipId final {
    std::uint64_t value{};

    friend bool operator==(const AnimationClipId&, const AnimationClipId&) = default;
};

struct AnimationPlayerId final {
    std::uint64_t value{};

    friend bool operator==(const AnimationPlayerId&, const AnimationPlayerId&) = default;
};

struct AnimationAssetPaths final {
    std::filesystem::path skeleton;
    std::filesystem::path animation;
};

enum class PlaybackState {
    stopped,
    playing,
    paused,
};

using AnimationDuration = std::chrono::duration<float>;

class AnimationSystem final {
public:
    AnimationSystem();
    ~AnimationSystem();

    AnimationSystem(AnimationSystem&&) noexcept;
    AnimationSystem& operator=(AnimationSystem&&) noexcept;
    AnimationSystem(const AnimationSystem&) = delete;
    AnimationSystem& operator=(const AnimationSystem&) = delete;

    [[nodiscard]] AnimationClipId load(const AnimationAssetPaths& paths);
    [[nodiscard]] AnimationPlayerId create_player(AnimationClipId clip);
    [[nodiscard]] bool remove_player(AnimationPlayerId player);
    void clear_players() noexcept;

    void play(AnimationPlayerId player);
    void pause(AnimationPlayerId player);
    void stop(AnimationPlayerId player);
    void seek(AnimationPlayerId player, AnimationDuration time);
    void set_playback_rate(AnimationPlayerId player, float rate);
    void advance(AnimationDuration elapsed);

    [[nodiscard]] bool contains(AnimationClipId clip) const noexcept;
    [[nodiscard]] bool contains(AnimationPlayerId player) const noexcept;
    [[nodiscard]] PlaybackState state(AnimationPlayerId player) const;
    [[nodiscard]] Transform root_transform(AnimationPlayerId player) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv::animation
