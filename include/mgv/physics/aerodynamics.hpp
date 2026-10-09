#pragma once

namespace mgv::physics {

/// The aerodynamic coefficients of a golf ball in flight.
///
/// These were constants -- 0.3 of drag and 0.2 of lift, whatever the ball was
/// doing. A constant lift coefficient is the expensive one: lift rises with
/// the spin ratio, so holding it fixed means a wedge turning at seven
/// thousand revolutions flies with a driver's lift, and the steep, high,
/// short flight that makes a short iron a short iron does not happen.
///
/// The fits below are Smits and Smith's, from wind tunnel measurements over
/// 40,000 < Re < 250,000 and spin ratios from 0.04 to 1.4, which spans a
/// driver through a wedge. They are stated here as pure functions of the two
/// quantities that dimensional analysis says matter, so that they can be
/// checked against the published numbers without standing up a world.
///
/// Reference: A. J. Smits and D. R. Smith, "A new aerodynamic model of a golf
/// ball in flight", Science and Golf II (1994), pp. 340-347.

/// Dynamic viscosity of air at around twenty degrees, in pascal seconds.
inline constexpr float air_dynamic_viscosity = 1.81e-5F;

/// The spin ratio: how fast the ball's surface is turning compared with how
/// fast it is travelling.
///
/// `spin` is the component of the angular velocity across the direction of
/// flight, in rad/s -- spin about the direction of travel is a rifle spin and
/// makes no lift. Zero when the ball is not moving, which is the only answer
/// that keeps the coefficients finite.
[[nodiscard]] float spin_ratio(float speed, float spin, float radius) noexcept;

/// Reynolds number for a sphere of this radius at this speed, from the
/// kinematic viscosity the air density implies.
///
/// Density rather than a fixed kinematic viscosity, because the same course a
/// mile up has thinner air and the Reynolds number follows it.
[[nodiscard]] float reynolds_number(float speed, float radius, float air_density) noexcept;

/// Drag coefficient.
///
///     C_D = 0.24 + 0.18 S + 0.06 sin(pi (Re - 90,000) / 200,000)
///
/// The sine term is the transition of the boundary layer, over the band it
/// was measured across. Its argument is clamped to that band so the term can
/// never go negative: extended, the sine would start subtracting drag.
///
/// It is worth knowing which way this term points, because it is the one
/// number here with real doubt attached. Carrying it at the published +0.06
/// puts a tour driver, a 7 iron and a pitching wedge within a few per cent of
/// their measured apex and descent angle; negating it, or dropping it, sends
/// a driver's apex to 40 and then 49 metres against a measured 31. Below the
/// band the fit also understates drag badly -- a ball at Re 5e4 measures
/// nearer 0.5 -- which costs nothing at the speeds that happen down there.
[[nodiscard]] float drag_coefficient(float spin_ratio, float reynolds_number) noexcept;

/// Lift coefficient.
///
///     C_L = 0.54 S^0.4
///
/// No spin, no lift. The exponent being well under one is why the first few
/// hundred revolutions buy most of the lift a ball will ever get, and why
/// adding spin to an already spinning ball does comparatively little.
[[nodiscard]] float lift_coefficient(float spin_ratio) noexcept;

/// The rate at which spin bleeds off, per second.
///
/// Smits and Smith measured the spin-down of a ball in flight as
///
///     dw/dt = -4.0e-6 v^2 S / R^2,    v in mph
///
/// which, with S = R w / v, is a decay *rate* proportional to airspeed:
/// `dw/dt = -k v w / R`. That is the form, and it matters more than the
/// number -- a fixed moment would not vanish with the spin and would carry a
/// lightly spinning ball through zero and out the other side. Nathan notes
/// that their data make this rate independent of Reynolds number over
/// (1.0-2.5)e5, which is most of a golf shot.
///
/// Scaled by air density, which the published form does not carry: the moment
/// is aerodynamic, so thinner air holds spin for longer.
[[nodiscard]] float spin_decay_rate(float speed, float radius, float air_density) noexcept;

} // namespace mgv::physics
