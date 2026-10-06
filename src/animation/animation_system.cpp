#include "mgv/animation/animation_system.hpp"

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/transform.h>
#include <ozz/base/span.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mgv::animation {
namespace {

std::atomic<std::uint64_t> next_clip_id{1};
std::atomic<std::uint64_t> next_player_id{1};

template <typename Id>
[[nodiscard]] Id allocate_id(std::atomic<std::uint64_t>& next, const char* kind) {
    auto value = next.load(std::memory_order_relaxed);
    while (value != std::numeric_limits<std::uint64_t>::max()) {
        if (next.compare_exchange_weak(
                value,
                value + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return Id{value};
        }
    }
    throw std::overflow_error{std::string{kind} + " identifier capacity exhausted"};
}

template <typename Value>
void load_archive(const std::filesystem::path& path, Value& value, const char* kind) {
    const auto native_path = path.string();
    ozz::io::File file{native_path.c_str(), "rb"};
    if (!file.opened()) {
        throw std::runtime_error{
            "Unable to open Ozz " + std::string{kind} + " archive: " + native_path};
    }
    ozz::io::IArchive archive{&file};
    if (!archive.TestTag<Value>()) {
        throw std::runtime_error{
            "Invalid Ozz " + std::string{kind} + " archive: " + native_path};
    }
    archive >> value;
}

[[nodiscard]] float valid_seconds(AnimationDuration duration, const char* label) {
    const auto value = duration.count();
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument{std::string{label} + " must be finite and non-negative"};
    }
    return value;
}

} // namespace

struct AnimationSystem::Impl final {
    struct Clip final {
        AnimationClipId id;
        ozz::animation::Skeleton skeleton;
        ozz::animation::Animation animation;
    };

    struct Player final {
        Player(AnimationPlayerId player_id, AnimationClipId clip_id, int joints, int soa_joints)
            : id{player_id},
              clip{clip_id},
              context{joints},
              locals(static_cast<std::size_t>(soa_joints)),
              models(static_cast<std::size_t>(joints)) {}

        AnimationPlayerId id;
        AnimationClipId clip;
        ozz::animation::SamplingJob::Context context;
        std::vector<ozz::math::SoaTransform> locals;
        std::vector<ozz::math::Float4x4> models;
        float time{};
        float playback_rate{1.0F};
        PlaybackState state{PlaybackState::stopped};
        Transform root;
    };

    [[nodiscard]] Clip& find(AnimationClipId id) {
        const auto found = std::ranges::find_if(
            clips, [id](const auto& clip) { return clip->id == id; });
        if (found == clips.end()) {
            throw std::out_of_range{"Unknown animation clip identifier"};
        }
        return **found;
    }

    [[nodiscard]] const Clip& find(AnimationClipId id) const {
        const auto found = std::ranges::find_if(
            clips, [id](const auto& clip) { return clip->id == id; });
        if (found == clips.end()) {
            throw std::out_of_range{"Unknown animation clip identifier"};
        }
        return **found;
    }

    [[nodiscard]] Player& find(AnimationPlayerId id) {
        const auto found = std::ranges::find_if(
            players, [id](const auto& player) { return player->id == id; });
        if (found == players.end()) {
            throw std::out_of_range{"Unknown animation player identifier"};
        }
        return **found;
    }

    [[nodiscard]] const Player& find(AnimationPlayerId id) const {
        const auto found = std::ranges::find_if(
            players, [id](const auto& player) { return player->id == id; });
        if (found == players.end()) {
            throw std::out_of_range{"Unknown animation player identifier"};
        }
        return **found;
    }

    void sample(Player& player) {
        const auto& clip = find(player.clip);
        ozz::animation::SamplingJob sampling;
        sampling.animation = &clip.animation;
        sampling.context = &player.context;
        sampling.ratio = player.time / clip.animation.duration();
        sampling.output = ozz::make_span(player.locals);
        if (!sampling.Run()) {
            throw std::runtime_error{"Ozz animation sampling failed"};
        }

        ozz::animation::LocalToModelJob local_to_model;
        local_to_model.skeleton = &clip.skeleton;
        local_to_model.input = ozz::make_span(player.locals);
        local_to_model.output = ozz::make_span(player.models);
        if (!local_to_model.Run()) {
            throw std::runtime_error{"Ozz local-to-model conversion failed"};
        }

        ozz::math::Transform root;
        if (!ozz::math::ToAffine(player.models.front(), &root)) {
            throw std::runtime_error{"Ozz root transform could not be decomposed"};
        }
        player.root
            .set_position({root.translation.x, root.translation.y, root.translation.z})
            .set_rotation({root.rotation.x, root.rotation.y, root.rotation.z, root.rotation.w})
            .set_scale({root.scale.x, root.scale.y, root.scale.z});
    }

