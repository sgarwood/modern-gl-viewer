#include "mgv/primitives.hpp"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace {

[[nodiscard]] float length(const mgv::Vec3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

} // namespace

TEST_CASE("a sky dome sits on a sphere of the requested radius") {
    const auto dome = mgv::make_sky_dome(500.0F, 8, 12);

    REQUIRE_FALSE(dome.empty());
    for (const auto& vertex : dome.vertices) {
        CHECK(length(vertex.position) == Catch::Approx(500.0F).epsilon(1.0e-4F));
    }
    CHECK(dome.indices.size() % 3 == 0);
}

TEST_CASE("sky dome normals face inwards towards the eye at its centre") {
    const auto dome = mgv::make_sky_dome(100.0F, 6, 10);

    for (const auto& vertex : dome.vertices) {
        const auto outward = vertex.position;
        const auto alignment = vertex.normal.x * outward.x + vertex.normal.y * outward.y +
                               vertex.normal.z * outward.z;
        CHECK(alignment < 0.0F);
        CHECK(length(vertex.normal) == Catch::Approx(1.0F).epsilon(1.0e-4F));
    }
}

TEST_CASE("a sky dome reaches the zenith and continues below the horizon") {
    const auto dome = mgv::make_sky_dome(10.0F, 16, 16);

    auto lowest = dome.vertices.front().position.y;
    auto highest = lowest;
    for (const auto& vertex : dome.vertices) {
        lowest = std::min(lowest, vertex.position.y);
        highest = std::max(highest, vertex.position.y);
    }
    CHECK(highest == Catch::Approx(10.0F).epsilon(1.0e-4F));
    CHECK(lowest < 0.0F);
}

TEST_CASE("sky dome construction rejects degenerate parameters") {
    CHECK_THROWS(mgv::make_sky_dome(0.0F, 8, 8));
    CHECK_THROWS(mgv::make_sky_dome(10.0F, 1, 8));
    CHECK_THROWS(mgv::make_sky_dome(10.0F, 8, 2));
}

TEST_CASE("a ground plane is centred, level, and fully subdivided") {
    const auto plane = mgv::make_ground_plane(8.0F, 4);

    CHECK(plane.vertices.size() == 25);
    CHECK(plane.indices.size() == 4 * 4 * 6);
    for (const auto& vertex : plane.vertices) {
        CHECK(vertex.position.y == 0.0F);
        CHECK(std::abs(vertex.position.x) <= 4.0F);
        CHECK(std::abs(vertex.position.z) <= 4.0F);
        CHECK(vertex.normal == mgv::Vec3{0.0F, 1.0F, 0.0F});
    }
}

TEST_CASE("ground plane construction rejects degenerate parameters") {
    CHECK_THROWS(mgv::make_ground_plane(0.0F, 4));
    CHECK_THROWS(mgv::make_ground_plane(4.0F, 0));
}

TEST_CASE("course terrain reproduces the physics surface across the green") {
    const mgv::CourseTerrainDescription description;

    // Inside the green the drawn surface must equal the analytic surface the
    // physics heightmap samples, or the ball rolls on something the player
    // cannot see.
    const auto analytic = [](float x, float z) {
        return -0.05F * z + 0.1F * std::sin(0.4F * z) * std::cos(0.4F * x);
    };
    for (float z = -7.0F; z <= 7.0F; z += 1.75F) {
        for (float x = -7.0F; x <= 7.0F; x += 1.75F) {
            CHECK(mgv::course_terrain_height(x, z, description) ==
                  Catch::Approx(analytic(x, z)).margin(1.0e-5F));
        }
    }
}

TEST_CASE("course terrain is continuous across the edge of the green") {
    const mgv::CourseTerrainDescription description;
    const auto edge = description.green_half_extent;

    const auto inside = mgv::course_terrain_height(0.0F, -edge + 0.01F, description);
    const auto outside = mgv::course_terrain_height(0.0F, -edge - 0.01F, description);

    CHECK(std::abs(inside - outside) < 0.01F);
}

TEST_CASE("course terrain rolls beyond the green") {
    const mgv::CourseTerrainDescription description;

    auto lowest = 0.0F;
    auto highest = 0.0F;
    for (float z = -300.0F; z <= 300.0F; z += 7.0F) {
        for (float x = -300.0F; x <= 300.0F; x += 7.0F) {
            const auto relief =
                mgv::course_terrain_height(x, z, description) + 0.05F * z;
            lowest = std::min(lowest, relief);
            highest = std::max(highest, relief);
        }
    }
    CHECK(highest - lowest > 1.0F);
}

