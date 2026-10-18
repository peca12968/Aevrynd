#pragma once
#include <Aevrynd/BeveledBox3D.h>
#include <optional>

namespace Aevrynd {
// 尺寸為全長，splitAxis為0/1/2；seam及bevel使用公尺。
// 每塊板有真倒角與獨立紋理相位，UV密度不依板數改變。
// 無效或不可表示的幾何回傳nullopt，不回傳部分完成的面板。
[[nodiscard]] inline std::optional<Mesh3D> CreatePlankedPanel3D(Vec3 center,Vec3 size,unsigned splitAxis,
    unsigned boardCount,float seam,float bevel,unsigned phase=0) {
    if(!BeveledBoxDetail3D::Finite(center)||!BeveledBoxDetail3D::Finite(size)||
       size.x<=0||size.y<=0||size.z<=0||splitAxis>2||boardCount==0||boardCount>128||
       !std::isfinite(seam)||seam<0||!std::isfinite(bevel)||bevel<=0)return std::nullopt;
    const Vec3 sourceLow=center-size*.5f,sourceHigh=center+size*.5f;
    if(!BeveledBoxDetail3D::Finite(sourceLow)||!BeveledBoxDetail3D::Finite(sourceHigh))return std::nullopt;
    const std::array<float,3> lengths{size.x,size.y,size.z},origin{center.x,center.y,center.z};
    const float step=lengths[splitAxis]/float(boardCount);
    if(!(seam<step&&2*bevel<(std::min)({step-seam,size.x,size.y,size.z})))return std::nullopt;
    Mesh3D result;
    for(unsigned board=0;board<boardCount;++board) {
        const float lower=-lengths[splitAxis]*.5f+float(board)*step+(board==0?0.f:seam*.5f);
        const float upper=-lengths[splitAxis]*.5f+float(board+1)*step-(board+1==boardCount?0.f:seam*.5f);
        const float lo=(std::max)(origin[splitAxis]-lengths[splitAxis]*.5f,origin[splitAxis]+lower);
        const float hi=(std::min)(origin[splitAxis]+lengths[splitAxis]*.5f,origin[splitAxis]+upper);
        const float midpoint=lo+(hi-lo)*.5f;
        // 用實際端點重建半徑並向內一個ULP；不讓浮點加法越過原包圍盒。
        const float radius=std::nextafter((std::min)(midpoint-lo,hi-midpoint),0.f);
        if(!(radius>bevel))return std::nullopt;
        Vec3 plankCenter=center,plankSize=size;
        if(splitAxis==0){plankCenter.x=midpoint;plankSize.x=radius*2.f;}
        else if(splitAxis==1){plankCenter.y=midpoint;plankSize.y=radius*2.f;}
        else {plankCenter.z=midpoint;plankSize.z=radius*2.f;}
        const float shade=.86f+.02f*float((board*5u+phase)%7u);
        const auto first=result.vertices.size();
        if(!AppendBeveledBox3D(result,plankCenter,plankSize,bevel,{shade,shade,shade,1}))return std::nullopt;
        for(std::size_t index=first;index<result.vertices.size();++index) {
            auto& v=result.vertices[index];const auto n=UnpackNormalSnorm8(v.normal);
            if(v.x<sourceLow.x||v.x>sourceHigh.x||v.y<sourceLow.y||v.y>sourceHigh.y||
               v.z<sourceLow.z||v.z>sourceHigh.z)return std::nullopt;
            const Vec3 p{v.x-center.x,v.y-center.y,v.z-center.z};
            const float ax=std::abs(n.x),ay=std::abs(n.y),az=std::abs(n.z);
            const float variation=float((board*13u+phase)%17u)*.071f;
            // 最大法線軸投影，端面／倒角亦有非退化UV；直板沿Y保留木紋。
            if(ay>=ax&&ay>=az){v.u=p.x*.5f+variation;v.v=p.z*.5f;}
            else if(ax>=az){v.u=p.y*.5f+variation;v.v=p.z*.5f;}
            else {v.u=p.y*.5f+variation;v.v=p.x*.5f;}
            if(!std::isfinite(v.u)||!std::isfinite(v.v))return std::nullopt;
        }
    }
    if(!result.Valid())return std::nullopt;
    return result;
}
}
