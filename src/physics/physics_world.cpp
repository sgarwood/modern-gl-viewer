#include "mgv/physics/physics_world.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace mgv::physics {
namespace {

[[nodiscard]] Vec3 add(Vec3 lhs, Vec3 rhs) {
    return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

[[nodiscard]] Vec3 subtract(Vec3 lhs, Vec3 rhs) {
    return {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
}

[[nodiscard]] Vec3 scaled(Vec3 value, float scale) {
    return {value.x * scale, value.y * scale, value.z * scale};
}

[[nodiscard]] float dot(Vec3 lhs, Vec3 rhs) {
    return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

[[nodiscard]] Vec3 cross(Vec3 lhs, Vec3 rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x
    };
}

[[nodiscard]] float length(Vec3 v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

} // namespace

class PhysicsWorldImpl final {
public:
    PhysicsWorldImpl(
        PhysicsConfiguration value,
        std::unique_ptr<CollisionDetector> detector)
        : configuration{value}, collision_detector{std::move(detector)} {
        if (configuration.fixed_time_step.seconds() <= 0.0F) {
            throw std::invalid_argument{"Physics fixed timestep must be positive"};
        }
        if (configuration.maximum_substeps == 0) {
            throw std::invalid_argument{"Physics maximum substeps must be positive"};
        }
        if (!collision_detector) {
            throw std::invalid_argument{"Physics world requires a collision detector"};
        }
    }

    [[nodiscard]] RigidBody& find_mutable(BodyId id) {
        const auto found = std::ranges::find(bodies, id, &RigidBody::id);
        if (found == bodies.end()) {
            throw std::out_of_range{"Unknown physics body identifier"};
        }
        return *found;
    }

    [[nodiscard]] const RigidBody& find(BodyId id) const {
        const auto found = std::ranges::find(bodies, id, &RigidBody::id);
        if (found == bodies.end()) {
            throw std::out_of_range{"Unknown physics body identifier"};
        }
        return *found;
    }

    
    void set_velocity(BodyId id, LinearVelocity linear, AngularVelocity angular) {
        auto& body = find_mutable(id);
        if (body.motion_ == MotionType::static_body) {
            return;
        }
        body.velocity_ = linear;
        body.angular_velocity_ = angular;
    }

    void apply_impulse(BodyId id, Impulse impulse) {
        auto& body = find_mutable(id);
        if (body.motion_ == MotionType::static_body) {
            return;
        }
        const auto velocity_delta = scaled(
            impulse.newton_seconds(),
            body.inverse_mass());
        body.velocity_ = LinearVelocity{add(
            body.velocity_.metres_per_second(),
            velocity_delta)};
    }

    void integrate(float time_step) {
        const auto gravity = configuration.gravity.metres_per_second_squared();
        for (auto& body : bodies) {
            if (body.motion_ == MotionType::static_body) {
                continue;
            }
            
            Vec3 a_aero = {0.0F, 0.0F, 0.0F};
            Vec3 v_body = body.velocity_.metres_per_second();
            Vec3 v_wind = wind_.metres_per_second();
            Vec3 v_air = subtract(v_body, v_wind);
            float v_air_len = length(v_air);
            
            if (v_air_len > 0.0F && body.inverse_mass() > 0.0F) {
                // Approximate golf ball constants
                const float rho = 1.225F;
                const float r = 0.02135F;
                const float A = 3.14159265F * r * r;
                const float mass = body.mass_.kilograms();
                
                // 1. Drag
                const float C_d = 0.3F;
                float drag_accel_factor = -0.5F * rho * C_d * A * v_air_len / mass;
                a_aero = add(a_aero, scaled(v_air, drag_accel_factor));
                
                // 2. Magnus Effect (Lift)
                Vec3 omega = body.angular_velocity_.radians_per_second();
                Vec3 lift_dir = cross(omega, v_air);
                float lift_len = length(lift_dir);
                
                if (lift_len > 0.0F) {
                    // Simple constant lift coefficient for the vertical slice proof
                    const float C_l = 0.2F;
                    // F_lift = 0.5 * rho * C_l * A * |v|^2 * normalize(lift_dir)
                    float lift_accel_factor = (0.5F * rho * C_l * A * (v_air_len * v_air_len)) / (mass * lift_len);
                    a_aero = add(a_aero, scaled(lift_dir, lift_accel_factor));
                }
            }

            const auto total_acceleration = add(gravity, a_aero);
            
            const auto velocity = add(
                body.velocity_.metres_per_second(),
                scaled(total_acceleration, time_step));
            const auto position = add(
                body.position_.metres(),
                scaled(velocity, time_step));
            body.velocity_ = LinearVelocity{velocity};
            body.position_ = Position{position};
        }
    }

    void resolve(
        RigidBody& first,
        RigidBody& second,
        const ContactManifold& contact) {
        const auto first_inverse_mass = first.inverse_mass();
        const auto second_inverse_mass = second.inverse_mass();
        const auto total_inverse_mass = first_inverse_mass + second_inverse_mass;
        if (total_inverse_mass <= 0.0F) {
            return;
        }

        const auto correction = scaled(
            contact.normal(),
            contact.penetration().metres() / total_inverse_mass);
        first.position_ = Position{subtract(
            first.position_.metres(), scaled(correction, first_inverse_mass))};
        second.position_ = Position{add(
            second.position_.metres(), scaled(correction, second_inverse_mass))};

        const auto relative_velocity = subtract(
            second.velocity_.metres_per_second(),
            first.velocity_.metres_per_second());
        const auto separating_velocity = dot(relative_velocity, contact.normal());
        
        // Normal impulse (bouncing)
        if (separating_velocity < 0.0F) {
            const auto restitution = std::min(first.restitution_, second.restitution_);
            // If hitting sand heavily, plug the ball (drop restitution to 0)
            float final_restitution = restitution;
            if (first.material().surface == TerrainSurface::Sand || second.material().surface == TerrainSurface::Sand) {
                if (separating_velocity < -5.0F) final_restitution = 0.0F;
            }
            
            const auto impulse_magnitude =
                -(1.0F + final_restitution) * separating_velocity / total_inverse_mass;
            const auto impulse = scaled(contact.normal(), impulse_magnitude);
            first.velocity_ = LinearVelocity{subtract(
                first.velocity_.metres_per_second(), scaled(impulse, first_inverse_mass))};
            second.velocity_ = LinearVelocity{add(
                second.velocity_.metres_per_second(), scaled(impulse, second_inverse_mass))};
        }
        
        // Friction and Rolling Resistance (Tangential)
        // Recalculate relative velocity after normal impulse
        const auto new_relative_velocity = subtract(
            second.velocity_.metres_per_second(),
            first.velocity_.metres_per_second());
        const auto v_normal = dot(new_relative_velocity, contact.normal());
        const auto v_tangent = subtract(new_relative_velocity, scaled(contact.normal(), v_normal));
        const float v_tangent_len = length(v_tangent);
        
        if (v_tangent_len > 0.001F) {
            Vec3 tangent_dir = scaled(v_tangent, 1.0F / v_tangent_len);
            
            // Average dynamic friction
            float mu = (first.material().dynamic_friction + second.material().dynamic_friction) * 0.5F;
            
            // Rolling resistance modifier
            float rolling_res = std::max(first.material().rolling_resistance, second.material().rolling_resistance);
            float sand_mod = std::max(first.material().sand_topdressing, second.material().sand_topdressing);
            mu += rolling_res + (sand_mod * 0.2F);
            
            // Bumpiness (Bobbles) adds micro-deflections to the tangent
            float bumpiness = std::max(first.material().bumpiness, second.material().bumpiness);
            if (bumpiness > 0.0F) {
                // Slower putts are deflected more severely relative to their velocity
                float deflection_mag = bumpiness * 0.05F / (v_tangent_len + 0.1F);
                // Pseudo-random deflection based on position (deterministic)
                float noise_z = std::sin(first.position_.metres().x * 100.0F) * deflection_mag;
                tangent_dir = add(tangent_dir, {0.0F, 0.0F, noise_z});
            }
            
            float friction_impulse_mag = std::min(v_tangent_len / total_inverse_mass, mu * 9.81F * 0.016F);
            const auto friction_impulse = scaled(tangent_dir, friction_impulse_mag);
            
            first.velocity_ = LinearVelocity{subtract(
                first.velocity_.metres_per_second(), scaled(friction_impulse, first_inverse_mass))};
            second.velocity_ = LinearVelocity{add(
                second.velocity_.metres_per_second(), scaled(friction_impulse, second_inverse_mass))};
        }
    }

    void step(float time_step) {
        integrate(time_step);
        collisions.clear();
        for (std::size_t first_index = 0; first_index < bodies.size(); ++first_index) {
            for (std::size_t second_index = first_index + 1;
                 second_index < bodies.size();
                 ++second_index) {
                auto& first = bodies[first_index];
                auto& second = bodies[second_index];
                if (first.motion() == MotionType::static_body &&
                    second.motion() == MotionType::static_body) {
                    continue;
                }
                auto contact = collision_detector->detect(first, second);
                if (contact) {
                    collisions.push_back({
                        .first = first.id(),
                        .second = second.id(),
                        .contact = *contact,
                    });
                    resolve(first, second, *contact);
                }
            }
        }
    }

    PhysicsConfiguration configuration;
    std::unique_ptr<CollisionDetector> collision_detector;
    std::deque<RigidBody> bodies;
    std::vector<Collision> collisions;
    LinearVelocity wind_{};
    double accumulated_time{};
    std::uint64_t next_id{1};
};

PhysicsWorld::PhysicsWorld() : PhysicsWorld{PhysicsConfiguration{}} {}

PhysicsWorld::PhysicsWorld(PhysicsConfiguration configuration)
    : PhysicsWorld{
          configuration,
          std::make_unique<DiscreteCollisionDetector>()} {}

PhysicsWorld::PhysicsWorld(
    PhysicsConfiguration configuration,
    std::unique_ptr<CollisionDetector> collision_detector)
    : impl_{std::make_unique<PhysicsWorldImpl>(
          configuration,
          std::move(collision_detector))} {}

PhysicsWorld::~PhysicsWorld() = default;
PhysicsWorld::PhysicsWorld(PhysicsWorld&&) noexcept = default;
PhysicsWorld& PhysicsWorld::operator=(PhysicsWorld&&) noexcept = default;

void PhysicsWorld::set_wind(LinearVelocity wind) {
    impl_->wind_ = wind;
}

BodyId PhysicsWorld::add_body(RigidBodyDefinition definition) {
    if (impl_->next_id == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{"Physics body identifier capacity exhausted"};
    }
    const BodyId id{impl_->next_id++};
    impl_->bodies.emplace_back(id, std::move(definition));
    return id;
}

bool PhysicsWorld::remove_body(BodyId id) {
    const auto found = std::ranges::find(impl_->bodies, id, &RigidBody::id);
    if (found == impl_->bodies.end()) {
        return false;
    }
    impl_->bodies.erase(found);
    impl_->collisions.clear();
    return true;
}

bool PhysicsWorld::contains(BodyId id) const noexcept {
    return std::ranges::find(impl_->bodies, id, &RigidBody::id) != impl_->bodies.end();
}

const RigidBody& PhysicsWorld::body(BodyId id) const {
    return impl_->find(id);
}

std::span<const Collision> PhysicsWorld::collisions() const noexcept {
    return impl_->collisions;
}


void PhysicsWorld::set_velocity(BodyId id, LinearVelocity linear, AngularVelocity angular) {
    impl_->set_velocity(id, linear, angular);
}

void PhysicsWorld::apply_impulse(BodyId id, Impulse impulse) {
    impl_->apply_impulse(id, impulse);
}

void PhysicsWorld::simulate(Duration elapsed_time) {
    const auto fixed_time_step = impl_->configuration.fixed_time_step.seconds();
    const auto fixed_time_step_value = static_cast<double>(fixed_time_step);
    impl_->accumulated_time += static_cast<double>(elapsed_time.seconds());
    std::size_t completed_steps{};
    while (impl_->accumulated_time >= fixed_time_step_value &&
           completed_steps < impl_->configuration.maximum_substeps) {
        impl_->step(fixed_time_step);
        impl_->accumulated_time -= fixed_time_step_value;
        ++completed_steps;
    }
    if (completed_steps == impl_->configuration.maximum_substeps &&
        impl_->accumulated_time >= fixed_time_step_value) {
        impl_->accumulated_time = std::fmod(
            impl_->accumulated_time,
            fixed_time_step_value);
    }
}

} // namespace mgv::physics
