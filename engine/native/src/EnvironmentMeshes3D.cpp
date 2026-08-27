#include "Aevrynd/EnvironmentMeshes3D.h"
#include <algorithm>
#include <cmath>

namespace Aevrynd {
namespace {
constexpr float Pi=3.14159265358979323846f;

void AppendTransformed(Mesh3D& destination,const Mesh3D& source,Vec3 position,Vec3 scale,Color tint={1,1,1,1}){
    const uint32_t base=uint32_t(destination.vertices.size());
    destination.vertices.reserve(destination.vertices.size()+source.vertices.size());
    destination.indices.reserve(destination.indices.size()+source.indices.size());
    for(const Vertex3D& vertex:source.vertices){
        const Vec4 unpacked=UnpackNormalSnorm8(vertex.normal);
        const Vec3 normal=Normalize({unpacked.x/(std::max)(.001f,std::abs(scale.x)),unpacked.y/(std::max)(.001f,std::abs(scale.y)),unpacked.z/(std::max)(.001f,std::abs(scale.z))});
        const Color sourceColor=UnpackColorRgba8(vertex.color);destination.vertices.push_back({position.x+vertex.x*scale.x,position.y+vertex.y*scale.y,position.z+vertex.z*scale.z,PackNormalSnorm8(normal,unpacked.w),PackColorRgba8({sourceColor.r*tint.r,sourceColor.g*tint.g,sourceColor.b*tint.b,sourceColor.a*tint.a}),vertex.u,vertex.v});
    }
    for(uint32_t index:source.indices)destination.indices.push_back(base+index);
}
}

Mesh3D CreateGrassClumpMesh3D(unsigned bladeCount,unsigned segments){
    bladeCount=(std::max)(3u,bladeCount);segments=(std::clamp)(segments,2u,5u);Mesh3D mesh;
    mesh.vertices.reserve(std::size_t(bladeCount)*(segments+1)*2);mesh.indices.reserve(std::size_t(bladeCount)*segments*12);
    for(unsigned blade=0;blade<bladeCount;++blade){
        const float angle=2.f*Pi*float(blade)/float(bladeCount)+std::sin(float(blade)*12.9898f)*.34f;
        const Vec3 right{std::cos(angle),0,-std::sin(angle)},forward{std::sin(angle),0,std::cos(angle)};
        const float radial=.08f+.24f*float((blade*7u)%11u)/10.f,height=.68f+.32f*float((blade*5u)%13u)/12.f,width=.055f+.035f*float((blade*3u)%7u)/6.f;
        const uint32_t base=uint32_t(mesh.vertices.size());
        for(unsigned segment=0;segment<=segments;++segment){
            const float t=float(segment)/float(segments),taper=1.f-t*.88f,bend=t*t*(.1f+.12f*float(blade%3));
            const Vec3 center=forward*radial+forward*bend+Vec3{0,t*height,0};
            const Vec3 normal=Normalize(Cross(right,Vec3{0,height,0}+forward*bend));
            const uint32_t color=PackColorRgba8({.72f+.2f*t,.84f+.13f*t,.58f+.18f*t,1});
            for(float side:{-1.f,1.f}){const Vec3 point=center+right*(side*width*taper);mesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal,1.f-t),color});}
        }
        for(unsigned segment=0;segment<segments;++segment){const uint32_t a=base+segment*2,b=a+1,c=a+2,d=a+3;mesh.indices.insert(mesh.indices.end(),{a,c,b,b,c,d,b,c,a,b,d,c});}
    }
    return mesh;
}

Mesh3D CreateBroadleafCrownMesh3D(unsigned slices,unsigned stacks){
    slices=(std::max)(6u,slices);stacks=(std::max)(4u,stacks);const Mesh3D lobe=Mesh3D::CreateUnitSphere(slices,stacks);Mesh3D mesh;
    AppendTransformed(mesh,lobe,{0,.08f,0},{1.15f,.92f,1.04f});
    AppendTransformed(mesh,lobe,{-.48f,-.08f,.16f},{.82f,.7f,.78f});
    AppendTransformed(mesh,lobe,{.43f,-.04f,.2f},{.87f,.74f,.8f});
    AppendTransformed(mesh,lobe,{.08f,.2f,-.42f},{.88f,.76f,.75f});
    AppendTransformed(mesh,lobe,{-.18f,.32f,.34f},{.72f,.68f,.7f});
    return mesh;
}

Mesh3D CreateTreeLodMesh3D(unsigned sides){
    Mesh3D mesh;AppendTransformed(mesh,Mesh3D::CreateUnitCylinder((std::max)(3u,sides)),{0,.5f,0},{.12f,1.f,.12f},{.34f,.2f,.1f,1});AppendTransformed(mesh,CreateBroadleafCrownMesh3D(6,4),{0,1.13f,0},{.5f,.38f,.5f},{.12f,.42f,.09f,1});return mesh;
}

Mesh3D CreateSkyDomeMesh3D(unsigned slices,unsigned stacks){
    Mesh3D mesh=Mesh3D::CreateUnitSphere((std::max)(8u,slices),(std::max)(6u,stacks));for(Vertex3D& vertex:mesh.vertices){const Vec4 source=UnpackNormalSnorm8(vertex.normal);const float height=Clamp(vertex.y*2.f,0.f,1.f),haze=Clamp(1.f-std::abs(vertex.y)*4.f,0.f,1.f);const Color horizon{.46f,.67f,.86f,1},zenith{.065f,.19f,.39f,1};const Color color{horizon.r+(zenith.r-horizon.r)*height+.1f*haze,horizon.g+(zenith.g-horizon.g)*height+.055f*haze,horizon.b+(zenith.b-horizon.b)*height,1};vertex.normal=PackNormalSnorm8({-source.x,-source.y,-source.z},-1.f);vertex.color=PackColorRgba8(color);}for(std::size_t index=0;index<mesh.indices.size();index+=3)std::swap(mesh.indices[index+1],mesh.indices[index+2]);return mesh;
}

Mesh3D CreateCurvedRibbonMesh3D(std::span<const Vec3> centers,std::span<const float> halfWidths,float heightOffset){
    Mesh3D mesh;if(centers.size()<2||centers.size()!=halfWidths.size())return mesh;mesh.vertices.reserve(centers.size()*2);mesh.indices.reserve((centers.size()-1)*6);
    for(std::size_t index=0;index<centers.size();++index){
        const Vec3 previous=centers[index?index-1:index],next=centers[index+1<centers.size()?index+1:index];const Vec3 tangent=Normalize(Vec3{next.x-previous.x,0,next.z-previous.z});const Vec3 side=Normalize(Vec3{-tangent.z,0,tangent.x});
        for(float sign:{-1.f,1.f}){const Vec3 point=centers[index]+side*(halfWidths[index]*sign)+Vec3{0,heightOffset,0};mesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8({0,1,0}),PackColorRgba8({.86f,.94f,1,1})});}
    }
    for(uint32_t index=0;index+1<uint32_t(centers.size());++index){const uint32_t a=index*2,b=a+1,c=a+2,d=a+3;mesh.indices.insert(mesh.indices.end(),{a,b,c,b,d,c});}
    return mesh;
}
}
