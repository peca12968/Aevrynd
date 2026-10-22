#pragma once
#include "MistwatchRepairShelterBoardUv.h"
#include <cmath>
#include <stdexcept>

namespace Mistwatch::ShelterRoofUv {
inline constexpr unsigned RoofBoards=20,VerticesPerBoard=96,IndicesPerBoard=132;
inline constexpr unsigned OwnedVertices=RoofBoards*VerticesPerBoard,OwnedIndices=RoofBoards*IndicesPerBoard;
inline constexpr float SourceULow=32.f/1024.f,SourceUWidth=48.f/1024.f;
inline constexpr float SourceVBase=.1f,SourceVPhaseStep=.025f,SourceCyclesPerMeter=.5f;
inline constexpr float StaticDecodedScale=2048.f*(16.f/32767.f);
static_assert(ShelterBoardCounts[0]==RoofBoards&&ShelterSplitAxes[0]==0);
static_assert(ShelterVisualVertices==7392&&ShelterVisualIndices==10164);

inline double SignedJacobian(const Aevrynd::Mesh3D& mesh,std::size_t first) {
    const auto& a=mesh.vertices[mesh.indices[first]];
    const auto& b=mesh.vertices[mesh.indices[first+1]];
    const auto& c=mesh.vertices[mesh.indices[first+2]];
    return (double(b.u)-a.u)*(double(c.v)-a.v)-(double(c.u)-a.u)*(double(b.v)-a.v);
}

// 先完整套用原牆板合同，再只擁有part0的1920組u/v；不接受任意外部mesh。
[[nodiscard]] inline Aevrynd::Mesh3D CreatePlankedRepairShelterRoofUv() {
    const auto source=ShelterBoardUv::CreatePlankedRepairShelterBoardUv();
    if(!source.Valid()||source.vertices.size()!=ShelterVisualVertices||source.indices.size()!=ShelterVisualIndices)
        throw std::runtime_error("Roof UV source membership");
    auto result=source;
    for(unsigned board=0;board<RoofBoards;++board) {
        const auto first=std::size_t(board)*VerticesPerBoard;
        const auto bounds=ShelterBoardUv::BoardBounds(source,first);
        const float phase=SourceVBase+SourceVPhaseStep*float(board%5u);
        for(std::size_t i=first;i<first+VerticesPerBoard;++i) {
            auto& p=result.vertices[i];const auto n=Aevrynd::UnpackNormalSnorm8(p.normal);
            const float ax=std::abs(n.x),ay=std::abs(n.y),az=std::abs(n.z);
            float across{},distance{},length{};
            // 保留原主軸與正向投影；不靠翻UV／normal修正手性。
            if(ay>=ax&&ay>=az) {
                across=ShelterBoardUv::Unit(p.x,bounds.low.x,bounds.high.x);
                distance=p.z-bounds.low.z;length=bounds.high.z-bounds.low.z;
            }else if(ax>=az) {
                across=ShelterBoardUv::Unit(p.y,bounds.low.y,bounds.high.y);
                distance=p.z-bounds.low.z;length=bounds.high.z-bounds.low.z;
            }else {
                across=ShelterBoardUv::Unit(p.y,bounds.low.y,bounds.high.y);
                distance=p.x-bounds.low.x;length=bounds.high.x-bounds.low.x;
            }
            if(!std::isfinite(distance)||distance<0.f||distance>length)
                throw std::runtime_error("Roof UV longitudinal source extent");
            const float sourceU=SourceULow+SourceUWidth*across;
            const float sourceV=phase+distance*SourceCyclesPerMeter;
            p.u=sourceU/StaticDecodedScale;p.v=sourceV/StaticDecodedScale;
            if(!std::isfinite(p.u)||!std::isfinite(p.v))throw std::runtime_error("Roof UV nonfinite result");
        }
    }
    // 真原Jacobian與新Jacobian均須非零且同號；首錯即停，沒有替代投影。
    for(std::size_t first=0;first<OwnedIndices;first+=3) {
        const double before=SignedJacobian(source,first),after=SignedJacobian(result,first);
        if(!std::isfinite(before)||before==0||!std::isfinite(after)||after==0||std::signbit(before)!=std::signbit(after))
            throw std::runtime_error("Roof UV signed Jacobian changed");
    }
    result.InvalidateRaycastIndex();return result;
}
}
