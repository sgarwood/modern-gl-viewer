#pragma once

#include "mgv/physics/collider.hpp"

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
    [[nodiscard]] const Mass& mass() const noexcept;
    [[nodiscard]] float restitution() const noexcept;

private:
    friend class RigidBodyBuilder;

    RigidBodyDefinition(
        MotionType motion,
        Collider collider,
        Position position,
        LinearVelocity velocity,
        Mass mass,
        float restitution);

    MotionType motion_;
    Collider collider_;
    Position position_;
    LinearVelocity velocity_;
    Mass mass_;
    float restitution_{};
};

class RigidBodyBuilder final {
public:
    explicit RigidBodyBuilder(Collider collider);

    RigidBodyBuilder& motion(MotionType value) noexcept;
    RigidBodyBuilder& at(Position value) noexcept;
    RigidBodyBuilder& velocity(LinearVelocity value) noexcept;
    RigidBodyBuilder& mass(Mass value) noexcept;
    RigidBodyBuilder& restitution(float value);

    [[nodiscard]] RigidBodyDefinition build() const;

private:
    Collider collider_;
    MotionType motion_{MotionType::dynamic};
    Position position_;
    LinearVelocity velocity_;
    Mass mass_{1.0F};
    float restitution_{};
};

class RigidBody final {
public:
    RigidBody(BodyId id, RigidBodyDefinition definition);

    [[nodiscard]] BodyId id() const noexcept;
    [[nodiscard]] MotionType motion() const noexcept;
    [[nodiscard]] const Collider& collider() const noexcept;
    [[nodiscard]] const Position& position() const noexcept;
    [[nodiscard]] const LinearVelocity& linear_velocity() const noexcept;
    [[nodiscard]] const Mass& mass() const noexcept;
    [[nodiscard]] float inverse_mass() const noexcept;
    [[nodiscard]] float restitution() const noexcept;

private:
    friend class PhysicsWorldImpl;

    BodyId id_;
    MotionType motion_;
    Collider collider_;
    Position position_;
    LinearVelocity velocity_;
    Mass mass_;
    float restitution_{};
};

} // namespace mgv::physics
