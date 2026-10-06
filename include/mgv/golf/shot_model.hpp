#pragma once

#include "mgv/golf/shot.hpp"
#include "mgv/physics/units.hpp"

namespace mgv::golf {

struct BallLaunch final {
    physics::LinearVelocity linear_velocity;
    physics::AngularVelocity angular_velocity;
};

class ShotModel {
public:
    virtual ~ShotModel() = default;
    ShotModel(const ShotModel&) = delete;
    ShotModel& operator=(const ShotModel&) = delete;

    [[nodiscard]] virtual BallLaunch resolve(const Shot& shot) const = 0;

protected:
    ShotModel() = default;
};

struct StandardShotModelConfiguration final {
    float putting_smash_factor{1.5F};
    physics::Length ball_radius{0.021335F};
};

class StandardShotModel final : public ShotModel {
public:
    StandardShotModel();
    explicit StandardShotModel(StandardShotModelConfiguration configuration);

    [[nodiscard]] BallLaunch resolve(const Shot& shot) const override;

private:
    StandardShotModelConfiguration configuration_;
};

} // namespace mgv::golf