TEST_CASE("the surface class cuts the green, its collar, the approach, and the rough") {
    const mgv::CourseTerrainDescription description;
    const auto half = description.green_half_extent;

    const auto putting = mgv::course_surface_class(0.0F, 0.0F, description);
    const auto collar = mgv::course_surface_class(0.0F, -half * 0.95F, description);
    const auto approach = mgv::course_surface_class(0.0F, -60.0F, description);
    const auto rough = mgv::course_surface_class(90.0F, -60.0F, description);

    SECTION("each surface is cut shorter than the one outside it") {
        CHECK(putting.cut_height < collar.cut_height);
        CHECK(collar.cut_height <= approach.cut_height);
        CHECK(approach.cut_height < rough.cut_height);
    }
    SECTION("a putting surface is a few millimetres and rough is centimetres") {
        CHECK(putting.cut_height < 0.005F);
        CHECK(rough.cut_height > 0.04F);
        // The eye reads cut height, so the two have to differ by an order of
        // magnitude, not by a tint.
        CHECK(rough.cut_height / putting.cut_height > 10.0F);
    }
    SECTION("the green and the approach are mown across each other") {
        CHECK(putting.mow < -0.9F);
        CHECK(approach.mow > 0.9F);
    }
    SECTION("an unmown collar separates the two patterns") {
        // Walking out from the middle of the green to the approach, the mow
        // signal has to pass through zero somewhere: that unmown ring is what
        // gives a green its crisp edge instead of letting the two striping
        // patterns run into each other.
        auto quietest = 1.0F;
        auto quietest_cut = 0.0F;
        for (float radius = half * 0.5F; radius <= half * 1.4F; radius += half * 0.01F) {
            const auto sample = mgv::course_surface_class(0.0F, -radius, description);
            if (std::abs(sample.mow) < quietest) {
                quietest = std::abs(sample.mow);
                quietest_cut = sample.cut_height;
            }
        }
        CHECK(quietest < 0.1F);
        SECTION("and it is cut between the two") {
            CHECK(quietest_cut > putting.cut_height);
            CHECK(quietest_cut < approach.cut_height);
        }
    }
    SECTION("rough carries no stripes at all") {
        CHECK(std::abs(rough.mow) < 1.0e-4F);
    }
}

TEST_CASE("the surface class stays within its declared ranges") {
    const mgv::CourseTerrainDescription description;

    for (float z = -400.0F; z <= 400.0F; z += 11.0F) {
        for (float x = -400.0F; x <= 400.0F; x += 11.0F) {
            const auto surface = mgv::course_surface_class(x, z, description);
            CHECK(surface.cut_height > 0.0F);
            CHECK(surface.cut_height <= 0.07F);
            CHECK(surface.mow >= -1.0F);
            CHECK(surface.mow <= 1.0F);
        }
    }
}

TEST_CASE("course terrain geometry is watertight where the rings meet the green") {
    mgv::CourseTerrainDescription description;
    description.green_resolution = 8;
    description.rings = 4;
    description.outer_extent = 80.0F;

    const auto terrain = mgv::make_course_terrain(description);

    REQUIRE_FALSE(terrain.empty());
    CHECK(terrain.indices.size() % 3 == 0);
    for (const auto index : terrain.indices) {
        CHECK(index < terrain.vertices.size());
    }

    SECTION("every vertex sits on the analytic surface") {
        for (const auto& vertex : terrain.vertices) {
            CHECK(vertex.position.y ==
                  Catch::Approx(mgv::course_terrain_height(
                                    vertex.position.x, vertex.position.z, description))
                      .margin(1.0e-4F));
        }
    }
    SECTION("the terrain reaches its outer extent") {
        auto furthest = 0.0F;
        for (const auto& vertex : terrain.vertices) {
            furthest = std::max(furthest, std::abs(vertex.position.x));
        }
        CHECK(furthest == Catch::Approx(80.0F).epsilon(1.0e-4F));
    }
}

