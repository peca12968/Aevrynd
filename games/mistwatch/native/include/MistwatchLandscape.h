#pragma once
#include "MistwatchWorld.h"
#include <limits>
namespace Mistwatch {
// 遠景環帶只提供視覺地理背景，不替代可玩區域的地形或角色碰撞。
inline float LandscapeHeight(const Aevrynd::HeightField3D& core,float x,float z) {
    const float edgeX=std::clamp(x,-50.f,50.f),edgeZ=std::clamp(z,-20.f,100.f);
    const float distance=std::hypot(x-edgeX,z-edgeZ);
    const float t=std::clamp(distance/60.f,0.f,1.f),blend=t*t*t*(t*(t*6.f-15.f)+10.f);
    const float ridge=24.f+18.f*std::sin(x*.023f+z*.007f)+14.f*std::cos(z*.027f-x*.009f)+
        8.f*std::sin(x*.057f+z*.041f)+.11f*distance;
    const float edge=core.SampleHeight(edgeX,edgeZ);
    const float original=edge+(ridge-edge)*blend;
    if(x==edgeX||distance>=60.f)return original;
    // 邊界夾取不能把河床凹槽拖成遠山直溝；在四個外側格距內平滑收束。
    // 從同一解析地形來源取得切槽深度，不將乾地的插值誤差當作溪谷。
    const float incision=(std::max)(0.f,HillElevation(edgeX,edgeZ)-GroundElevation(edgeX,edgeZ));
    if(incision==0.f)return original;
    const float releaseT=std::clamp(std::abs(x-edgeX)/24.f,0.f,1.f);
    const float release=releaseT*releaseT*releaseT*(releaseT*(releaseT*6.f-15.f)+10.f);
    return original+incision*release*(1.f-blend);
}
inline Aevrynd::Vec3 LandscapeNormal(const Aevrynd::HeightField3D& core,float x,float z) {
    if(core.Contains(x,z))return core.SampleNormal(x,z);
    return Aevrynd::Normalize({LandscapeHeight(core,x-.5f,z)-LandscapeHeight(core,x+.5f,z),1.f,
        LandscapeHeight(core,x,z-.5f)-LandscapeHeight(core,x,z+.5f)});
}
inline Aevrynd::Mesh3D CreateLandscapeMesh(const Aevrynd::HeightField3D& core,bool layerHeight=true) {
    using namespace Aevrynd;
    const auto axis=[](float start,unsigned samples) {
        std::vector<float> values;
        for(int step=25;step>=1;--step)values.push_back(start-float(step)*6.f);
        for(unsigned step=0;step<samples;++step)values.push_back(start+float(step)*.5f);
        const float end=start+float(samples-1)*.5f;
        for(unsigned step=1;step<=25;++step)values.push_back(end+float(step)*6.f);
        return values;
    };
    const auto xs=axis(-50,201),zs=axis(-20,241);
    const auto original=CreateGroundMesh(core,layerHeight);
    Mesh3D mesh;
    std::vector<std::uint32_t> lookup(xs.size()*zs.size(),(std::numeric_limits<std::uint32_t>::max)());
    const auto vertex=[&](unsigned x,unsigned z) {
        auto& index=lookup[std::size_t(z)*xs.size()+x];
        if(index!=(std::numeric_limits<std::uint32_t>::max)())return index;
        index=std::uint32_t(mesh.vertices.size());
        if(x>=25&&x<=225&&z>=25&&z<=265) {
            // 接縫沿用原始半公尺格點的完整頂點，保留高度、法線與材質色。
            mesh.vertices.push_back(original.vertices[std::size_t(z-25)*201+x-25]);
        } else {
            const float y=LandscapeHeight(core,xs[x],zs[z]);
            const Vec3 normal=LandscapeNormal(core,xs[x],zs[z]);
            const float rock=std::clamp((.82f-normal.y)/.35f,0.f,1.f);
            const Color green{.38f,.47f,.30f,1},stone{.50f,.48f,.43f,1};
            const Color color{green.r+(stone.r-green.r)*rock,green.g+(stone.g-green.g)*rock,
                green.b+(stone.b-green.b)*rock,1};
            const auto packed=PackColorRgba8(color);
            mesh.vertices.push_back({xs[x],y,zs[z],PackNormalSnorm8(normal),
                layerHeight?GroundLayerHeightColor(packed,y,core):packed});
        }
        return index;
    };
    for(unsigned z=0;z+1<zs.size();++z)for(unsigned x=0;x+1<xs.size();++x) {
        if(x>=25&&x<225&&z>=25&&z<265)continue;
        const auto a=vertex(x,z),b=vertex(x+1,z),c=vertex(x,z+1),d=vertex(x+1,z+1);
        mesh.indices.insert(mesh.indices.end(),{a,c,b,b,c,d});
    }
    return mesh;
}
inline float LandscapeSurfaceHeight(const Aevrynd::HeightField3D& core,float x,float z) {
    // 樹根貼到真正的格點三角面，不使用格點之間的解析曲面高度。
    const auto cell=[](float value,float start,float end) {
        if(value<start)return Aevrynd::Vec2{start+std::floor((value-start)/6.f)*6.f,6.f};
        if(value>=end)return Aevrynd::Vec2{end+std::floor((value-end)/6.f)*6.f,6.f};
        return Aevrynd::Vec2{start+std::floor((value-start)/.5f)*.5f,.5f};
    };
    const auto cx=cell(x,-50.f,50.f),cz=cell(z,-20.f,100.f);
    const float tx=(x-cx.x)/cx.y,tz=(z-cz.x)/cz.y;
    const float a=LandscapeHeight(core,cx.x,cz.x),b=LandscapeHeight(core,cx.x+cx.y,cz.x);
    const float c=LandscapeHeight(core,cx.x,cz.x+cz.y),d=LandscapeHeight(core,cx.x+cx.y,cz.x+cz.y);
    return tx+tz<=1.f?a+(b-a)*tx+(c-a)*tz:d+(c-d)*(1.f-tx)+(b-d)*(1.f-tz);
}
inline std::vector<Aevrynd::Transform3D> LandscapeTrees(const Aevrynd::HeightField3D& core) {
    using namespace Aevrynd;
    std::vector<Transform3D> trees;
    std::uint32_t seed=0x52494447u;
    const auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    for(unsigned attempt=0;attempt<9000&&trees.size()<520;++attempt) {
        const float x=-150.f+300.f*random(),z=-110.f+300.f*random();
        if(x>=-53.f&&x<=53.f&&z>=-23.f&&z<=103.f)continue;
        if(LandscapeNormal(core,x,z).y<.78f)continue;
        const float patch=.55f+.25f*std::sin(x*.06f-z*.04f);
        if(random()>patch)continue;
        bool occupied=false;
        for(const auto& tree:trees)occupied|=std::hypot(x-tree.position.x,z-tree.position.z)<5.f;
        if(occupied)continue;
        const float scale=.5f+.35f*random();
        trees.push_back({{x,LandscapeSurfaceHeight(core,x,z),z},{0,random()*6.2831853f,0},{scale,scale,scale}});
    }
    return trees;
}
}
