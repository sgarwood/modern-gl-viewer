#pragma once

#include "mgv/physics/collision_detector.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace mgv::physics {

struct PhysicsConfiguration final {
    Duration fixed_time_step{1.0F / 60.0F};
    Acceleration gravity{{0.0F, -9.81F, 0.0F}};
    /// Density of the air the bodies fly through, in kg/m^3. Sea level on a
    /// standard day. Alongside gravity because it is the same kind of fact
    /// about the world, and because a test isolating contact behaviour wants
    /// to switch it off in the same breath as gravity.
    float air_density{1.225F};
    std::size_t maximum_substeps{8};
};


struct RaycastHit final {
    BodyId body;
    Position position;
    Vec3 normal;
    float distance;
};

class PhysicsWorldImpl;



struct FoliageVolume {
    Vec3 min;
    Vec3 max;
    float density;
};

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
    void add_foliage_volume(FoliageVolume volume);
    void set_air_density(float rho);
    void set_wetness(float wetness);
    [[nodiscard]] std::optional<RaycastHit> raycast(Position origin, Vec3 direction) const;
    void apply_impulse(BodyId id, Impulse impulse);
    void set_velocity(BodyId id, LinearVelocity linear, AngularVelocity angular);
    /// Moves a body outright, without giving it the velocity the move
    /// implies. For teleporting rather than for simulating.
    void set_position(BodyId id, Position position);
    /// Replaces a static body's collider and places it.
    ///
    /// The world a ball can land on is far larger than any heightmap fine
    /// enough to putt on, so the collidable patch follows the ball rather
    /// than covering the course. Restricted to static bodies: swapping the
    /// shape under something with momentum has no defensible answer for what
    /// its velocity should become.
    void set_static_collider(BodyId id, Collider collider, Position position);
    void simulate(Duration elapsed_time);

private:
    std::unique_ptr<PhysicsWorldImpl> impl_;
};

} // namespace mgv::physics
