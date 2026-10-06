#include "mgv/engine.hpp"

#include "animation/ozz_test_assets.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <thread>
#include <variant>
#include <utility>
#include <vector>

namespace {

class FakeMesh final : public mgv::MeshResource {};
class FakePipeline final : public mgv::RenderPipelineResource {};
class FakeTexture final : public mgv::TextureResource {};
class FakeSampler final : public mgv::SamplerResource {};

struct RenderProbe final {
    int draws{};
    std::vector<float> elapsed;
};

class FakeBackend final : public mgv::RenderBackend {
public:
    explicit FakeBackend(RenderProbe& probe) : probe_{probe} {}

    [[nodiscard]] mgv::RenderBackendCapabilities capabilities() const noexcept override {
        return {};
    }
    [[nodiscard]] std::unique_ptr<mgv::MeshResource> create_mesh(
        const mgv::MeshData&) override {
        return std::make_unique<FakeMesh>();
    }
    [[nodiscard]] std::unique_ptr<mgv::RenderPipelineResource> create_pipeline(
        const mgv::RenderPipelineDescriptor&) override {
        return std::make_unique<FakePipeline>();
    }
    [[nodiscard]] std::unique_ptr<mgv::TextureResource> create_texture(
        const mgv::ImageData&) override {
        return std::make_unique<FakeTexture>();
    }
    [[nodiscard]] std::unique_ptr<mgv::SamplerResource> create_sampler(
        const mgv::SamplerDescriptor&) override {
        return std::make_unique<FakeSampler>();
    }
    void begin_frame(const mgv::Frame& frame) override {
        probe_.elapsed.push_back(frame.elapsed_seconds);
    }
    void draw(const mgv::DrawPacket&) override { ++probe_.draws; }
    void end_frame() noexcept override {}

private:
    RenderProbe& probe_;
};

class FakeClock final : public mgv::Clock {
public:
    [[nodiscard]] time_point now() const noexcept override { return now_; }

    void advance(std::chrono::duration<float> elapsed) {
        now_ += std::chrono::duration_cast<time_point::duration>(elapsed);
    }

private:
    time_point now_{};
};

class FakeNetworkEvents final : public mgv::network::NetworkEventSource {
public:
    [[nodiscard]] std::optional<mgv::network::NetworkEvent> poll_event() override {
        if (events.empty()) {
            return std::nullopt;
        }
        auto event = std::move(events.front());
        events.pop_front();
        return event;
    }

    std::deque<mgv::network::NetworkEvent> events;
};

class TransformDecoder final : public mgv::NetworkEventDecoder {
public:
    TransformDecoder(mgv::EntityId entity, std::thread::id& thread)
        : entity_{entity}, thread_{thread} {}

    [[nodiscard]] std::vector<mgv::EngineCommand> decode(
        const mgv::network::NetworkEvent&) override {
        thread_ = std::this_thread::get_id();
        mgv::Transform transform;
        transform.set_position({7.0F, 8.0F, 9.0F});
        return {mgv::SetEntityTransformCommand{entity_, transform}};
    }

private:
    mgv::EntityId entity_;
    std::thread::id& thread_;
};

class FakeShotModel final : public mgv::golf::ShotModel {
public:
    explicit FakeShotModel(std::thread::id& resolved_on) : resolved_on_{resolved_on} {}

    [[nodiscard]] mgv::golf::BallLaunch resolve(const mgv::golf::Shot& shot) const override {
        resolved_on_ = std::this_thread::get_id();
        last_shot = shot;
        return {
            .linear_velocity = mgv::physics::LinearVelocity{{2.0F, 0.0F, 0.0F}},
            .angular_velocity = mgv::physics::AngularVelocity{{0.0F, 3.0F, 0.0F}},
        };
    }

    mutable std::optional<mgv::golf::Shot> last_shot;

private:
    std::thread::id& resolved_on_;
};

struct LaunchMonitorProbe final {
    bool started{};
    bool stopped{};
};

class FakeLaunchMonitor final : public mgv::hardware::LaunchMonitor {
public:
    explicit FakeLaunchMonitor(LaunchMonitorProbe& probe) : probe_{probe} {}

    void set_callback(ShotCallback callback) override { callback_ = std::move(callback); }
    void start() override { probe_.started = true; }
    void stop() override { probe_.stopped = true; }

