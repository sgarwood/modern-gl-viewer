import sys
from pathlib import Path
import bpy
from math import radians
from mathutils import Matrix, Vector

MAPPING = [
    ("Hips", "Hips"), ("Spine", "Abdomen"), ("Spine1", "Torso"), ("Neck1", "Neck"),
    ("Head", "Head"), ("LeftShoulder", "Shoulder.L"), ("LeftArm", "UpperArm.L"),
    ("LeftForeArm", "LowerArm.L"), ("LeftHand", "Palm.L"),
    ("RightShoulder", "Shoulder.R"), ("RightArm", "UpperArm.R"),
    ("RightForeArm", "LowerArm.R"), ("RightHand", "Palm.R"),
    ("LeftUpLeg", "UpperLeg.L"), ("LeftLeg", "LowerLeg.L"), ("LeftFoot", "Foot.L"),
    ("RightUpLeg", "UpperLeg.R"), ("RightLeg", "LowerLeg.R"), ("RightFoot", "Foot.R"),
]
SHIN_OF_FOOT = {"Foot.L": "LowerLeg.L", "Foot.R": "LowerLeg.R"}

def arguments():
    after = sys.argv[sys.argv.index("--") + 1:]
    return Path(after[0]).resolve(), Path(after[1]).resolve(), Path(after[2]).resolve(), int(after[3]) if len(after) > 3 else 2, after[4] if len(after) > 4 else None, float(after[5]) if len(after) > 5 else 0.0

def only_armature(objects):
    return [o for o in objects if o.type == "ARMATURE"][0]

