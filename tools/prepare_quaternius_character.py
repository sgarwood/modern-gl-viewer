"""Prepare Quaternius' Male_Casual Blender asset for the MGV runtime.

Run with Blender, not CPython:

    blender --background Male_Casual.blend --python tools/prepare_quaternius_character.py \
        -- assets/characters/quaternius_male_casual.glb

The source model uses seven flat-colour materials. MGV currently gives a
skinned character one material, so this script converts those colours into a
small palette texture and gives every face a constant UV in its palette slot.
The rig and the model's Man_Idle action are retained.
"""

from __future__ import annotations

import sys
from pathlib import Path

import bpy


def output_path() -> Path:
    try:
        separator = sys.argv.index("--")
        value = sys.argv[separator + 1]
    except (ValueError, IndexError) as error:
        raise SystemExit("expected an output .glb path after --") from error
    return Path(value).resolve()


def main() -> None:
    mesh_object = bpy.data.objects["BaseHuman"]
    armature = bpy.data.objects["HumanArmature"]
    mesh = mesh_object.data

    source_colours = [tuple(material.diffuse_color) for material in mesh.materials]
    if not source_colours:
        raise RuntimeError("the Quaternius mesh has no source materials")

    stripe_width = 16
    image_width = stripe_width * len(source_colours)
    image_height = 16
    palette = bpy.data.images.new("QuaterniusPalette", image_width, image_height, alpha=True)
    pixels: list[float] = []
    for _y in range(image_height):
        for x in range(image_width):
            colour = source_colours[min(x // stripe_width, len(source_colours) - 1)]
            pixels.extend(colour)
    palette.pixels = pixels
    palette.pack()

    uv_layer = mesh.uv_layers.new(name="MaterialPalette")
    for polygon in mesh.polygons:
        u = (polygon.material_index * stripe_width + stripe_width / 2) / image_width
        for loop_index in polygon.loop_indices:
            uv_layer.data[loop_index].uv = (u, 0.5)
        polygon.material_index = 0

    material = bpy.data.materials.new("QuaterniusMaleCasual")
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = palette
    texture.interpolation = "Closest"
    principled = nodes.get("Principled BSDF")
    links.new(texture.outputs["Color"], principled.inputs["Base Color"])
    links.new(texture.outputs["Alpha"], principled.inputs["Alpha"])
    mesh.materials.clear()
    mesh.materials.append(material)

    idle = bpy.data.actions.get("Man_Idle")
    if idle is None:
        raise RuntimeError("the Quaternius model has no Man_Idle action")
    if armature.animation_data is None:
        armature.animation_data_create()
    armature.animation_data.action = idle

    bpy.ops.object.select_all(action="DESELECT")
    mesh_object.select_set(True)
    armature.select_set(True)
    bpy.context.view_layer.objects.active = armature

    destination = output_path()
    destination.parent.mkdir(parents=True, exist_ok=True)
    result = bpy.ops.export_scene.gltf(
        filepath=str(destination),
        export_format="GLB",
        use_selection=True,
        export_animations=True,
        export_animation_mode="ACTIVE_ACTIONS",
        export_nla_strips=False,
        export_force_sampling=True,
        export_skins=True,
        export_all_influences=False,
        export_materials="EXPORT",
        export_image_format="AUTO",
        export_yup=True,
        export_cameras=False,
        export_lights=False,
        export_copyright="Quaternius Male Casual character — CC0 1.0",
    )
    if result != {"FINISHED"}:
        raise RuntimeError(f"glTF export failed: {result}")


main()
