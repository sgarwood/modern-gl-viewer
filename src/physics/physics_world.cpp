#include <iostream>
#include "mgv/physics/physics_world.hpp"

#include "mgv/physics/aerodynamics.hpp"

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

/// Below this relative surface speed a contact counts as rolling rather than
/// sliding, in m/s.
inline constexpr float slip_tolerance = 1.0e-3F;

} // namespace

class PhysicsWorldImpl final {
public:
    PhysicsWorldImpl(
        PhysicsConfiguration value,
        std::unique_ptr<CollisionDetector> detector)
        : configuration{value},
          collision_detector{std::move(detector)},
          air_density_{value.air_density} {
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
        rouse(body);
    }

    void set_position(BodyId id, Position position) {
        auto& body = find_mutable(id);
        body.position_ = position;
        rouse(body);
    }

    void set_static_collider(BodyId id, Collider collider, Position position) {
        auto& body = find_mutable(id);
        if (body.motion_ != MotionType::static_body) {
            throw std::logic_error{"Only a static body's collider may be replaced"};
        }
        body.collider_ = std::move(collider);
        body.position_ = position;
        // The ground has moved out from under whatever was lying on it. The
        // streamed collision patch is rebuilt from the same analytic surface,
        // so the heights should agree to a fraction of a millimetre -- but
        // "should" is doing a lot of work in that sentence, and a ball left
        // asleep over a patch that disagreed would hang in the air.
        for (auto& sleeper : bodies) {
            if (sleeper.motion_ == MotionType::dynamic) {
                rouse(sleeper);
            }
        }
    }

    
    [[nodiscard]] std::optional<RaycastHit> raycast(Position origin, Vec3 direction) const {
        std::optional<RaycastHit> closest_hit;
        for (const auto& body : bodies) {
            auto hit = raycast_body(body, origin, direction);
            if (hit) {
                if (!closest_hit || hit->distance < closest_hit->distance) {
                    closest_hit = hit;
                }
            }
        }
        return closest_hit;
    }

    [[nodiscard]] std::optional<RaycastHit> raycast_body(const RigidBody& body, Position origin, Vec3 direction) const {
        auto normalize_v = [](Vec3 v) {
            float len = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
            if (len < 1e-6f) return Vec3{0,0,0};
            return Vec3{v.x/len, v.y/len, v.z/len};
        };
        auto subtract_v = [](Vec3 a, Vec3 b) { return Vec3{a.x-b.x, a.y-b.y, a.z-b.z}; };
        auto add_v = [](Vec3 a, Vec3 b) { return Vec3{a.x+b.x, a.y+b.y, a.z+b.z}; };
        auto scaled_v = [](Vec3 a, float s) { return Vec3{a.x*s, a.y*s, a.z*s}; };
        auto dot_v = [](Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; };

        Vec3 o = origin.metres();
        Vec3 d = normalize_v(direction);
        Vec3 pos = body.position().metres();

        if (std::holds_alternative<SphereCollider>(body.collider().shape())) {
            float r = std::get<SphereCollider>(body.collider().shape()).radius.metres();
            Vec3 oc = subtract_v(o, pos);
            float a = dot_v(d, d);
            float b = 2.0F * dot_v(oc, d);
            float c = dot_v(oc, oc) - r * r;
            float discriminant = b * b - 4 * a * c;
            if (discriminant > 0.0F) {
                float t1 = (-b - std::sqrt(discriminant)) / (2.0F * a);
                if (t1 > 0.0F) {
                    Vec3 hit_p = add_v(o, scaled_v(d, t1));
                    Vec3 normal = normalize_v(subtract_v(hit_p, pos));
                    return RaycastHit{body.id(), Position{hit_p}, normal, t1};
                }
            }
        }
        
        if (std::holds_alternative<BoxCollider>(body.collider().shape())) {
            Vec3 extents = std::get<BoxCollider>(body.collider().shape()).half_extents.metres();
            Vec3 min = subtract_v(pos, extents);
            Vec3 max = add_v(pos, extents);
            
            float t1 = (min.x - o.x) / d.x;
            float t2 = (max.x - o.x) / d.x;
            float t3 = (min.y - o.y) / d.y;
            float t4 = (max.y - o.y) / d.y;
            float t5 = (min.z - o.z) / d.z;
            float t6 = (max.z - o.z) / d.z;
            
            float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
            float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));
            
