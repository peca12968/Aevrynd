#include "Aevrynd/CharacterPresentation3D.h"

#include "Aevrynd/SkinnedMaterialSidecar3D.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>

namespace Aevrynd {
namespace {

void SetError(std::string* error, std::string value) {
    if (error) *error = std::move(value);
}

Surface3D CharacterSurface(std::string_view name) {
    const auto has = [&](std::string_view value) { return name.find(value) != std::string_view::npos; };
    if (has("Metal") || has("Gold") || has("Buckle")) return {.22f, .82f, 0.f, .96f};
    if (has("Eye") || has("Iris")) return {.16f, 0.f, 0.f, 1.f};
    if (has("Hair")) return {.31f, 0.f, 0.f, .94f};
    if (has("Skin") || has("Face")) return {.43f, 0.f, 0.f, 1.f};
    if (has("Lip")) return {.36f, 0.f, 0.f, 1.f};
    if (has("Leather") || has("Boot")) return {.48f, 0.f, 0.f, .96f};
    return {.68f, 0.f, 0.f, 1.f};
}

std::vector<Material3D> SourceCharacterMaterials(const SkinnedModelAssetData& asset) {
    std::vector<Material3D> result;
    result.reserve(asset.materialSlotNames.size());
    for (std::size_t slot = 0; slot < asset.materialSlotNames.size(); ++slot) {
        Material3D material;
        material.useUv = false;
        if (asset.materials.empty()) {
            material.surface = CharacterSurface(asset.materialSlotNames[slot]);
        } else {
            const SkinnedMaterialDefinition3D& source = asset.materials[slot];
            material.tint = source.baseColorFactor;
            material.surface = {
                source.roughnessFactor,
                source.metallicFactor,
                Clamp((std::max)({source.emissiveFactor.x, source.emissiveFactor.y, source.emissiveFactor.z}) * source.emissiveStrength, 0.f, 1.f),
                source.occlusionStrength};
            material.normalScale = source.normalScale;
            material.alphaMode = source.alphaMode == SkinnedMaterialAlphaMode3D::Mask
                ? MaterialAlphaMode3D::Mask
                : source.alphaMode == SkinnedMaterialAlphaMode3D::Blend ? MaterialAlphaMode3D::Blend : MaterialAlphaMode3D::Opaque;
            material.alphaCutoff = source.alphaCutoff;
            material.doubleSided = source.doubleSided;
        }
        result.push_back(material);
    }
    return result;
}

Quaternion3D Multiply(Quaternion3D left, Quaternion3D right) {
    return NormalizeQuaternion3D({
        left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
        left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
        left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
        left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z});
}

JointTransform3D Delta(JointTransform3D bind, Vec3 rotation, Vec3 translation = {}) {
    bind.translation = bind.translation + translation;
    if (bind.usesQuaternion) bind.quaternion = Multiply(bind.quaternion, QuaternionFromEuler3D(rotation));
    else bind.rotation = bind.rotation + rotation;
    return bind;
}

AnimationTrack3D Track(const Skeleton3D& skeleton, std::size_t joint, float duration,
    std::initializer_list<std::pair<float, Vec3>> rotations,
    std::initializer_list<std::pair<float, Vec3>> translations = {}) {
    AnimationTrack3D track;
    track.joint = joint;
    for (const auto& [phase, rotation] : rotations) {
        Vec3 translation{};
        for (const auto& [translationPhase, value] : translations) {
            if (std::abs(translationPhase - phase) <= .00001f) {
                translation = value;
                break;
            }
        }
        track.keyframes.push_back({phase * duration, Delta(skeleton.joints[joint].bindTransform, rotation, translation)});
    }
    return track;
}

Matrix4 BindJointTransform(const Skeleton3D& skeleton, std::size_t joint) {
    std::vector<Matrix4> globals(skeleton.joints.size());
    for (std::size_t index = 0; index < skeleton.joints.size(); ++index) {
        const Matrix4 local = skeleton.joints[index].bindTransform.Matrix();
        const int parent = skeleton.joints[index].parent;
        globals[index] = parent < 0 ? local : globals[std::size_t(parent)] * local;
    }
    return globals[joint];
}

struct AppendedRange {
    std::uint32_t first{}, count{}, slot{};
};

AppendedRange AppendSkinnedPart(SkinnedModel3D& model, const Mesh3D& source, Vec3 position,
    Vec3 scale, std::uint16_t joint, Color color, std::uint32_t slot,
    const Matrix4& attachment = Matrix4::Identity(), Vec3 rotation = {}) {
    const Matrix4 transform = attachment * CreateTransformMatrix(position, rotation, scale);
    const Matrix4 normalTransform = CreateNormalMatrix(transform);
    const std::uint32_t vertexBase = std::uint32_t(model.bindMesh.vertices.size());
    const std::uint32_t firstIndex = std::uint32_t(model.bindMesh.indices.size());
    model.bindMesh.vertices.reserve(model.bindMesh.vertices.size() + source.vertices.size());
    model.influences.reserve(model.influences.size() + source.vertices.size());
    for (const Vertex3D& vertex : source.vertices) {
        const Vec3 point = TransformPoint(transform, {vertex.x, vertex.y, vertex.z});
        const Vec4 packed = UnpackNormalSnorm8(vertex.normal);
        const Vec3 normal = TransformNormal(normalTransform, {packed.x, packed.y, packed.z});
        model.bindMesh.vertices.push_back({point.x, point.y, point.z, PackNormalSnorm8(normal, packed.w), PackColorRgba8(color), vertex.u, vertex.v});
        SkinInfluences3D influence{};
        influence.joints[0] = joint;
        influence.weights[0] = 65535;
        model.influences.push_back(influence);
    }
    model.bindMesh.indices.reserve(model.bindMesh.indices.size() + source.indices.size());
    for (std::uint32_t index : source.indices) model.bindMesh.indices.push_back(vertexBase + index);
    return {firstIndex, std::uint32_t(source.indices.size()), slot};
}

void AppendRange(std::vector<SkinnedSubmesh3D>& output, AppendedRange range) {
    if (!output.empty() && output.back().materialSlot == range.slot &&
        output.back().firstIndex + output.back().indexCount == range.first) {
        output.back().indexCount += range.count;
    } else {
        output.push_back({range.first, range.count, range.slot, 0});
    }
}

Mesh3D CreateSwordBladeMesh() {
    Mesh3D mesh;
    // 菱形截面形成中央稜線；肩部先微幅展寬，再沿長刃逐段收尖。
    constexpr std::array<std::array<Vec3, 4>, 4> Stations{{
        {{{-.035f, .08f, .02f}, {0.f, .08f, .032f}, {.035f, .08f, .02f}, {0.f, .08f, .008f}}},
        {{{-.042f, .035f, .02f}, {0.f, .035f, .033f}, {.042f, .035f, .02f}, {0.f, .035f, .007f}}},
        {{{-.027f, -.34f, .02f}, {0.f, -.34f, .029f}, {.027f, -.34f, .02f}, {0.f, -.34f, .011f}}},
        {{{-.011f, -.44f, .02f}, {0.f, -.44f, .026f}, {.011f, -.44f, .02f}, {0.f, -.44f, .014f}}}}};
    constexpr std::array<Vec3, 4> RidgeNormals{{
        Vec3{-.92f, .08f, 0.f}, Vec3{0.f, .08f, .92f},
        Vec3{.92f, .08f, 0.f}, Vec3{0.f, .08f, -.92f}}};
    mesh.vertices.reserve(17);
    mesh.indices.reserve(90);
    for (const auto& station : Stations) {
        for (std::size_t side = 0; side < station.size(); ++side) {
            const Vec3 point = station[side];
            mesh.vertices.push_back({point.x, point.y, point.z,
                PackNormalSnorm8(Normalize(RidgeNormals[side])), 0xffffffffu});
        }
    }
    mesh.vertices.push_back({0.f, -.48f, .02f, PackNormalSnorm8({0.f, -1.f, 0.f}), 0xffffffffu});
    mesh.indices.insert(mesh.indices.end(), {0, 1, 2, 0, 2, 3});
    for (std::uint32_t station = 0; station < 3; ++station) {
        const std::uint32_t upper = station * 4, lower = upper + 4;
        for (std::uint32_t side = 0; side < 4; ++side) {
            const std::uint32_t next = (side + 1) % 4;
            mesh.indices.insert(mesh.indices.end(), {
                upper + side, lower + side, lower + next,
                upper + side, lower + next, upper + next});
        }
    }
    for (std::uint32_t side = 0; side < 4; ++side) {
        const std::uint32_t next = (side + 1) % 4;
        mesh.indices.insert(mesh.indices.end(), {12 + side, 16, 12 + next});
    }
    return mesh;
}

} // 匿名命名空間

bool LoadCharacterMaterialBindings3D(Engine& engine, MaterialLibrary3D& library,
    const SkinnedModelAssetData& asset, const std::filesystem::path& materialRoot,
    const std::filesystem::path& sidecarPath, std::vector<Material3D>& output,
    std::string* error) {
    SetError(error, {});
    if (library.Initialized() || asset.materialSlotNames.empty() ||
        (!asset.materials.empty() && asset.materials.size() != asset.materialSlotNames.size())) {
        SetError(error, "Character material arguments are invalid");
        return false;
    }
    SkinnedMaterialSidecarResult3D parsed =
        LoadSkinnedMaterialSidecarFile3D(sidecarPath, asset.materialSlotNames, materialRoot);
    if (!parsed) {
        SetError(error, "Character material sidecar failed: " + parsed.error.message);
        return false;
    }
    const std::vector<Material3D> source = SourceCharacterMaterials(asset);
    if (!parsed.PreserveSourcePbr(source)) {
        SetError(error, "Character material PBR override state is invalid");
        return false;
    }
    std::vector<MaterialAssetDefinition3D> definitions = std::move(parsed.materials);
    if (!library.Initialize(engine, materialRoot) || !library.LoadAll(definitions)) {
        library.Shutdown();
        SetError(error, "Character PBR texture batch loading failed");
        return false;
    }
    std::vector<Material3D> candidate;
    candidate.reserve(asset.materialSlotNames.size());
    for (const std::string& name : asset.materialSlotNames) {
        const Material3D* material = library.Find(name);
        if (!material) {
            library.Shutdown();
            SetError(error, "Character PBR material binding failed: " + name);
            return false;
        }
        candidate.push_back(*material);
    }
    output.swap(candidate);
    return true;
}

bool ResolveCharacterRig3D(const SkinnedModelAssetData& asset, HumanoidSkeletonMap3D& output,
    SkeletonSemanticError3D* error) noexcept {
    return ResolveHumanoidSkeletonMap3D(asset.jointNames, asset.model,
        asset.model.skeletonCompatibilityId, output, error);
}

bool CreateFallbackHumanoidAnimationSet3D(const SkinnedModelAssetData& asset,
    const HumanoidSkeletonMap3D& rig, FallbackHumanoidAnimationSet3D& output,
    std::string* error) {
    SetError(error, {});
    if (!rig.Valid(asset.model.skeleton, asset.model.skeletonCompatibilityId)) {
        SetError(error, "Fallback humanoid animation rig is invalid");
        return false;
    }
    FallbackHumanoidAnimationSet3D candidate;
    auto& clips = candidate.clips;
    for (auto& clip : clips) {
        clip.duration = 1.f;
        clip.looping = true;
    }
    // 依待機語意選取來源，不讓握持原型或檔案排列決定待機姿勢。
    for (const std::string_view name : {"IdleBreathing", "Idle"}) {
        const auto found = std::ranges::find_if(asset.clips, [&](const auto& source) {
            return source.name == name && source.clip.Valid(asset.model.skeleton);
        });
        if (found != asset.clips.end()) {
            clips[std::size_t(FallbackHumanoidClip3D::Idle)] = found->clip;
            clips[std::size_t(FallbackHumanoidClip3D::Idle)].looping = true;
            break;
        }
    }
    const Skeleton3D& skeleton = asset.model.skeleton;
    const auto joint = [&](HumanoidJoint3D semantic) { return rig.Joint(semantic); };
    const auto namedJoint = [&](std::string_view name) {
        const auto found = std::ranges::find(asset.jointNames, name);
        return found == asset.jointNames.end()
            ? skeleton.joints.size()
            : std::size_t(found - asset.jointNames.begin());
    };
    const std::size_t hips = joint(HumanoidJoint3D::Hips), spine = joint(HumanoidJoint3D::Spine);
    const std::size_t leftUpperLeg = joint(HumanoidJoint3D::LeftUpperLeg), leftLowerLeg = joint(HumanoidJoint3D::LeftLowerLeg), leftFoot = joint(HumanoidJoint3D::LeftFoot);
    const std::size_t rightUpperLeg = joint(HumanoidJoint3D::RightUpperLeg), rightLowerLeg = joint(HumanoidJoint3D::RightLowerLeg), rightFoot = joint(HumanoidJoint3D::RightFoot);
    const std::size_t leftUpperArm = joint(HumanoidJoint3D::LeftUpperArm), leftLowerArm = joint(HumanoidJoint3D::LeftLowerArm), leftHand = joint(HumanoidJoint3D::LeftHand);
    const std::size_t rightUpperArm = joint(HumanoidJoint3D::RightUpperArm), rightLowerArm = joint(HumanoidJoint3D::RightLowerArm), rightHand = joint(HumanoidJoint3D::RightHand);
    const std::size_t head = joint(HumanoidJoint3D::Head);
    const std::size_t leftFingers = namedJoint("LeftFingers"), rightFingers = namedJoint("RightFingers");
    const std::size_t leftToe = namedJoint("LeftToe"), rightToe = namedJoint("RightToe");
    const auto appendOptional = [&](AnimationClip3D& clip, std::size_t optionalJoint,
                                    std::initializer_list<std::pair<float, Vec3>> rotations,
                                    std::initializer_list<std::pair<float, Vec3>> translations = {}) {
        if (optionalJoint < skeleton.joints.size() &&
            std::ranges::none_of(clip.tracks, [&](const auto& track) {
                return track.joint == optionalJoint;
            }))
            clip.tracks.push_back(Track(skeleton, optionalJoint, clip.duration,
                                        rotations, translations));
    };
    AnimationClip3D& idle = clips[std::size_t(FallbackHumanoidClip3D::Idle)];
    // 僅補上缺少的關節；保留來源軌跡並避免同關節重複軌跡。
    appendOptional(idle, head,
        {{0.f,{0,-.018f,0}}, {.5f,{.012f,.018f,0}}, {1.f,{0,-.018f,0}}});
    appendOptional(idle, leftHand,
        {{0.f,{.02f,0,-.08f}}, {.5f,{.035f,0,-.10f}}, {1.f,{.02f,0,-.08f}}});
    appendOptional(idle, rightHand,
        {{0.f,{.02f,0,.08f}}, {.5f,{.035f,0,.10f}}, {1.f,{.02f,0,.08f}}});
    appendOptional(idle, leftFingers, {{0.f,{.16f,0,-.05f}}, {.5f,{.24f,0,-.08f}}, {1.f,{.16f,0,-.05f}}});
    appendOptional(idle, rightFingers, {{0.f,{.16f,0,.05f}}, {.5f,{.24f,0,.08f}}, {1.f,{.16f,0,.05f}}});
    const auto gait = [&](FallbackHumanoidClip3D id, float duration, float hipAmount, float kneeAmount) {
        AnimationClip3D& clip = clips[std::size_t(id)];
        clip.duration = duration;
        clip.looping = true;
        const float trailingHip = hipAmount * .72f;
        const float supportKnee = kneeAmount * .22f;
        const float passingKnee = kneeAmount * .64f;
        const float swingKnee = kneeAmount;
        const float ankleLift = id == FallbackHumanoidClip3D::Run ? .28f : .18f;
        clip.tracks = {
            // 接觸、承重、後推、擺回四階段。舊三點曲線會在正式擷取幀把
            // 大腿鎖在兩個極值，造成跨步過寬與腳掌突然抬高。
            Track(skeleton, leftUpperLeg, duration, {{0.f,{hipAmount,0,0}}, {.25f,{0,0,0}}, {.5f,{-trailingHip,0,0}}, {.75f,{0,0,0}}, {1.f,{hipAmount,0,0}}}),
            Track(skeleton, rightUpperLeg, duration, {{0.f,{-trailingHip,0,0}}, {.25f,{0,0,0}}, {.5f,{hipAmount,0,0}}, {.75f,{0,0,0}}, {1.f,{-trailingHip,0,0}}}),
            Track(skeleton, leftLowerLeg, duration, {{0.f,{supportKnee,0,0}}, {.25f,{supportKnee,0,0}}, {.5f,{swingKnee,0,0}}, {.75f,{passingKnee,0,0}}, {1.f,{supportKnee,0,0}}}),
            Track(skeleton, rightLowerLeg, duration, {{0.f,{swingKnee,0,0}}, {.25f,{passingKnee,0,0}}, {.5f,{supportKnee,0,0}}, {.75f,{supportKnee,0,0}}, {1.f,{swingKnee,0,0}}}),
            Track(skeleton, leftUpperArm, duration, {{0.f,{-hipAmount*.7f,0,0}}, {.5f,{hipAmount*.7f,0,0}}, {1.f,{-hipAmount*.7f,0,0}}}),
            Track(skeleton, rightUpperArm, duration, {{0.f,{hipAmount*.7f,0,0}}, {.5f,{-hipAmount*.7f,0,0}}, {1.f,{hipAmount*.7f,0,0}}}),
            Track(skeleton, hips, duration, {{0.f,{0,-.035f,0}}, {.25f,{0,.025f,0}}, {.5f,{0,-.035f,0}}, {.75f,{0,.025f,0}}, {1.f,{0,-.035f,0}}},
                  {{0.f,{-.022f,-.007f,0}}, {.25f,{0,.009f,0}}, {.5f,{.022f,-.007f,0}}, {.75f,{0,.009f,0}}, {1.f,{-.022f,-.007f,0}}}),
            Track(skeleton, leftFoot, duration, {{0.f,{-.10f,0,0}}, {.25f,{ankleLift,0,0}}, {.5f,{.06f,0,0}}, {.75f,{-.04f,0,0}}, {1.f,{-.10f,0,0}}}),
            Track(skeleton, rightFoot, duration, {{0.f,{.06f,0,0}}, {.25f,{-.04f,0,0}}, {.5f,{-.10f,0,0}}, {.75f,{ankleLift,0,0}}, {1.f,{.06f,0,0}}}),
            Track(skeleton, leftHand, duration, {{0.f,{.08f,0,-.10f}}, {.5f,{.18f,0,-.16f}}, {1.f,{.08f,0,-.10f}}}),
            Track(skeleton, rightHand, duration, {{0.f,{.18f,0,.16f}}, {.5f,{.08f,0,.10f}}, {1.f,{.18f,0,.16f}}}),
            Track(skeleton, head, duration, {{0.f,{0,-.035f,0}}, {.5f,{0,.035f,0}}, {1.f,{0,-.035f,0}}})};
        appendOptional(clip, leftFingers, {{0.f,{.20f,0,-.06f}}, {.5f,{.34f,0,-.10f}}, {1.f,{.20f,0,-.06f}}});
        appendOptional(clip, rightFingers, {{0.f,{.34f,0,.10f}}, {.5f,{.20f,0,.06f}}, {1.f,{.34f,0,.10f}}});
        appendOptional(clip, leftToe, {{0.f,{-.12f,0,0}}, {.25f,{ankleLift*.8f,0,0}}, {.5f,{0,0,0}}, {.75f,{-.05f,0,0}}, {1.f,{-.12f,0,0}}});
        appendOptional(clip, rightToe, {{0.f,{0,0,0}}, {.25f,{-.05f,0,0}}, {.5f,{-.12f,0,0}}, {.75f,{ankleLift*.8f,0,0}}, {1.f,{0,0,0}}});
    };
    gait(FallbackHumanoidClip3D::Walk, .86f, .24f, .36f);
    gait(FallbackHumanoidClip3D::Run, .58f, .48f, .64f);

    AnimationClip3D& crouch = clips[std::size_t(FallbackHumanoidClip3D::Crouch)];
    crouch.duration = .7f; crouch.looping = true;
    crouch.tracks = {
        Track(skeleton, hips, crouch.duration, {{0.f,{0,0,0}}, {.25f,{.08f,0,0}}, {1.f,{.08f,0,0}}}, {{0.f,{0,0,0}}, {.25f,{0,-.14f,0}}, {1.f,{0,-.14f,0}}}),
        Track(skeleton, spine, crouch.duration, {{0.f,{0,0,0}}, {.25f,{.16f,0,0}}, {1.f,{.16f,0,0}}}),
        Track(skeleton, leftUpperLeg, crouch.duration, {{0.f,{0,0,0}}, {.25f,{-.38f,0,0}}, {1.f,{-.38f,0,0}}}),
        Track(skeleton, rightUpperLeg, crouch.duration, {{0.f,{0,0,0}}, {.25f,{-.38f,0,0}}, {1.f,{-.38f,0,0}}}),
        Track(skeleton, leftLowerLeg, crouch.duration, {{0.f,{0,0,0}}, {.25f,{.58f,0,0}}, {1.f,{.58f,0,0}}}),
        Track(skeleton, rightLowerLeg, crouch.duration, {{0.f,{0,0,0}}, {.25f,{.58f,0,0}}, {1.f,{.58f,0,0}}}),
        Track(skeleton, leftFoot, crouch.duration, {{0.f,{0,0,0}}, {.25f,{-.22f,0,0}}, {1.f,{-.22f,0,0}}}),
        Track(skeleton, rightFoot, crouch.duration, {{0.f,{0,0,0}}, {.25f,{-.22f,0,0}}, {1.f,{-.22f,0,0}}})};
    appendOptional(crouch, leftToe, {{0.f,{0,0,0}}, {.25f,{.24f,0,0}}, {1.f,{.24f,0,0}}});
    appendOptional(crouch, rightToe, {{0.f,{0,0,0}}, {.25f,{.24f,0,0}}, {1.f,{.24f,0,0}}});

    AnimationClip3D& jump = clips[std::size_t(FallbackHumanoidClip3D::Jump)];
    jump.duration = .55f; jump.looping = false;
    jump.tracks = {
        Track(skeleton, leftUpperLeg, jump.duration, {{0.f,{0,0,0}}, {.35f,{-.48f,0,0}}, {1.f,{-.25f,0,0}}}),
        Track(skeleton, rightUpperLeg, jump.duration, {{0.f,{0,0,0}}, {.35f,{-.48f,0,0}}, {1.f,{-.25f,0,0}}}),
        Track(skeleton, leftLowerLeg, jump.duration, {{0.f,{0,0,0}}, {.35f,{.75f,0,0}}, {1.f,{.35f,0,0}}}),
        Track(skeleton, rightLowerLeg, jump.duration, {{0.f,{0,0,0}}, {.35f,{.75f,0,0}}, {1.f,{.35f,0,0}}})};
    appendOptional(jump, leftToe, {{0.f,{-.15f,0,0}}, {.35f,{.42f,0,0}}, {1.f,{.18f,0,0}}});
    appendOptional(jump, rightToe, {{0.f,{-.15f,0,0}}, {.35f,{.42f,0,0}}, {1.f,{.18f,0,0}}});
    AnimationClip3D& fall = clips[std::size_t(FallbackHumanoidClip3D::Fall)];
    fall.duration = .6f; fall.looping = true;
    fall.tracks = {
        Track(skeleton, leftUpperArm, fall.duration, {{0.f,{0,0,-.55f}}, {1.f,{0,0,-.55f}}}),
        Track(skeleton, rightUpperArm, fall.duration, {{0.f,{0,0,.55f}}, {1.f,{0,0,.55f}}})};

    const auto attack = [&](FallbackHumanoidClip3D id, bool left, float yaw, float reach) {
        AnimationClip3D& clip = clips[std::size_t(id)];
        clip.duration = .55f; clip.looping = false;
        const std::size_t upper = left ? leftUpperArm : rightUpperArm;
        const std::size_t lower = left ? leftLowerArm : rightLowerArm;
        const std::size_t hand = left ? leftHand : rightHand;
        const float side = left ? -1.f : 1.f;
        clip.tracks = {
            Track(skeleton, spine, clip.duration, {{0.f,{0,-yaw*.3f,0}}, {.22f,{0,yaw,0}}, {.55f,{0,-yaw*.55f,0}}, {1.f,{0,0,0}}}),
            // Keep the striking shoulder in front of the torso.  The previous positive
            // yaw followed the torso twist and placed the whole arm behind the ribcage
            // at the authoritative hit-window sample, then snapped it into a lateral
            // T-pose.  A shorter forward arc remains readable from front and gameplay
            // three-quarter cameras without changing attack timing.
            Track(skeleton, upper, clip.duration, {{0.f,{-.25f,0,side*.18f}}, {.22f,{-.55f,side*-.15f,side*.9f}}, {.55f,{-1.05f,side*-reach,side*.48f}}, {1.f,{-.22f,0,0}}}),
            Track(skeleton, lower, clip.duration, {{0.f,{-.25f,0,0}}, {.22f,{-.72f,0,0}}, {.55f,{-.25f,0,0}}, {1.f,{0,0,0}}}),
            Track(skeleton, hand, clip.duration, {{0.f,{0,0,0}}, {.22f,{-.18f,side*.18f,0}}, {.55f,{.2f,side*-.12f,0}}, {1.f,{0,0,0}}})};
        appendOptional(clip, left ? leftFingers : rightFingers,
            {{0.f,{.22f,0,side*.05f}}, {.22f,{.48f,0,side*.12f}}, {.55f,{.55f,0,side*.08f}}, {1.f,{.22f,0,side*.05f}}});
    };
    attack(FallbackHumanoidClip3D::Attack1, false, .35f, .25f);
    attack(FallbackHumanoidClip3D::Attack2, true, -.42f, .32f);
    attack(FallbackHumanoidClip3D::Attack3, false, .48f, .35f);

    AnimationClip3D& hit = clips[std::size_t(FallbackHumanoidClip3D::Hit)];
    hit.duration = .45f; hit.looping = false;
    hit.tracks = {Track(skeleton, spine, hit.duration, {{0.f,{0,0,0}}, {.25f,{-.5f,0,0}}, {1.f,{0,0,0}}})};
    AnimationClip3D& death = clips[std::size_t(FallbackHumanoidClip3D::Death)];
    death.duration = 1.f; death.looping = false;
    // 先屈肢再側倒，避免只有骨盆旋轉而四肢保持僵直的木板姿態。
    // Track 使用 bind 後局部增量；此處採小幅不對稱收束，不假稱世界地面 IK。
    death.tracks = {Track(skeleton, hips, death.duration,
        {{0.f,{0,0,0}}, {.2f,{.04f,0,.18f}}, {.55f,{.07f,0,1.02f}}, {.85f,{.05f,0,1.48f}}, {1.f,{.05f,0,1.5f}}},
        {{0.f,{0,0,0}}, {.2f,{0,-.10f,0}}, {.55f,{0,-.43f,0}}, {.85f,{0,-.70f,0}}, {1.f,{0,-.72f,0}}})};
    const auto deathLimb = [&](std::size_t index, Vec3 settled) {
        death.tracks.push_back(Track(skeleton,index,death.duration,
            {{0.f,{0,0,0}}, {.2f,settled*.25f}, {.55f,settled*.85f}, {.85f,settled}, {1.f,settled}}));
    };
    deathLimb(spine,{.12f,.03f,-.06f});
    deathLimb(joint(HumanoidJoint3D::Head),{.06f,-.04f,.04f});
    deathLimb(leftUpperArm,{-.32f,-.12f,-.20f});
    deathLimb(rightUpperArm,{-.42f,.10f,.16f});
    deathLimb(leftLowerArm,{-.82f,0,0});
    deathLimb(rightLowerArm,{-1.08f,0,0});
    deathLimb(leftUpperLeg,{-.36f,0,-.06f});
    deathLimb(rightUpperLeg,{-.62f,0,.08f});
    deathLimb(leftLowerLeg,{.72f,0,0});
    deathLimb(rightLowerLeg,{1.12f,0,0});
    deathLimb(leftFoot,{-.08f,0,0});
    deathLimb(rightFoot,{-.12f,0,0});

    for (const AnimationClip3D& clip : clips) {
        if (!clip.Valid(skeleton)) {
            SetError(error, "Generated fallback humanoid animation is invalid");
            return false;
        }
    }
    output = std::move(candidate);
    return true;
}

bool ValidateCharacterSwordGeometry3D(const CharacterEquipmentAsset3D& equipment) noexcept {
    const auto& model=equipment.model;const auto& contract=equipment.swordGeometry;
    const CharacterSwordGeometry3D canonical{};
    const auto same=[](Vec3 a,Vec3 b){const float error=Length(a-b);return std::isfinite(error)&&error<=.0001f;};
    constexpr std::uint32_t BladeVertexCount=17,BoxVertexCount=24,Cylinder12VertexCount=26,GripVertexCount=34;
    constexpr std::uint32_t BladeIndexCount=90,BoxIndexCount=36,Cylinder12IndexCount=144,GripIndexCount=192;
    constexpr std::uint32_t MetalVertexCount=BladeVertexCount+BoxVertexCount*3+Cylinder12VertexCount*2;
    constexpr std::uint32_t MetalIndexCount=BladeIndexCount+BoxIndexCount*3+Cylinder12IndexCount*2;
    constexpr std::uint32_t TotalVertexCount=MetalVertexCount+GripVertexCount;
    constexpr std::uint32_t TotalIndexCount=MetalIndexCount+GripIndexCount;
    if(!contract.available||!model.Valid()||model.bindMesh.vertices.size()!=TotalVertexCount||model.bindMesh.indices.size()!=TotalIndexCount||
       equipment.submeshes.size()!=2||contract.tipVertexIndices!=canonical.tipVertexIndices||
       !same(contract.grip,canonical.grip)||!same(contract.handleBladeEnd,canonical.handleBladeEnd)||
       !same(contract.handlePommelEnd,canonical.handlePommelEnd)||!same(contract.bladeBase,canonical.bladeBase)||!same(contract.bladeTip,canonical.bladeTip))return false;
    const auto& metal=equipment.submeshes[0];const auto& handle=equipment.submeshes[1];
    if(metal.firstIndex!=0||metal.indexCount!=MetalIndexCount||metal.materialSlot!=0||
       handle.firstIndex!=MetalIndexCount||handle.indexCount!=GripIndexCount||handle.materialSlot!=1)return false;
    // 依正式 primitive 的索引契約逐項核對，不能讓有效 landmark 掩蓋空三角形或反向繞序。
    std::size_t topologyCursor{};
    const auto matchIndices=[&](std::initializer_list<std::uint32_t> expected){
        for(const auto index:expected)if(model.bindMesh.indices[topologyCursor++]!=index)return false;
        return true;
    };
    if(!matchIndices({0,1,2,0,2,3}))return false;
    for(std::uint32_t station=0;station<3;++station){
        const auto upper=station*4u,lower=upper+4u;
        for(std::uint32_t side=0;side<4;++side){
            const auto next=(side+1u)%4u;
            if(!matchIndices({upper+side,lower+side,lower+next,upper+side,lower+next,upper+next}))return false;
        }
    }
    for(std::uint32_t side=0;side<4;++side){const auto next=(side+1u)%4u;if(!matchIndices({12u+side,16u,12u+next}))return false;}
    std::uint32_t vertexBase=BladeVertexCount;
    for(std::uint32_t box=0;box<3;++box){for(std::uint32_t face=0;face<6;++face){
        const auto base=vertexBase+face*4u;
        if(!matchIndices({base,base+2,base+1,base,base+3,base+2}))return false;
    }vertexBase+=BoxVertexCount;}
    const auto matchCylinder=[&](std::uint32_t sides){
        const auto bottom=vertexBase+sides*2u,top=bottom+1u;
        for(std::uint32_t side=0;side<sides;++side){const auto next=(side+1u)%sides,a=vertexBase+side*2u,b=a+1u,c=vertexBase+next*2u,d=c+1u;
            if(!matchIndices({a,b,c,c,b,d,b,top,d,a,c,bottom}))return false;}
        vertexBase+=sides*2u+2u;return true;
    };
    if(!matchCylinder(12)||!matchCylinder(12)||!matchCylinder(16))return false;
    if(topologyCursor!=model.bindMesh.indices.size())return false;
    const auto handIndex=model.influences[0].joints[0];
    if(handIndex>=model.inverseBindMatrices.size())return false;
    for(std::size_t index=0;index<model.bindMesh.vertices.size();++index){
        const auto& influence=model.influences[index];
        if(influence.joints[0]!=handIndex||influence.weights[0]!=65535||influence.weights[1]||influence.weights[2]||influence.weights[3])return false;
        const auto normal=UnpackNormalSnorm8(model.bindMesh.vertices[index].normal);
        const float magnitude=normal.x*normal.x+normal.y*normal.y+normal.z*normal.z;
        if(!std::isfinite(magnitude)||magnitude<.5f)return false;
    }
    const auto local=[&](std::size_t index){const auto& vertex=model.bindMesh.vertices[index];return TransformPoint(model.inverseBindMatrices[handIndex],{vertex.x,vertex.y,vertex.z});};
    constexpr std::array<std::array<Vec3,4>,4> BladeStations{{
        {{{-.035f,.08f,.02f},{0,.08f,.032f},{.035f,.08f,.02f},{0,.08f,.008f}}},
        {{{-.042f,.035f,.02f},{0,.035f,.033f},{.042f,.035f,.02f},{0,.035f,.007f}}},
        {{{-.027f,-.34f,.02f},{0,-.34f,.029f},{.027f,-.34f,.02f},{0,-.34f,.011f}}},
        {{{-.011f,-.44f,.02f},{0,-.44f,.026f},{.011f,-.44f,.02f},{0,-.44f,.014f}}}}};
    for(std::size_t station=0;station<BladeStations.size();++station)for(std::size_t side=0;side<4;++side)
        if(!same(local(station*4+side),BladeStations[station][side]))return false;
    for(const auto index:contract.tipVertexIndices)if(!same(local(index),contract.bladeTip))return false;
    Vec3 base{},grip{};
    for(std::size_t index=0;index<4;++index)base=base+local(index);
    constexpr std::size_t GripVertexBase=MetalVertexCount;
    for(std::size_t index=GripVertexBase;index<TotalVertexCount;++index)grip=grip+local(index);
    return same(base*.25f,contract.bladeBase)&&same(grip*(1.f/GripVertexCount),contract.grip)&&
           same(local(GripVertexBase+32),contract.handleBladeEnd)&&same(local(GripVertexBase+33),contract.handlePommelEnd)&&
           Dot(contract.bladeTip-contract.bladeBase,contract.bladeBase-contract.grip)>0.f;
}

bool CreateCharacterEquipmentAsset3D(const SkinnedModelAssetData& character,
    const HumanoidSkeletonMap3D& rig, CharacterEquipmentShape3D shape,
    CharacterEquipmentAsset3D& output, std::string* error) {
    SetError(error, {});
    if (!rig.Valid(character.model.skeleton, character.model.skeletonCompatibilityId)) {
        SetError(error, "Character equipment rig is invalid");
        return false;
    }
    CharacterEquipmentAsset3D candidate;
    candidate.model.skeleton = character.model.skeleton;
    candidate.model.inverseBindMatrices = character.model.inverseBindMatrices;
    candidate.model.skeletonCompatibilityId = character.model.skeletonCompatibilityId;
    const std::size_t joint = shape == CharacterEquipmentShape3D::Shoulder
        ? rig.Joint(HumanoidJoint3D::RightUpperArm)
        : rig.Joint(HumanoidJoint3D::RightHand);
    const Matrix4 bindAttachment = BindJointTransform(character.model.skeleton, joint);
    const Vec3 anchor = TransformPoint(bindAttachment, {});
    const Mesh3D box = Mesh3D::CreateUnitBox();
    if (shape == CharacterEquipmentShape3D::Shoulder) {
        const Mesh3D dome = Mesh3D::CreateUnitSphere(16, 10);
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, dome, anchor + Vec3{.08f,.02f,0}, {.24f,.13f,.25f}, std::uint16_t(joint), {.32f,.36f,.42f,1}, 0));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, anchor + Vec3{.16f,-.08f,0}, {.10f,.08f,.20f}, std::uint16_t(joint), {.14f,.17f,.21f,1}, 0));
    } else if (shape == CharacterEquipmentShape3D::Sword) {
        const Mesh3D blade = CreateSwordBladeMesh();
        const Mesh3D grip = Mesh3D::CreateUnitCylinder(16);
        const Mesh3D ornament = Mesh3D::CreateUnitCylinder(12);
        // 握把與刀身偏移皆為手部局部座標；先轉回 bind 模型空間，再交由同一骨架蒙皮。
        // 刀身已是公尺制菱形截面；護手向外微彎，金屬環與配重球補足可信輪廓。
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, blade, {}, {1,1,1}, std::uint16_t(joint), {.72f,.79f,.86f,1}, 0, bindAttachment));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, {0,.061f,.02f}, {.06f,.022f,.038f}, std::uint16_t(joint), {.42f,.25f,.08f,1}, 0, bindAttachment));
        // 每側 quillon 實長 7.5 公分並朝刀身彎折；總長 21 公分，但水平投影僅約 10 公分。
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, {-.03971f,.02478f,.02f}, {.075f,.018f,.028f}, std::uint16_t(joint), {.36f,.20f,.06f,1}, 0, bindAttachment, {0,0,1.30899694f}));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, {.03971f,.02478f,.02f}, {.075f,.018f,.028f}, std::uint16_t(joint), {.36f,.20f,.06f,1}, 0, bindAttachment, {0,0,-1.30899694f}));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, ornament, {0,.045f,.02f}, {.042f,.014f,.042f}, std::uint16_t(joint), {.58f,.63f,.68f,1}, 0, bindAttachment));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, ornament, {0,.165f,.02f}, {.024f,.014f,.024f}, std::uint16_t(joint), {.4f,.23f,.07f,1}, 0, bindAttachment));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, grip, {0,.105f,.02f}, {.042f,.12f,.042f}, std::uint16_t(joint), {.12f,.048f,.018f,1}, 1, bindAttachment));
    } else {
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, {0,-.18f,.02f}, {.035f,.38f,.035f}, std::uint16_t(joint), {.26f,.14f,.055f,1}, 1, bindAttachment));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, {.07f,-.47f,.02f}, {.18f,.12f,.05f}, std::uint16_t(joint), {.42f,.46f,.5f,1}, 0, bindAttachment));
    }
    if (!candidate.model.Valid() || candidate.submeshes.empty()) {
        SetError(error, "Generated character equipment is invalid");
        return false;
    }
    if(shape==CharacterEquipmentShape3D::Sword){
        candidate.swordGeometry.available=true;
        if(!ValidateCharacterSwordGeometry3D(candidate)){SetError(error,"Generated sword geometry contract failed");return false;}
    }
    output = std::move(candidate);
    return true;
}

