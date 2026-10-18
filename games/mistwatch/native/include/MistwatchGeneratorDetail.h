#pragma once
#include <Aevrynd/Mesh3D.h>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace Mistwatch {
inline constexpr std::size_t GeneratorDetailGroups=3;
inline constexpr std::size_t GeneratorDetailTriangleBudget=1800;
inline constexpr unsigned GeneratorIndicatorSides=8;
inline constexpr float GeneratorIndicatorRadius=.022f;
inline Aevrynd::Vec3 GeneratorIndicatorCentre(unsigned lamp) {
    if(lamp>=3)throw std::out_of_range("Generator indicator index");
    return {.17f+.22f*float(lamp),1.01f,-.568f};
}
inline Aevrynd::Color GeneratorIndicatorColour(unsigned circuit,unsigned lamp) {
    if(lamp>=3)throw std::out_of_range("Generator indicator index");
    return circuit&(1u<<lamp)?Aevrynd::Color{.2f,1,.3f,1}:Aevrynd::Color{1,.15f,.1f,1};
}
// 燈鏡沿負 Z 面嵌入檢修面板；僅 4.4 公分寬，不新增 GPU 資源。
inline std::array<std::array<Aevrynd::Vec3,3>,GeneratorIndicatorSides> GeneratorIndicatorLens(unsigned lamp) {
    const auto centre=GeneratorIndicatorCentre(lamp);
    std::array<std::array<Aevrynd::Vec3,3>,GeneratorIndicatorSides> result;
    const auto point=[&](unsigned side) {
        const float angle=6.28318530718f*float(side)/float(GeneratorIndicatorSides);
        return Aevrynd::Vec3{centre.x+GeneratorIndicatorRadius*std::cos(angle),
            centre.y+GeneratorIndicatorRadius*std::sin(angle),centre.z};
    };
    for(unsigned side=0;side<GeneratorIndicatorSides;++side)
        result[side]={centre,point(side+1u),point(side)};
    return result;
}
// 分件範圍僅供作者稽核；正式 GPU 上傳依三種材質合併，不逐零件派送。
struct GeneratorDetailPart {
    std::size_t group{},firstIndex{},indexCount{};
    Aevrynd::Vec3 centre{};
};
struct GeneratorDetailGeometry {
    std::array<Aevrynd::Mesh3D,GeneratorDetailGroups> meshes;
    std::vector<GeneratorDetailPart> parts;
};
inline std::array<Aevrynd::Material3D,GeneratorDetailGroups> GeneratorDetailMaterials() {
    // 既有 Surface PBR 的漆面、深色機件、裸露鋼件；無自發光或透明遮掩。
    return {{{{.22f,.29f,.25f,1},{.58f,.18f,0,1},true},
             {{.055f,.067f,.064f,1},{.72f,.28f,0,1},true},
             {{.46f,.49f,.50f,1},{.34f,.82f,0,1},true}}};
}
namespace GeneratorDetailAuthor {
using Aevrynd::Vec3;
inline void Triangle(Aevrynd::Mesh3D& mesh,Vec3 a,Vec3 b,Vec3 c) {
    const Vec3 cross=Aevrynd::Cross(b-a,c-a);
    const float length=std::sqrt(Aevrynd::Dot(cross,cross));
    if(!std::isfinite(length)||length<=1.e-9f)throw std::runtime_error("Degenerate generator triangle");
    // 細小螺栓不能套用 Normalize 的大面積閾值，仍使用引擎真正法線封裝。
    const auto normal=Aevrynd::PackNormalSnorm8(cross*(1.f/length));
    const auto first=static_cast<std::uint32_t>(mesh.vertices.size());
    mesh.vertices.push_back({a.x,a.y,a.z,normal,0xffffffffu,0,0});
    mesh.vertices.push_back({b.x,b.y,b.z,normal,0xffffffffu,1,0});
    mesh.vertices.push_back({c.x,c.y,c.z,normal,0xffffffffu,0,1});
    mesh.indices.insert(mesh.indices.end(),{first,first+1u,first+2u});
}
inline void Quad(Aevrynd::Mesh3D& mesh,Vec3 a,Vec3 b,Vec3 c,Vec3 d) {
    Triangle(mesh,a,b,c);Triangle(mesh,a,c,d);
}
inline void Box(GeneratorDetailGeometry& result,std::size_t group,Vec3 centre,Vec3 half,float tilt=0) {
    auto& mesh=result.meshes.at(group);const auto first=mesh.indices.size();
    const float x0=centre.x-half.x,x1=centre.x+half.x;
    const float y0=centre.y-half.y,y1=centre.y+half.y;
    const float z0=centre.z-half.z,z1=centre.z+half.z;
    const auto rotate=[&](Vec3 p){const float y=p.y-centre.y,z=p.z-centre.z;
        return Vec3{p.x,centre.y+y*std::cos(tilt)-z*std::sin(tilt),
            centre.z+y*std::sin(tilt)+z*std::cos(tilt)};};
    const auto face=[&](Vec3 a,Vec3 b,Vec3 c,Vec3 d){Quad(mesh,rotate(a),rotate(b),rotate(c),rotate(d));};
    face({x0,y0,z0},{x0,y1,z0},{x1,y1,z0},{x1,y0,z0});
    face({x0,y0,z1},{x1,y0,z1},{x1,y1,z1},{x0,y1,z1});
    face({x0,y0,z0},{x0,y0,z1},{x0,y1,z1},{x0,y1,z0});
    face({x1,y0,z0},{x1,y1,z0},{x1,y1,z1},{x1,y0,z1});
    face({x0,y0,z0},{x1,y0,z0},{x1,y0,z1},{x0,y0,z1});
    face({x0,y1,z0},{x0,y1,z1},{x1,y1,z1},{x1,y1,z0});
    result.parts.push_back({group,first,mesh.indices.size()-first,centre});
}
inline void Prism(GeneratorDetailGeometry& result,std::size_t group,
                  const std::vector<Aevrynd::Vec2>& outline,float lowZ,float highZ) {
    auto& mesh=result.meshes.at(group);const auto first=mesh.indices.size();Vec3 centre{};
    for(const auto p:outline){centre.x+=p.x;centre.y+=p.y;}
    const float inverse=1.f/static_cast<float>(outline.size());
    centre.x*=inverse;centre.y*=inverse;centre.z=(lowZ+highZ)*.5f;
    for(std::size_t i=0;i<outline.size();++i) {
        const auto a=outline[i],b=outline[(i+1u)%outline.size()];
        Triangle(mesh,{centre.x,centre.y,highZ},{a.x,a.y,highZ},{b.x,b.y,highZ});
        Triangle(mesh,{centre.x,centre.y,lowZ},{b.x,b.y,lowZ},{a.x,a.y,lowZ});
        Quad(mesh,{a.x,a.y,lowZ},{b.x,b.y,lowZ},{b.x,b.y,highZ},{a.x,a.y,highZ});
    }
    result.parts.push_back({group,first,mesh.indices.size()-first,centre});
}
inline void Bolt(GeneratorDetailGeometry& result,float x,float y,float z) {
    std::vector<Aevrynd::Vec2> ring;ring.reserve(6);
    for(unsigned i=0;i<6;++i) {
        const float angle=6.28318530718f*static_cast<float>(i)/6.f;
        ring.push_back({x+.022f*std::cos(angle),y+.022f*std::sin(angle)});
    }
    Prism(result,2,ring,z-.011f,z+.011f);
}
}
inline GeneratorDetailGeometry CreateGeneratorDetail() {
    using namespace GeneratorDetailAuthor;
    GeneratorDetailGeometry result;
    for(auto& mesh:result.meshes){mesh.vertices.reserve(2100);mesh.indices.reserve(2100);}
    result.parts.reserve(96);
    // 八角截面有真斜面、厚度與底面，並非在舊方塊上貼圖。
    Prism(result,0,{{-.70f,.16f},{.70f,.16f},{.78f,.24f},{.78f,1.02f},
                   {.70f,1.10f},{-.70f,1.10f},{-.78f,1.02f},{-.78f,.24f}},-.50f,.50f);
    // 兩條接地滑橇、支座與側護框。
    for(float z:{-.43f,.43f}) {
        Box(result,1,{0,.04f,z},{.82f,.04f,.065f});
        for(float x:{-.63f,.63f})Box(result,2,{x,.125f,z},{.075f,.045f,.055f});
        Box(result,2,{0,.22f,z*1.30f},{.82f,.028f,.024f});
    }
    // 正面負 Z：完整檢修門、暗縫、鉸鏈、U 形把手，正常南側／西側來路可見。
    Box(result,1,{-.29f,.635f,-.511f},{.335f,.365f,.012f});
    Prism(result,0,{{-.595f,.30f},{.015f,.30f},{.035f,.32f},{.035f,.95f},
                   {.015f,.97f},{-.595f,.97f},{-.615f,.95f},{-.615f,.32f}},-.538f,-.523f);
    for(float y:{.40f,.84f})Box(result,2,{-.62f,y,-.545f},{.024f,.055f,.013f});
    for(float y:{.55f,.73f})Box(result,2,{-.055f,y,-.559f},{.025f,.022f,.024f});
    Box(result,2,{-.055f,.64f,-.580f},{.020f,.11f,.012f});
    // 右側前置通風口：九條傾斜式凸百葉與有厚度的格柵框。
    Box(result,1,{.39f,.65f,-.513f},{.285f,.315f,.014f});
    for(unsigned row=0;row<9;++row) {
        const float y=.39f+static_cast<float>(row)*.063f;
        Box(result,0,{.39f,y,-.542f},{.27f,.014f,.018f},.38f);
    }
    for(float x:{.10f,.68f})Box(result,2,{x,.65f,-.547f},{.014f,.335f,.013f});
    // 西／東端大通風框與可見深色機件，不只裝飾背面小鉚釘。
    for(float x:{-.792f,.792f}) {
        Box(result,1,{x,.65f,0},{.010f,.30f,.34f});
        for(unsigned row=0;row<8;++row)
            Box(result,0,{x*1.02f,.405f+static_cast<float>(row)*.07f,0},{.009f,.015f,.34f});
        for(float z:{-.36f,.36f})Box(result,2,{x,.65f,z},{.016f,.32f,.016f});
    }
    // 背側電機維修蓋與頂部搬運把手；未啟動也不假亮。
    Box(result,1,{0,.61f,.513f},{.42f,.27f,.012f});
    Box(result,0,{0,.61f,.535f},{.40f,.25f,.012f});
    for(float x:{-.47f,.47f}) {
        for(float z:{-.24f,.24f})Box(result,2,{x,1.105f,z},{.025f,.050f,.027f});
        Box(result,2,{x,1.155f,0},{.025f,.018f,.27f});
    }
    for(float y:{.32f,.95f})for(float x:{-.57f,.00f,.15f,.63f})Bolt(result,x,y,-.558f);
    for(float y:{.40f,.82f})for(float x:{-.36f,.36f})Bolt(result,x,y,.557f);
    // 控制燈座與漆面保護帶共用既有深色／鋼件材質群；燈鏡由真電路狀態著色。
    Box(result,1,{.39f,1.01f,-.518f},{.29f,.045f,.014f});
    for(unsigned lamp=0;lamp<3;++lamp) {
        const auto centre=GeneratorIndicatorCentre(lamp);
        for(unsigned layer=0;layer<2;++layer) {
            const float radius=layer==0?.034f:.028f;
            std::vector<Aevrynd::Vec2> ring;ring.reserve(GeneratorIndicatorSides);
            for(unsigned side=0;side<GeneratorIndicatorSides;++side) {
                const float angle=6.28318530718f*float(side)/float(GeneratorIndicatorSides);
                ring.push_back({centre.x+radius*std::cos(angle),centre.y+radius*std::sin(angle)});
            }
            Prism(result,layer==0?2u:1u,ring,layer==0?-.559f:-.564f,layer==0?-.539f:-.553f);
        }
    }
    std::size_t triangles{};for(const auto& mesh:result.meshes)triangles+=mesh.indices.size()/3u;
    if(triangles>GeneratorDetailTriangleBudget)throw std::runtime_error("Generator detail triangle budget exceeded");
    return result;
}
}
