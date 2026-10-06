#include <cmath>
#include <iostream>
#include <mutex>
#include "mgv/engine.hpp"

#include "mgv/camera_controller.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <limits>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mgv {

namespace {
    Vec3 subtract(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
    Vec3 add(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
    Vec3 scaled(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
    Vec3 cross(const Vec3& a, const Vec3& b) {
        return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    }
    Vec3 normalize(const Vec3& a) {
        float len = std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
        if (len < 1e-6f) return {0,0,0};
        return {a.x / len, a.y / len, a.z / len};
    }
}

namespace {

class SteadyClock final : public Clock {
public:
    [[nodiscard]] time_point now() const noexcept override {
        return std::chrono::steady_clock::now();
    }
};

struct EntityRecord final {
    EntityId id;
    RenderableId renderable;
    Transform transform;
    std::optional<physics::BodyId> body;
    std::optional<animation::AnimationPlayerId> animation_player;
};

constexpr std::size_t maximum_network_events_per_tick{1'024};
std::atomic<std::uint64_t> next_entity_id{1};

[[nodiscard]] EntityId allocate_entity_id() {
    auto value = next_entity_id.load(std::memory_order_relaxed);
    while (value != std::numeric_limits<std::uint64_t>::max()) {
        if (next_entity_id.compare_exchange_weak(
                value,
                value + 1,
                std::memory_order_relaxed,
                std::memory_order_relaxed)) {
            return EntityId{value};
        }
    }
    throw std::overflow_error{"Entity identifier capacity exhausted"};
}

} // namespace

struct Engine::Impl final {
    Impl(
        std::unique_ptr<RenderBackend> backend,
        std::unique_ptr<Clock> clock_value,
        physics::PhysicsConfiguration physics_value,
        std::unique_ptr<golf::ShotModel> shot_model_value)
        : renderer{std::move(backend)},
          clock{std::move(clock_value)},
          physics_configuration{physics_value},
          physics_world{physics_value},
          shot_model{std::move(shot_model_value)} {
        if (!clock) {
            throw std::invalid_argument{"Engine requires a clock"};
        }
        if (!shot_model) {
            throw std::invalid_argument{"Engine requires a shot model"};
        }
        camera_input = make_orbit_camera_controller(renderer);
        started_at = clock->now();
        previous_tick = started_at;
    }

    ~Impl() { detach_launch_monitor(); }

    [[nodiscard]] EntityRecord& find(EntityId id) {
        const auto found = std::ranges::find(entities, id, &EntityRecord::id);
        if (found == entities.end()) {
            throw std::out_of_range{"Unknown entity identifier"};
        }
        return *found;
    }

    [[nodiscard]] const EntityRecord& find(EntityId id) const {
        const auto found = std::ranges::find(entities, id, &EntityRecord::id);
        if (found == entities.end()) {
            throw std::out_of_range{"Unknown entity identifier"};
        }
        return *found;
    }

    [[nodiscard]] std::vector<EntityId> replace_entities(
        const std::vector<RenderableId>& renderables) {
        detach_launch_monitor();
        active_ball.reset();
        physics_world = physics::PhysicsWorld{physics_configuration};
        animation_system.clear_players();
        commands.clear();
        entities.clear();
        entities.reserve(renderables.size());
        std::vector<EntityId> ids;
        ids.reserve(renderables.size());
        for (const auto renderable : renderables) {
            const auto entity = allocate_entity_id();
            entities.push_back({
                .id = entity,
                .renderable = renderable,
                .transform = renderer.renderable_transform(renderable),
                .body = std::nullopt,
                .animation_player = std::nullopt,
            });
            ids.push_back(entity);
        }
        return ids;
    }

    void poll_network() {
        if (!network_events) {
            return;
        }
        for (std::size_t count = 0; count < maximum_network_events_per_tick; ++count) {
            auto event = network_events->poll_event();
            if (!event) {
                return;
            }
            auto decoded = network_decoder->decode(*event);
            for (auto& command : decoded) {
                commands.push_back(std::move(command));
            }
        }
    }

    void enqueue(EngineCommand command) {
        std::lock_guard<std::mutex> lock(commands_mutex);
        commands.push_back(std::move(command));
    }

    void detach_launch_monitor() noexcept {
        if (!launch_monitor) {
            return;
        }
        try {
            launch_monitor->stop();
            launch_monitor->set_callback({});
        } catch (...) {
        }
        launch_monitor.reset();
    }

    void launch_ball(EntityId entity_id, const golf::Shot& shot) {
        const auto& entity = find(entity_id);
        if (!entity.body) {
            throw std::logic_error{"Cannot launch an entity without a physics body"};
        }
        const auto launch = shot_model->resolve(shot);
        physics_world.set_velocity(
            *entity.body,
            launch.linear_velocity,
            launch.angular_velocity);
    }

    void execute(EngineCommand command) {
        std::visit(
            [this](auto&& value) {
                using Command = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, CameraInputCommand>) {
                    if (value.action == InputAction::fire_test_shot) {
                        if (active_ball) {
                            launch_ball(
                                *active_ball,
                                golf::FullSwingData{
                                    .ball_speed_mps = 55.0F,
                                    .launch_angle_deg = 15.0F,
                                    .launch_direction_deg = 0.0F,
                                    .total_spin_rpm = 3'000.0F,
                                    .spin_axis_deg = 0.0F,
                                });
                        }
                    } else if (value.action == InputAction::toggle_range_finder) {
                        is_range_finder_active_ = !is_range_finder_active_;
                        auto cam = renderer.camera();
                        if (is_range_finder_active_) {
                            cam.look_at(Vec3{0.0f, 1.5f, 5.0f}, Vec3{0.0f, 0.0f, -10.0f});
                            range_finder_base_target_ = Vec3{0.0f, 0.0f, -10.0f};
                            std::cout << "[UI] RANGE FINDER DEPLOYED. Press F to Ping Distance." << std::endl;
                        } else {
                            cam.look_at(Vec3{-8.0f, 6.0f, -8.0f}, Vec3{0.0f, 0.0f, 0.0f});
                            std::cout << "[UI] RANGE FINDER STOWED." << std::endl;
                        }
                        renderer.set_camera(cam);
                    } else if (value.action == InputAction::range_finder_ping) {
                        if (is_range_finder_active_) {
                            auto cam = renderer.camera();
                            Vec3 dir = normalize(subtract(cam.target(), cam.position()));
                            auto hit = physics_world.raycast(physics::Position{cam.position()}, dir);
                            if (hit) {
                                std::cout << "[UI] PING! Target Acquired at: " << hit->distance << " meters." << std::endl;
                            } else {
                                std::cout << "[UI] PING! No Target In Sight." << std::endl;
                            }
                        }
                    } else {
                        camera_input->handle(value.action);
                    }
                } else if constexpr (std::is_same_v<Command, SetEntityTransformCommand>) {
                    auto& entity = find(value.entity);
                    entity.transform = std::move(value.transform);
                    renderer.set_renderable_transform(entity.renderable, entity.transform);
                
                } else if constexpr (std::is_same_v<Command, SetWeatherCommand>) {
                    float temp_k = value.temperature_c + 273.15f;
                    float rho = 101325.0f / (287.058f * temp_k);
                    physics_world.set_air_density(rho);
                    
                    float rad = value.wind_direction_deg * (3.14159265f / 180.0f);
                    float vx = value.wind_speed_mps * std::sin(rad);
                    float vz = -value.wind_speed_mps * std::cos(rad);
                    physics_world.set_wind(physics::LinearVelocity{Vec3{vx, 0.0f, vz}});
                    
                    std::cout << "Engine applied live weather: Rho=" << rho << " kg/m^3, Wind=(" << vx << ", 0, " << vz << ")" << std::endl;
                } else if constexpr (std::is_same_v<Command, ApplyEntityImpulseCommand>) {
                    const auto& entity = find(value.entity);
                    if (!entity.body) {
                        throw std::logic_error{"Cannot apply an impulse to an unbound entity"};
                    }
                    physics_world.apply_impulse(*entity.body, value.impulse);
                } else if constexpr (std::is_same_v<Command, SetEntityVelocityCommand>) {
                    const auto& entity = find(value.entity);
                    if (!entity.body) {
                        throw std::logic_error{"Cannot set velocity on an unbound entity"};
                    }
                    physics_world.set_velocity(*entity.body, value.linear, value.angular);
                } else if constexpr (std::is_same_v<Command, LaunchBallCommand>) {
                    launch_ball(value.entity, value.shot);
                } else if constexpr (std::is_same_v<Command, PlayAnimationCommand>) {
                    animation_system.play(value.player);
                } else if constexpr (std::is_same_v<Command, PauseAnimationCommand>) {
                    animation_system.pause(value.player);
                } else if constexpr (std::is_same_v<Command, StopAnimationCommand>) {
                    animation_system.stop(value.player);
                } else if constexpr (std::is_same_v<Command, SeekAnimationCommand>) {
                    animation_system.seek(value.player, value.time);
                } else if constexpr (
                    std::is_same_v<Command, SetAnimationPlaybackRateCommand>) {
                    animation_system.set_playback_rate(value.player, value.rate);
                }
            },
            std::move(command));
    }

void drain_commands() {
        std::deque<EngineCommand> local_commands;
        {
            std::lock_guard<std::mutex> lock(commands_mutex);
            local_commands = std::move(commands);
            commands.clear();
        }
        while (!local_commands.empty()) {
            auto command = std::move(local_commands.front());
            local_commands.pop_front();
            execute(std::move(command));
        }
    }

    void synchronize_physics() {
        for (auto& entity : entities) {
            if (!entity.body) {
                continue;
            }
            entity.transform.set_position(physics_world.body(*entity.body).position().metres());
            renderer.set_renderable_transform(entity.renderable, entity.transform);
        }
    }

    void synchronize_animation() {
        for (auto& entity : entities) {
            if (!entity.animation_player) {
                continue;
            }
            entity.transform = animation_system.root_transform(*entity.animation_player);
            renderer.set_renderable_transform(entity.renderable, entity.transform);
        }
    }

    Renderer renderer;
    std::unique_ptr<Clock> clock;
    physics::PhysicsConfiguration physics_configuration;
    physics::PhysicsWorld physics_world;
    std::unique_ptr<golf::ShotModel> shot_model;
    animation::AnimationSystem animation_system;
    std::unique_ptr<InputSink> camera_input;
    std::deque<EngineCommand> commands;
    std::mutex commands_mutex;
    std::vector<EntityRecord> entities;
    std::unique_ptr<network::NetworkEventSource> network_events;
    std::unique_ptr<NetworkEventDecoder> network_decoder;
    Clock::time_point started_at;
    Clock::time_point previous_tick;

    bool is_range_finder_active_{false};
    float sway_time_{0.0f};
    Vec3 range_finder_base_target_{};
    std::optional<EntityId> active_ball;
    std::unique_ptr<hardware::LaunchMonitor> launch_monitor;

};

Engine::Engine(std::unique_ptr<RenderBackend> backend)
    : Engine{
          std::move(backend),
          std::make_unique<SteadyClock>(),
          physics::PhysicsConfiguration{},
          std::make_unique<golf::StandardShotModel>()} {}

Engine::Engine(
    std::unique_ptr<RenderBackend> backend,
    std::unique_ptr<Clock> clock,
    physics::PhysicsConfiguration physics)
    : Engine{
          std::move(backend),
          std::move(clock),
          physics,
          std::make_unique<golf::StandardShotModel>()} {}

Engine::Engine(
    std::unique_ptr<RenderBackend> backend,
    std::unique_ptr<Clock> clock,
    physics::PhysicsConfiguration physics,
    std::unique_ptr<golf::ShotModel> shot_model)
    : impl_{std::make_unique<Impl>(
          std::move(backend),
          std::move(clock),
          physics,
          std::move(shot_model))} {}

Engine::~Engine() = default;
Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

std::vector<EntityId> Engine::load(const AssetPaths& assets) {
    return impl_->replace_entities(impl_->renderer.load(assets));
}

std::vector<EntityId> Engine::set_scene(Scene scene) {
    return impl_->replace_entities(impl_->renderer.set_scene(std::move(scene)));
}

physics::BodyId Engine::bind_physics(
    EntityId entity,
    physics::RigidBodyDefinition body) {
    auto& record = impl_->find(entity);
    if (record.animation_player) {
        throw std::logic_error{"Cannot bind physics to an animated entity"};
    }
    if (record.body) {
        throw std::logic_error{"Entity already has a physics body"};
    }
    const auto id = impl_->physics_world.add_body(std::move(body));
    record.body = id;
    record.transform.set_position(impl_->physics_world.body(id).position().metres());
    impl_->renderer.set_renderable_transform(record.renderable, record.transform);
    return id;
}

physics::BodyId Engine::bind_golf_ball(
    EntityId entity,
    physics::RigidBodyDefinition body) {
    if (impl_->active_ball) {
        throw std::logic_error{"Engine already has an active golf ball"};
    }
    if (body.motion() != physics::MotionType::dynamic ||
        !std::holds_alternative<physics::SphereCollider>(body.collider().shape())) {
        throw std::invalid_argument{"Active golf ball must be a dynamic sphere"};
    }
    const auto id = bind_physics(entity, std::move(body));
    impl_->active_ball = entity;
    return id;
}

animation::AnimationClipId Engine::load_animation(
    const animation::AnimationAssetPaths& assets) {
    return impl_->animation_system.load(assets);
}

animation::AnimationPlayerId Engine::bind_animation(
    EntityId entity,
    animation::AnimationClipId clip) {
    auto& record = impl_->find(entity);
    if (record.body) {
        throw std::logic_error{"Cannot bind animation to a physics entity"};
    }
    if (record.animation_player) {
        throw std::logic_error{"Entity already has an animation player"};
    }
    const auto player = impl_->animation_system.create_player(clip);
    record.animation_player = player;
    record.transform = impl_->animation_system.root_transform(player);
    impl_->renderer.set_renderable_transform(record.renderable, record.transform);
    return player;
}

void Engine::remove(EntityId entity) {
    const auto found = std::ranges::find(impl_->entities, entity, &EntityRecord::id);
    if (found == impl_->entities.end()) {
        throw std::out_of_range{"Unknown entity identifier"};
    }
    if (impl_->active_ball == entity) {
        impl_->detach_launch_monitor();
        impl_->active_ball.reset();
    }
    if (found->body) {
        static_cast<void>(impl_->physics_world.remove_body(*found->body));
    }
    if (found->animation_player) {
        static_cast<void>(impl_->animation_system.remove_player(*found->animation_player));
    }
    impl_->renderer.remove_renderable(found->renderable);
    impl_->entities.erase(found);
}

void Engine::enqueue(EngineCommand command) {
    impl_->enqueue(std::move(command));
}

void Engine::enqueue(InputAction action) {
    enqueue(CameraInputCommand{action});
}

void Engine::submit_shot(golf::Shot shot) {
    if (!impl_->active_ball) {
        throw std::logic_error{"Engine has no active golf ball"};
    }
    impl_->enqueue(LaunchBallCommand{*impl_->active_ball, std::move(shot)});
}

void Engine::attach_launch_monitor(std::unique_ptr<hardware::LaunchMonitor> monitor) {
    if (!monitor) {
        throw std::invalid_argument{"Engine requires a launch monitor"};
    }
    if (!impl_->active_ball) {
        throw std::logic_error{"Bind an active golf ball before attaching a launch monitor"};
    }
    if (impl_->launch_monitor) {
        throw std::logic_error{"Engine already has a launch monitor"};
    }

    const auto ball = *impl_->active_ball;
    monitor->set_callback([implementation = impl_.get(), ball](const hardware::ShotData& shot) {
        implementation->enqueue(LaunchBallCommand{ball, shot});
    });
    monitor->start();
    impl_->launch_monitor = std::move(monitor);
}

void Engine::attach_network(
    std::unique_ptr<network::NetworkEventSource> events,
    std::unique_ptr<NetworkEventDecoder> decoder) {
    if (!events || !decoder) {
        throw std::invalid_argument{"Engine networking requires an event source and decoder"};
    }
    impl_->network_events = std::move(events);
    impl_->network_decoder = std::move(decoder);
}

void Engine::tick(Viewport viewport) {
    if (viewport.framebuffer_width <= 0 || viewport.framebuffer_height <= 0) {
        throw std::invalid_argument{"Engine viewport dimensions must be positive"};
    }
    impl_->poll_network();
    impl_->drain_commands();

    const auto now = impl_->clock->now();
    if (now < impl_->previous_tick) {
        throw std::runtime_error{"Engine clock moved backwards"};
    }
    const auto elapsed = std::chrono::duration<float>{now - impl_->previous_tick}.count();
    impl_->previous_tick = now;
    impl_->animation_system.advance(animation::AnimationDuration{elapsed});
    impl_->synchronize_animation();
    impl_->physics_world.simulate(physics::Duration{elapsed});
    impl_->synchronize_physics();

    if (impl_->is_range_finder_active_) {
        impl_->sway_time_ += elapsed;
        auto cam = impl_->renderer.camera();
        
        Vec3 forward = normalize(subtract(impl_->range_finder_base_target_, cam.position()));
        Vec3 right = normalize(cross(forward, Vec3{0,1,0}));
        Vec3 up = normalize(cross(right, forward));
        
        // Procedural Sniper Sway (Perlin approximation)
        float yaw_sway = std::sin(impl_->sway_time_ * 1.5f) * 0.1f + std::cos(impl_->sway_time_ * 0.8f) * 0.05f;
        float pitch_sway = std::cos(impl_->sway_time_ * 1.2f) * 0.08f + std::sin(impl_->sway_time_ * 0.5f) * 0.04f;
        
        Vec3 swayed_target = add(impl_->range_finder_base_target_, add(scaled(right, yaw_sway), scaled(up, pitch_sway)));
        cam.look_at(cam.position(), swayed_target);
        impl_->renderer.set_camera(cam);
    }


    const auto total_elapsed = std::chrono::duration<float>{now - impl_->started_at}.count();
    impl_->renderer.render({
        .framebuffer_width = viewport.framebuffer_width,
        .framebuffer_height = viewport.framebuffer_height,
        .elapsed_seconds = total_elapsed,
    });
}

bool Engine::contains(EntityId entity) const noexcept {
    return std::ranges::find(impl_->entities, entity, &EntityRecord::id) !=
           impl_->entities.end();
}

Transform Engine::transform(EntityId entity) const {
    return impl_->find(entity).transform;
}

animation::PlaybackState Engine::animation_state(
    animation::AnimationPlayerId player) const {
    return impl_->animation_system.state(player);
}

Camera Engine::camera() const {
    return impl_->renderer.camera();
}

RenderStatistics Engine::last_frame_statistics() const noexcept {
    return impl_->renderer.last_frame_statistics();
}

} // namespace mgv