TEST_CASE("course terrain construction rejects degenerate descriptions") {
    mgv::CourseTerrainDescription description;
    description.green_resolution = 4;
    description.rings = 2;

    auto no_green = description;
    no_green.green_half_extent = 0.0F;
    CHECK_THROWS(mgv::make_course_terrain(no_green));

    auto inverted = description;
    inverted.outer_extent = description.green_half_extent * 0.5F;
    CHECK_THROWS(mgv::make_course_terrain(inverted));

    auto no_rings = description;
    no_rings.rings = 0;
    CHECK_THROWS(mgv::make_course_terrain(no_rings));
}

TEST_CASE("a grass blade tapers to a single vertex at its tip") {
    const auto blade = mgv::make_grass_blade(4);

    REQUIRE_FALSE(blade.empty());
    CHECK(blade.vertices.size() == 9);   // four pairs plus the tip
    CHECK(blade.indices.size() == 3 * 7);

    const auto& tip = blade.vertices.back();
    CHECK(tip.position.x == 0.0F);
    CHECK(tip.position.y == Catch::Approx(1.0F));

    SECTION("the blade is of unit height, so instance scale is its length") {
        auto tallest = 0.0F;
        for (const auto& vertex : blade.vertices) {
            tallest = std::max(tallest, vertex.position.y);
            CHECK(vertex.position.y >= 0.0F);
        }
        CHECK(tallest == Catch::Approx(1.0F));
    }
    SECTION("it narrows from base to tip") {
        CHECK(std::abs(blade.vertices[0].position.x) >
              std::abs(blade.vertices[6].position.x));
    }
}

TEST_CASE("a grass blade rejects a degenerate segment count") {
    CHECK_THROWS(mgv::make_grass_blade(0));
}

TEST_CASE("a grass field takes each blade from the cut where it stands") {
    const mgv::CourseTerrainDescription terrain;
    mgv::GrassFieldDescription field;
    field.centre = {40.0F, -60.0F};   // deep in the rough, beside the approach
    field.radius = 6.0F;
    field.density = 120.0F;

    const auto grass = mgv::make_grass_field(field, terrain);

    REQUIRE(grass.instances.size() > 50);
    for (const auto& instance : grass.instances) {
        const auto height = instance.parameters.x;
        const auto width = instance.parameters.y;
        CHECK(height > 0.0F);
        CHECK(height < 0.12F);
        CHECK(width > 0.0F);
        CHECK(width < height);

        SECTION("every blade stands on the terrain") {
            CHECK(instance.position.y == Catch::Approx(mgv::course_terrain_height(
                      instance.position.x, instance.position.z, terrain)).margin(1.0e-4F));
        }
        SECTION("and inside the field") {
            const auto dx = instance.position.x - field.centre.x;
            const auto dz = instance.position.z - field.centre.y;
            CHECK(std::sqrt(dx * dx + dz * dz) <= field.radius + 1.0e-3F);
        }
    }
}

TEST_CASE("rough grows taller blades than a fairway does") {
    const mgv::CourseTerrainDescription terrain;
    const auto average_height = [&terrain](mgv::Vec2 centre) {
        mgv::GrassFieldDescription field;
        field.centre = centre;
        field.radius = 4.0F;
        field.density = 300.0F;
        const auto grass = mgv::make_grass_field(field, terrain);
        auto total = 0.0F;
        for (const auto& instance : grass.instances) {
            total += instance.parameters.x;
        }
        return total / static_cast<float>(grass.instances.size());
    };

    const auto fairway = average_height({0.0F, -60.0F});
    const auto rough = average_height({45.0F, -60.0F});

    CHECK(rough > fairway * 3.0F);
}

TEST_CASE("a putting surface grows no blades worth drawing") {
    const mgv::CourseTerrainDescription terrain;
    mgv::GrassFieldDescription field;
    field.centre = {0.0F, 0.0F};
    field.radius = 3.0F;
    field.density = 400.0F;

    const auto grass = mgv::make_grass_field(field, terrain);

    // Three millimetres of bentgrass is below a pixel from any stance, so the
    // field degenerates to the single placeholder instance.
    REQUIRE(grass.instances.size() == 1);
    CHECK(grass.instances.front().parameters.x == 0.0F);
}

TEST_CASE("a grass field rejects a degenerate description") {
    const mgv::CourseTerrainDescription terrain;
    mgv::GrassFieldDescription field;

    auto no_radius = field;
    no_radius.radius = 0.0F;
    CHECK_THROWS(mgv::make_grass_field(no_radius, terrain));

    auto no_density = field;
    no_density.density = 0.0F;
    CHECK_THROWS(mgv::make_grass_field(no_density, terrain));
}

