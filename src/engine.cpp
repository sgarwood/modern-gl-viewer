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
        physics::PhysicsConfiguration physics_value)
        : renderer{std::move(backend)},
          clock{std::move(clock_value)},
          physics_configuration{physics_value},
          physics_world{physics_value} {
        if (!clock) {
            throw std::invalid_argument{"Engine requires a clock"};
        }
        camera_input = make_orbit_camera_controller(renderer);
        started_at = clock->now();
        previous_tick = started_at;
    }

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

    void execute(EngineCommand command) {
        std::visit(
            [this](auto&& value) {
                using Command = std::remove_cvref_t<decltype(value)>;
                if constexpr (std::is_same_v<Command, CameraInputCommand>) {
                    camera_input->handle(value.action);
                } else if constexpr (std::is_same_v<Command, SetEntityTransformCommand>) {
                    auto& entity = find(value.entity);
                    entity.transform = std::move(value.transform);
                    renderer.set_renderable_transform(entity.renderable, entity.transform);
                } else if constexpr (std::is_same_v<Command, ApplyEntityImpulseCommand>) {
                    const auto& entity = find(value.entity);
                    if (!entity.body) {
                        throw std::logic_error{"Cannot apply an impulse to an unbound entity"};
                    }
                    physics_world.apply_impulse(*entity.body, value.impulse);
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
        while (!commands.empty()) {
            auto command = std::move(commands.front());
            commands.pop_front();
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
    animation::AnimationSystem animation_system;
    std::unique_ptr<InputSink> camera_input;
    std::deque<EngineCommand> commands;
    std::vector<EntityRecord> entities;
    std::unique_ptr<network::NetworkEventSource> network_events;
    std::unique_ptr<NetworkEventDecoder> network_decoder;
    Clock::time_point started_at;
    Clock::time_point previous_tick;
};

Engine::Engine(std::unique_ptr<RenderBackend> backend)
    : Engine{
          std::move(backend),
          std::make_unique<SteadyClock>(),
          physics::PhysicsConfiguration{}} {}

Engine::Engine(
    std::unique_ptr<RenderBackend> backend,
    std::unique_ptr<Clock> clock,
    physics::PhysicsConfiguration physics)
    : impl_{std::make_unique<Impl>(
          std::move(backend),
          std::move(clock),
          physics)} {}

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
    impl_->commands.push_back(std::move(command));
}

void Engine::enqueue(InputAction action) {
    enqueue(CameraInputCommand{action});
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
