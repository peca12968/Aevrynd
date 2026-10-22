#pragma once
#include "MistwatchRepairShelterVisual.h"
#include <algorithm>
#include <limits>

namespace Mistwatch::ShelterBoardUv {
// 原1K worn_planks第一塊直板內的32..80px，沒有取到圖內拼板縫。
inline constexpr float SourceULow=32.f/1024.f,SourceUHigh=80.f/1024.f;
inline constexpr float SourceVBase=.1f,SourceVPhaseStep=.025f,SourceCyclesPerMeter=.5f;
// 原材質textureScale=1不變：static 15bit round(32767/16)=2048。
inline constexpr float StaticDecodedScale=2048.f*(16.f/32767.f);
inline constexpr unsigned VerticesPerBoard=96,IndicesPerBoard=132;
struct Bounds {Aevrynd::Vec3 low,high;};
inline Bounds BoardBounds(const Aevrynd::Mesh3D& mesh,std::size_t begin){
    Bounds result{{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity()},
                  {-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}};
    if(begin>mesh.vertices.size()||mesh.vertices.size()-begin<VerticesPerBoard)throw std::runtime_error("Shelter UV source board range");
    for(std::size_t i=begin;i<begin+VerticesPerBoard;++i){const auto& v=mesh.vertices[i];
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))throw std::runtime_error("Shelter UV nonfinite geometry");
        result.low.x=(std::min)(result.low.x,v.x);result.high.x=(std::max)(result.high.x,v.x);
        result.low.y=(std::min)(result.low.y,v.y);result.high.y=(std::max)(result.high.y,v.y);
        result.low.z=(std::min)(result.low.z,v.z);result.high.z=(std::max)(result.high.z,v.z);
    }
    if(!(result.high.x>result.low.x&&result.high.y>result.low.y&&result.high.z>result.low.z))throw std::runtime_error("Shelter UV empty board extent");
    return result;
}
inline float Unit(float value,float low,float high){
    const float result=(value-low)/(high-low);
    if(!std::isfinite(result)||result<0.f||result>1.f)throw std::runtime_error("Shelter UV source coordinate range");
    return result; // 不clamp幾何或UV以冒充合法。
}
inline Aevrynd::Mesh3D CreatePlankedRepairShelterBoardUv(){
    auto result=Mistwatch::CreatePlankedRepairShelter();
    if(result.vertices.size()!=ShelterVisualVertices||result.indices.size()!=ShelterVisualIndices)
        throw std::runtime_error("Shelter UV source geometry count");
    std::size_t begin{};
    for(unsigned part=0;part<ShelterBoardCounts.size();++part){
        for(unsigned board=0;board<ShelterBoardCounts[part];++board){
            const auto first=begin+std::size_t(board)*VerticesPerBoard;
            if(part<1||part>3)continue; // 屋頂與設備連UV也保留原raw。
            const auto bounds=BoardBounds(result,first);
            const float phase=SourceVBase+SourceVPhaseStep*float((board+part*3u)%5u);
            for(std::size_t i=first;i<first+VerticesPerBoard;++i){auto& v=result.vertices[i];
                const auto n=Aevrynd::UnpackNormalSnorm8(v.normal);
                const float ax=std::abs(n.x),ay=std::abs(n.y),az=std::abs(n.z);
                float across{},along{};
                if(ay>=ax&&ay>=az){
                    // 水平端面及含Y倒角不可強制V=y；用完整非退化水平投影。
                    across=ShelterSplitAxes[part]==0?Unit(v.x,bounds.low.x,bounds.high.x):Unit(v.z,bounds.low.z,bounds.high.z);
                    const float depth=ShelterSplitAxes[part]==0?Unit(v.z,bounds.low.z,bounds.high.z):Unit(v.x,bounds.low.x,bounds.high.x);
                    const float end=v.y>=bounds.low.y+(bounds.high.y-bounds.low.y)*.5f?
                        (bounds.high.y-bounds.low.y)*SourceCyclesPerMeter:0.f;
                    along=phase+end+depth*SourceVPhaseStep;
                }else{
                    across=ax>=az?Unit(v.z,bounds.low.z,bounds.high.z):Unit(v.x,bounds.low.x,bounds.high.x);
                    along=phase+(v.y-bounds.low.y)*SourceCyclesPerMeter;
                }
                const float sourceU=SourceULow+(SourceUHigh-SourceULow)*across;
                if(!std::isfinite(along)||along<0.f||along>=1.f)throw std::runtime_error("Shelter UV fixed longitudinal domain");
                v.u=sourceU/StaticDecodedScale;v.v=along/StaticDecodedScale;
            }
        }
        begin+=std::size_t(ShelterBoardCounts[part])*VerticesPerBoard;
    }
    if(begin!=result.vertices.size())throw std::runtime_error("Shelter UV complete board membership");
    result.InvalidateRaycastIndex();return result;
}
}
