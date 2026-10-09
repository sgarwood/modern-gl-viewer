#pragma once

#include "mgv/transform.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

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

/// What happens when a playing clip reaches its final pose.
enum class PlaybackMode {
    loop,
    once,
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

    /// Re-points a player at another clip, from the start.
    ///
    /// The two clips must be built on skeletons of the same size, which is
    /// the one thing a player's buffers depend on. Swapping a clip is how a
    /// character changes what it is doing without losing the binding that
    /// says which mesh it deforms.
    void set_clip(AnimationPlayerId player, AnimationClipId clip);

    void play(AnimationPlayerId player);
    void pause(AnimationPlayerId player);
    void stop(AnimationPlayerId player);
    void seek(AnimationPlayerId player, AnimationDuration time);
    void set_playback_rate(AnimationPlayerId player, float rate);
    void set_playback_mode(AnimationPlayerId player, PlaybackMode mode);
    void advance(AnimationDuration elapsed);

    [[nodiscard]] bool contains(AnimationClipId clip) const noexcept;
    [[nodiscard]] bool contains(AnimationPlayerId player) const noexcept;
    [[nodiscard]] PlaybackState state(AnimationPlayerId player) const;
    [[nodiscard]] Transform root_transform(AnimationPlayerId player) const;
    [[nodiscard]] AnimationDuration duration(AnimationClipId clip) const;

    /// The clip a player is playing.
    [[nodiscard]] AnimationClipId clip_of(AnimationPlayerId player) const;

    /// Number of joints in a clip's skeleton.
    [[nodiscard]] std::size_t joint_count(AnimationClipId clip) const;

    /// The skeleton's joint names, in skeleton order.
    ///
    /// A mesh's skin indexes its own joint list, which has no reason to be
    /// ordered the way the skeleton is -- the two are usually converted by
    /// different tools. Names are what the two are matched on.
    [[nodiscard]] std::vector<std::string> joint_names(AnimationClipId clip) const;

    /// Writes the player's current joint matrices, in model space, into
    /// `out`, and returns how many were written.
    ///
    /// The runtime has always computed these; it simply did not publish
    /// them, and `root_transform` returned one of them. Multiplied by a
    /// mesh's inverse bind matrices they become the skinning palette.
    ///
    /// Fills a caller's buffer rather than returning one, because this is
    /// read every frame for every animated character and a vector per
    /// character per frame is a waste nobody needs.
    std::size_t joint_matrices(AnimationPlayerId player, std::span<Mat4> out) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv::animation
