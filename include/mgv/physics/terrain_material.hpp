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
    float static_friction{0.5f};
    float dynamic_friction{0.4f};
    
    // Core rolling mechanics
    float rolling_resistance{0.05f}; // Derived from stimpmeter for greens
    float restitution{0.3f};         // Bounciness

    // Micro-surface modifiers
    float bumpiness{0.0f};           // 0.0 (perfect) to 1.0 (heavily bobbled/unrepaired). Causes micro-deflections.
    float sand_topdressing{0.0f};    // 0.0 to 1.0. Increases rolling resistance and brings more gravity (break) into play.
};

} // namespace mgv::physics
