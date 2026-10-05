#pragma once

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

struct ApplyEntityImpulseCommand final {
    EntityId entity;
    physics::Impulse impulse;
};

using EngineCommand = std::variant<
    CameraInputCommand,
    SetEntityTransformCommand,
    ApplyEntityImpulseCommand>;

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
    void remove(EntityId entity);

    void enqueue(EngineCommand command);
    void enqueue(InputAction action);
    void attach_network(
        std::unique_ptr<network::NetworkEventSource> events,
        std::unique_ptr<NetworkEventDecoder> decoder);
    void tick(Viewport viewport);

    [[nodiscard]] bool contains(EntityId entity) const noexcept;
    [[nodiscard]] Transform transform(EntityId entity) const;
    [[nodiscard]] Camera camera() const;
    [[nodiscard]] RenderStatistics last_frame_statistics() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mgv
