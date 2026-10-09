#pragma once

#include "mgv/animation/animation_system.hpp"
#include "mgv/golf/shot_model.hpp"
#include "mgv/hardware/launch_monitor.hpp"
#include "mgv/imported_model.hpp"
#include "mgv/input.hpp"
#include "mgv/network/network_service.hpp"
#include "mgv/physics/physics_world.hpp"
#include "mgv/renderer.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
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
    float temperature_c{};
    float wind_speed_mps{};
    /// Compass bearing the wind blows *towards*, degrees clockwise from -Z.
    float wind_direction_deg{};
    /// Station pressure, in pascals. Carry distance depends on air density
    /// and air density depends on this, so a course at altitude plays several
    /// percent longer than the same course at the coast.
    float pressure_pa{physics::standard_sea_level_pressure};
    /// Relative humidity, 0 to 1.
    float relative_humidity{};
    /// How wet the turf is, 0 to 1. Observed, not guessed from temperature.
    float turf_wetness{};
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

    std::vector<EntityId> load(const AssetPaths& assets, ModelFit fit = ModelFit::fit_to_view);
    std::vector<EntityId> set_scene(Scene scene);
    [[nodiscard]] physics::BodyId bind_physics(
        EntityId entity,
        physics::RigidBodyDefinition body);
    [[nodiscard]] physics::BodyId bind_golf_ball(
        EntityId entity,
        physics::RigidBodyDefinition body);
    [[nodiscard]] physics::BodyId add_static_collider(
        physics::RigidBodyDefinition body);
    /// Replaces a static collider's shape and position, so a collidable
    /// patch can follow the action rather than cover the world.
    void set_static_collider(
        physics::BodyId body,
        physics::Collider collider,
        physics::Position position);
    [[nodiscard]] animation::AnimationClipId load_animation(
        const animation::AnimationAssetPaths& assets);
    [[nodiscard]] animation::AnimationPlayerId bind_animation(
        EntityId entity,
        animation::AnimationClipId clip);
    /// Deforms an entity's mesh by an animation player's skeleton.
    ///
    /// The skin's joints are matched to the skeleton's **by name**. A mesh
    /// and a skeleton are usually converted by different tools and have no
    /// reason to agree on joint order; matching by index instead gives a
    /// character that animates almost correctly, which is far harder to
    /// diagnose than one that does not animate at all.
    ///
    /// Throws if the skin names a joint the skeleton does not have.
    void bind_skin(
        EntityId entity,
        animation::AnimationPlayerId player,
        const ImportedSkin& skin);
    void remove(EntityId entity);
    /// Replaces an entity's geometry in place, keeping its material,
    /// transform, physics binding, and identity. Returns false if the
    /// backend cannot update geometry in place.
    bool update_mesh(EntityId entity, const MeshData& mesh);
    void add_foliage_volume(physics::FoliageVolume volume);

    /// Switches an animation player to another clip and plays it immediately.
    /// A full swing can be kept as one source asset while a launch-monitor
    /// session enters it at impact and plays the remainder only once.
    void play_clip(
        animation::AnimationPlayerId player,
        animation::AnimationClipId clip,
        animation::AnimationDuration start = animation::AnimationDuration::zero(),
        animation::PlaybackMode mode = animation::PlaybackMode::loop);

    /// The clip's joint names, in skeleton order. What a socket can name.
    [[nodiscard]] std::vector<std::string> joint_names(
        animation::AnimationClipId clip) const;

    /// Where a named joint currently is, in the skeleton's own space.
    ///
    /// This is what a prop is socketed to: multiply it by whatever world
    /// transform the character is drawn with and a club ends up in a hand
    /// and stays there, because the hand and the club are now reading the
    /// same animation rather than two copies of it.
    ///
    /// Empty if the skeleton has no joint of that name, which is the
    /// ordinary case when an asset is swapped for one rigged differently.
    [[nodiscard]] std::optional<Transform> joint_transform(
        animation::AnimationPlayerId player,
        std::string_view joint) const;

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
    /// How long the previous tick covered, in seconds. Game logic driven
    /// alongside the engine uses this rather than keeping a second clock.
    [[nodiscard]] float last_tick_seconds() const noexcept;
    [[nodiscard]] std::optional<physics::LinearVelocity> linear_velocity(EntityId entity) const;
    [[nodiscard]] std::optional<physics::RaycastHit> raycast(
        physics::Position origin,
        Vec3 direction) const;
    [[nodiscard]] Camera camera() const;
    void set_camera(Camera camera);
    void set_environment(Environment environment);
    [[nodiscard]] Environment environment() const;
    [[nodiscard]] RenderStatistics last_frame_statistics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
