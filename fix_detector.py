import re

with open('src/physics/collision_detector.cpp', 'r') as f:
    content = f.read()

# First, remove any duplicate sphere_heightmap functions if they exist (to clean up the mess)
content = re.sub(r'\[\[nodiscard\]\] std::optional<Manifold> sphere_heightmap.*?^\}', '', content, flags=re.DOTALL | re.MULTILINE)

# Now, insert sphere_heightmap correctly inside the anonymous namespace
sphere_heightmap_code = """
[[nodiscard]] std::optional<Manifold> sphere_heightmap(
    const SphereCollider& sphere,
    Vec3 sphere_position,
    const HeightmapCollider& heightmap,
    Vec3 heightmap_position) {
    
    // Transform sphere into heightmap local space
    Vec3 local_pos = subtract(sphere_position, heightmap_position);
    
    // Map to grid coordinates
    float gx = local_pos.x / heightmap.scale_x;
    float gz = local_pos.z / heightmap.scale_z;
    
    // Check bounds
    if (gx < 0 || gx >= heightmap.width - 1 || gz < 0 || gz >= heightmap.depth - 1) {
        return std::nullopt;
    }
    
    int x0 = static_cast<int>(gx);
    int z0 = static_cast<int>(gz);
    float tx = gx - x0;
    float tz = gz - z0;
    
    // Get heights of the 4 corners of the quad
    float h00 = heightmap.heights[z0 * heightmap.width + x0];
    float h10 = heightmap.heights[z0 * heightmap.width + (x0 + 1)];
    float h01 = heightmap.heights[(z0 + 1) * heightmap.width + x0];
    float h11 = heightmap.heights[(z0 + 1) * heightmap.width + (x0 + 1)];
    
    // Bilinear interpolation for height
    float hy0 = h00 * (1 - tx) + h10 * tx;
    float hy1 = h01 * (1 - tx) + h11 * tx;
    float height_at_pos = hy0 * (1 - tz) + hy1 * tz;
    
    // If the sphere is above the ground by more than its radius, no collision
    if (local_pos.y > height_at_pos + sphere.radius.metres()) {
        return std::nullopt;
    }
    
    // Calculate normal using cross product of diagonals
    Vec3 v1 = {heightmap.scale_x, h10 - h00, 0.0f};
    Vec3 v2 = {0.0f, h01 - h00, heightmap.scale_z};
    Vec3 normal = {
        v1.y * v2.z - v1.z * v2.y,
        v1.z * v2.x - v1.x * v2.z,
        v1.x * v2.y - v1.y * v2.x
    };
    
    // Normalize
    float mag = std::sqrt(dot(normal, normal));
    if (mag > 0) {
        normal = scaled(normal, 1.0f / mag);
    } else {
        normal = {0.0f, 1.0f, 0.0f};
    }
    
    // Calculate penetration distance
    float penetration = (height_at_pos + sphere.radius.metres()) - local_pos.y;
    
    if (penetration > 0) {
        return Manifold{.normal = normal, .penetration = penetration};
    }
    return std::nullopt;
}
"""

content = content.replace("} // namespace", sphere_heightmap_code + "\n} // namespace")

# Update detect function properly
detect_block = """
std::optional<ContactManifold> DiscreteCollisionDetector::detect(
    const RigidBody& first,
    const RigidBody& second) const {
    const auto& first_shape = first.collider().shape();
    const auto& second_shape = second.collider().shape();
    const auto first_position = first.position().metres();
    const auto second_position = second.position().metres();
    std::optional<Manifold> manifold;

    if (const auto* first_sphere = std::get_if<SphereCollider>(&first_shape)) {
        if (const auto* second_sphere = std::get_if<SphereCollider>(&second_shape)) {
            manifold = sphere_sphere(*first_sphere, first_position, *second_sphere, second_position);
        } else if (const auto* second_box = std::get_if<BoxCollider>(&second_shape)) {
            manifold = sphere_box(*first_sphere, first_position, *second_box, second_position);
        } else if (const auto* second_hm = std::get_if<HeightmapCollider>(&second_shape)) {
            manifold = sphere_heightmap(*first_sphere, first_position, *second_hm, second_position);
        }
    } else if (const auto* first_box = std::get_if<BoxCollider>(&first_shape)) {
        if (const auto* second_sphere = std::get_if<SphereCollider>(&second_shape)) {
            manifold = sphere_box(*second_sphere, second_position, *first_box, first_position);
            if (manifold) manifold->normal = negate(manifold->normal);
        } else if (const auto* second_box = std::get_if<BoxCollider>(&second_shape)) {
            manifold = box_box(*first_box, first_position, *second_box, second_position);
        }
    } else if (const auto* first_hm = std::get_if<HeightmapCollider>(&first_shape)) {
        if (const auto* second_sphere = std::get_if<SphereCollider>(&second_shape)) {
            manifold = sphere_heightmap(*second_sphere, second_position, *first_hm, first_position);
            if (manifold) manifold->normal = negate(manifold->normal);
        }
    }

    if (!manifold) {
        return std::nullopt;
    }
    return ContactManifold{manifold->normal, Length{manifold->penetration}};
}
"""

content = re.sub(r'std::optional<ContactManifold> DiscreteCollisionDetector::detect.*?^}', detect_block.strip(), content, flags=re.DOTALL | re.MULTILINE)

with open('src/physics/collision_detector.cpp', 'w') as f:
    f.write(content)
