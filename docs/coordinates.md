# Coordinate-system contract

## Canonical world

The core uses a right-handed, Y-up world. With the default camera:

```text
              +Y (up)
               |
               |
               o---- +X (right)
              /
            +Z (toward the default camera)
```

`Camera` defaults to position `(0, 0, 3)`, target `(0, 0, 0)`, and up `(0, 1, 0)`. It looks along
−Z. Its default projection follows the OpenGL convention with normalized device Z in `[-1, +1]`.
The render backend reports its `ClipSpaceConvention`; the camera also supports `[0, +1]` depth and
inverted clip-space Y for APIs with different conventions. `Mat4` values are column-major.

The camera is a core value type, independent of Qt, GLFW, and OpenGL:

```cpp
mgv::Camera camera;
camera.look_at({4.0F, 3.0F, 6.0F}, {0.0F, 0.0F, 0.0F});
camera.set_perspective(50.0F, 0.1F, 500.0F);
renderer.set_camera(camera);
```

Invalid fields of view, clipping planes, aspect ratios, zero-length view directions, and parallel
up/view directions are rejected.

## OBJ input

Wavefront OBJ stores numbers but does not standardize or encode:

- world handedness;
- which axis is up;
- physical units;
- the texture-coordinate origin.

`ObjLoader` therefore preserves positions, explicit normals, and UVs exactly. Missing normals are
generated from the authored winding. The renderer centers the resulting bounds and applies a
uniform display scale, but does not rotate axes, mirror geometry, or convert units.

| Authored convention | Current result |
|---|---|
| Right-handed, Y-up | Directly matches the canonical world |
| Right-handed, Z-up | Loads; visually rotated until an import transform is added |
| Left-handed | Loads; may appear mirrored and have reversed face winding |
| Arbitrary units | Loads; display fitting removes the initial size difference |

The correct future extension point is an `ImportTransform` applied to positions and normals before
GPU resource creation. A handedness flip must also reverse triangle winding; doing only a scale of
`-1` on one axis is incomplete.
