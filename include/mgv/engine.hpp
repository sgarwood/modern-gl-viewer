#pragma once

#include "mgv/animation/animation_system.hpp"
#include "mgv/golf/shot_model.hpp"
#include "mgv/hardware/launch_monitor.hpp"
#include "mgv/input.hpp"
#include "mgv/network/network_service.hpp"
#include "mgv/physics/physics_world.hpp"
#include "mgv/renderer.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <variant>
#include <vector>

namespace mgv {

struct EntityId final {
    std::uint64_t value{};

    friend bool operator==(const EntityId&, const EntityId&) = default;
};

struct Viewport final {
    int framebuffer_width{1};
    int framebuffer_height{1};
};

class Clock {
public:
    using time_point = std::chrono::steady_clock::time_point;

    virtual ~Clock() = default;
    Clock(const Clock&) = delete;
    Clock& operator=(const Clock&) = delete;

    [[nodiscard]] virtual time_point now() const noexcept = 0;

protected:
    Clock() = default;
};

struct CameraInputCommand final {
    InputAction action;
};

struct SetEntityTransformCommand final {
    EntityId entity;
    Transform transform;
};



struct SetWeatherCommand final {
    float temperature_c;
    float wind_speed_mps;
    float wind_direction_deg;
};

struct SetEntityVelocityCommand final {
    EntityId entity;
    physics::LinearVelocity linear;
    physics::AngularVelocity angular;
};

struct ApplyEntityImpulseCommand final {
    EntityId entity;
    physics::Impulse impulse;
};

struct LaunchBallCommand final {
    EntityId entity;
    golf::Shot shot;
};

struct PlayAnimationCommand final {
    animation::AnimationPlayerId player;
};

struct PauseAnimationCommand final {
    animation::AnimationPlayerId player;
};

struct StopAnimationCommand final {
    animation::AnimationPlayerId player;
};

struct SeekAnimationCommand final {
    animation::AnimationPlayerId player;
    animation::AnimationDuration time;
};

struct SetAnimationPlaybackRateCommand final {
    animation::AnimationPlayerId player;
    float rate{1.0F};
};

using EngineCommand = std::variant<
    CameraInputCommand,
    SetEntityTransformCommand,
    ApplyEntityImpulseCommand,
    SetEntityVelocityCommand,
    LaunchBallCommand,
    SetWeatherCommand,
    PlayAnimationCommand,
    PauseAnimationCommand,
    StopAnimationCommand,
    SeekAnimationCommand,
    SetAnimationPlaybackRateCommand>;

class NetworkEventDecoder {
public:
    virtual ~NetworkEventDecoder() = default;
    NetworkEventDecoder(const NetworkEventDecoder&) = delete;
    NetworkEventDecoder& operator=(const NetworkEventDecoder&) = delete;

    [[nodiscard]] virtual std::vector<EngineCommand> decode(
        const network::NetworkEvent& event) = 0;

protected:
    NetworkEventDecoder() = default;
};

class Engine final {
public:
    explicit Engine(std::unique_ptr<RenderBackend> backend);
    Engine(
        std::unique_ptr<RenderBackend> backend,
        std::unique_ptr<Clock> clock,
        physics::PhysicsConfiguration physics = {});
    Engine(
        std::unique_ptr<RenderBackend> backend,
        std::unique_ptr<Clock> clock,
        physics::PhysicsConfiguration physics,
        std::unique_ptr<golf::ShotModel> shot_model);
    ~Engine();

    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    std::vector<EntityId> load(const AssetPaths& assets);
    std::vector<EntityId> set_scene(Scene scene);
    [[nodiscard]] physics::BodyId bind_physics(
        EntityId entity,
        physics::RigidBodyDefinition body);
    [[nodiscard]] physics::BodyId bind_golf_ball(
        EntityId entity,
        physics::RigidBodyDefinition body);
    [[nodiscard]] animation::AnimationClipId load_animation(
        const animation::AnimationAssetPaths& assets);
    [[nodiscard]] animation::AnimationPlayerId bind_animation(
        EntityId entity,
        animation::AnimationClipId clip);
    void remove(EntityId entity);

    void enqueue(EngineCommand command);
    void enqueue(InputAction action);
    void submit_shot(golf::Shot shot);
    void attach_launch_monitor(std::unique_ptr<hardware::LaunchMonitor> monitor);
    void attach_network(
        std::unique_ptr<network::NetworkEventSource> events,
        std::unique_ptr<NetworkEventDecoder> decoder);
    void tick(Viewport viewport);

    [[nodiscard]] bool contains(EntityId entity) const noexcept;
    [[nodiscard]] Transform transform(EntityId entity) const;
    [[nodiscard]] animation::PlaybackState animation_state(
        animation::AnimationPlayerId player) const;
    [[nodiscard]] Camera camera() const;
    [[nodiscard]] RenderStatistics last_frame_statistics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
