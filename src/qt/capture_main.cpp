// Offscreen renderer for visual verification.
//
// Renders the same course session the interactive window builds, into an
// offscreen framebuffer, and writes a PNG. This is how rendering work is
// reviewed and how visual regressions are caught without a display.

#include "mgv/course_session.hpp"
#include "mgv/engine.hpp"
#include "mgv/opengl_backend.hpp"
#include "mgv/primitives.hpp"
#include "mgv/runtime_paths.hpp"

#include <QByteArray>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QString>
#include <QStringList>
#include <QSurfaceFormat>

#include <cmath>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <memory>
#include <optional>

namespace {

/// A clock that advances a fixed step each time the engine ticks.
///
/// A capture renders as fast as the machine allows, so wall-clock time
/// between ticks is however long a frame happened to take. Driving the
/// engine from it makes a captured sequence unrepeatable and, worse,
/// untruthful: a ball's flight would last as long as the renderer felt like
/// rather than as long as the physics says.
class FixedStepClock final : public mgv::Clock {
public:
    explicit FixedStepClock(std::chrono::duration<double> step) : step_{step} {}

    [[nodiscard]] time_point now() const noexcept override {
        const auto at = origin_ + std::chrono::duration_cast<time_point::duration>(step_ * ticks_);
        ++ticks_;
        return at;
    }

private:
    time_point origin_{};
    std::chrono::duration<double> step_;
    mutable long long ticks_{};
};


[[nodiscard]] std::filesystem::path path_from(const QString& value) {
    return std::filesystem::path{value.toStdString()};
}

[[nodiscard]] QString qstring_from(const std::filesystem::path& value) {
    return QString::fromStdString(value.string());
}

/// Parses "x,y,z" into a vector, returning nothing when the text is not three
/// finite numbers.
[[nodiscard]] const char* round_event_name(mgv::game::RoundEvent event) {
    switch (event) {
    case mgv::game::RoundEvent::ball_struck: return "ball struck";
    case mgv::game::RoundEvent::long_shot_struck: return "long shot struck";
    case mgv::game::RoundEvent::ball_came_to_rest: return "ball came to rest";
    case mgv::game::RoundEvent::ball_holed: return "ball holed";
    case mgv::game::RoundEvent::reached_ball: return "reached ball";
    case mgv::game::RoundEvent::range_finder_deployed: return "range finder deployed";
    case mgv::game::RoundEvent::range_finder_stowed: return "range finder stowed";
    case mgv::game::RoundEvent::none: break;
    }
    return "none";
}

[[nodiscard]] std::optional<mgv::Vec3> parse_vec3(const QString& value) {
    const auto parts = value.split(QChar{','}, Qt::SkipEmptyParts);
    if (parts.size() != 3) {
        return std::nullopt;
    }
    mgv::Vec3 result;
    bool ok_x{};
    bool ok_y{};
    bool ok_z{};
    result.x = parts[0].trimmed().toFloat(&ok_x);
    result.y = parts[1].trimmed().toFloat(&ok_y);
    result.z = parts[2].trimmed().toFloat(&ok_z);
    if (!ok_x || !ok_y || !ok_z) {
        return std::nullopt;
    }
    return result;
}

} // namespace

