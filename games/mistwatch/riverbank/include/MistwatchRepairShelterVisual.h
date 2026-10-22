#pragma once
#include "MistwatchWorld.h"
#include <Aevrynd/PlankedPanel3D.h>
#include <array>
#include <stdexcept>

namespace Mistwatch {
inline constexpr std::array<unsigned,5> ShelterBoardCounts{20,16,20,20,1};
inline constexpr std::array<unsigned,5> ShelterSplitAxes{0,2,0,0,0};
inline constexpr unsigned ShelterVisualBoards=77;
inline constexpr unsigned ShelterVisualVertices=ShelterVisualBoards*96u;
inline constexpr unsigned ShelterVisualIndices=ShelterVisualBoards*132u;

// 真板縫與倒角留在原五個碰撞體內；不改入口與1.45m屋頂下緣。
inline Aevrynd::Mesh3D CreatePlankedRepairShelter() {
    Aevrynd::Mesh3D result;
    const auto solids=RepairShelterSolids();
    if(solids.size()!=ShelterBoardCounts.size())
        throw std::runtime_error("Shelter source dimensions changed");
    result.vertices.reserve(ShelterVisualVertices);
    result.indices.reserve(ShelterVisualIndices);
    for(std::size_t part=0;part<solids.size();++part) {
        auto panel=Aevrynd::CreatePlankedPanel3D(solids[part].center,solids[part].extents*2.f,
            ShelterSplitAxes[part],ShelterBoardCounts[part],.007f,.004f,unsigned(part)*3u);
        if(!panel)throw std::runtime_error("Planked shelter source panel failed");
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        result.vertices.insert(result.vertices.end(),panel->vertices.begin(),panel->vertices.end());
        for(const auto index:panel->indices)result.indices.push_back(base+index);
    }
    result.InvalidateRaycastIndex();
    return result;
}
}
