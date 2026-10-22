#pragma once
#include "MistwatchSupplyCrate.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>

namespace Mistwatch {
// static MeshInstance: Renderer round(.5/16*32767)=1024，World shader同式解碼。
// 不是skinned的8bit textureScale；原材質 .5 完全不改。
inline constexpr float SupplyCrateStaticDecodedScale=1024.f*(16.f/32767.f);
struct SupplyCrateUvBoard {
    unsigned id{},firstVertex{},vertexCount{},firstIndex{},indexCount{},longAxis{};
    Aevrynd::Vec3 center{},size{};
    std::array<Aevrynd::Vec3,3> axes{};
    bool beveled{};
};
inline std::array<SupplyCrateUvBoard,34> SupplyCrateUvBoards() {
    using namespace Aevrynd;
    std::array<SupplyCrateUvBoard,34> boards{};unsigned count{},vertex{},index{};
    const auto add=[&](Vec3 center,Vec3 size,unsigned longAxis,bool beveled,
        std::array<Vec3,3> axes=std::array<Vec3,3>{{{1,0,0},{0,1,0},{0,0,1}}}) {
        if(count>=boards.size())throw std::runtime_error("Crate source board count overflow");
        const unsigned vc=beveled?96u:24u,ic=beveled?132u:36u;
        boards[count]={count,vertex,vc,index,ic,longAxis,center,size,axes,beveled};
        ++count;vertex+=vc;index+=ic;
    };
    // 只描述原CreateSupplyCrate的34件順序／basis；不重新建立任何幾何。
    for(unsigned row=0;row<4;++row){const float y=.13f+.18f*float(row);
        for(const float side:{-1.f,1.f}){
            add({0,y,side*.346f},{.78f,.171f,.056f},0,false);
            add({side*.346f,y,0},{.056f,.171f,.676f},2,false);
        }}
    for(unsigned board=0;board<5;++board){const float x=(float(board)-2.f)*.156f;
        add({x,.759f,0},{.15f,.07f,.78f},2,false);
        add({x,.041f,0},{.15f,.07f,.78f},2,false);
    }
    for(const float x:{-.327f,.327f})for(const float z:{-.327f,.327f})
        add({x,.4f,z},{.076f,.66f,.076f},1,true);
    const Vec3 front=Normalize(Vec3{.654f,.5f,0}),sideAxis=Normalize(Vec3{0,.5f,.654f});
    const float length=Length(Vec3{.654f,.5f,0});
    for(const float side:{-1.f,1.f}){
        add({0,.4f,side*.370f},{length,.060f,.016f},0,true,
            std::array<Vec3,3>{{front,{-front.y,front.x,0},{0,0,1}}});
        add({side*.370f,.4f,0},{length,.060f,.016f},0,true,
            std::array<Vec3,3>{{sideAxis,{0,-sideAxis.z,sideAxis.y},{1,0,0}}});
    }
    if(count!=34||vertex!=1392||index!=1992)throw std::runtime_error("Original crate board spans changed");
    return boards;
}
namespace SupplyCrateUvDetail {
inline float Dimension(Aevrynd::Vec3 v,unsigned axis){return axis==0?v.x:axis==1?v.y:v.z;}
inline Aevrynd::Vec3 Position(const Aevrynd::Vertex3D& v){return {v.x,v.y,v.z};}
inline void Face(Aevrynd::Mesh3D& mesh,const SupplyCrateUvBoard& board,unsigned first,unsigned count) {
    using namespace Aevrynd;
    const auto a=Position(mesh.vertices[first]),b=Position(mesh.vertices[first+1]),c=Position(mesh.vertices[first+2]);
    const auto n=Cross(b-a,c-a);
    if(!std::isfinite(Length(n))||Length(n)==0)throw std::runtime_error("Degenerate original crate face");
    unsigned drop{};float largest=std::abs(Dot(n,board.axes[0]));
    for(unsigned k=1;k<3;++k){const float value=std::abs(Dot(n,board.axes[k]));if(value>largest){drop=k;largest=value;}}
    // 面的主法向軸被略去；側面U沿真長軸，端面取其餘兩軸，避免zero Jacobian。
    const unsigned uAxis=drop==board.longAxis?(board.longAxis+1)%3:board.longAxis;
    const unsigned vAxis=3-drop-uAxis;
    const float tileCenter=(float(board.id%8)*128.f+64.f)/1024.f;
    for(unsigned k=0;k<count;++k){auto& v=mesh.vertices[first+k];const auto local=Position(v)-board.center;
        const float u=.5f+Dot(local,board.axes[uAxis])/Dimension(board.size,uAxis);
        const float transverse=Dot(local,board.axes[vAxis])/Dimension(board.size,vAxis);
        // 1024原圖8橫板：中心±24px；同一UV也給roughness/NormalDX，不改資產RGB。
        v.u=u/SupplyCrateStaticDecodedScale;
        v.v=(tileCenter+transverse*(48.f/1024.f))/SupplyCrateStaticDecodedScale;
        if(!std::isfinite(v.u)||!std::isfinite(v.v))throw std::runtime_error("Nonfinite crate authored UV");
    }
}
}
inline SupplyCrateGeometry CreateSupplyCrateBoardUv() {
    auto result=CreateSupplyCrate();const auto boards=SupplyCrateUvBoards();
    if(result.wood.vertices.size()!=1392||result.wood.indices.size()!=1992||result.metal.indices.size()!=708)
        throw std::runtime_error("Original supply source counts changed");
    for(const auto& board:boards){unsigned first=board.firstVertex;
        const unsigned faces=board.beveled?26u:6u;
        for(unsigned face=0;face<faces;++face){const unsigned count=board.beveled&&face>=18?3u:4u;
            SupplyCrateUvDetail::Face(result.wood,board,first,count);first+=count;
        }
        if(first!=board.firstVertex+board.vertexCount)throw std::runtime_error("Crate source face spans changed");
    }
    // XYZ不改但UV內容變了；不沿用舊不可變mesh的cache語義。
    result.wood.InvalidateRaycastIndex();
    return result;
}
}