int main(int argc, char** argv) {
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(4, 1);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);

    QGuiApplication application{argc, argv};
    QGuiApplication::setApplicationName("mgv-capture");
    QGuiApplication::setApplicationVersion("0.2.0");

    const auto asset_directory = mgv::locate_asset_directory(
        MGV_DEFAULT_ASSET_DIR,
        std::filesystem::path{QGuiApplication::applicationDirPath().toStdString()});

    QCommandLineParser parser;
    parser.setApplicationDescription("Renders a course session offscreen and writes a PNG.");
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption out_option{{"o", "out"}, "Output PNG path.", "file", "capture.png"};
    const QCommandLineOption width_option{"width", "Framebuffer width.", "pixels", "1280"};
    const QCommandLineOption height_option{"height", "Framebuffer height.", "pixels", "720"};
    const QCommandLineOption frames_option{
        "frames", "Frames to simulate before capturing.", "count", "1"};
    const QCommandLineOption samples_option{
        "samples", "Multisample count, 0 to disable.", "count", "4"};
    const QCommandLineOption model_option{
        "model", "Ball model to load.", "model.obj", qstring_from(asset_directory / "ball.obj")};
    const QCommandLineOption shader_dir_option{
        "shaders",
        "Directory holding the course shaders.",
        "directory",
        qstring_from(asset_directory / "shaders")};
    const QCommandLineOption wetness_option{
        "wetness", "Surface wetness, 0 to 1.", "fraction"};
    const QCommandLineOption turbidity_option{
        "turbidity", "Atmospheric turbidity, 1.7 to 10.", "value"};
    const QCommandLineOption grass_option{
        "grass", "Blades per square metre of rough; 0 disables.", "density"};
    const QCommandLineOption grass_radius_option{
        "grass-radius", "How far the blade field extends, in metres.", "metres"};
    const QCommandLineOption wind_option{
        "wind", "Wind speed in metres per second.", "mps"};
    const QCommandLineOption ball_option{
        "ball", "Where the ball lies, \"x,0,z\" in the ground plane.", "vec3"};
    const QCommandLineOption character_option{
        "character",
        "Rigged glTF to stand on the course, as \"model.glb,skeleton.ozz,animation.ozz\".",
        "paths"};
    const QCommandLineOption character_at_option{
        "character-at", "Where the character stands, \"x,facing,z\".", "vec3"};
    const QCommandLineOption character_scale_option{
        "character-scale", "Uniform scale for the character.", "scale"};
    const QCommandLineOption character_z_up_option{
        "character-z-up", "Stand a Z-up asset upright."};
    const QCommandLineOption follow_through_option{
        "follow-through", "Clip the character plays when a long shot is struck.", "ozz"};
    const QCommandLineOption trace_option{
        "trace", "Print the round state and camera as the sequence runs."};
    const QCommandLineOption rate_option{
        "fps", "Frames per second the fixed-step clock advances at.", "hz", "60"};
    const QCommandLineOption frame_dir_option{
        "frames-to",
        "Write every frame into this directory, to watch a sequence play out.",
        "directory"};
    const QCommandLineOption shot_option{
        "shot", "Fire the deterministic test shot on the first frame."};
    const QCommandLineOption walk_option{
        "walk",
        "Metres to advance the camera towards its target each frame, which "
        "exercises the detail streaming.",
        "metres"};
    const QCommandLineOption camera_option{"camera", "Camera ground position and eye height, \"x,height,z\".", "vec3"};
    const QCommandLineOption target_option{"target", "Look-at ground position and height, \"x,height,z\".", "vec3"};
    const QCommandLineOption fov_option{"fov", "Vertical field of view, degrees.", "degrees"};
    const QCommandLineOption azimuth_option{"sun-azimuth", "Sun bearing, degrees.", "degrees"};
    const QCommandLineOption elevation_option{
        "sun-elevation", "Sun elevation above the horizon, degrees.", "degrees"};
    const QCommandLineOption exposure_option{"exposure", "Exposure value at ISO 100; daylight is near 15.", "ev100"};
    for (const auto& option : {out_option,
                               width_option,
                               height_option,
                               frames_option,
                               samples_option,
                               model_option,
                               shader_dir_option,
                               wetness_option,
                               turbidity_option,
                               grass_option,
                               grass_radius_option,
                               wind_option,
                               ball_option,
                               walk_option,
                               shot_option,
                               character_option,
                               character_at_option,
                               character_scale_option,
                               character_z_up_option,
                               follow_through_option,
                               frame_dir_option,
                               rate_option,
                               trace_option,
                               camera_option,
                               target_option,
                               fov_option,
                               azimuth_option,
                               elevation_option,
                               exposure_option}) {
        parser.addOption(option);
    }
    parser.process(application);

    const auto width = parser.value(width_option).toInt();
    const auto height = parser.value(height_option).toInt();
    const auto frames = parser.value(frames_option).toInt();
    const auto samples = parser.value(samples_option).toInt();
    if (width <= 0 || height <= 0 || frames <= 0) {
        std::fprintf(stderr, "Width, height, and frame count must all be positive.\n");
        return 2;
    }

    QOffscreenSurface surface;
    surface.setFormat(format);
    surface.create();
    if (!surface.isValid()) {
        std::fprintf(stderr, "Unable to create an offscreen surface.\n");
        return 1;
    }

    QOpenGLContext context;
    context.setFormat(format);
    if (!context.create()) {
        std::fprintf(stderr, "Unable to create an OpenGL 4.1 core context.\n");
        return 1;
    }
    if (!context.makeCurrent(&surface)) {
        std::fprintf(stderr, "Unable to make the OpenGL context current.\n");
        return 1;
    }

    QOpenGLFramebufferObjectFormat fbo_format;
    fbo_format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    fbo_format.setSamples(samples);
    QOpenGLFramebufferObject framebuffer{width, height, fbo_format};
    if (!framebuffer.isValid() || !framebuffer.bind()) {
        std::fprintf(stderr, "Unable to create an offscreen framebuffer.\n");
        return 1;
    }

    try {
        const auto fps = std::max(parser.value(rate_option).toDouble(), 1.0);
        auto engine = std::make_unique<mgv::Engine>(
            mgv::make_opengl_backend([&context](const char* name) {
                return context.getProcAddress(QByteArray{name});
            }),
            std::make_unique<FixedStepClock>(std::chrono::duration<double>{1.0 / fps}));

        auto description = mgv::default_course_session(asset_directory);
        if (parser.isSet(model_option)) {
            description.assets.ball_model = path_from(parser.value(model_option));
        }
        if (parser.isSet(shader_dir_option)) {
            description.assets.shader_directory = path_from(parser.value(shader_dir_option));
        }
        if (parser.isSet(camera_option)) {
            const auto position = parse_vec3(parser.value(camera_option));
            if (!position) {
                std::fprintf(stderr, "--camera expects \"x,height,z\".\n");
                return 2;
            }
            description.viewpoint.position = {position->x, position->z};
            description.viewpoint.eye_height = position->y;
        }
        if (parser.isSet(target_option)) {
            const auto target = parse_vec3(parser.value(target_option));
            if (!target) {
                std::fprintf(stderr, "--target expects \"x,height,z\".\n");
                return 2;
            }
            description.viewpoint.target = {target->x, target->z};
            description.viewpoint.target_height = target->y;
        }
        if (parser.isSet(fov_option)) {
            description.viewpoint.vertical_field_of_view_degrees =
                parser.value(fov_option).toFloat();
        }
        if (parser.isSet(azimuth_option) || parser.isSet(elevation_option)) {
            const auto azimuth = parser.isSet(azimuth_option)
                ? parser.value(azimuth_option).toFloat()
                : 140.0F;
            const auto elevation = parser.isSet(elevation_option)
                ? parser.value(elevation_option).toFloat()
                : 50.0F;
            description.environment.sun.direction =
                mgv::sun_direction_from_angles(azimuth, elevation);
        }
        if (parser.isSet(exposure_option)) {
            description.environment.exposure =
                mgv::exposure_from_ev100(parser.value(exposure_option).toFloat());
        }
        if (parser.isSet(wetness_option)) {
            description.environment.surface_wetness = parser.value(wetness_option).toFloat();
        }
        if (parser.isSet(turbidity_option)) {
            description.environment.turbidity = parser.value(turbidity_option).toFloat();
        }
        if (parser.isSet(character_option)) {
            const auto parts = parser.value(character_option).split(QChar{','});
            if (parts.size() != 3) {
                std::fprintf(
                    stderr, "--character expects \"model,skeleton,animation\".\n");
                return 2;
            }
            mgv::CourseCharacter character;
            character.model = path_from(parts[0].trimmed());
            character.skeleton = path_from(parts[1].trimmed());
            character.animation = path_from(parts[2].trimmed());
            if (parser.isSet(follow_through_option)) {
                character.follow_through = path_from(parser.value(follow_through_option));
            }
            if (parser.isSet(character_at_option)) {
                const auto at = parse_vec3(parser.value(character_at_option));
                if (!at) {
                    std::fprintf(stderr, "--character-at expects \"x,facing,z\".\n");
                    return 2;
                }
                character.position = {at->x, at->z};
                character.facing_degrees = at->y;
            }
            if (parser.isSet(character_scale_option)) {
                character.scale = parser.value(character_scale_option).toFloat();
            }
            character.z_up = parser.isSet(character_z_up_option);
            description.character = character;
        }
        if (parser.isSet(ball_option)) {
            const auto lie = parse_vec3(parser.value(ball_option));
            if (!lie) {
                std::fprintf(stderr, "--ball expects \"x,0,z\".\n");
                return 2;
            }
            description.ball_start = {lie->x, lie->z};
        }
        if (parser.isSet(grass_option)) {
            description.grass.density = parser.value(grass_option).toFloat();
        }
        if (parser.isSet(grass_radius_option)) {
            description.grass.radius = parser.value(grass_radius_option).toFloat();
        }
        if (parser.isSet(wind_option)) {
            description.environment.wind_speed = parser.value(wind_option).toFloat();
        }

        auto session = mgv::configure_course_session(*engine, description);
        const auto walk = parser.isSet(walk_option) ? parser.value(walk_option).toFloat() : 0.0F;
        auto streamed = 0;
        if (parser.isSet(shot_option)) {
            mgv::play_test_shot(*engine, session);
        }
        for (int frame = 0; frame < frames; ++frame) {
            if (walk != 0.0F) {
                // Step along the ground towards where the camera is looking,
                // keeping the eye at its height above the terrain.
                auto camera = engine->camera();
                const auto eye = camera.position();
                const auto target = camera.target();
                const auto forward_x = target.x - eye.x;
                const auto forward_z = target.z - eye.z;
                const auto length = std::sqrt(forward_x * forward_x + forward_z * forward_z);
                if (length > 1.0e-4F) {
                    const auto step_x = forward_x / length * walk;
                    const auto step_z = forward_z / length * walk;
                    const mgv::Vec2 next{eye.x + step_x, eye.z + step_z};
                    const auto ground =
                        mgv::course_terrain_height(next.x, next.y, description.terrain);
                    camera.look_at(
                        {next.x, ground + description.viewpoint.eye_height, next.y},
                        {target.x + step_x, target.y, target.z + step_z});
                    engine->set_camera(std::move(camera));
                }
            }
            streamed += mgv::stream_course(*engine, session, description) ? 1 : 0;
            const auto event = mgv::advance_round(*engine, session, description);
            if (event != mgv::game::RoundEvent::none) {
                std::printf("frame %d  round: %s\n", frame, round_event_name(event));
            }
            if (parser.isSet(trace_option) && frame % 20 == 0) {
                const auto at = engine->camera().position();
                const auto to = engine->camera().target();
                const auto lie = engine->transform(session.ball).position();
                std::printf(
                    "frame %3d  state=%d  camera=(%.1f, %.1f, %.1f)->(%.1f, %.1f, %.1f)  ball=(%.1f, %.1f, %.1f)",
                    frame, static_cast<int>(mgv::round_state(session)),
                    at.x, at.y, at.z, to.x, to.y, to.z, lie.x, lie.y, lie.z);
                if (session.character) {
                    const auto who = engine->transform(*session.character);
                    const auto p = who.position();
                    std::printf("  player=(%.1f, %.1f, %.1f) scale=%.2f",
                                p.x, p.y, p.z, who.scale().x);
                }
                std::printf("\n");
            }
            engine->tick({width, height});
            if (parser.isSet(frame_dir_option)) {
                const auto directory = parser.value(frame_dir_option);
                QDir{}.mkpath(directory);
                const auto name = QString{"%1/frame-%2.png"}
                                      .arg(directory)
                                      .arg(frame, 4, 10, QChar{'0'});
                static_cast<void>(framebuffer.toImage().save(name, "PNG"));
                // toImage() leaves the default framebuffer bound. Without
                // binding ours again every later frame renders to the
                // window system's buffer, and the sequence comes out as the
                // first frame repeated.
                static_cast<void>(framebuffer.bind());
            }
        }
        std::printf("streamed=%d\n", streamed);
        const auto eye = engine->camera().position();
        std::printf(
            "camera=(%.2f, %.2f, %.2f)\n",
            static_cast<double>(eye.x),
            static_cast<double>(eye.y),
            static_cast<double>(eye.z));

        // Where the ball actually came to rest, which is the quickest way to
        // see that it is resting on the terrain rather than falling past it.
        const auto lie = engine->transform(session.ball).position();
        std::printf(
            "ball=(%.3f, %.3f, %.3f) terrain=%.3f\n",
            static_cast<double>(lie.x),
            static_cast<double>(lie.y),
            static_cast<double>(lie.z),
            static_cast<double>(mgv::course_terrain_height(lie.x, lie.z, description.terrain)));

        const auto statistics = engine->last_frame_statistics();
        std::printf(
            "submitted=%zu culled=%zu opaque=%zu translucent=%zu\n",
            statistics.submitted,
            statistics.culled,
            statistics.opaque,
            statistics.translucent);

        const auto image = framebuffer.toImage();
        const auto out = parser.value(out_option);
        if (!image.save(out, "PNG")) {
            std::fprintf(stderr, "Unable to write %s\n", qPrintable(out));
            return 1;
        }
        std::printf("wrote %s (%dx%d)\n", qPrintable(out), width, height);
        engine.reset();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "capture failed: %s\n", error.what());
        return 1;
    }

    framebuffer.release();
    context.doneCurrent();
    return 0;
}
