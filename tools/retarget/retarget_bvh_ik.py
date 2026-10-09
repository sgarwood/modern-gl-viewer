"""Retarget a CMU BVH motion onto the shipped character's rig.

    blender --background --python retarget.py -- <character.glb> <motion.bvh> <out.glb> [step]

The two skeletons agree on nothing but anatomy: CMU's has different bone
names, different proportions and different rest orientations. What they do
share is a world frame once Blender has imported both, so the retarget
works in world space and copies how far each bone has turned *from its own
rest*, rather than its absolute orientation:

    delta       = source_pose * source_rest^-1
    destination = delta * destination_rest

Rotation only, except at the root and the feet. The feet need their
position carried too, because this rig parents Foot.L and Foot.R to the
armature root rather than to the shins -- it drives them by IK -- so a leg
that bends without them leaves its foot standing where it was.
"""

import sys
from pathlib import Path

import bpy
from math import radians

from mathutils import Matrix, Vector

# CMU joint -> rig bone, listed parent first so each bone is posed after
# whatever it hangs from.
MAPPING = [
    ("Hips", "Hips"),
    ("Spine", "Abdomen"),
    ("Spine1", "Torso"),
    ("Neck1", "Neck"),
    ("Head", "Head"),
    ("LeftShoulder", "Shoulder.L"),
    ("LeftArm", "UpperArm.L"),
    ("LeftForeArm", "LowerArm.L"),
    ("LeftHand", "Palm.L"),
    ("RightShoulder", "Shoulder.R"),
    ("RightArm", "UpperArm.R"),
    ("RightForeArm", "LowerArm.R"),
    ("RightHand", "Palm.R"),
    ("LeftUpLeg", "UpperLeg.L"),
    ("LeftLeg", "LowerLeg.L"),
    ("LeftFoot", "Foot.L"),
    ("RightUpLeg", "UpperLeg.R"),
    ("RightLeg", "LowerLeg.R"),
    ("RightFoot", "Foot.R"),
]
SHIN_OF_FOOT = {"Foot.L": "LowerLeg.L", "Foot.R": "LowerLeg.R"}


def arguments():
    try:
        after = sys.argv[sys.argv.index("--") + 1:]
        character, motion, out = after[0], after[1], after[2]
    except (ValueError, IndexError) as error:
        raise SystemExit("usage: ... -- <character.glb> <motion.bvh> <out.glb> [step]") from error
    step = int(after[3]) if len(after) > 3 else 2
    trim = after[4] if len(after) > 4 else None
    yaw = float(after[5]) if len(after) > 5 else 0.0
    return Path(character).resolve(), Path(motion).resolve(), Path(out).resolve(), step, trim, yaw


def only_armature(objects):
    found = [o for o in objects if o.type == "ARMATURE"]
    if len(found) != 1:
        raise SystemExit(f"expected exactly one armature, found {len(found)}")
    return found[0]


