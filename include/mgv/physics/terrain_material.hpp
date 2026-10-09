#pragma once

namespace mgv::physics {

enum class TerrainSurface {
    Fairway,
    Rough,
    Green,
    Sand,
    Divot
};

struct TerrainMaterial {
    TerrainSurface surface{TerrainSurface::Fairway};
    float static_friction{0.5F};
    /// Coulomb friction, which acts only while the contact is *sliding*.
    ///
    /// This is what converts a landing ball's backspin into a stopped --
    /// or reversed -- ball. Once the contact stops slipping it stops
    /// acting, and `rolling_resistance` takes over.
    float dynamic_friction{0.4F};

    /// Rolling resistance, which acts only once the contact is rolling.
    ///
    /// A couple rather than a force: it opposes the spin, and the static
    /// friction that keeps the ball rolling passes the deceleration on to
    /// its centre. For a solid sphere that works out at five sevenths of
    /// `rolling_resistance * g`, which is what makes a green's speed a
    /// measurable quantity rather than a tuning knob --- see
    /// `rolling_resistance_from_stimp`.
    float rolling_resistance{0.05F};
    float restitution{0.3F};         // Bounciness

    // Micro-surface modifiers
    float bumpiness{0.0F};           // 0.0 (perfect) to 1.0 (heavily bobbled/unrepaired). Causes micro-deflections.
    float sand_topdressing{0.0F};    // 0.0 to 1.0. Increases rolling resistance and brings more gravity (break) into play.
};

/// The rolling resistance of a green that stimps at `feet`.
///
/// A stimpmeter releases a ball onto the green at 1.83 m/s and the green's
/// speed is how far it then runs, in feet. A rolling sphere losing speed to
/// resistance alone decelerates at five sevenths of `c * g`, so
///
///     c = 7 v^2 / (10 g d)
///
/// and the whole relation collapses to one constant over the green speed.
/// Ten feet, a typical members' green, comes out at 0.078; the eleven to
/// thirteen of a tournament setup at 0.071 down to 0.060.
///
/// This exists so that a green can be described by the number a
/// greenkeeper would actually measure, and so that the solver can be
/// checked against it rather than against itself.
[[nodiscard]] constexpr float rolling_resistance_from_stimp(float feet) noexcept {
    // 7 * 1.829^2 / (10 * 9.81 * 0.3048), with the foot-to-metre conversion
    // folded in so that `feet` can be passed as measured.
    return feet > 0.0F ? 0.7832F / feet : 0.0F;
}

} // namespace mgv::physics
