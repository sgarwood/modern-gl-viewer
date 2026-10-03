#pragma once

#include "mgv/physics/rigid_body.hpp"

#include <optional>

namespace mgv::physics {

class ContactManifold final {
public:
    ContactManifold(Vec3 normal, Length penetration);

    [[nodiscard]] const Vec3& normal() const noexcept;
    [[nodiscard]] const Length& penetration() const noexcept;

private:
    Vec3 normal_;
    Length penetration_;
};

struct Collision final {
    BodyId first;
    BodyId second;
    ContactManifold contact;
};

class CollisionDetector {
public:
    virtual ~CollisionDetector() = default;
    CollisionDetector(const CollisionDetector&) = delete;
    CollisionDetector& operator=(const CollisionDetector&) = delete;

    [[nodiscard]] virtual std::optional<ContactManifold> detect(
        const RigidBody& first,
        const RigidBody& second) const = 0;

protected:
    CollisionDetector() = default;
};

class DiscreteCollisionDetector final : public CollisionDetector {
public:
    [[nodiscard]] std::optional<ContactManifold> detect(
        const RigidBody& first,
        const RigidBody& second) const override;
};

} // namespace mgv::physics