namespace {

struct Extent final {
    float width{};
    float height{};
    float foliage_base{};
};

[[nodiscard]] Extent measure(const mgv::MeshData& tree) {
    auto lowest_foliage = std::numeric_limits<float>::max();
    auto highest = 0.0F;
    auto widest = 0.0F;
    for (const auto& vertex : tree.vertices) {
        highest = std::max(highest, vertex.position.y);
        widest = std::max(widest, std::sqrt(vertex.position.x * vertex.position.x +
                                            vertex.position.z * vertex.position.z));
        // tex_coord.x marks canopy rather than bark.
        if (vertex.tex_coord.x > 0.5F) {
            lowest_foliage = std::min(lowest_foliage, vertex.position.y);
        }
    }
    return {.width = widest * 2.0F, .height = highest, .foliage_base = lowest_foliage};
}

[[nodiscard]] mgv::MeshData grow(mgv::TreeSpecies species) {
    return mgv::make_tree({.species = species, .height = 12.0F, .spread = 1.0F, .seed = 5u});
}

} // namespace

TEST_CASE("each species is identifiable by its proportions alone") {
    const auto oak = measure(grow(mgv::TreeSpecies::oak));
    const auto beech = measure(grow(mgv::TreeSpecies::beech));
    const auto maple = measure(grow(mgv::TreeSpecies::maple));
    const auto pine = measure(grow(mgv::TreeSpecies::pine));

    SECTION("an oak spreads wider than anything else") {
        CHECK(oak.width > beech.width);
        CHECK(oak.width > maple.width);
        CHECK(oak.width > pine.width);
    }
    SECTION("a pine is the narrowest") {
        CHECK(pine.width < beech.width);
        CHECK(pine.width < maple.width);
    }
    SECTION("a beech carries its crown higher than an oak") {
        CHECK(beech.foliage_base > oak.foliage_base);
    }
    SECTION("every species reaches about the height it was asked for") {
        for (const auto& extent : {oak, beech, maple, pine}) {
            CHECK(extent.height > 9.0F);
            CHECK(extent.height < 16.0F);
        }
    }
}

TEST_CASE("spread widens a crown without raising it") {
    const auto narrow = measure(
        mgv::make_tree({.species = mgv::TreeSpecies::maple, .height = 12.0F, .spread = 0.7F, .seed = 3u}));
    const auto wide = measure(
        mgv::make_tree({.species = mgv::TreeSpecies::maple, .height = 12.0F, .spread = 1.4F, .seed = 3u}));

    CHECK(wide.width > narrow.width * 1.4F);
    CHECK(wide.height == Catch::Approx(narrow.height).epsilon(0.25F));
}

TEST_CASE("a tree is built from both bark and canopy") {
    const auto tree = grow(mgv::TreeSpecies::oak);

    auto bark = 0;
    auto canopy = 0;
    for (const auto& vertex : tree.vertices) {
        (vertex.tex_coord.x > 0.5F ? canopy : bark) += 1;
    }
    CHECK(bark > 0);
    CHECK(canopy > bark);
    CHECK(tree.indices.size() % 3 == 0);
    for (const auto index : tree.indices) {
        CHECK(index < tree.vertices.size());
    }
}

TEST_CASE("a tree rejects a degenerate description") {
    CHECK_THROWS(mgv::make_tree({.species = mgv::TreeSpecies::oak, .height = 0.0F}));
    CHECK_THROWS(mgv::make_tree({.species = mgv::TreeSpecies::oak, .height = 9.0F, .spread = 0.0F}));
}

TEST_CASE("every species has its own palette") {
    const auto oak = mgv::tree_palette(mgv::TreeSpecies::oak);
    const auto beech = mgv::tree_palette(mgv::TreeSpecies::beech);
    const auto pine = mgv::tree_palette(mgv::TreeSpecies::pine);

    SECTION("beech bark is the pale grey it is famous for") {
        CHECK(beech.bark.x > oak.bark.x * 1.5F);
        // Grey, not brown: the three channels stay close together.
        CHECK(beech.bark.x - beech.bark.z < 0.03F);
    }
    SECTION("pine needles are the darkest foliage") {
        CHECK(pine.leaf_sun.y < oak.leaf_sun.y);
        CHECK(pine.leaf_shade.y < oak.leaf_shade.y);
    }
    SECTION("sunlit foliage is lighter than shaded foliage") {
        for (const auto species : {mgv::TreeSpecies::oak, mgv::TreeSpecies::beech,
                                   mgv::TreeSpecies::maple, mgv::TreeSpecies::pine}) {
            const auto palette = mgv::tree_palette(species);
            CHECK(palette.leaf_sun.y > palette.leaf_shade.y);
        }
    }
}

