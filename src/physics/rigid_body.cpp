#include "mgv/physics/rigid_body.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace mgv::physics {

RigidBodyDefinition::RigidBodyDefinition(
    MotionType motion,
    Collider collider,
    Position position,
    LinearVelocity velocity,
    Mass mass,
    float restitution)
    : motion_{motion},
      collider_{std::move(collider)},
      position_{position},
      velocity_{velocity},
      mass_{mass},
      restitution_{restitution} {}

MotionType RigidBodyDefinition::motion() const noexcept { return motion_; }
const Collider& RigidBodyDefinition::collider() const noexcept { return collider_; }
const Position& RigidBodyDefinition::position() const noexcept { return position_; }
const LinearVelocity& RigidBodyDefinition::velocity() const noexcept { return velocity_; }
const Mass& RigidBodyDefinition::mass() const noexcept { return mass_; }
float RigidBodyDefinition::restitution() const noexcept { return restitution_; }

RigidBodyBuilder::RigidBodyBuilder(Collider collider) : collider_{std::move(collider)} {}

RigidBodyBuilder& RigidBodyBuilder::motion(MotionType value) noexcept {
    motion_ = value;
    return *this;
}

RigidBodyBuilder& RigidBodyBuilder::at(Position value) noexcept {
    position_ = value;
    return *this;
}

RigidBodyBuilder& RigidBodyBuilder::velocity(LinearVelocity value) noexcept {
    velocity_ = value;
    return *this;
}

RigidBodyBuilder& RigidBodyBuilder::mass(Mass value) noexcept {
    mass_ = value;
    return *this;
}

RigidBodyBuilder& RigidBodyBuilder::restitution(float value) {
    if (!std::isfinite(value) || value < 0.0F || value > 1.0F) {
        throw std::invalid_argument{"Restitution must be between zero and one"};
    }
    restitution_ = value;
    return *this;
}

RigidBodyDefinition RigidBodyBuilder::build() const {
    return RigidBodyDefinition{
        motion_, collider_, position_, velocity_, mass_, restitution_};
}

RigidBody::RigidBody(BodyId id, RigidBodyDefinition definition)
    : id_{id},
      motion_{definition.motion()},
      collider_{definition.collider()},
      position_{definition.position()},
      velocity_{definition.velocity()},
      mass_{definition.mass()},
      restitution_{definition.restitution()} {}

BodyId RigidBody::id() const noexcept { return id_; }
MotionType RigidBody::motion() const noexcept { return motion_; }
const Collider& RigidBody::collider() const noexcept { return collider_; }
const Position& RigidBody::position() const noexcept { return position_; }
const LinearVelocity& RigidBody::linear_velocity() const noexcept { return velocity_; }
const Mass& RigidBody::mass() const noexcept { return mass_; }

float RigidBody::inverse_mass() const noexcept {
    return motion_ == MotionType::dynamic ? 1.0F / mass_.kilograms() : 0.0F;
}

float RigidBody::restitution() const noexcept { return restitution_; }

} // namespace mgv::physics
