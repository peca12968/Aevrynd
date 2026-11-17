#include "RiverbankAvatarMotion.h"
#include "RiverbankJumpPresentation.h"
#include "MistwatchWorld.h"

#include <cmath>
#include <iostream>
#include <limits>

int main(int argc, char** argv) {
    using namespace Aevrynd;
    using namespace Mistwatch;
    if (argc != 2) return 1;
    const auto asset = LoadRiverbankAvatarAsset(std::filesystem::path(argv[1]));
    const RiverbankAvatarMotion motion(asset);
    if (!motion.HasJumpClip()) {
        std::cerr << "Missing authored runtime jump\n";
        return 2;
    }
    const auto sole = std::ranges::find(asset.materialSlotNames, RiverbankBootSole);
    const auto slot = static_cast<std::size_t>(sole - asset.materialSlotNames.begin());
    std::vector<std::uint32_t> soleIndices;
    for (const auto& part : asset.submeshes)
        if (part.materialSlot == slot)
            for (auto index : std::span(asset.model.bindMesh.indices).subspan(part.firstIndex, part.indexCount))
                soleIndices.push_back(index);
    if (soleIndices.empty()) return 3;

    const auto terrain = CreateTerrain();
    const RiverbankWalkSurface surface{terrain};
    const auto position = [&](float x,float z,float relativeHeight) {
        return Vec3{x,surface.SampleHeight(x,z)+relativeHeight,z};
    };
    AnimationPose3D sampled, pose;
    SkinningWorkspace3D skinning;
    Mesh3D mesh;
    RiverbankJumpPresentation view;
    RiverbankAvatarMotion::Player actor{};
    auto footGap = [&](Vec3 feet, float height) {
        const auto* support = height > RiverbankJumpPresentation::GroundTolerance ? nullptr : &surface;
        const auto normal = support ? support->SampleNormal(feet.x,feet.z) : Vec3{0,1,0};
        const auto world = motion.Evaluate(asset.model.skeleton,actor,feet,sampled,pose,normal,support,view.ClipTime(height));
        if (!SkinModelInto(asset.model,pose,skinning,mesh)) throw std::runtime_error("Could not skin jump pose");
        auto lowest=std::numeric_limits<float>::infinity();
        for (auto index : soleIndices) {
            const auto& v=mesh.vertices[index];
            const auto point=TransformPoint(world,{v.x,v.y,v.z});
            lowest=std::min(lowest,point.y-surface.SampleHeight(point.x,point.z));
        }
        return lowest;
    };

    float previousHeight=0.f, minimum=0.f, apex=0.f, finalGround=0.f;
    float previousPhase=RiverbankJumpPresentation::FlightClipStart;
    for (unsigned age=1; age<=RiverbankJump::FlightTicks; ++age) {
        const auto height=RiverbankJump::Height(age);
        view.Observe(age,height,height-previousHeight,true);
        const auto phase=view.ClipTime(height);
        if (!phase || *phase < previousPhase || std::abs(*phase-(.20f+.60f*static_cast<float>(age)/RiverbankJump::FlightTicks))>.001f) return 4;
        previousHeight=height;
        previousPhase=*phase;
        const auto gap=footGap(position(3.f,2.f,height),height);
        minimum=std::min(minimum,gap);
        if (age==RiverbankJump::FlightTicks/2) apex=gap;
        if (age==RiverbankJump::FlightTicks) finalGround=gap;
    }
    for (unsigned frame=1; frame<13; ++frame) {
        view.Advance(1.f/60.f,0.f);
        minimum=std::min(minimum,footGap(position(3.f,2.f,0.f),0.f));
    }
    if (view.ClipTime(0.f)) return 5;
    // 用中段加入及不帶垂直位移的重置檢查，避免播放反向或誤播落地。
    const auto descending=RiverbankJump::Height(12);
    view.Observe(101,descending,descending-RiverbankJump::Height(11),true);
    if (std::abs(view.ClipTime(descending).value_or(0.f)-.65f)>.001f) return 6;
    view.Observe(102,0.f,0.f,true);
    if (view.ClipTime(0.f)) return 7;
    std::cout << "native_jump minimum_sole_m=" << minimum << " apex_sole_m=" << apex
              << " grounded_sole_m=" << finalGround << '\n';
    return minimum >= -.006f && apex > .72f && finalGround < .05f ? 0 : 8;
}
