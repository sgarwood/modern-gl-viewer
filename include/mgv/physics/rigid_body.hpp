#pragma once

#include "mgv/physics/collider.hpp"
#include "mgv/physics/terrain_material.hpp"

#include <cstdint>

namespace mgv::physics {

class PhysicsWorldImpl;

struct BodyId final {
    std::uint64_t value{};

    friend bool operator==(const BodyId&, const BodyId&) = default;
};

enum class MotionType {
    static_body,
    dynamic,
};

class RigidBodyDefinition final {
public:
    [[nodiscard]] MotionType motion() const noexcept;
    [[nodiscard]] const Collider& collider() const noexcept;
    [[nodiscard]] const Position& position() const noexcept;
    [[nodiscard]] const LinearVelocity& velocity() const noexcept;
    [[nodiscard]] const AngularVelocity& angular_velocity() const noexcept;
    [[nodiscard]] const Mass& mass() const noexcept;
    [[nodiscard]] float restitution() const noexcept;
    [[nodiscard]] const TerrainMaterial& material() const noexcept;

private:
    friend class RigidBodyBuilder;

    RigidBodyDefinition(
        MotionType motion,
        Collider collider,
        Position position,
        LinearVelocity velocity,
        AngularVelocity angular_velocity,
        Mass mass,
        float restitution,
        TerrainMaterial material);

    MotionType motion_;
    Collider collider_;
    Position position_;
    LinearVelocity velocity_;
    AngularVelocity angular_velocity_;
    Mass mass_;
    float restitution_{};
    TerrainMaterial material_{};
};

class RigidBodyBuilder final {
public:
    explicit RigidBodyBuilder(Collider collider);

    RigidBodyBuilder& motion(MotionType value) noexcept;
    RigidBodyBuilder& at(Position value) noexcept;
    RigidBodyBuilder& velocity(LinearVelocity value) noexcept;
    RigidBodyBuilder& angular_velocity(AngularVelocity value) noexcept;
    RigidBodyBuilder& mass(Mass value) noexcept;
    RigidBodyBuilder& restitution(float value);
    RigidBodyBuilder& material(TerrainMaterial value);

    [[nodiscard]] RigidBodyDefinition build() const;

private:
    Collider collider_;
    MotionType motion_{MotionType::dynamic};
    Position position_;
    LinearVelocity velocity_;
    AngularVelocity angular_velocity_;
    Mass mass_{1.0F};
    float restitution_{};
    TerrainMaterial material_{};
};

class RigidBody final {
public:
    RigidBody(BodyId id, RigidBodyDefinition definition);

    [[nodiscard]] BodyId id() const noexcept;
    [[nodiscard]] MotionType motion() const noexcept;
    [[nodiscard]] const Collider& collider() const noexcept;
    [[nodiscard]] const Position& position() const noexcept;
    [[nodiscard]] const LinearVelocity& linear_velocity() const noexcept;
    [[nodiscard]] const AngularVelocity& angular_velocity() const noexcept;
    [[nodiscard]] const Mass& mass() const noexcept;
    [[nodiscard]] float inverse_mass() const noexcept;
    /// The reciprocal of the body's moment of inertia about its centre, in
    /// reciprocal kilogram metres squared.
    ///
    /// Zero for a static body, and zero for any dynamic body that is not a
    /// sphere. A single scalar is only honest for a sphere, whose inertia is
    /// the same about every axis; a box needs a tensor, and giving it one
    /// number would make it spin wrongly rather than not at all. The only
    /// dynamic body in this project is a ball, so the restriction costs
    /// nothing and the lie would have cost something.
    [[nodiscard]] float inverse_inertia() const noexcept;
    [[nodiscard]] float restitution() const noexcept;
    [[nodiscard]] const TerrainMaterial& material() const noexcept;

private:
    friend class PhysicsWorldImpl;

    BodyId id_;
    MotionType motion_;
    Collider collider_;
    Position position_;
    LinearVelocity velocity_;
    AngularVelocity angular_velocity_;
    Mass mass_;
    float restitution_{};
    TerrainMaterial material_{};
};

} // namespace mgv::physics