TEST_CASE("a leaf is a closed outline about its stem") {
    const auto leaf = mgv::make_leaf(5);

    REQUIRE_FALSE(leaf.empty());
    CHECK(leaf.vertices.front().position.z == Catch::Approx(-0.5F));
    CHECK(leaf.vertices.back().position.z == Catch::Approx(0.5F));
    for (const auto& vertex : leaf.vertices) {
        CHECK(std::abs(vertex.position.x) < 0.3F);
        CHECK(vertex.position.y == 0.0F);
    }
    CHECK(leaf.indices.size() % 3 == 0);
}

TEST_CASE("a leaf rejects a degenerate segment count") {
    CHECK_THROWS(mgv::make_leaf(1));
}

TEST_CASE("leaf litter heaps into its drifts and rests on the terrain") {
    const mgv::CourseTerrainDescription terrain;
    const std::vector<mgv::LeafPile> piles{
        {.centre = {30.0F, -70.0F}, .radius = 1.6F, .depth = 0.12F},
        {.centre = {-25.0F, -40.0F}, .radius = 1.1F, .depth = 0.07F},
    };
    const mgv::LeafLitterDescription litter;

    const auto drift = mgv::make_leaf_litter(piles, litter, terrain);

    REQUIRE(drift.instances.size() > 200);
    for (const auto& leaf : drift.instances) {
        const auto ground = mgv::course_terrain_height(leaf.position.x, leaf.position.z, terrain);
        SECTION("no leaf is below the ground or floating above the heap") {
            CHECK(leaf.position.y >= ground - 1.0e-4F);
            CHECK(leaf.position.y <= ground + 0.13F);
        }
        SECTION("each leaf knows how deep it lies, for the shader to shade it") {
            CHECK(leaf.parameters.w >= 0.0F);
            CHECK(leaf.parameters.w <= 1.0F);
        }

        auto inside_a_pile = false;
        for (const auto& pile : piles) {
            const auto dx = leaf.position.x - pile.centre.x;
            const auto dz = leaf.position.z - pile.centre.y;
            inside_a_pile = inside_a_pile ||
                            std::sqrt(dx * dx + dz * dz) <= pile.radius + 1.0e-3F;
        }
        CHECK(inside_a_pile);
    }
}

TEST_CASE("leaf litter thins towards the rim of a drift") {
    const mgv::CourseTerrainDescription terrain;
    const std::vector<mgv::LeafPile> piles{
        {.centre = {0.0F, -60.0F}, .radius = 2.0F, .depth = 0.12F}};

    const auto drift = mgv::make_leaf_litter(piles, {}, terrain);

    // A drift that keeps its density to the very edge ends on a hard circle.
    // Compare the inner half of the disc with the outer half by area.
    auto inner = 0;
    auto outer = 0;
    for (const auto& leaf : drift.instances) {
        const auto distance = std::sqrt(
            leaf.position.x * leaf.position.x +
            (leaf.position.z + 60.0F) * (leaf.position.z + 60.0F));
        (distance < 2.0F * 0.7071F ? inner : outer) += 1;
    }
    REQUIRE(inner > 0);
    // Equal area, so equal density would give equal counts.
    CHECK(outer < inner);
}

TEST_CASE("leaf litter with no drifts draws nothing") {
    const mgv::CourseTerrainDescription terrain;

    const auto drift = mgv::make_leaf_litter({}, {}, terrain);

    REQUIRE(drift.instances.size() == 1);
    CHECK(drift.instances.front().parameters.x == 0.0F);
}

TEST_CASE("leaf litter rejects a degenerate description") {
    const mgv::CourseTerrainDescription terrain;
    const std::vector<mgv::LeafPile> piles{{.centre = {}, .radius = 1.0F, .depth = 0.1F}};

    mgv::LeafLitterDescription no_density;
    no_density.density = 0.0F;
    CHECK_THROWS(mgv::make_leaf_litter(piles, no_density, terrain));

    mgv::LeafLitterDescription no_size;
    no_size.leaf_length = 0.0F;
    CHECK_THROWS(mgv::make_leaf_litter(piles, no_size, terrain));
}
