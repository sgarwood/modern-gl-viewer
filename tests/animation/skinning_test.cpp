#include "mgv/engine.hpp"
#include "mgv/gltf_loader.hpp"

#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <memory>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

/// A skeleton whose joints are deliberately *not* in the order the mesh's
/// skin lists them: an extra joint sits between the two the skin wants.
///
/// A skeleton and a mesh are usually converted by different tools and have
/// no reason to agree on joint order. If the binding matched by index, this
/// fixture would quietly skin the banner to the wrong bone.
class RiggedAssets final {
public:
    RiggedAssets() : directory_{make_directory()} {
        ozz::animation::offline::RawSkeleton raw_skeleton;
        raw_skeleton.roots.resize(1);
        auto& root = raw_skeleton.roots.front();
        root.name = "Root";
        root.transform.translation = ozz::math::Float3{0.0F, 0.0F, 0.0F};
        root.children.resize(2);
        root.children[0].name = "Extra";
        root.children[0].transform.translation = ozz::math::Float3{5.0F, 0.0F, 0.0F};
        root.children[1].name = "Tip";
        root.children[1].transform.translation = ozz::math::Float3{0.0F, 1.0F, 0.0F};

        const auto skeleton = ozz::animation::offline::SkeletonBuilder{}(raw_skeleton);
        if (!skeleton) {
            throw std::runtime_error{"Unable to build rigged test skeleton"};
        }

        ozz::animation::offline::RawAnimation raw_animation;
        raw_animation.name = "raise-tip";
        raw_animation.duration = 1.0F;
        raw_animation.tracks.resize(static_cast<std::size_t>(skeleton->num_joints()));
        for (int joint = 0; joint < skeleton->num_joints(); ++joint) {
            auto& track = raw_animation.tracks[static_cast<std::size_t>(joint)];
            const std::string name = skeleton->joint_names()[joint];
            const auto rest = name == "Tip" ? ozz::math::Float3{0.0F, 1.0F, 0.0F}
                            : name == "Extra" ? ozz::math::Float3{5.0F, 0.0F, 0.0F}
                                              : ozz::math::Float3{0.0F, 0.0F, 0.0F};
            // Still at rest at time zero; the tip rises one metre by the end.
            track.translations.push_back({0.0F, rest});
            const auto moved = name == "Tip"
                ? ozz::math::Float3{rest.x, rest.y + 1.0F, rest.z}
                : rest;
            track.translations.push_back({1.0F, moved});
        }

        const auto animation = ozz::animation::offline::AnimationBuilder{}(raw_animation);
        if (!animation) {
            throw std::runtime_error{"Unable to build rigged test animation"};
        }

        save(directory_ / "skeleton.ozz", *skeleton);
        save(directory_ / "animation.ozz", *animation);
    }

    ~RiggedAssets() {
        std::error_code ignored;
        std::filesystem::remove_all(directory_, ignored);
    }
    RiggedAssets(const RiggedAssets&) = delete;
    RiggedAssets& operator=(const RiggedAssets&) = delete;
    RiggedAssets(RiggedAssets&&) = delete;
    RiggedAssets& operator=(RiggedAssets&&) = delete;

    [[nodiscard]] mgv::animation::AnimationAssetPaths paths() const {
        return {.skeleton = directory_ / "skeleton.ozz", .animation = directory_ / "animation.ozz"};
    }

private:
    [[nodiscard]] static std::filesystem::path make_directory() {
        std::random_device device;
        auto path = std::filesystem::temp_directory_path() /
                    ("mgv-rigged-" + std::to_string(device()));
        std::filesystem::create_directories(path);
        return path;
    }

    template <typename Value>
    static void save(const std::filesystem::path& path, const Value& value) {
        ozz::io::File file{path.string().c_str(), "wb"};
        if (!file.opened()) {
            throw std::runtime_error{"Unable to write " + path.string()};
        }
        ozz::io::OArchive archive{&file};
        archive << value;
    }

    std::filesystem::path directory_;
};

class FakeMesh final : public mgv::MeshResource {};
class FakePipeline final : public mgv::RenderPipelineResource {};
class FakeTexture final : public mgv::TextureResource {};
class FakeSampler final : public mgv::SamplerResource {};

struct Captured final {
    std::vector<std::vector<mgv::Mat4>> palettes;
};

class CapturingBackend final : public mgv::RenderBackend {
public:
    explicit CapturingBackend(Captured& captured) : captured_{captured} {}

    [[nodiscard]] mgv::RenderBackendCapabilities capabilities() const noexcept override {
        return {.clip_space = {}, .wireframe = false, .max_sampled_textures = 16,
                .max_skinning_joints = 64};
    }
    std::unique_ptr<mgv::MeshResource> create_mesh(const mgv::MeshData&) override {
        return std::make_unique<FakeMesh>();
    }
    std::unique_ptr<mgv::RenderPipelineResource> create_pipeline(
        const mgv::RenderPipelineDescriptor&) override {
        return std::make_unique<FakePipeline>();
    }
    std::unique_ptr<mgv::TextureResource> create_texture(const mgv::ImageData&) override {
        return std::make_unique<FakeTexture>();
    }
    std::unique_ptr<mgv::SamplerResource> create_sampler(const mgv::SamplerDescriptor&) override {
        return std::make_unique<FakeSampler>();
    }
    void begin_frame(const mgv::Frame&) override { captured_.palettes.clear(); }
    void draw(const mgv::DrawPacket& packet) override {
        captured_.palettes.emplace_back(packet.joints.begin(), packet.joints.end());
    }
    void end_frame() noexcept override {}

private:
    Captured& captured_;
};

