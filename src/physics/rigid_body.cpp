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
    AngularVelocity angular_velocity,
    Mass mass,
    float restitution,
    TerrainMaterial material)
    : motion_{motion},
      collider_{std::move(collider)},
      position_{position},
      velocity_{velocity},
      angular_velocity_{angular_velocity},
      mass_{mass},
      restitution_{restitution},
      material_{material} {}

MotionType RigidBodyDefinition::motion() const noexcept { return motion_; }
const Collider& RigidBodyDefinition::collider() const noexcept { return collider_; }
const Position& RigidBodyDefinition::position() const noexcept { return position_; }
const LinearVelocity& RigidBodyDefinition::velocity() const noexcept { return velocity_; }
const AngularVelocity& RigidBodyDefinition::angular_velocity() const noexcept { return angular_velocity_; }
const Mass& RigidBodyDefinition::mass() const noexcept { return mass_; }
float RigidBodyDefinition::restitution() const noexcept { return restitution_; }
const TerrainMaterial& RigidBodyDefinition::material() const noexcept { return material_; }

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

RigidBodyBuilder& RigidBodyBuilder::angular_velocity(AngularVelocity value) noexcept {
    angular_velocity_ = value;
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

RigidBodyBuilder& RigidBodyBuilder::material(TerrainMaterial value) {
    material_ = value;
    return *this;
}

RigidBodyDefinition RigidBodyBuilder::build() const {
    return RigidBodyDefinition{
        motion_, collider_, position_, velocity_, angular_velocity_, mass_, restitution_, material_};
}

RigidBody::RigidBody(BodyId id, RigidBodyDefinition definition)
    : id_{id},
      motion_{definition.motion()},
      collider_{definition.collider()},
      position_{definition.position()},
      velocity_{definition.velocity()},
      angular_velocity_{definition.angular_velocity()},
      mass_{definition.mass()},
      restitution_{definition.restitution()},
      material_{definition.material()} {}

BodyId RigidBody::id() const noexcept { return id_; }
MotionType RigidBody::motion() const noexcept { return motion_; }
const Collider& RigidBody::collider() const noexcept { return collider_; }
const Position& RigidBody::position() const noexcept { return position_; }
const LinearVelocity& RigidBody::linear_velocity() const noexcept { return velocity_; }
const AngularVelocity& RigidBody::angular_velocity() const noexcept { return angular_velocity_; }
const Mass& RigidBody::mass() const noexcept { return mass_; }

float RigidBody::inverse_inertia() const noexcept {
    if (motion_ != MotionType::dynamic) {
        return 0.0F;
    }
    const auto radius = sphere_radius(collider_);
    if (!radius) {
        return 0.0F;
    }
    // A solid sphere: I = 2/5 m r^2. A golf ball is not quite uniform -- its
    // core is lighter than its cover -- but the difference is under a couple
    // of percent of I, and well inside the uncertainty on the surface
    // coefficients it gets multiplied by.
    return 2.5F / (mass_.kilograms() * *radius * *radius);
}

float RigidBody::inverse_mass() const noexcept {
    return motion_ == MotionType::dynamic ? 1.0F / mass_.kilograms() : 0.0F;
}

float RigidBody::restitution() const noexcept { return restitution_; }
const TerrainMaterial& RigidBody::material() const noexcept { return material_; }
bool RigidBody::sleeping() const noexcept { return sleeping_; }

} // namespace mgv::physics
