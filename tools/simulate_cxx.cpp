#include "../include/mgv/physics/physics_world.hpp"
#include "../include/mgv/physics/collider.hpp"
#include "../include/mgv/physics/rigid_body.hpp"
#include "../src/physics/collision_detector.cpp"
#include "../src/physics/physics_world.cpp"
#include "../src/physics/collider.cpp"
#include "../src/physics/rigid_body.cpp"
#include "../src/physics/units.cpp"

#include <iostream>
#include <fstream>
#include <vector>

using namespace mgv::physics;
using namespace mgv;

int main() {
    PhysicsWorld world;
    world.set_gravity(mgv::physics::Acceleration{9.81f});

    // 50ft green (approx 15x15 meters)
    int width = 15;
    int depth = 15;
    float scale = 1.0f;
    std::vector<float> heights(width * depth, 0.0f);
    
    // Create double breaker saddle shape
    for(int z=0; z<depth; ++z) {
        for(int x=0; x<width; ++x) {
            float fx = (x - width/2.0f) * 0.1f;
            float fz = (z - depth/2.0f) * 0.1f;
            heights[z*width + x] = (fx*fx - fz*fz) * 0.2f;
        }
    }

    auto ground_collider = Collider::heightmap(width, depth, scale, scale, heights);
    world.add_body(RigidBody{ground_collider, Mass::infinite(), Position{{0, 0, 0}}});

    // Add golf ball
    auto ball_collider = Collider::sphere(Length{0.0213f}); // 42.6mm diameter
    RigidBody ball{ball_collider, Mass{0.0459f}, Position{{2.0f, 1.0f, 2.0f}}};
    // Putt it
    ball.apply_impulse(mgv::Vec3{-0.1f, 0.0f, 0.1f});
    auto ball_id = world.add_body(ball);

    std::ofstream out("trajectory.csv");
    out << "x,y,z\n";

    // Simulate 200 frames
    for(int i=0; i<200; ++i) {
        world.step(Time{0.016f});
        auto pos = world.get_body(ball_id).position().metres();
        out << pos.x << "," << pos.y << "," << pos.z << "\n";
    }
    
    return 0;
}