            if (tmax >= 0.0F && tmin <= tmax) {
                if (tmin > 0.0F) {
                    Vec3 hit_p = add_v(o, scaled_v(d, tmin));
                    Vec3 normal = {0, 1, 0};
                    return RaycastHit{body.id(), Position{hit_p}, normal, tmin};
                }
            }
        }
        if (std::holds_alternative<CapsuleCollider>(body.collider().shape())) {
            const auto& capsule = std::get<CapsuleCollider>(body.collider().shape());
            const float r = capsule.radius.metres();
            const float h = capsule.half_height.metres();
            // The capsule stands on Y, so this is a circle in plan. The caps
            // are left out: on a trunk they are underground and inside the
            // canopy, and a range finder that reported the distance to a
            // hemisphere buried under a tree would be worse than one that
            // reported nothing.
            const float ox = o.x - pos.x;
            const float oz = o.z - pos.z;
            const float a = d.x * d.x + d.z * d.z;
            if (a > 1e-6F) {
                const float b = 2.0F * (ox * d.x + oz * d.z);
                const float c = ox * ox + oz * oz - r * r;
                const float discriminant = b * b - 4.0F * a * c;
                if (discriminant > 0.0F) {
                    const float t1 = (-b - std::sqrt(discriminant)) / (2.0F * a);
                    if (t1 > 0.0F) {
                        const Vec3 hit_p = add_v(o, scaled_v(d, t1));
                        if (hit_p.y >= pos.y - h && hit_p.y <= pos.y + h) {
                            const Vec3 normal = normalize_v(
                                Vec3{hit_p.x - pos.x, 0.0F, hit_p.z - pos.z});
                            return RaycastHit{body.id(), Position{hit_p}, normal, t1};
                        }
                    }
                }
            }
        }

        if (std::holds_alternative<HeightmapCollider>(body.collider().shape())) {
            const auto& hm = std::get<HeightmapCollider>(body.collider().shape());
            float min_h = 0.0f, max_h = 0.0f;
            for (float h : hm.heights) {
                if (h < min_h) min_h = h;
                if (h > max_h) max_h = h;
            }
            float half_w = (static_cast<float>(hm.width) * hm.scale_x) * 0.5f;
            float half_d = (static_cast<float>(hm.depth) * hm.scale_z) * 0.5f;
            Vec3 min = subtract_v(pos, Vec3{half_w, -min_h, half_d});
            Vec3 max = add_v(pos, Vec3{half_w, max_h, half_d});
            
            float t1 = (min.x - o.x) / d.x;
            float t2 = (max.x - o.x) / d.x;
            float t3 = (min.y - o.y) / d.y;
            float t4 = (max.y - o.y) / d.y;
            float t5 = (min.z - o.z) / d.z;
            float t6 = (max.z - o.z) / d.z;
            
            float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
            float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));
            
            if (tmax >= 0.0F && tmin <= tmax && tmin > 0.0F) {
                Vec3 hit_p = add_v(o, scaled_v(d, tmin));
                return RaycastHit{body.id(), Position{hit_p}, Vec3{0,1,0}, tmin};
            }
        }
        return std::nullopt;
    }

    void apply_impulse(BodyId id, Impulse impulse) {
        auto& body = find_mutable(id);
        rouse(body);
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
            if (body.motion_ == MotionType::static_body || body.sleeping_) {
                continue;
            }
            
            Vec3 a_aero = {0.0F, 0.0F, 0.0F};
            Vec3 v_body = body.velocity_.metres_per_second();
            Vec3 v_wind = wind_.metres_per_second();
            
            // Check Foliage Wakes
            bool inside_foliage = false;
            float current_density = 0.0f;
            for (const auto& vol : foliage_volumes_) {
                // Check if in wake (simplified: if we are downwind (Z > vol.max.z) and within X/Y bounds)
                // For this slice, just assume wind blows along Z axis.
                // Wake zone extends 20 meters behind the tree.
                if (body.position_.metres().x >= vol.min.x && body.position_.metres().x <= vol.max.x &&
                    body.position_.metres().y >= vol.min.y && body.position_.metres().y <= vol.max.y) {
                    
                    if (body.position_.metres().z >= vol.min.z && body.position_.metres().z <= vol.max.z) {
                        inside_foliage = true;
                        current_density = vol.density;
                        // Inside tree: wind drops to 20%
                        v_wind = scaled(v_wind, 0.2f);
                    } else if (body.position_.metres().z > vol.max.z && body.position_.metres().z < vol.max.z + 20.0f) {
                        // Wake recovery
                        float dist = body.position_.metres().z - vol.max.z;
                        float recovery = 0.2f + 0.8f * (dist / 20.0f);
                        v_wind = scaled(v_wind, recovery);
                        
                        // Add turbulence (von Karman vortex)
                        v_wind.x += std::sin(dist * 2.0f) * 1.5f;
                    }
                }
            }
            
            if (inside_foliage && length(v_body) > 1.0f) {
                // Probabilistic branch collision
                float chance = current_density * length(v_body) * 0.01f;
                // Deterministic pseudo-random based on position
                float roll = std::abs(std::sin(body.position_.metres().z * 100.0f));
                if (roll < chance) {
                    // Deflect!
                    body.velocity_ = LinearVelocity{Vec3{v_body.x * 0.2f, -std::abs(v_body.y) * 0.5f, v_body.z * 0.2f}};
                    std::cout << "[PHYSICS] THWACK! Ball hit a tree branch!" << std::endl;
                    v_body = body.velocity_.metres_per_second();
                }
            }
            
            Vec3 v_air = subtract(v_body, v_wind);
            float v_air_len = length(v_air);
            
            const auto radius = sphere_radius(body.collider());
            if (v_air_len > 0.0F && body.inverse_mass() > 0.0F && radius) {
                const float rho = air_density_;
                // The body's own radius, not a golf ball's. Four things
                // downstream of this -- the reference area, the Reynolds
                // number, the moment of inertia and the lever arm to a
                // contact -- have to be computed from one radius, or a ball
                // ends up rolling at a speed its own spin contradicts.
                const float r = *radius;
                const float A = 3.14159265F * r * r;
                const float mass = body.mass_.kilograms();

                const Vec3 omega = body.angular_velocity_.radians_per_second();
                // Magnus acts along omega x v, and the length of that product
                // also gives the spin across the line of flight -- the only
                // part of the spin that makes lift. Spin about the direction
                // of travel is a rifle spin and does nothing here.
                const Vec3 lift_axis = cross(omega, v_air);
                const float lift_axis_length = length(lift_axis);
                const float ratio =
                    spin_ratio(v_air_len, lift_axis_length / v_air_len, r);
                const float reynolds = reynolds_number(v_air_len, r, rho);

                // 1. Drag
                const float drag_factor =
                    -0.5F * rho * drag_coefficient(ratio, reynolds) * A * v_air_len / mass;
                a_aero = add(a_aero, scaled(v_air, drag_factor));

                // 2. Magnus effect
                if (lift_axis_length > 0.0F) {
                    const float lift = 0.5F * rho * lift_coefficient(ratio) * A *
                        v_air_len * v_air_len / mass;
                    a_aero = add(a_aero, scaled(lift_axis, lift / lift_axis_length));
                }

                // 3. Spin decay. A rate rather than a moment, so that it
                // vanishes with the spin instead of carrying a lightly
                // spinning ball through zero; solved implicitly, so that it
                // cannot do so however coarse the clock is.
                const float retained =
                    1.0F / (1.0F + spin_decay_rate(v_air_len, r, rho) * time_step);
                body.angular_velocity_ = AngularVelocity{scaled(omega, retained)};
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
        const ContactManifold& contact,
        float time_step) {
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
        
        // Friction, and the spin it trades against.
        //
        // What matters at a contact is the velocity of the material at the
        // contact point, not of the body's centre. A ball with backspin is
        // dragging its underside forward across the turf *faster* than it is
        // travelling, and that is the whole reason a struck approach shot
        // stops where it lands. Solved on centre velocities alone, spin is a
        // decoration: it bends the flight and then has no say in what
        // happens on the ground.
        const auto first_radius = sphere_radius(first.collider());
        const auto second_radius = sphere_radius(second.collider());
        // The normal runs from the first body to the second, so the contact
        // lies a radius along it from the first centre and a radius back
        // from the second.
        const auto first_arm = scaled(contact.normal(), first_radius.value_or(0.0F));
        const auto second_arm = scaled(contact.normal(), -second_radius.value_or(0.0F));
        const auto surface_velocity = [](const RigidBody& body, Vec3 arm) {
            return add(
                body.velocity_.metres_per_second(),
                cross(body.angular_velocity_.radians_per_second(), arm));
        };
        const auto contact_velocity = subtract(
            surface_velocity(second, second_arm), surface_velocity(first, first_arm));
        const auto contact_normal_speed = dot(contact_velocity, contact.normal());
        const auto slip = subtract(
            contact_velocity, scaled(contact.normal(), contact_normal_speed));
        const auto slip_speed = length(slip);

        // Friction and rolling resistance are no longer summed into one
        // coefficient. They act at different times -- Coulomb friction while
        // the contact slides, rolling resistance once it does not -- and
        // adding them made a putt and a landing drive the same event.
        float mu = (first.material().dynamic_friction + second.material().dynamic_friction) * 0.5F;
        float rolling_resistance =
            std::max(first.material().rolling_resistance, second.material().rolling_resistance);
        const auto sand =
            std::max(first.material().sand_topdressing, second.material().sand_topdressing);

        // Water lubricates a sliding contact and thickens a rolling one, so
        // it pulls the two in opposite directions.
        mu *= 1.0F - (wetness_ * 0.4F);
        rolling_resistance = rolling_resistance * (1.0F + (wetness_ * 0.6F)) + (sand * 0.2F);

        // The load the surface reacts, as an impulse over this step: the
        // weight it carries, times the step. Expressing the Coulomb limit as
        // mu * g * dt alone gives a velocity rather than an impulse, and only
        // happens to come out near the right number for a body that weighs
        // what a golf ball weighs.
        const auto load_impulse = 9.81F * time_step / total_inverse_mass;

        bool rolling = slip_speed <= slip_tolerance;
        if (!rolling) {
            const auto tangent_dir = scaled(slip, 1.0F / slip_speed);
            // The effective mass at the contact for a tangential impulse.
            // The inertia terms are what split the slip between the centre
            // and the spin -- five sevenths and two sevenths for a sphere --
            // and leaving them out is what made spin inert on landing.
            const auto angular_term = [&tangent_dir](const RigidBody& body, Vec3 arm) {
                const auto moment = cross(arm, tangent_dir);
                return body.inverse_inertia() * dot(moment, moment);
            };
            const auto tangential_inverse_mass = total_inverse_mass +
                angular_term(first, first_arm) + angular_term(second, second_arm);
            if (tangential_inverse_mass > 0.0F) {
                const auto arresting_impulse = slip_speed / tangential_inverse_mass;
                const auto limit = mu * load_impulse;
                const auto impulse = scaled(tangent_dir, std::min(arresting_impulse, limit));

                // tangent_dir is the second body's surface motion relative to
                // the first, so friction drags the first along it and the
                // second against it. Reversing these two drives the contact
                // instead of retarding it: a ball sliding across level ground
                // accelerates, and one resting on any slope runs away without
                // bound.
                first.velocity_ = LinearVelocity{add(
                    first.velocity_.metres_per_second(), scaled(impulse, first_inverse_mass))};
                second.velocity_ = LinearVelocity{subtract(
                    second.velocity_.metres_per_second(), scaled(impulse, second_inverse_mass))};
                first.angular_velocity_ = AngularVelocity{add(
                    first.angular_velocity_.radians_per_second(),
                    scaled(cross(first_arm, impulse), first.inverse_inertia()))};
                second.angular_velocity_ = AngularVelocity{subtract(
                    second.angular_velocity_.radians_per_second(),
                    scaled(cross(second_arm, impulse), second.inverse_inertia()))};

                // The surface could absorb the whole slip, so by the end of
                // this step the contact is rolling and resistance takes over.
                rolling = arresting_impulse <= limit;
            }
        }

        if (!rolling || rolling_resistance <= 0.0F) {
            return;
        }

        // Rolling resistance: a couple opposing the spin, with the static
        // friction that maintains the roll passing the deceleration on to the
        // centre. For a solid sphere that works out at five sevenths of
        // c * g, which is what lets a green's speed be stated in feet and
        // checked against a stimpmeter rather than against this solver.
        //
        // Applied to the one dynamic sphere in the contact. On a golf course
        // that is the ball against the ground, and two dynamic spheres
        // rolling on each other is not a thing this game has.
        RigidBody* ball = nullptr;
        Vec3 arm{};
        float ball_radius = 0.0F;
        if (first.inverse_inertia() > 0.0F && second_inverse_mass <= 0.0F) {
            ball = &first;
            arm = first_arm;
            ball_radius = *first_radius;
        } else if (second.inverse_inertia() > 0.0F && first_inverse_mass <= 0.0F) {
            ball = &second;
            arm = second_arm;
            ball_radius = *second_radius;
        }
        if (ball == nullptr) {
            return;
        }

        const auto centre_velocity = ball->velocity_.metres_per_second();
        const auto normal_speed = dot(centre_velocity, contact.normal());
        const auto rolling_velocity = subtract(
            centre_velocity, scaled(contact.normal(), normal_speed));
        auto rolling_speed = length(rolling_velocity);
        if (rolling_speed <= 0.0F) {
            return;
        }
        auto direction = scaled(rolling_velocity, 1.0F / rolling_speed);

        // A bobbled surface deflects a roll, and the slower the roll the more
        // of the deflection shows -- which is why a putt dying at the hole
        // wanders off its line and one hit firmly holds it. Deterministic in
        // the ball's position, so a replayed shot replays.
        const auto bumpiness = std::max(first.material().bumpiness, second.material().bumpiness);
        if (bumpiness > 0.0F) {
            const auto across = cross(contact.normal(), direction);
            const auto deflection = bumpiness * 0.05F / (rolling_speed + 0.1F) *
                std::sin(ball->position_.metres().x * 100.0F);
            direction = add(direction, scaled(across, deflection));
            const auto deflected_length = length(direction);
            if (deflected_length > 0.0F) {
                direction = scaled(direction, 1.0F / deflected_length);
            }
        }

        // The share of the resistance that reaches the centre: the inertia
        // term over the inertia and mass terms together, five sevenths for a
        // solid sphere.
        const auto inertia_term = ball->inverse_inertia() * ball_radius * ball_radius;
        const auto reaching_centre = inertia_term / (ball->inverse_mass() + inertia_term);
        rolling_speed -= std::min(
            rolling_speed, rolling_resistance * 9.81F * reaching_centre * time_step);

        const auto slowed = scaled(direction, rolling_speed);
        ball->velocity_ = LinearVelocity{add(scaled(contact.normal(), normal_speed), slowed)};
        // Spin follows the roll. A rolling ball's spin is not a free
        // variable, and leaving it where it was while the centre slows
        // re-introduces a slip for friction to undo next step. Spin about the
        // normal is left alone: a ball can rotate like a top while it rolls,
        // and that component is what cut spin on a putt becomes.
        const auto spin_about_normal = dot(ball->angular_velocity_.radians_per_second(), contact.normal());
        ball->angular_velocity_ = AngularVelocity{add(
            scaled(contact.normal(), spin_about_normal),
            scaled(cross(slowed, arm), 1.0F / (ball_radius * ball_radius)))};
    }

    /// Whether this body is taking part in the simulation this step.
    [[nodiscard]] static bool integrating(const RigidBody& body) {
        return body.motion_ == MotionType::dynamic && !body.sleeping_;
    }

    static void rouse(RigidBody& body) {
        body.sleeping_ = false;
        body.still_seconds_ = 0.0F;
    }

    void update_sleep(float time_step) {
        for (auto& body : bodies) {
            if (body.motion_ == MotionType::static_body || body.sleeping_) {
                continue;
            }
            const auto still =
                length(body.velocity_.metres_per_second()) <=
                    configuration.sleep_linear_speed &&
                length(body.angular_velocity_.radians_per_second()) <=
                    configuration.sleep_angular_speed;
            if (!still) {
                body.still_seconds_ = 0.0F;
                continue;
            }
            body.still_seconds_ += time_step;
            if (body.still_seconds_ >= configuration.sleep_delay.seconds()) {
                body.sleeping_ = true;
                // Zeroed rather than left as they were: whatever remains is
                // under the threshold by definition, and a round asking
                // whether the ball has stopped deserves a straight answer.
                body.velocity_ = LinearVelocity{};
                body.angular_velocity_ = AngularVelocity{};
            }
        }
    }

    void step(float time_step) {
        integrate(time_step);
        collisions.clear();

        // Only the pairs with a dynamic body in them. Two pieces of scenery
        // cannot move relative to one another, so most of a course's pairs
        // can never produce a contact: ninety trees make four thousand pairs
        // and all but ninety of them are tree against tree.
        //
        // Walking them all anyway cost 5% of a 60 Hz budget with nothing
        // happening, and 55% at three hundred trees, because the count is
        // quadratic in a number that only grows as a course gains scenery.
        //
        // This is not a spatial broad phase -- a dynamic body is still
        // compared against every static one -- but it removes the whole of
        // the quadratic term, and one ball against a course's furniture is
        // linear.
        dynamic_indices.clear();
        for (std::size_t index = 0; index < bodies.size(); ++index) {
            if (bodies[index].motion() == MotionType::dynamic) {
                dynamic_indices.push_back(index);
            }
        }

        for (const auto dynamic_index : dynamic_indices) {
            for (std::size_t other = 0; other < bodies.size(); ++other) {
                if (other == dynamic_index) {
                    continue;
                }
                // A pair of dynamic bodies belongs to the lower-numbered of
                // the two, so that it is resolved once rather than twice.
                if (bodies[other].motion() == MotionType::dynamic &&
                    other < dynamic_index) {
                    continue;
                }
                // Kept in index order, because the contact normal runs from
                // the first body to the second and half the solver depends
                // on knowing which is which.
                auto& first = bodies[std::min(dynamic_index, other)];
                auto& second = bodies[std::max(dynamic_index, other)];
                auto contact = collision_detector->detect(first, second);
                if (contact) {
                    collisions.push_back({
                        .first = first.id(),
                        .second = second.id(),
                        .contact = *contact,
                    });
                    // Something moving has arrived against something that had
                    // stopped, so the sleeper is part of the simulation again.
                    if (integrating(first) && second.sleeping_) {
                        rouse(second);
                    } else if (integrating(second) && first.sleeping_) {
                        rouse(first);
                    }
                    // With neither body integrating there is nothing to
                    // resolve, and resolving anyway is exactly the creep this
                    // is here to stop: the correction pushes a resting ball
                    // along the contact normal, which on a slope is downhill,
                    // every step for as long as it lies there.
                    if (integrating(first) || integrating(second)) {
                        resolve(first, second, *contact, time_step);
                    }
                }
            }
        }
        update_sleep(time_step);
    }

    PhysicsConfiguration configuration;
    std::unique_ptr<CollisionDetector> collision_detector;
    std::deque<RigidBody> bodies;
    std::vector<Collision> collisions;
    /// Indices of the dynamic bodies, rebuilt each step. A member rather
    /// than a local so that a step does not allocate.
    std::vector<std::size_t> dynamic_indices;
    std::vector<FoliageVolume> foliage_volumes_;
    LinearVelocity wind_{};
    float air_density_{};
    float wetness_{0.0f};
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



void PhysicsWorld::set_wetness(float wetness) {
    impl_->wetness_ = wetness;
}

void PhysicsWorld::set_air_density(float rho) {
    impl_->air_density_ = rho;
}


void PhysicsWorld::add_foliage_volume(FoliageVolume volume) {
    impl_->foliage_volumes_.push_back(volume);
}

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

void PhysicsWorld::set_position(BodyId id, Position position) {
    impl_->set_position(id, position);
}

void PhysicsWorld::set_static_collider(BodyId id, Collider collider, Position position) {
    impl_->set_static_collider(id, std::move(collider), position);
}


std::optional<RaycastHit> PhysicsWorld::raycast(Position origin, Vec3 direction) const {
    return impl_->raycast(origin, direction);
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
