#include "mgv/physics/units.hpp"

#include <cmath>
#include <stdexcept>

namespace mgv::physics {
namespace {

void require_finite(float value, const char* message) {
    if (!std::isfinite(value)) {
        throw std::invalid_argument{message};
    }
}

void require_finite(Vec3 value, const char* message) {
    require_finite(value.x, message);
    require_finite(value.y, message);
    require_finite(value.z, message);
}

} // namespace

Duration::Duration(float seconds) : seconds_{seconds} {
    require_finite(seconds_, "Duration must be finite");
    if (seconds_ < 0.0F) {
        throw std::invalid_argument{"Duration must not be negative"};
    }
}

float Duration::seconds() const noexcept { return seconds_; }

Length::Length(float metres) : metres_{metres} {
    require_finite(metres_, "Length must be finite");
    if (metres_ < 0.0F) {
        throw std::invalid_argument{"Length must not be negative"};
    }
}

float Length::metres() const noexcept { return metres_; }

Mass::Mass(float kilograms) : kilograms_{kilograms} {
    require_finite(kilograms_, "Mass must be finite");
    if (kilograms_ <= 0.0F) {
        throw std::invalid_argument{"Mass must be positive"};
    }
}

float Mass::kilograms() const noexcept { return kilograms_; }

Position::Position(Vec3 metres) : metres_{metres} {
    require_finite(metres_, "Position must be finite");
}

const Vec3& Position::metres() const noexcept { return metres_; }

Dimensions::Dimensions(Vec3 metres) : metres_{metres} {
    require_finite(metres_, "Dimensions must be finite");
}

const Vec3& Dimensions::metres() const noexcept { return metres_; }

LinearVelocity::LinearVelocity(Vec3 metres_per_second)
    : metres_per_second_{metres_per_second} {
    require_finite(metres_per_second_, "Linear velocity must be finite");
}

const Vec3& LinearVelocity::metres_per_second() const noexcept {
    return metres_per_second_;
}

Acceleration::Acceleration(Vec3 metres_per_second_squared)
    : metres_per_second_squared_{metres_per_second_squared} {
    require_finite(metres_per_second_squared_, "Acceleration must be finite");
}

const Vec3& Acceleration::metres_per_second_squared() const noexcept {
    return metres_per_second_squared_;
}

Impulse::Impulse(Vec3 newton_seconds) : newton_seconds_{newton_seconds} {
    require_finite(newton_seconds_, "Impulse must be finite");
}

const Vec3& Impulse::newton_seconds() const noexcept { return newton_seconds_; }

} // namespace mgv::physics