def main():
    character, motion, out, step, trim, yaw = arguments()
    bpy.ops.wm.read_factory_settings(use_empty=True)

    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(character))
    target = only_armature(set(bpy.data.objects) - before)

    before = set(bpy.data.objects)
    bpy.ops.import_anim.bvh(filepath=str(motion), update_scene_fps=True, update_scene_duration=True)
    source = only_armature(set(bpy.data.objects) - before)

    source.rotation_mode = "XYZ"
    source.rotation_euler.z += radians(yaw)
    bpy.context.view_layer.update()

    source_to_world = source.matrix_world.to_quaternion()
    target_to_world = target.matrix_world.to_quaternion()
    world_to_target = target_to_world.inverted()
    source_rest = {name: source_to_world @ source.data.bones[name].matrix_local.to_quaternion() for name, _ in MAPPING}
    target_rest = {name: target_to_world @ target.data.bones[name].matrix_local.to_quaternion() for _, name in MAPPING}

    source_hip = (source.matrix_world @ source.data.bones["Hips"].matrix_local).to_translation()
    target_hip = (target.matrix_world @ target.data.bones["Hips"].matrix_local).to_translation()
    scale = (target_hip.z / source_hip.z) if abs(source_hip.z) > 1e-6 else 1.0

    shin_rest_rotation = {shin: target.data.bones[shin].matrix_local.to_quaternion() for shin in SHIN_OF_FOOT.values()}
    ankle_offset = {foot: target.data.bones[foot].matrix_local.to_translation() - target.data.bones[shin].matrix_local.to_translation() for foot, shin in SHIN_OF_FOOT.items()}

    for bone in target.pose.bones: bone.rotation_mode = "QUATERNION"

    scene = bpy.context.scene
    start, end = scene.frame_start, scene.frame_end
    if trim:
        first, last = (int(v) for v in trim.split(":"))
        start, end = max(start, first), min(end, last)
    
    target.animation_data_create()
    action = bpy.data.actions.new("GolfSwing")
    target.animation_data.action = action

    bpy.ops.object.mode_set(mode='OBJECT')
    ik_target_L = bpy.data.objects.new("IK_Target_L", None); scene.collection.objects.link(ik_target_L)
    ik_target_R = bpy.data.objects.new("IK_Target_R", None); scene.collection.objects.link(ik_target_R)
    ik_rot_L = bpy.data.objects.new("IK_Rot_L", None); scene.collection.objects.link(ik_rot_L)
    ik_rot_R = bpy.data.objects.new("IK_Rot_R", None); scene.collection.objects.link(ik_rot_R)
    
    for ob in [ik_target_L, ik_target_R, ik_rot_L, ik_rot_R]:
        ob.rotation_mode = 'QUATERNION'

    lowest = []
    ground_rest = min(target.data.bones[foot].matrix_local.to_translation().z for foot in SHIN_OF_FOOT)
    root = target.pose.bones.get("Bone")
    origin = None
    written = 0
    for frame in range(start, end + 1, step):
        scene.frame_set(frame)
        out_frame = frame - start + 1

        hips_now = source.matrix_world @ source.pose.bones["Hips"].matrix.to_translation()
        if origin is None: origin = hips_now.copy()
        if root is not None:
            travel = (hips_now - origin) * scale
            travel.z = 0.0
            root.matrix = Matrix.Translation(travel) @ root.bone.matrix_local
            bpy.context.view_layer.update()

        for source_name, target_name in MAPPING:
            posed = source_to_world @ source.pose.bones[source_name].matrix.to_quaternion()
            delta = posed @ source_rest[source_name].inverted()
            want = world_to_target @ (delta @ target_rest[target_name])

            bone = target.pose.bones[target_name]
            shin = SHIN_OF_FOOT.get(target_name)
            if shin is None:
                head = bone.matrix.to_translation()
            else:
                posed_shin = target.pose.bones[shin]
                turn = posed_shin.matrix.to_quaternion() @ shin_rest_rotation[shin].inverted()
                head = posed_shin.matrix.to_translation() + (turn @ ankle_offset[target_name])
            bone.matrix = Matrix.Translation(head) @ want.to_matrix().to_4x4()
            bpy.context.view_layer.update()
            bone.keyframe_insert("rotation_quaternion", frame=out_frame)
            if shin: bone.keyframe_insert("location", frame=out_frame)

        if root is not None:
            ground = min(target.pose.bones[f].head.z for f in SHIN_OF_FOOT)
            root.matrix = Matrix.Translation(travel + (ground_rest - ground) * Vector((0.0, 0.0, 1.0))) @ root.bone.matrix_local
            bpy.context.view_layer.update()
            root.keyframe_insert("location", frame=out_frame)

        # FK Evaluation for IK
        mat_L = target.matrix_world @ target.pose.bones["Palm.L"].matrix
        mat_R = target.matrix_world @ target.pose.bones["Palm.R"].matrix
        palm_l_world = mat_L.to_translation()
        palm_r_world = mat_R.to_translation()
        head_world = (target.matrix_world @ target.pose.bones["Head"].matrix).to_translation()

        mid = (palm_l_world + palm_r_world) * 0.5
        to_hands = mid - head_world
        dist = to_hands.length
        min_dist = 0.55
        if dist < min_dist: mid = head_world + (to_hands / dist) * min_dist

        # Club shaft points along +Z of Palm.L in this rig
        club_shaft_dir = mat_L.to_quaternion() @ Vector((0, 0, 1))

        # Hands offset
        ik_target_L.location = mid - club_shaft_dir * 0.05
        ik_target_R.location = mid + club_shaft_dir * 0.05
        ik_target_L.keyframe_insert("location", frame=out_frame)
        ik_target_R.keyframe_insert("location", frame=out_frame)

        ik_rot_L.rotation_quaternion = mat_L.to_quaternion()
        ik_rot_R.rotation_quaternion = mat_R.to_quaternion()
        ik_rot_L.keyframe_insert("rotation_quaternion", frame=out_frame)
        ik_rot_R.keyframe_insert("rotation_quaternion", frame=out_frame)

        written += 1
        lowest.append((target.pose.bones["Palm.L"].head.z, frame))

    # Add constraints AFTER animation loop so they don't break FK extraction
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.mode_set(mode='POSE')
    
    ik_L = target.pose.bones["LowerArm.L"].constraints.new('IK')
    ik_L.target = ik_target_L
    ik_L.chain_count = 2
    
    ik_R = target.pose.bones["LowerArm.R"].constraints.new('IK')
    ik_R.target = ik_target_R
    ik_R.chain_count = 2

    rot_L = target.pose.bones["Palm.L"].constraints.new('COPY_ROTATION')
    rot_L.target = ik_rot_L
    rot_L.target_space = 'WORLD'
    rot_L.owner_space = 'WORLD'

    rot_R = target.pose.bones["Palm.R"].constraints.new('COPY_ROTATION')
    rot_R.target = ik_rot_R
    rot_R.target_space = 'WORLD'
    rot_R.owner_space = 'WORLD'

    scene.frame_start, scene.frame_end = 1, end - start + 1
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.data.objects.remove(source, do_unlink=True)
    
    bpy.ops.object.select_all(action="DESELECT")
    target.select_set(True)
    for child in target.children:
        child.select_set(True)
    
    bpy.ops.export_scene.gltf(
        filepath=str(out),
        export_format="GLB",
        export_animations=True,
        export_skins=True,
        export_yup=True,
        use_selection=True
    )
    print(f"wrote {out}")

main()