def main():
    character, motion, out, step, trim, yaw = arguments()
    bpy.ops.wm.read_factory_settings(use_empty=True)

    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(character))
    target = only_armature(set(bpy.data.objects) - before)

    before = set(bpy.data.objects)
    # Take the scene's rate from the capture. Left at Blender's 24 fps a
    # 120 fps capture exports five times too slow, which reads as a man
    # swinging underwater.
    bpy.ops.import_anim.bvh(filepath=str(motion), update_scene_fps=True,
                            update_scene_duration=True)
    source = only_armature(set(bpy.data.objects) - before)

    # Turn the capture to face where this course's target is. Which way the
    # subject happened to hit is a property of the recording session, not
    # of the swing.
    source.rotation_mode = "XYZ"
    source.rotation_euler.z += radians(yaw)
    bpy.context.view_layer.update()

    missing = [s for s, _ in MAPPING if s not in source.pose.bones]
    missing += [d for _, d in MAPPING if d not in target.pose.bones]
    if missing:
        raise SystemExit(f"these bones are not in their skeleton: {missing}")

    # Rest orientation of every bone, in world space, for both skeletons.
    # Orientations only, never whole matrices: an imported armature often
    # carries a scale on its object, and feeding that through the pose
    # matrices stretches every bone it touches.
    source_to_world = source.matrix_world.to_quaternion()
    target_to_world = target.matrix_world.to_quaternion()
    world_to_target = target_to_world.inverted()
    source_rest = {
        name: source_to_world @ source.data.bones[name].matrix_local.to_quaternion()
        for name, _ in MAPPING
    }
    target_rest = {
        name: target_to_world @ target.data.bones[name].matrix_local.to_quaternion()
        for _, name in MAPPING
    }

    # Match the rigs' sizes so the root travel means the same thing on both.
    source_hip = (source.matrix_world @ source.data.bones["Hips"].matrix_local).to_translation()
    target_hip = (target.matrix_world @ target.data.bones["Hips"].matrix_local).to_translation()
    scale = (target_hip.z / source_hip.z) if abs(source_hip.z) > 1e-6 else 1.0

    shin_rest_rotation = {
        shin: target.data.bones[shin].matrix_local.to_quaternion()
        for shin in SHIN_OF_FOOT.values()
    }
    ankle_offset = {
        foot: target.data.bones[foot].matrix_local.to_translation()
        - target.data.bones[shin].matrix_local.to_translation()
        for foot, shin in SHIN_OF_FOOT.items()
    }

    for bone in target.pose.bones:
        bone.rotation_mode = "QUATERNION"

    scene = bpy.context.scene
    start, end = scene.frame_start, scene.frame_end
    if trim:
        first, last = (int(v) for v in trim.split(":"))
        start, end = max(start, first), min(end, last)
    target.animation_data_create()
    action = bpy.data.actions.new("GolfSwing")
    target.animation_data.action = action


    bpy.context.view_layer.objects.active = target
    bpy.ops.object.mode_set(mode='POSE')
    

    
    # Create an empty to act as the right hand IK target (offset from Palm.L)
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.object.empty_add(type='PLAIN_AXES')
    ik_target = bpy.context.active_object
    ik_target.name = 'RightHandGrip'
    
    # Parent it to Palm.L
    copy_trans = ik_target.constraints.new('COPY_TRANSFORMS')
    copy_trans.target = target
    copy_trans.subtarget = 'Palm.L'
    
    # Offset the right hand down the club shaft (local -Z) and slightly outwards
    ik_target.delta_location = (0.0, 0.0, -0.1) # adjust as needed (0.1 units down)
    
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.mode_set(mode='POSE')
    
    ik = target.pose.bones["LowerArm.R"].constraints.new('IK')
    ik.target = ik_target
    ik.chain_count = 2

    lowest = []
    ground_rest = min(
        target.data.bones[foot].matrix_local.to_translation().z for foot in SHIN_OF_FOOT)
    root = target.pose.bones.get("Bone")
    origin = None
    written = 0
    for frame in range(start, end + 1, step):
        scene.frame_set(frame)
        # Keys are written rebased to frame one. Writing them at the source
        # frame numbers leaves a trimmed clip with its whole lead-in intact
        # as dead air, and the game plays that instead of the motion.
        out_frame = frame - start + 1

        hips_now = source.matrix_world @ source.pose.bones["Hips"].matrix.to_translation()
        if origin is None:
            origin = hips_now.copy()
        if root is not None:
            # Horizontal travel is taken from the capture. The vertical is
            # not: the subject has different proportions and stood on a
            # different floor, so carrying their hip height across either
            # lifts this character off the turf or buries them in it. The
            # height is decided afterwards, by the feet.
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
                # Carry the ankle on the shin. Not to the shin bone's tail:
                # this rig drives the legs by IK, so the shin is shorter
                # than the calf it stands for and its tail lands mid-leg.
                posed = target.pose.bones[shin]
                turn = posed.matrix.to_quaternion() @ shin_rest_rotation[shin].inverted()
                head = posed.matrix.to_translation() + (turn @ ankle_offset[target_name])
            bone.matrix = Matrix.Translation(head) @ want.to_matrix().to_4x4()
            bpy.context.view_layer.update()
            bone.keyframe_insert("rotation_quaternion", frame=out_frame)
            if shin:
                bone.keyframe_insert("location", frame=out_frame)
        # Stand them on the ground: drop the root until the lower foot is
        # back where it rests. Planting the feet and letting the hips find
        # their own height is the way round that survives a change of
        # proportions; pinning the hips instead leaves the feet hovering
        # through the finish, when the legs are at their straightest.
        if root is not None:
            ground = min(target.pose.bones[f].head.z for f in SHIN_OF_FOOT)
            root.matrix = Matrix.Translation(
                travel + (ground_rest - ground) * Vector((0.0, 0.0, 1.0))
            ) @ root.bone.matrix_local
            bpy.context.view_layer.update()
            root.keyframe_insert("location", frame=out_frame)

        written += 1
        lowest.append((target.pose.bones["Palm.L"].head.z, frame))
        if written == -1:
            for probe in ("Hips", "Torso", "Head", "Palm.L", "Foot.L", "LowerLeg.L"):
                head = target.pose.bones[probe].head
                print(f"    probe {probe:12s} ({head.x:7.2f},{head.y:7.2f},{head.z:7.2f})")
            print(f"    scale {scale:.4f}  travel {travel.length:.2f}")


    bpy.context.view_layer.objects.active = target
    target.select_set(True)
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.nla.bake(
        frame_start=1,
        frame_end=end - start + 1,
        only_selected=True,
        visual_keying=True,
        clear_constraints=True,
        use_current_action=True,
        bake_types={'POSE'}
    )

    if lowest:
        # Impact is where the hands bottom out *after* the top of the
        # backswing. Taking the lowest point of the whole take finds the
        # address instead, where the hands are just as low and nothing has
        # happened yet.
        # The hands are highest at the finish, not at the top of the
        # backswing, so the finish is the global maximum. Impact is the dip
        # in the second before it -- searching the whole take instead finds
        # the address, where the hands are just as low and nothing has
        # happened yet.
        finish_height, finish_frame = max(lowest)
        window = [(h, f) for h, f in lowest
                  if finish_frame - scene.render.fps >= f > finish_frame - 2 * scene.render.fps]
        window += [(h, f) for h, f in lowest if finish_frame > f > finish_frame - scene.render.fps]
        if window:
            depth, frame = min(window)
            print(f"    finish frame {finish_frame} (height {finish_height:.2f}); "
                  f"impact frame {frame} (height {depth:.2f})")
    print(f"retargeted {written} keys from {motion.name} ({start}..{end} step {step}) "
          f"at {scene.render.fps} fps = {(end - start) / scene.render.fps:.2f}s")
    # Drop the capture skeleton before writing. Left in, the file holds two
    # armatures, and CMU names Hips, Neck and Head exactly as this rig does
    # -- so a runtime that binds a skin to a skeleton by name can pick the
    # wrong bone and drag the head off into the sky.
    scene.frame_start, scene.frame_end = 1, end - start + 1
    bpy.ops.object.mode_set(mode='OBJECT')
    bpy.data.objects.remove(source, do_unlink=True)
    ik_empty = bpy.data.objects.get('RightHandGrip')
    if ik_empty:
        bpy.data.objects.remove(ik_empty, do_unlink=True)
    bpy.ops.object.select_all(action="DESELECT")
    bpy.ops.export_scene.gltf(filepath=str(out), export_format="GLB",
                              export_animations=True, export_skins=True,
                              export_yup=True)
    print(f"wrote {out}")


main()
