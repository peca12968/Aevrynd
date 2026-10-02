#pragma once
#include "MistwatchWorld.h"
#include <Aevrynd/Mesh3D.h>
namespace Mistwatch {
inline Aevrynd::Mesh3D CreateLegacyRangerHut() {
    using namespace Aevrynd;
    Mesh3D result;
    const auto box=Mesh3D::CreateUnitBox();
    const auto solids=RangerHutSolids();
    for(std::size_t i=0;i<solids.size();++i) {
        if(i==2)continue;
        const auto transform=CreateTransformMatrix(solids[i].center-Session::rangerLog,{},solids[i].extents*2.f);
        const auto normal=CreateNormalMatrix(transform);
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        for(auto v:box.vertices) {
            const auto p=TransformPoint(transform,{v.x,v.y,v.z});
            const auto n=UnpackNormalSnorm8(v.normal);
            v.x=p.x;v.y=p.y;v.z=p.z;v.normal=PackNormalSnorm8(TransformNormal(normal,{n.x,n.y,n.z}));
            result.vertices.push_back(v);
        }
        for(auto index:box.indices)result.indices.push_back(base+index);
    }
    return result;
}
// 改善外觀仍從原有碰撞體取牆尺寸，桌子另依同一碰撞體繪製。
// 西側入口與四個窗孔不新增板材；新坡屋面只位於舊屋頂上方。
inline Aevrynd::Mesh3D CreateRangerHut() {
    using namespace Aevrynd;
    Mesh3D result;
    const auto box=Mesh3D::CreateUnitBox();
    const auto append=[&](Vec3 center,Vec3 size,Color tint,Vec3 rotation=Vec3{}) {
        const auto matrix=CreateTransformMatrix(center,rotation,size);
        const auto normals=CreateNormalMatrix(matrix);
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        for(auto v:box.vertices) {
            const auto p=TransformPoint(matrix,{v.x,v.y,v.z});
            const auto n=UnpackNormalSnorm8(v.normal);
            v.x=p.x;v.y=p.y;v.z=p.z;
            v.normal=PackNormalSnorm8(TransformNormal(normals,{n.x,n.y,n.z}));
            v.color=PackColorRgba8(tint);result.vertices.push_back(v);
        }
        for(auto index:box.indices)result.indices.push_back(base+index);
    };
    const auto solids=RangerHutSolids();
    append(solids[0].center-Session::rangerLog,solids[0].extents*2.f,{.62f,.59f,.52f,1});
    for(std::size_t i=1;i<solids.size();++i) {
        if(i==2)continue;
        const auto center=solids[i].center-Session::rangerLog;
        const auto size=solids[i].extents*2.f;
        if(size.x<.4f&&size.z<.4f) {
            append(center,size,{.56f,.5f,.42f,1});continue;
        }
        const unsigned rows=static_cast<unsigned>(std::round(size.y/.2f));
        const float height=size.y/float(rows);
        // 薄內襯封閉板縫，但外層板材仍有真正凹槽；不把牆變成可穿越的孔。
        auto back=center,backSize=size;
        if(size.x<size.z){back.x-=.078f;backSize.x=.036f;}
        else {back.z-=std::copysign(.078f,back.z);backSize.z=.036f;}
        append(back,backSize,{.38f,.35f,.3f,1});
        for(unsigned row=0;row<rows;++row) {
            auto p=center,s=size;
            p.y=center.y-size.y*.5f+height*(float(row)+.5f);s.y=height-.006f;
            const float shade=.73f+.035f*float((row+unsigned(i))%4);
            append(p,s,{shade,shade*.92f,shade*.8f,1});
        }
    }
    // 雙坡屋面保持原屋頂平面腳印，新屋面最低點高於 2.85 公尺。
    const float angle=std::atan(.85f/2.48f),length=std::hypot(2.48f,.85f);
    for(const float side:{-1.f,1.f})
        append({side*(1.24f-std::sin(angle)*.04f),3.335f-std::cos(angle)*.04f,0},
            {length,.012f,5.988f},{.29f,.27f,.23f,1},{0,0,-side*angle});
    for(const float side:{-1.f,1.f})for(unsigned strip=0;strip<12;++strip) {
        const float z=-2.75f+.5f*float(strip);
        const float shade=.39f+.018f*float(strip%3);
        append({side*1.24f,3.335f,z},{length,.1f,.494f},{shade,shade*.96f,shade*.89f,1},{0,0,-side*angle});
    }
    append({0,3.78f,0},{.16f,.12f,5.994f},{.29f,.28f,.25f,1});
    const auto triangle=[&](Vec3 a,Vec3 b,Vec3 c) {
        const auto base=static_cast<std::uint32_t>(result.vertices.size());
        const auto normal=PackNormalSnorm8(Normalize(Cross(b-a,c-a)));
        for(const auto p:{a,b,c})result.vertices.push_back({p.x,p.y,p.z,normal,PackColorRgba8({.59f,.54f,.46f,1}),p.x*.2f,p.y*.2f});
        result.indices.insert(result.indices.end(),{base,base+1,base+2});
    };
    for(const float side:{-1.f,1.f}) {
        // 山牆與屋面使用同一坡度；底帶略微交疊，避免檐口留下亮縫。
        append({0,2.88f,side*2.945f},{4.96f,.065f,.07f},{.59f,.54f,.46f,1});
        const Vec3 a{-2.48f,2.91f,side*2.98f},b{2.48f,2.91f,side*2.98f},c{0,3.76f,side*2.98f};
        const Vec3 d{-2.48f,2.91f,side*2.91f},e{2.48f,2.91f,side*2.91f},f{0,3.76f,side*2.91f};
        if(side>0){triangle(a,b,c);triangle(d,f,e);}
        else {triangle(a,c,b);triangle(d,e,f);}
        for(const auto face:std::array<std::array<Vec3,4>,3>{{{a,d,e,b},{a,c,f,d},{b,e,f,c}}}) {
            if(side>0){triangle(face[0],face[1],face[2]);triangle(face[0],face[2],face[3]);}
            else {triangle(face[0],face[2],face[1]);triangle(face[0],face[3],face[2]);}
        }
    }
    return result;
}
inline Aevrynd::BoxCollider3D RangerHutCameraRoofSolid() {
    // 只供鏡頭使用的保守屋面包圍盒；角色碰撞、屋內淨高及存檔不變。
    return {Session::rangerLog+Aevrynd::Vec3{0,3.35f,0},{2.5f,.5f,3.f}};
}
}
