#pragma once
#include <Aevrynd/BeveledBox3D.h>
#include <Aevrynd/GeometryNormals3D.h>
#include <Aevrynd/Mesh3D.h>
#include <stdexcept>

namespace Mistwatch {
struct SupplyCrateGeometry { Aevrynd::Mesh3D wood, metal; };
namespace CrateStructureDetail {
inline void BeveledBoard(Aevrynd::Mesh3D& mesh,Aevrynd::Vec3 center,Aevrynd::Vec3 size,
    Aevrynd::Color tint,Aevrynd::Vec3 axis0={1,0,0},Aevrynd::Vec3 axis1={0,1,0},
    Aevrynd::Vec3 axis2={0,0,1}) {
    if(!Aevrynd::AppendBeveledBox3D(mesh,center,size,.006f,tint,axis0,axis1,axis2))
        throw std::runtime_error("Crate bevel must have valid geometry and append transaction");
}
}
inline SupplyCrateGeometry CreateSupplyCrate() {
    using namespace Aevrynd;
    SupplyCrateGeometry result;
    const auto box=Mesh3D::CreateUnitBox();
    const auto cylinder=Mesh3D::CreateUnitCylinder(8);
    Mesh3D rivet;
    for(std::size_t i=0;i<cylinder.indices.size();i+=3) {
        const auto& a=cylinder.vertices[cylinder.indices[i]];
        const auto& b=cylinder.vertices[cylinder.indices[i+2]];
        const auto& c=cylinder.vertices[cylinder.indices[i+1]];
        const auto normal=PackNormalSnorm8(Normalize(Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z})));
        for(auto vertex:{a,b,c}) {
            vertex.normal=normal;
            rivet.indices.push_back(static_cast<std::uint32_t>(rivet.vertices.size()));
            rivet.vertices.push_back(vertex);
        }
    }
    const auto append=[](Mesh3D& destination,const Mesh3D& source,Vec3 center,Vec3 size,Color tint,Vec3 rotation=Vec3{}) {
        const auto base=static_cast<std::uint32_t>(destination.vertices.size());
        const auto transform=CreateTransformMatrix(center,rotation,size);
        const auto normals=CreateNormalMatrix(transform);
        for(auto vertex:source.vertices) {
            const auto p=TransformPoint(transform,{vertex.x,vertex.y,vertex.z});
            const auto n=UnpackNormalSnorm8(vertex.normal);
            vertex.x=p.x;vertex.y=p.y;vertex.z=p.z;
            vertex.normal=PackNormalSnorm8(TransformNormal(normals,{n.x,n.y,n.z}));
            vertex.color=PackColorRgba8(tint);
            destination.vertices.push_back(vertex);
        }
        for(const auto index:source.indices)destination.indices.push_back(base+index);
    };
    for(unsigned row=0;row<4;++row) {
        const float y=.13f+.18f*float(row);
        const float shade=.8f+.045f*float(row%3);
        const Color tint{shade,shade*.96f,shade*.89f,1};
        for(const float side:{-1.f,1.f}) {
            append(result.wood,box,{0,y,side*.346f},{.78f,.171f,.056f},tint);
            append(result.wood,box,{side*.346f,y,0},{.056f,.171f,.676f},tint);
        }
    }
    for(unsigned board=0;board<5;++board) {
        const float x=(float(board)-2.f)*.156f;
        const Color tint{.9f-.025f*float(board%3),.86f-.025f*float(board%3),.78f,1};
        append(result.wood,box,{x,.759f,0},{.15f,.07f,.78f},tint);
        append(result.wood,box,{x,.041f,0},{.15f,.07f,.78f},tint);
    }
    const Color braceTint{.76f,.72f,.65f,1};
    for(const float x:{-.327f,.327f})for(const float z:{-.327f,.327f})
        CrateStructureDetail::BeveledBoard(result.wood,{x,.4f,z},{.076f,.66f,.076f},braceTint);
    const Vec3 frontAxis=Normalize(Vec3{.654f,.5f,0});
    const Vec3 sideAxis=Normalize(Vec3{0,.5f,.654f});
    const float braceLength=Length(Vec3{.654f,.5f,0});
    for(const float side:{-1.f,1.f}) {
        CrateStructureDetail::BeveledBoard(result.wood,{0,.4f,side*.370f},{braceLength,.060f,.016f},
            braceTint,frontAxis,{-frontAxis.y,frontAxis.x,0},{0,0,1});
        CrateStructureDetail::BeveledBoard(result.wood,{side*.370f,.4f,0},{braceLength,.060f,.016f},
            braceTint,sideAxis,{0,-sideAxis.z,sideAxis.y},{1,0,0});
    }
    const Color iron{.16f,.18f,.17f,1};
    for(const float x:{-.23f,.23f}) {
        for(const float z:{-.393f,.393f})
            append(result.metal,box,{x,.4f,z},{.045f,.78f,.008f},iron);
        for(const float y:{.004f,.796f})
            append(result.metal,box,{x,y,0},{.045f,.008f,.78f},iron);
        for(const float y:{.22f,.58f})
            append(result.metal,rivet,{x,y,-.397f},{.018f,.006f,.018f},{.3f,.31f,.28f,1},{1.5707963f,0,0});
    }
    append(result.metal,box,{0,.59f,-.397f},{.13f,.085f,.006f},{.62f,.52f,.28f,1});
    return result;
}
}