    void emit(const mgv::golf::Shot& shot) { callback_(shot); }

private:
    LaunchMonitorProbe& probe_;
    ShotCallback callback_;
};

[[nodiscard]] mgv::Scene triangle_scene() {
    auto mesh = std::make_shared<const mgv::MeshData>(mgv::MeshData{
        .vertices = {{{-0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.5F, -0.5F, 0.0F}, {}, {}},
                     {{0.0F, 0.5F, 0.0F}, {}, {}}},
        .indices = {0, 1, 2},
    });
    auto material = std::make_shared<const mgv::MaterialInstance>(
        std::make_shared<const mgv::Material>(
            mgv::ShaderSources{"vertex", "fragment", "test.vert", "test.frag"}));
    mgv::Scene scene;
    static_cast<void>(scene.add(mgv::Renderable{std::move(mesh), std::move(material)}));
    return scene;
}

[[nodiscard]] mgv::physics::RigidBodyDefinition moving_body() {
    return mgv::physics::RigidBodyBuilder{
        mgv::physics::Collider::sphere(mgv::physics::Length{0.5F})}
        .velocity(mgv::physics::LinearVelocity{{1.0F, 0.0F, 0.0F}})
        .build();
}

} // namespace

TEST_CASE("engine drains queued input only at a deterministic tick boundary") {
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    auto* clock_view = clock.get();
    mgv::Engine engine{std::make_unique<FakeBackend>(probe), std::move(clock)};
    static_cast<void>(engine.set_scene(triangle_scene()));
    const auto original = engine.camera();

    engine.enqueue(mgv::CameraInputCommand{mgv::InputAction::orbit_left});
    CHECK(engine.camera().position() == original.position());
    clock_view->advance(std::chrono::duration<float>{0.25F});
    engine.tick({800, 600});

    CHECK(engine.camera().position() != original.position());
    REQUIRE(probe.elapsed.size() == 1);
    CHECK(probe.elapsed.front() == Catch::Approx(0.25F));
}

TEST_CASE("engine synchronizes a physics body to its bound renderable") {
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    auto* clock_view = clock.get();
    mgv::physics::PhysicsConfiguration physics;
    physics.fixed_time_step = mgv::physics::Duration{0.1F};
    physics.gravity = mgv::physics::Acceleration{{}};
    mgv::Engine engine{
        std::make_unique<FakeBackend>(probe), std::move(clock), physics};
    const auto entities = engine.set_scene(triangle_scene());
    REQUIRE(entities.size() == 1);
    static_cast<void>(engine.bind_physics(entities.front(), moving_body()));

    clock_view->advance(std::chrono::duration<float>{0.1F});
    engine.tick({800, 600});

    CHECK(engine.transform(entities.front()).position().x ==
          Catch::Approx(0.1F).margin(0.00001F));
}

TEST_CASE("engine applies queued physics commands at the tick boundary") {
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    auto* clock_view = clock.get();
    mgv::physics::PhysicsConfiguration physics;
    physics.fixed_time_step = mgv::physics::Duration{0.1F};
    physics.gravity = mgv::physics::Acceleration{{}};
    mgv::Engine engine{
        std::make_unique<FakeBackend>(probe), std::move(clock), physics};
    const auto entity = engine.set_scene(triangle_scene()).front();
    static_cast<void>(engine.bind_physics(
        entity,
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{0.5F})}
            .mass(mgv::physics::Mass{1.0F})
            .build()));

    engine.enqueue(mgv::ApplyEntityImpulseCommand{
        entity, mgv::physics::Impulse{{1.0F, 0.0F, 0.0F}}});
    CHECK(engine.transform(entity).position().x == Catch::Approx(0.0F));
    clock_view->advance(std::chrono::duration<float>{0.1F});
    engine.tick({800, 600});

    CHECK(engine.transform(entity).position().x ==
          Catch::Approx(0.1F).margin(0.00001F));
}

TEST_CASE("engine applies queued animation commands and binds the root pose to an entity") {
    const TestAnimationAssets assets;
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    auto* clock_view = clock.get();
    mgv::Engine engine{std::make_unique<FakeBackend>(probe), std::move(clock)};
    const auto entity = engine.set_scene(triangle_scene()).front();
    const auto clip = engine.load_animation(assets.paths());
    const auto player = engine.bind_animation(entity, clip);

    engine.enqueue(mgv::PlayAnimationCommand{player});
    CHECK(engine.transform(entity).position().x == Catch::Approx(0.0F));
    clock_view->advance(std::chrono::duration<float>{0.25F});
    engine.tick({800, 600});
    CHECK(engine.transform(entity).position().x == Catch::Approx(0.5F).margin(0.001F));

    engine.enqueue(mgv::PauseAnimationCommand{player});
    clock_view->advance(std::chrono::duration<float>{0.25F});
    engine.tick({800, 600});
    CHECK(engine.transform(entity).position().x == Catch::Approx(0.5F).margin(0.001F));
    CHECK(engine.animation_state(player) == mgv::animation::PlaybackState::paused);

    engine.enqueue(mgv::SeekAnimationCommand{
        player, mgv::animation::AnimationDuration{0.25F}});
    engine.enqueue(mgv::SetAnimationPlaybackRateCommand{player, 2.0F});
    engine.enqueue(mgv::PlayAnimationCommand{player});
    clock_view->advance(std::chrono::duration<float>{0.25F});
    engine.tick({800, 600});
    CHECK(engine.transform(entity).position().x == Catch::Approx(1.5F).margin(0.001F));
    CHECK(engine.animation_state(player) == mgv::animation::PlaybackState::playing);

    engine.enqueue(mgv::StopAnimationCommand{player});
    engine.tick({800, 600});
    CHECK(engine.transform(entity).position().x == Catch::Approx(0.0F));
    CHECK(engine.animation_state(player) == mgv::animation::PlaybackState::stopped);
}