[[nodiscard]] mgv::Scene banner_scene(const mgv::ImportedModel& model) {
    auto material = std::make_shared<const mgv::Material>(
        mgv::ShaderSources{"vertex", "fragment", "skinned.vert", "skinned.frag"});
    mgv::Scene scene;
    scene.add(mgv::Renderable{
        std::make_shared<const mgv::MeshData>(model.primitives.front().mesh),
        std::make_shared<const mgv::MaterialInstance>(mgv::MaterialInstance{std::move(material)}),
    });
    return scene;
}

[[nodiscard]] mgv::ImportedModel load_banner() {
    return mgv::GltfLoader{}.load(
        std::filesystem::path{MGV_TEST_FIXTURES} / "rigged" / "banner.gltf");
}

} // namespace

TEST_CASE("a bound skin reaches the backend as one matrix per skin joint") {
    const RiggedAssets assets;
    const auto model = load_banner();
    REQUIRE(model.skin.has_value());

    Captured captured;
    mgv::Engine engine{std::make_unique<CapturingBackend>(captured)};
    const auto entities = engine.set_scene(banner_scene(model));
    REQUIRE(entities.size() == 1);

    const auto clip = engine.load_animation(assets.paths());
    const auto player = engine.bind_animation(entities.front(), clip);
    engine.bind_skin(entities.front(), player, *model.skin);

    engine.tick({64, 64});

    REQUIRE(captured.palettes.size() == 1);
    // Two joints in the skin, even though the skeleton has three.
    CHECK(captured.palettes.front().size() == 2);
}

TEST_CASE("at the rest pose the skinning palette is the identity") {
    const RiggedAssets assets;
    const auto model = load_banner();

    Captured captured;
    mgv::Engine engine{std::make_unique<CapturingBackend>(captured)};
    const auto entities = engine.set_scene(banner_scene(model));
    const auto clip = engine.load_animation(assets.paths());
    const auto player = engine.bind_animation(entities.front(), clip);
    engine.bind_skin(entities.front(), player, *model.skin);

    engine.tick({64, 64});

    // A joint matrix times its own inverse bind is the identity when the
    // joint has not moved. If this is not the identity, a character appears
    // mangled before it has been animated at all -- which is the single most
    // common way skinning is wired up wrong.
    // Ozz quantises its keyframes -- rotations to three sixteen-bit
    // components, translations to half floats -- so a round trip through a
    // clip is accurate to around one part in ten thousand, not to the bit.
    // The tolerance is that of the data, not of the arithmetic.
    constexpr float compression_tolerance = 2.0e-4F;
    REQUIRE(captured.palettes.front().size() == 2);
    for (const auto& matrix : captured.palettes.front()) {
        for (std::size_t element = 0; element < 16; ++element) {
            CHECK(matrix[element] ==
                  Catch::Approx(mgv::identity_matrix[element]).margin(compression_tolerance));
        }
    }
}

TEST_CASE("animating a joint moves only the part of the palette it owns") {
    const RiggedAssets assets;
    const auto model = load_banner();

    Captured captured;
    mgv::Engine engine{std::make_unique<CapturingBackend>(captured)};
    const auto entities = engine.set_scene(banner_scene(model));
    const auto clip = engine.load_animation(assets.paths());
    const auto player = engine.bind_animation(entities.front(), clip);
    engine.bind_skin(entities.front(), player, *model.skin);

    engine.enqueue(mgv::SeekAnimationCommand{player, mgv::animation::AnimationDuration{1.0F}});
    engine.tick({64, 64});

    REQUIRE(captured.palettes.front().size() == 2);
    const auto& root = captured.palettes.front()[0];
    const auto& tip = captured.palettes.front()[1];

    SECTION("the root is untouched") {
        CHECK(root[13] == Catch::Approx(0.0F).margin(2.0e-4F));
    }
    SECTION("the tip has risen by exactly what the clip asked for") {
        // Column-major: element 13 is the Y translation.
        CHECK(tip[13] == Catch::Approx(1.0F).margin(2.0e-3F));
    }
}

TEST_CASE("a skin naming a joint the skeleton lacks is refused") {
    const RiggedAssets assets;
    auto model = load_banner();
    model.skin->joint_names[1] = "NoSuchBone";

    Captured captured;
    mgv::Engine engine{std::make_unique<CapturingBackend>(captured)};
    const auto entities = engine.set_scene(banner_scene(model));
    const auto clip = engine.load_animation(assets.paths());
    const auto player = engine.bind_animation(entities.front(), clip);

    CHECK_THROWS_WITH(
        engine.bind_skin(entities.front(), player, *model.skin),
        Catch::Matchers::ContainsSubstring("NoSuchBone"));
}

TEST_CASE("a skin missing an inverse bind matrix is refused") {
    const RiggedAssets assets;
    auto model = load_banner();
    model.skin->inverse_bind_matrices.pop_back();

    Captured captured;
    mgv::Engine engine{std::make_unique<CapturingBackend>(captured)};
    const auto entities = engine.set_scene(banner_scene(model));
    const auto clip = engine.load_animation(assets.paths());
    const auto player = engine.bind_animation(entities.front(), clip);

    CHECK_THROWS_AS(
        engine.bind_skin(entities.front(), player, *model.skin), std::invalid_argument);
}
