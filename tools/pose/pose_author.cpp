// Authors a static pose as a one-key ozz animation.
//
// A pose is specified as "point this bone at this direction, in model space,
// then twist it this far about itself". Model space, because a rig's local
// joint axes are whatever the exporter felt like and guessing them is how an
// afternoon disappears. Twist, because aiming a bone leaves rotation about
// its own length undetermined, and that rotation is exactly what a golf
// finish is made of.
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/span.h>

#include "mgv/hardware/json.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>

namespace {

using Json = nlohmann::json;

struct Vec3 { float x{}, y{}, z{}; };
struct Quat { float x{}, y{}, z{}, w{1}; };

Vec3 normalize(Vec3 v) {
    const auto n = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    return n > 1e-6F ? Vec3{v.x/n, v.y/n, v.z/n} : Vec3{0,0,1};
}
Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x}; }
Vec3 add(Vec3 a, Vec3 b) { return {a.x+b.x, a.y+b.y, a.z+b.z}; }
float dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
Quat multiply(Quat a, Quat b) {
    return {a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
            a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
            a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w,
            a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z};
}
Quat conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }
Vec3 rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const auto s = q.w;
    const auto a = cross(u, v);
    const auto b = cross(u, a);
    return {v.x + 2*(s*a.x + b.x), v.y + 2*(s*a.y + b.y), v.z + 2*(s*a.z + b.z)};
}
Quat axis_angle(Vec3 axis, float radians) {
    const auto a = normalize(axis);
    const auto h = radians * 0.5F;
    const auto s = std::sin(h);
    return {a.x*s, a.y*s, a.z*s, std::cos(h)};
}
/// The shortest rotation taking `from` onto `to`.
Quat shortest_arc(Vec3 from, Vec3 to) {
    const auto f = normalize(from);
    const auto t = normalize(to);
    const auto d = dot(f, t);
    if (d > 0.99999F) { return {}; }
    if (d < -0.99999F) {
        auto axis = cross({1,0,0}, f);
        if (dot(axis, axis) < 1e-6F) { axis = cross({0,1,0}, f); }
        return axis_angle(axis, 3.14159265F);
    }
    const auto axis = cross(f, t);
    const auto q = Quat{axis.x, axis.y, axis.z, 1.0F + d};
    const auto n = std::sqrt(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    return {q.x/n, q.y/n, q.z/n, q.w/n};
}

/// One instruction: aim `joint`'s bone (towards `child`) along `direction`,
/// in model space, then twist `twist_degrees` about it.
///
/// Model space, because a rig's local joint axes are whatever its exporter
/// felt like and guessing them is how an afternoon disappears. Twist,
/// because aiming a bone leaves rotation about its own length undetermined,
/// and for a golf finish that rotation is most of the pose.
struct Aim {
    const char* joint;
    const char* child;        // nullptr to twist only
    Vec3 direction;
    float twist_degrees;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        // Authoring a pose for an unfamiliar rig starts with finding out
        // what the joints are called and which way the thing is lying, so
        // the tool answers that on its own.
        std::fprintf(
            stderr,
            "usage: pose_author <skeleton.ozz> [<pose.json> <out.ozz>]\n"
            "  with only a skeleton, lists its joints and their rest positions\n");
        if (argc != 2) {
            return 2;
        }
    }
    ozz::io::File in(argv[1], "rb");
    ozz::io::IArchive archive(&in);
    ozz::animation::Skeleton skeleton;
    archive >> skeleton;
    const auto count = skeleton.num_joints();

    // ---- the follow-through -------------------------------------------------
    // Rest model transforms, for the bone directions to aim from.
    std::vector<ozz::math::Float4x4> rest(count);
    {
        ozz::animation::LocalToModelJob job;
        job.skeleton = &skeleton;
        job.input = skeleton.joint_rest_poses();
        job.output = ozz::span<ozz::math::Float4x4>(rest.data(), rest.size());
        job.Run();
    }
    const auto position_of = [&](int joint) {
        alignas(16) float c[4];
        ozz::math::StorePtrU(rest[joint].cols[3], c);
        return Vec3{c[0], c[1], c[2]};
    };
    const auto index_of = [&](const char* name) {
        for (int j = 0; j < count; ++j) {
            if (std::strcmp(skeleton.joint_names()[j], name) == 0) { return j; }
        }
        std::fprintf(stderr, "no joint named %s\n", name);
        return -1;
    };

    if (argc == 2) {
        std::printf("%d joints\n", count);
        for (int j = 0; j < count; ++j) {
            const auto at = position_of(j);
            const auto parent = skeleton.joint_parents()[j];
            std::printf(
                "%2d  %-16s parent=%-16s (%7.3f, %7.3f, %7.3f)\n",
                j,
                skeleton.joint_names()[j],
                parent < 0 ? "-" : skeleton.joint_names()[parent],
                static_cast<double>(at.x),
                static_cast<double>(at.y),
                static_cast<double>(at.z));
        }
        return 0;
    }

    Json description;
    {
        std::ifstream input{argv[2]};
        if (!input) {
            std::fprintf(stderr, "cannot open pose %s\n", argv[2]);
            return 1;
        }
        input >> description;
    }
    std::vector<Aim> pose;
    std::vector<std::string> names;   // storage the Aim pointers refer to
    names.reserve(description.at("aims").size() * 2);
    for (const auto& entry : description.at("aims")) {
        names.push_back(entry.at("joint").get<std::string>());
        const auto* joint = names.back().c_str();
        const char* child = nullptr;
        if (entry.contains("child")) {
            names.push_back(entry.at("child").get<std::string>());
            child = names.back().c_str();
        }
        const auto direction = entry.value("direction", std::vector<float>{0, 0, 1});
        pose.push_back({
            joint,
            child,
            {direction.size() > 0 ? direction[0] : 0.0F,
             direction.size() > 1 ? direction[1] : 0.0F,
             direction.size() > 2 ? direction[2] : 1.0F},
            entry.value("twist_degrees", 0.0F),
        });
    }


    // Walk the hierarchy, resolving each aim against the model rotation its
    // parent has already accumulated.
    //
    // The accumulated rotation has to include each joint's own rest
    // rotation, not just the aim applied on top of it. Leaving the rest out
    // gives every joint below the root the wrong parent frame, and the aims
    // compound into a pose that is subtly and increasingly wrong the further
    // down the chain it goes -- which looks exactly like a slouch.
    const auto rest_rotation_of = [&](int joint) {
        const int soa = joint / 4, lane = joint % 4;
        const auto& r = skeleton.joint_rest_poses()[soa];
        alignas(16) float qx[4], qy[4], qz[4], qw[4];
        ozz::math::StorePtr(r.rotation.x, qx); ozz::math::StorePtr(r.rotation.y, qy);
        ozz::math::StorePtr(r.rotation.z, qz); ozz::math::StorePtr(r.rotation.w, qw);
        return Quat{qx[lane], qy[lane], qz[lane], qw[lane]};
    };

    std::vector<Quat> composed(count);
    std::vector<Quat> model(count);
    const auto parents = skeleton.joint_parents();
    for (int j = 0; j < count; ++j) {
        const char* name = skeleton.joint_names()[j];
        const auto parent_model = parents[j] < 0 ? Quat{} : model[parents[j]];
        Quat aimed;
        for (const auto& aim : pose) {
            if (std::strcmp(aim.joint, name) != 0) { continue; }
            Vec3 axis{0, 0, 1};
            if (aim.child != nullptr) {
                const auto child = index_of(aim.child);
                if (child < 0) { break; }
                const auto bone = position_of(child);
                const auto here = position_of(j);
                const Vec3 rest_direction{bone.x - here.x, bone.y - here.y, bone.z - here.z};
                // Both brought into this joint's parent frame.
                const auto from = rotate(conjugate(parent_model), normalize(rest_direction));
                const auto to = rotate(conjugate(parent_model), normalize(aim.direction));
                aimed = multiply(
                    axis_angle(to, aim.twist_degrees * 3.14159265F / 180.0F),
                    shortest_arc(from, to));
            } else {
                axis = rotate(conjugate(parent_model), normalize(aim.direction));
                aimed = axis_angle(axis, aim.twist_degrees * 3.14159265F / 180.0F);
            }
            break;
        }
        composed[j] = multiply(aimed, rest_rotation_of(j));
        model[j] = multiply(parent_model, composed[j]);
    }

    // Where the pose actually put everything. Authoring by aiming bones is
    // open-loop -- nothing checks that two hands meant to share a grip end
    // up in the same place -- so the tool reports the result and lets the
    // author close the loop.
    std::vector<Vec3> posed(count);
    for (int j = 0; j < count; ++j) {
        const int soa = j / 4, lane = j % 4;
        const auto& r = skeleton.joint_rest_poses()[soa];
        alignas(16) float tx[4], ty[4], tz[4];
        ozz::math::StorePtr(r.translation.x, tx);
        ozz::math::StorePtr(r.translation.y, ty);
        ozz::math::StorePtr(r.translation.z, tz);
        const Vec3 offset{tx[lane], ty[lane], tz[lane]};
        const auto parent = parents[j];
        posed[j] = parent < 0
            ? offset
            : add(posed[parent], rotate(model[parent], offset));
    }
    for (int j = 0; j < count; ++j) {
        std::printf(
            "    %-16s (%7.3f, %7.3f, %7.3f)\n",
            skeleton.joint_names()[j],
            static_cast<double>(posed[j].x),
            static_cast<double>(posed[j].y),
            static_cast<double>(posed[j].z));
    }

    // One key, at time zero. Translations and scales stay at rest: a pose
    // moves joints, it does not stretch them.
    ozz::animation::offline::RawAnimation raw;
    raw.name = description.value("name", std::string{"pose"});
    raw.duration = 1.0F;
    raw.tracks.resize(static_cast<std::size_t>(count));
    for (int j = 0; j < count; ++j) {
        const int soa = j / 4, lane = j % 4;
        const auto& r = skeleton.joint_rest_poses()[soa];
        alignas(16) float tx[4], ty[4], tz[4], sx[4], sy[4], sz[4];
        ozz::math::StorePtr(r.translation.x, tx); ozz::math::StorePtr(r.translation.y, ty);
        ozz::math::StorePtr(r.translation.z, tz);
        ozz::math::StorePtr(r.scale.x, sx); ozz::math::StorePtr(r.scale.y, sy);
        ozz::math::StorePtr(r.scale.z, sz);
        auto& track = raw.tracks[static_cast<std::size_t>(j)];
        const auto& q = composed[j];
        track.translations.push_back({0.0F, {tx[lane], ty[lane], tz[lane]}});
        track.rotations.push_back({0.0F, {q.x, q.y, q.z, q.w}});
        track.scales.push_back({0.0F, {sx[lane], sy[lane], sz[lane]}});
    }

    const auto animation = ozz::animation::offline::AnimationBuilder{}(raw);
    if (!animation) { std::fprintf(stderr, "failed to build pose\n"); return 1; }
    ozz::io::File out(argv[3], "wb");
    ozz::io::OArchive writer(&out);
    writer << *animation;
    std::printf("wrote %s (%d joints)\n", argv[3], count);
}
