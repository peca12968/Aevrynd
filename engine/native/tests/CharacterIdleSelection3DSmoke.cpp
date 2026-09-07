#include <Aevrynd/CharacterPresentation3D.h>

#include <algorithm>
#include <filesystem>
#include <iostream>

using namespace Aevrynd;

namespace {
bool Check(const std::filesystem::path& path) {
    auto loaded = LoadSkinnedModelAssetFileDetailed(path);
    HumanoidSkeletonMap3D rig;
    if (!loaded || !ResolveCharacterRig3D(loaded.asset, rig)) return false;
    auto& asset = loaded.asset;
    const auto makeClip = [&](std::string name, float duration, std::size_t joint) {
        NamedAnimationClip3D result;
        result.name = std::move(name);
        result.clip.duration = duration;
        auto transform = asset.model.skeleton.joints[joint].bindTransform;
        transform.translation.y += .123f;
        result.clip.tracks.push_back({joint, {{0.f, transform}, {duration, transform}}});
        return result;
    };
    const auto hips = rig.Joint(HumanoidJoint3D::Hips);
    const auto head = rig.Joint(HumanoidJoint3D::Head);
    const auto grip = makeClip("GripPrototype", 7.f, hips);
    const auto breathing = makeClip("IdleBreathing", 3.f, head);
    const auto plain = makeClip("Idle", 2.f, head);
    const auto run = [&](const char* label, std::vector<NamedAnimationClip3D> source,
                         float duration, bool preserveHead) {
        asset.clips = std::move(source);
        FallbackHumanoidAnimationSet3D result;
        std::string error;
        if (!CreateFallbackHumanoidAnimationSet3D(asset, rig, result, &error)) {
            std::cerr << label << ": creation failed: " << error << '\n';
            return false;
        }
        const auto& idle = result.clips[std::size_t(FallbackHumanoidClip3D::Idle)];
        if (idle.duration != duration || !idle.looping || !idle.Valid(asset.model.skeleton)) {
            std::cerr << label << ": expected idle duration=" << duration
                      << " actual=" << idle.duration << '\n';
            return false;
        }
        const auto track = std::ranges::find(idle.tracks, head, &AnimationTrack3D::joint);
        if (track == idle.tracks.end() || (preserveHead &&
            track->keyframes.front().transform.translation.y !=
                breathing.clip.tracks.front().keyframes.front().transform.translation.y)) {
            std::cerr << label << ": authored head track was lost\n";
            return false;
        }
        if (preserveHead) {
            const auto authored = std::ranges::find_if(asset.clips, [&](const auto& sourceClip) {
                return sourceClip.clip.duration == duration &&
                    (sourceClip.name == "IdleBreathing" || sourceClip.name == "Idle") &&
                    sourceClip.clip.Valid(asset.model.skeleton);
            });
            if (authored == asset.clips.end()) return false;
            for (const auto& expected : authored->clip.tracks) {
                const auto actual = std::ranges::find(idle.tracks, expected.joint, &AnimationTrack3D::joint);
                if (actual == idle.tracks.end() || actual->keyframes.size() != expected.keyframes.size()) return false;
                for (std::size_t index = 0; index < expected.keyframes.size(); ++index) {
                    const auto& a = actual->keyframes[index];
                    const auto& e = expected.keyframes[index];
                    if (a.time != e.time || Length(a.transform.translation - e.transform.translation) != 0.f ||
                        Length(a.transform.rotation - e.transform.rotation) != 0.f ||
                        Length(a.transform.scale - e.transform.scale) != 0.f ||
                        a.transform.usesQuaternion != e.transform.usesQuaternion ||
                        a.transform.quaternion.x != e.transform.quaternion.x ||
                        a.transform.quaternion.y != e.transform.quaternion.y ||
                        a.transform.quaternion.z != e.transform.quaternion.z ||
                        a.transform.quaternion.w != e.transform.quaternion.w) {
                        std::cerr << label << ": authored joint track changed\n";
                        return false;
                    }
                }
            }
        }
        return true;
    };
    if (!run("grip-before-idle", {grip, breathing}, 3.f, true) ||
        !run("idle-before-grip", {breathing, grip}, 3.f, true) ||
        !run("idle-name", {grip, plain}, 2.f, true) ||
        !run("breathing-preferred", {plain, breathing}, 3.f, true) ||
        !run("grip-only", {grip}, 1.f, false) ||
        !run("no-clips", {}, 1.f, false)) return false;
    auto invalid = breathing;
    invalid.clip.duration = 0.f;
    if (!run("invalid-named-idle", {invalid, grip}, 1.f, false) ||
        !run("invalid-breathing-valid-idle", {invalid, plain}, 2.f, true)) return false;
    auto duplicate = breathing;
    duplicate.clip.tracks.push_back(duplicate.clip.tracks.front());
    if (!run("duplicate-named-idle", {duplicate, grip}, 1.f, false)) return false;
    auto hands = breathing;
    for (auto semantic : {HumanoidJoint3D::LeftHand, HumanoidJoint3D::RightHand}) {
        hands.clip.tracks.push_back(makeClip("", 3.f, rig.Joint(semantic)).clip.tracks.front());
    }
    if (!run("authored-head-and-hands", {hands}, 3.f, true)) return false;
    return true;
}
}

int main() {
    const auto root = std::filesystem::path{AEVRYND_SOURCE_DIR} /
        "games/character_lab/assets/model-packs";
    if (!Check(root / "realistic-female/realistic-female.aeskin") ||
        !Check(root / "realistic-male/realistic-male.aeskin")) return 1;
    std::cout << "Character idle selection passed for female and male rigs\n";
}
