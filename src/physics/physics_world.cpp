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
            const auto velocity = add(
                body.velocity_.metres_per_second(),
                scaled(gravity, time_step));
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
        if (separating_velocity >= 0.0F) {
            return;
        }
        const auto restitution = std::min(first.restitution_, second.restitution_);
        const auto impulse_magnitude =
            -(1.0F + restitution) * separating_velocity / total_inverse_mass;
        const auto impulse = scaled(contact.normal(), impulse_magnitude);
        first.velocity_ = LinearVelocity{subtract(
            first.velocity_.metres_per_second(), scaled(impulse, first_inverse_mass))};
        second.velocity_ = LinearVelocity{add(
            second.velocity_.metres_per_second(), scaled(impulse, second_inverse_mass))};
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

BodyId PhysicsWorld::add_body(RigidBodyDefinition definition) {
    if (impl_->next_id == std::numeric_limits<std::uint64_t>::max()) {
        throw std::overflow_error{"Physics body identifier capacity exhausted"};
    }
    const BodyId id{impl_->next_id++};
    impl_->bodies.emplace_back(id, std::move(definition));
    return id;
}

const RigidBody& PhysicsWorld::body(BodyId id) const {
    return impl_->find(id);
}

std::span<const Collision> PhysicsWorld::collisions() const noexcept {
    return impl_->collisions;
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