    std::vector<std::unique_ptr<Clip>> clips;
    std::vector<std::unique_ptr<Player>> players;
};

AnimationSystem::AnimationSystem() : impl_{std::make_unique<Impl>()} {}
AnimationSystem::~AnimationSystem() = default;
AnimationSystem::AnimationSystem(AnimationSystem&&) noexcept = default;
AnimationSystem& AnimationSystem::operator=(AnimationSystem&&) noexcept = default;

AnimationClipId AnimationSystem::load(const AnimationAssetPaths& paths) {
    auto clip = std::make_unique<Impl::Clip>();
    clip->id = allocate_id<AnimationClipId>(next_clip_id, "Animation clip");
    load_archive(paths.skeleton, clip->skeleton, "skeleton");
    load_archive(paths.animation, clip->animation, "animation");
    if (clip->skeleton.num_joints() == 0) {
        throw std::invalid_argument{"Ozz skeleton must contain at least one joint"};
    }
    if (clip->skeleton.num_joints() != clip->animation.num_tracks()) {
        throw std::invalid_argument{"Ozz skeleton and animation track counts do not match"};
    }
    const auto id = clip->id;
    impl_->clips.push_back(std::move(clip));
    return id;
}

AnimationPlayerId AnimationSystem::create_player(AnimationClipId clip) {
    const auto& asset = impl_->find(clip);
    const auto id = allocate_id<AnimationPlayerId>(next_player_id, "Animation player");
    auto player = std::make_unique<Impl::Player>(
        id, clip, asset.skeleton.num_joints(), asset.skeleton.num_soa_joints());
    impl_->sample(*player);
    impl_->players.push_back(std::move(player));
    return id;
}

bool AnimationSystem::remove_player(AnimationPlayerId player) {
    const auto found = std::ranges::find_if(
        impl_->players, [player](const auto& value) { return value->id == player; });
    if (found == impl_->players.end()) {
        return false;
    }
    impl_->players.erase(found);
    return true;
}

void AnimationSystem::clear_players() noexcept {
    impl_->players.clear();
}

void AnimationSystem::play(AnimationPlayerId player) {
    impl_->find(player).state = PlaybackState::playing;
}

void AnimationSystem::pause(AnimationPlayerId player) {
    impl_->find(player).state = PlaybackState::paused;
}

void AnimationSystem::stop(AnimationPlayerId player) {
    auto& value = impl_->find(player);
    value.state = PlaybackState::stopped;
    value.time = 0.0F;
    impl_->sample(value);
}

void AnimationSystem::seek(AnimationPlayerId player, AnimationDuration time) {
    auto& value = impl_->find(player);
    const auto& clip = impl_->find(value.clip);
    value.time = std::min(
        valid_seconds(time, "Animation seek time"),
        clip.animation.duration());
    impl_->sample(value);
}

void AnimationSystem::set_playback_rate(AnimationPlayerId player, float rate) {
    if (!std::isfinite(rate) || rate <= 0.0F) {
        throw std::invalid_argument{"Animation playback rate must be finite and positive"};
    }
    impl_->find(player).playback_rate = rate;
}

void AnimationSystem::advance(AnimationDuration elapsed) {
    const auto seconds = valid_seconds(elapsed, "Animation elapsed time");
    if (seconds == 0.0F) {
        return;
    }
    for (auto& player_ptr : impl_->players) {
        auto& player = *player_ptr;
        if (player.state != PlaybackState::playing) {
            continue;
        }
        const auto& clip = impl_->find(player.clip);
        const auto advanced = static_cast<double>(player.time) +
                              static_cast<double>(seconds) *
                                  static_cast<double>(player.playback_rate);
        player.time = static_cast<float>(std::fmod(
            advanced,
            static_cast<double>(clip.animation.duration())));
        impl_->sample(player);
    }
}

bool AnimationSystem::contains(AnimationClipId clip) const noexcept {
    return std::ranges::find_if(
               impl_->clips, [clip](const auto& value) { return value->id == clip; }) !=
           impl_->clips.end();
}

bool AnimationSystem::contains(AnimationPlayerId player) const noexcept {
    return std::ranges::find_if(
               impl_->players, [player](const auto& value) { return value->id == player; }) !=
           impl_->players.end();
}

PlaybackState AnimationSystem::state(AnimationPlayerId player) const {
    return impl_->find(player).state;
}

Transform AnimationSystem::root_transform(AnimationPlayerId player) const {
    return impl_->find(player).root;
}

} // namespace mgv::animation