std::vector<float> CreateCharacterUpperBodyMask3D(const Skeleton3D& skeleton,
    const HumanoidSkeletonMap3D& rig) {
    std::vector<float> mask(skeleton.joints.size());
    if (!rig.Valid(skeleton, rig.compatibility)) return mask;
    const std::size_t spine = rig.Joint(HumanoidJoint3D::Spine);
    for (std::size_t joint = 0; joint < skeleton.joints.size(); ++joint) {
        int cursor = int(joint);
        while (cursor >= 0) {
            if (std::size_t(cursor) == spine) {
                mask[joint] = 1.f;
                break;
            }
            cursor = skeleton.joints[std::size_t(cursor)].parent;
        }
    }
    return mask;
}

bool CharacterSocketTransform3D(const Skeleton3D& skeleton, const HumanoidSkeletonMap3D& rig,
    HumanoidJoint3D semantic, const AnimationPose3D& pose, Matrix4 modelToWorld,
    Matrix4& output) noexcept {
    if (!rig.Valid(skeleton, rig.compatibility) || pose.localTransforms.size() != skeleton.joints.size() ||
        pose.globalMatrices.size() != skeleton.joints.size()) return false;
    const std::size_t joint = rig.Joint(semantic);
    const Matrix4 candidate = modelToWorld * pose.globalMatrices[joint];
    for (const auto& row : candidate.values) for (float value : row) if (!std::isfinite(value)) return false;
    output = candidate;
    return true;
}

} // 命名空間 Aevrynd
