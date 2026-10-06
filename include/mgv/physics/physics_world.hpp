#pragma once

#include "mgv/physics/collision_detector.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace mgv::physics {

struct PhysicsConfiguration final {
    Duration fixed_time_step{1.0F / 60.0F};
    Acceleration gravity{{0.0F, -9.81F, 0.0F}};
    std::size_t maximum_substeps{8};
};

class PhysicsWorldImpl;

class PhysicsWorld final {
public:
    PhysicsWorld();
    explicit PhysicsWorld(PhysicsConfiguration configuration);
    PhysicsWorld(
        PhysicsConfiguration configuration,
        std::unique_ptr<CollisionDetector> collision_detector);
    ~PhysicsWorld();

    PhysicsWorld(PhysicsWorld&&) noexcept;
    PhysicsWorld& operator=(PhysicsWorld&&) noexcept;
    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    [[nodiscard]] BodyId add_body(RigidBodyDefinition definition);
    [[nodiscard]] bool remove_body(BodyId id);
    [[nodiscard]] bool contains(BodyId id) const noexcept;
    [[nodiscard]] const RigidBody& body(BodyId id) const;
    [[nodiscard]] std::span<const Collision> collisions() const noexcept;

    void set_wind(LinearVelocity wind);
    void set_air_density(float rho);
    void apply_impulse(BodyId id, Impulse impulse);
    void set_velocity(BodyId id, LinearVelocity linear, AngularVelocity angular);
    void simulate(Duration elapsed_time);

private:
    std::unique_ptr<PhysicsWorldImpl> impl_;
};

} // namespace mgv::physics
