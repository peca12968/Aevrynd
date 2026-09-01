#include "Aevrynd/CharacterPresentation3D.h"

#include "Aevrynd/SkinnedMaterialSidecar3D.h"

#include <algorithm>
#include <cmath>
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

Vec3 BindJointPosition(const Skeleton3D& skeleton, std::size_t joint) {
    std::vector<Matrix4> globals(skeleton.joints.size());
    for (std::size_t index = 0; index < skeleton.joints.size(); ++index) {
        const Matrix4 local = skeleton.joints[index].bindTransform.Matrix();
        const int parent = skeleton.joints[index].parent;
        globals[index] = parent < 0 ? local : globals[std::size_t(parent)] * local;
    }
    return TransformPoint(globals[joint], {});
}

struct AppendedRange {
    std::uint32_t first{}, count{}, slot{};
};

AppendedRange AppendSkinnedPart(SkinnedModel3D& model, const Mesh3D& source, Vec3 position,
    Vec3 scale, std::uint16_t joint, Color color, std::uint32_t slot) {
    const Matrix4 transform = CreateTransformMatrix(position, {}, scale);
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
    const SkinnedMaterialSidecarResult3D parsed =
        LoadSkinnedMaterialSidecarFile3D(sidecarPath, asset.materialSlotNames, materialRoot);
    if (!parsed) {
        SetError(error, "Character material sidecar failed: " + parsed.error.message);
        return false;
    }
    std::vector<MaterialAssetDefinition3D> definitions = parsed.materials;
    const std::vector<Material3D> source = SourceCharacterMaterials(asset);
    for (std::size_t index = 0; index < definitions.size(); ++index) {
        const Material3D sidecar = definitions[index].material;
        definitions[index].material = source[index];
        definitions[index].material.textureScale = sidecar.textureScale;
        definitions[index].material.baseTextureScale = sidecar.baseTextureScale;
        definitions[index].material.detailTextureScale = sidecar.detailTextureScale;
        definitions[index].material.useUv = sidecar.useUv;
        definitions[index].material.castsShadow = sidecar.castsShadow;
        definitions[index].material.shadingModel = sidecar.shadingModel;
        definitions[index].material.alphaMode = sidecar.alphaMode;
        definitions[index].material.alphaCutoff = sidecar.alphaCutoff;
        definitions[index].material.doubleSided = sidecar.doubleSided;
    }
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
    if (!asset.clips.empty() && asset.clips.front().clip.Valid(asset.model.skeleton)) {
        clips[std::size_t(FallbackHumanoidClip3D::Idle)] = asset.clips.front().clip;
        clips[std::size_t(FallbackHumanoidClip3D::Idle)].looping = true;
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
        if (optionalJoint < skeleton.joints.size())
            clip.tracks.push_back(Track(skeleton, optionalJoint, clip.duration,
                                        rotations, translations));
    };
    AnimationClip3D& idle = clips[std::size_t(FallbackHumanoidClip3D::Idle)];
    idle.tracks.push_back(Track(skeleton, head, idle.duration,
        {{0.f,{0,-.018f,0}}, {.5f,{.012f,.018f,0}}, {1.f,{0,-.018f,0}}}));
    idle.tracks.push_back(Track(skeleton, leftHand, idle.duration,
        {{0.f,{.02f,0,-.08f}}, {.5f,{.035f,0,-.10f}}, {1.f,{.02f,0,-.08f}}}));
    idle.tracks.push_back(Track(skeleton, rightHand, idle.duration,
        {{0.f,{.02f,0,.08f}}, {.5f,{.035f,0,.10f}}, {1.f,{.02f,0,.08f}}}));
    appendOptional(idle, leftFingers, {{0.f,{.16f,0,-.05f}}, {.5f,{.24f,0,-.08f}}, {1.f,{.16f,0,-.05f}}});
    appendOptional(idle, rightFingers, {{0.f,{.16f,0,.05f}}, {.5f,{.24f,0,.08f}}, {1.f,{.16f,0,.05f}}});
    const auto gait = [&](FallbackHumanoidClip3D id, float duration, float hipAmount, float kneeAmount) {
        AnimationClip3D& clip = clips[std::size_t(id)];
        clip.duration = duration;
        clip.looping = true;
        clip.tracks = {
            Track(skeleton, leftUpperLeg, duration, {{0.f,{hipAmount,0,0}}, {.5f,{-hipAmount,0,0}}, {1.f,{hipAmount,0,0}}}),
            Track(skeleton, rightUpperLeg, duration, {{0.f,{-hipAmount,0,0}}, {.5f,{hipAmount,0,0}}, {1.f,{-hipAmount,0,0}}}),
            Track(skeleton, leftLowerLeg, duration, {{0.f,{kneeAmount,0,0}}, {.5f,{0,0,0}}, {1.f,{kneeAmount,0,0}}}),
            Track(skeleton, rightLowerLeg, duration, {{0.f,{0,0,0}}, {.5f,{kneeAmount,0,0}}, {1.f,{0,0,0}}}),
            Track(skeleton, leftUpperArm, duration, {{0.f,{-hipAmount*.7f,0,0}}, {.5f,{hipAmount*.7f,0,0}}, {1.f,{-hipAmount*.7f,0,0}}}),
            Track(skeleton, rightUpperArm, duration, {{0.f,{hipAmount*.7f,0,0}}, {.5f,{-hipAmount*.7f,0,0}}, {1.f,{hipAmount*.7f,0,0}}}),
            Track(skeleton, hips, duration, {{0.f,{0,-.035f,0}}, {.25f,{0,.025f,0}}, {.5f,{0,-.035f,0}}, {.75f,{0,.025f,0}}, {1.f,{0,-.035f,0}}},
                  {{0.f,{-.022f,-.007f,0}}, {.25f,{0,.009f,0}}, {.5f,{.022f,-.007f,0}}, {.75f,{0,.009f,0}}, {1.f,{-.022f,-.007f,0}}}),
            Track(skeleton, leftFoot, duration, {{0.f,{-.18f,0,0}}, {.25f,{.32f,0,0}}, {.5f,{.08f,0,0}}, {1.f,{-.18f,0,0}}}),
            Track(skeleton, rightFoot, duration, {{0.f,{.08f,0,0}}, {.5f,{-.18f,0,0}}, {.75f,{.32f,0,0}}, {1.f,{.08f,0,0}}}),
            Track(skeleton, leftHand, duration, {{0.f,{.08f,0,-.10f}}, {.5f,{.18f,0,-.16f}}, {1.f,{.08f,0,-.10f}}}),
            Track(skeleton, rightHand, duration, {{0.f,{.18f,0,.16f}}, {.5f,{.08f,0,.10f}}, {1.f,{.18f,0,.16f}}}),
            Track(skeleton, head, duration, {{0.f,{0,-.035f,0}}, {.5f,{0,.035f,0}}, {1.f,{0,-.035f,0}}})};
        appendOptional(clip, leftFingers, {{0.f,{.20f,0,-.06f}}, {.5f,{.34f,0,-.10f}}, {1.f,{.20f,0,-.06f}}});
        appendOptional(clip, rightFingers, {{0.f,{.34f,0,.10f}}, {.5f,{.20f,0,.06f}}, {1.f,{.34f,0,.10f}}});
        appendOptional(clip, leftToe, {{0.f,{-.20f,0,0}}, {.25f,{.38f,0,0}}, {.5f,{0,0,0}}, {1.f,{-.20f,0,0}}});
        appendOptional(clip, rightToe, {{0.f,{0,0,0}}, {.5f,{-.20f,0,0}}, {.75f,{.38f,0,0}}, {1.f,{0,0,0}}});
    };
    gait(FallbackHumanoidClip3D::Walk, .86f, .34f, .44f);
    gait(FallbackHumanoidClip3D::Run, .58f, .58f, .72f);

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
            Track(skeleton, upper, clip.duration, {{0.f,{-.25f,0,side*.22f}}, {.22f,{-1.15f,side*.25f,side*-.35f}}, {.55f,{-1.55f,side*reach,side*-.52f}}, {1.f,{-.22f,0,0}}}),
            Track(skeleton, lower, clip.duration, {{0.f,{-.25f,0,0}}, {.22f,{-.72f,0,0}}, {.55f,{-.25f,0,0}}, {1.f,{0,0,0}}}),
            Track(skeleton, hand, clip.duration, {{0.f,{0,0,0}}, {.22f,{-.18f,side*.18f,0}}, {.55f,{.2f,side*-.12f,0}}, {1.f,{0,0,0}}})};
        appendOptional(clip, left ? leftFingers : rightFingers,
            {{0.f,{.22f,0,side*.05f}}, {.22f,{.48f,0,side*.12f}}, {.55f,{.55f,0,side*.08f}}, {1.f,{.22f,0,side*.05f}}});
    };
    attack(FallbackHumanoidClip3D::Attack1, false, .35f, .25f);
    attack(FallbackHumanoidClip3D::Attack2, true, -.42f, .32f);
    attack(FallbackHumanoidClip3D::Attack3, false, 1.05f, .55f);

    AnimationClip3D& hit = clips[std::size_t(FallbackHumanoidClip3D::Hit)];
    hit.duration = .45f; hit.looping = false;
    hit.tracks = {Track(skeleton, spine, hit.duration, {{0.f,{0,0,0}}, {.25f,{-.5f,0,0}}, {1.f,{0,0,0}}})};
    AnimationClip3D& death = clips[std::size_t(FallbackHumanoidClip3D::Death)];
    death.duration = 1.f; death.looping = false;
    death.tracks = {Track(skeleton, hips, death.duration, {{0.f,{0,0,0}}, {.7f,{0,0,1.45f}}, {1.f,{0,0,1.5f}}}, {{0.f,{0,0,0}}, {.7f,{0,-.65f,0}}, {1.f,{0,-.72f,0}}})};

    for (const AnimationClip3D& clip : clips) {
        if (!clip.Valid(skeleton)) {
            SetError(error, "Generated fallback humanoid animation is invalid");
            return false;
        }
    }
    output = std::move(candidate);
    return true;
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
    const Vec3 anchor = BindJointPosition(character.model.skeleton, joint);
    const Mesh3D box = Mesh3D::CreateUnitBox();
    if (shape == CharacterEquipmentShape3D::Shoulder) {
        const Mesh3D dome = Mesh3D::CreateUnitSphere(16, 10);
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, dome, anchor + Vec3{.08f,.02f,0}, {.24f,.13f,.25f}, std::uint16_t(joint), {.32f,.36f,.42f,1}, 0));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, anchor + Vec3{.16f,-.08f,0}, {.10f,.08f,.20f}, std::uint16_t(joint), {.14f,.17f,.21f,1}, 0));
    } else if (shape == CharacterEquipmentShape3D::Sword) {
        const Mesh3D blade = Mesh3D::CreateUnitPyramid();
        const Mesh3D grip = Mesh3D::CreateUnitCylinder(12);
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, blade, anchor + Vec3{0,-.20f,.02f}, {.085f,.56f,.038f}, std::uint16_t(joint), {.68f,.73f,.78f,1}, 0));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, anchor + Vec3{0,.055f,.02f}, {.13f,.022f,.04f}, std::uint16_t(joint), {.34f,.2f,.07f,1}, 0));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, grip, anchor + Vec3{0,.105f,.02f}, {.05f,.12f,.05f}, std::uint16_t(joint), {.18f,.09f,.035f,1}, 1));
    } else {
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, anchor + Vec3{0,-.18f,.02f}, {.035f,.38f,.035f}, std::uint16_t(joint), {.26f,.14f,.055f,1}, 1));
        AppendRange(candidate.submeshes, AppendSkinnedPart(candidate.model, box, anchor + Vec3{.07f,-.47f,.02f}, {.18f,.12f,.05f}, std::uint16_t(joint), {.42f,.46f,.5f,1}, 0));
    }
    if (!candidate.model.Valid() || candidate.submeshes.empty()) {
        SetError(error, "Generated character equipment is invalid");
        return false;
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