TEST_CASE("engine prevents animation and physics from owning the same entity transform") {
    const TestAnimationAssets assets;
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    mgv::Engine engine{std::make_unique<FakeBackend>(probe), std::move(clock)};
    const auto entity = engine.set_scene(triangle_scene()).front();
    const auto clip = engine.load_animation(assets.paths());
    static_cast<void>(engine.bind_physics(entity, moving_body()));

    CHECK_THROWS_AS(engine.bind_animation(entity, clip), std::logic_error);

    const auto animated_entity = engine.set_scene(triangle_scene()).front();
    static_cast<void>(engine.bind_animation(animated_entity, clip));
    CHECK_THROWS_AS(engine.bind_physics(animated_entity, moving_body()), std::logic_error);
}

TEST_CASE("engine rejects removed entity handles without aliasing retained entities") {
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    mgv::Engine engine{std::make_unique<FakeBackend>(probe), std::move(clock)};
    const auto first = engine.set_scene(triangle_scene()).front();
    engine.remove(first);
    const auto replacement = engine.set_scene(triangle_scene()).front();

    CHECK(replacement != first);
    CHECK_FALSE(engine.contains(first));
    CHECK(engine.contains(replacement));
    CHECK_THROWS_AS(engine.transform(first), std::out_of_range);
}

TEST_CASE("entity handles cannot alias entities owned by another engine") {
    RenderProbe first_probe;
    RenderProbe second_probe;
    mgv::Engine first{
        std::make_unique<FakeBackend>(first_probe), std::make_unique<FakeClock>()};
    mgv::Engine second{
        std::make_unique<FakeBackend>(second_probe), std::make_unique<FakeClock>()};

    const auto first_entity = first.set_scene(triangle_scene()).front();
    const auto second_entity = second.set_scene(triangle_scene()).front();

    CHECK(first_entity != second_entity);
    CHECK_FALSE(second.contains(first_entity));
    CHECK_FALSE(first.contains(second_entity));
}

TEST_CASE("network events are decoded into commands on the engine thread") {
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    mgv::Engine engine{std::make_unique<FakeBackend>(probe), std::move(clock)};
    const auto entity = engine.set_scene(triangle_scene()).front();
    auto events = std::make_unique<FakeNetworkEvents>();
    events->events.push_back(mgv::network::DatagramReceived{
        mgv::network::Datagram{
            mgv::network::Endpoint{"127.0.0.1", 4242},
            {std::byte{0x01}},
        },
    });
    std::thread::id decoder_thread;
    engine.attach_network(
        std::move(events),
        std::make_unique<TransformDecoder>(entity, decoder_thread));
    const auto engine_thread = std::this_thread::get_id();

    engine.tick({800, 600});

    CHECK(decoder_thread == engine_thread);
    CHECK(engine.transform(entity).position() == mgv::Vec3{7.0F, 8.0F, 9.0F});
}

TEST_CASE("launch monitor shots execute through the injected model on the engine thread") {
    RenderProbe render_probe;
    auto clock = std::make_unique<FakeClock>();
    auto* clock_view = clock.get();
    std::thread::id resolved_on;
    auto shot_model = std::make_unique<FakeShotModel>(resolved_on);
    auto* shot_model_view = shot_model.get();
    LaunchMonitorProbe monitor_probe;
    auto monitor = std::make_unique<FakeLaunchMonitor>(monitor_probe);
    auto* monitor_view = monitor.get();

    {
        mgv::physics::PhysicsConfiguration physics;
        physics.fixed_time_step = mgv::physics::Duration{0.1F};
        physics.gravity = mgv::physics::Acceleration{{}};
        mgv::Engine engine{
            std::make_unique<FakeBackend>(render_probe),
            std::move(clock),
            physics,
            std::move(shot_model)};
        const auto ball = engine.set_scene(triangle_scene()).front();
        static_cast<void>(engine.bind_golf_ball(
            ball,
            mgv::physics::RigidBodyBuilder{
                mgv::physics::Collider::sphere(mgv::physics::Length{0.021335F})}
                .mass(mgv::physics::Mass{0.04593F})
                .build()));
        engine.attach_launch_monitor(std::move(monitor));

        monitor_view->emit(mgv::golf::FullSwingData{.ball_speed_mps = 42.0F});

        CHECK(monitor_probe.started);
        CHECK_FALSE(shot_model_view->last_shot.has_value());
        CHECK(engine.transform(ball).position().x == Catch::Approx(0.0F));

        clock_view->advance(std::chrono::duration<float>{0.1F});
        const auto engine_thread = std::this_thread::get_id();
        engine.tick({800, 600});

        REQUIRE(shot_model_view->last_shot.has_value());
        CHECK(std::get<mgv::golf::FullSwingData>(*shot_model_view->last_shot).ball_speed_mps ==
              Catch::Approx(42.0F));
        CHECK(resolved_on == engine_thread);
        CHECK(engine.transform(ball).position().x == Catch::Approx(0.2F).margin(0.001F));
    }

    CHECK(monitor_probe.stopped);
}

TEST_CASE("frontend test-shot input uses the same queued shot execution path") {
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    auto* clock_view = clock.get();
    std::thread::id resolved_on;
    auto shot_model = std::make_unique<FakeShotModel>(resolved_on);
    auto* shot_model_view = shot_model.get();
    mgv::physics::PhysicsConfiguration physics;
    physics.fixed_time_step = mgv::physics::Duration{0.1F};
    physics.gravity = mgv::physics::Acceleration{{}};
    mgv::Engine engine{
        std::make_unique<FakeBackend>(probe),
        std::move(clock),
        physics,
        std::move(shot_model)};
    const auto ball = engine.set_scene(triangle_scene()).front();
    static_cast<void>(engine.bind_golf_ball(
        ball,
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{0.021335F})}
            .mass(mgv::physics::Mass{0.04593F})
            .build()));

    engine.enqueue(mgv::InputAction::fire_test_shot);
    CHECK_FALSE(shot_model_view->last_shot.has_value());

    clock_view->advance(std::chrono::duration<float>{0.1F});
    engine.tick({800, 600});

    REQUIRE(shot_model_view->last_shot.has_value());
    CHECK(std::holds_alternative<mgv::golf::FullSwingData>(*shot_model_view->last_shot));
    CHECK(engine.transform(ball).position().x == Catch::Approx(0.2F).margin(0.001F));
}

TEST_CASE("engine requires a bound active ball before attaching a launch monitor") {
    RenderProbe probe;
    LaunchMonitorProbe monitor_probe;
    mgv::Engine engine{
        std::make_unique<FakeBackend>(probe), std::make_unique<FakeClock>()};

    CHECK_THROWS_AS(
        engine.attach_launch_monitor(std::make_unique<FakeLaunchMonitor>(monitor_probe)),
        std::logic_error);
    CHECK_FALSE(monitor_probe.started);
}

TEST_CASE("engine keeps an active golf ball on an unrendered static world collider") {
    RenderProbe probe;
    auto clock = std::make_unique<FakeClock>();
    auto* clock_view = clock.get();
    mgv::physics::PhysicsConfiguration physics;
    physics.fixed_time_step = mgv::physics::Duration{0.01F};
    mgv::Engine engine{
        std::make_unique<FakeBackend>(probe), std::move(clock), physics};
    const auto ball = engine.set_scene(triangle_scene()).front();
    static_cast<void>(engine.bind_golf_ball(
        ball,
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{0.8F})}
            .at(mgv::physics::Position{{0.0F, 0.8F, 0.0F}})
            .mass(mgv::physics::Mass{0.04593F})
            .build()));
    static_cast<void>(engine.add_static_collider(
        mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::box(mgv::physics::Dimensions{{100.0F, 0.5F, 100.0F}})}
            .motion(mgv::physics::MotionType::static_body)
            .at(mgv::physics::Position{{0.0F, -0.5F, 0.0F}})
            .build()));

    clock_view->advance(std::chrono::duration<float>{0.1F});
    engine.tick({800, 600});

    CHECK(engine.transform(ball).position().y == Catch::Approx(0.8F).margin(0.001F));
}

TEST_CASE("engine accepts only static bodies as unrendered world colliders") {
    RenderProbe probe;
    mgv::Engine engine{
        std::make_unique<FakeBackend>(probe), std::make_unique<FakeClock>()};

    CHECK_THROWS_AS(
        engine.add_static_collider(mgv::physics::RigidBodyBuilder{
            mgv::physics::Collider::sphere(mgv::physics::Length{1.0F})}
            .build()),
        std::invalid_argument);
}
