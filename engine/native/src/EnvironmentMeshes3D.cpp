#include "Aevrynd/EnvironmentMeshes3D.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

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

float Variation(unsigned seed,unsigned index){uint32_t value=seed*0x9e3779b9u+index*0x85ebca6bu+0x632be59bu;value^=value>>16;value*=0x7feb352du;value^=value>>15;return float(value&0xffffu)/65535.f;}

void RebuildNormals(Mesh3D& mesh){std::vector<Vec3> sums(mesh.vertices.size());for(size_t offset=0;offset<mesh.indices.size();offset+=3){const uint32_t a=mesh.indices[offset],b=mesh.indices[offset+1],c=mesh.indices[offset+2];const auto point=[&](uint32_t index){const Vertex3D& vertex=mesh.vertices[index];return Vec3{vertex.x,vertex.y,vertex.z};};const Vec3 normal=Cross(point(b)-point(a),point(c)-point(a));sums[a]=sums[a]+normal;sums[b]=sums[b]+normal;sums[c]=sums[c]+normal;}for(size_t index=0;index<mesh.vertices.size();++index){const uint32_t semantic=mesh.vertices[index].normal&0xff000000u;mesh.vertices[index].normal=(PackNormalSnorm8(Normalize(sums[index]))&0x00ffffffu)|semantic;}}

Mesh3D CreateIrregularLeafLobe(unsigned slices,unsigned stacks,unsigned seed){slices=(std::max)(5u,slices);stacks=(std::max)(3u,stacks);Mesh3D mesh;const unsigned stride=slices+1;mesh.vertices.reserve(size_t(stride)*(stacks+1));mesh.indices.reserve(size_t(slices)*(stacks-1)*6);for(unsigned stack=0;stack<=stacks;++stack){const float t=float(stack)/stacks,y=(t-.46f)*(1.f-.12f*t),profile=std::pow((std::max)(0.f,std::sin(Pi*t)),.72f)*(1.08f-.22f*t);for(unsigned slice=0;slice<=slices;++slice){const float angle=2.f*Pi*float(slice)/slices,edge=1.f+.12f*std::sin(angle*3.f+Variation(seed,2)*Pi)+.07f*std::sin(angle*5.f+Variation(seed,7)*Pi);const float x=std::cos(angle)*profile*edge,z=std::sin(angle)*profile*(.76f+.1f*std::cos(angle*2.f+Variation(seed,5)*Pi));const Color color{.46f+.12f*t+.05f*Variation(seed,slice),.7f+.18f*t,.32f+.1f*t,1};mesh.vertices.push_back({x,y,z,PackNormalSnorm8({},1.f),PackColorRgba8(color),float(slice)/slices,t});}}for(unsigned stack=0;stack<stacks;++stack)for(unsigned slice=0;slice<slices;++slice){const uint32_t a=stack*stride+slice,b=a+stride,c=b+1,d=a+1;if(stack)mesh.indices.insert(mesh.indices.end(),{a,b,d});if(stack+1<stacks)mesh.indices.insert(mesh.indices.end(),{d,b,c});}RebuildNormals(mesh);return mesh;}

void AppendBranch(Mesh3D& mesh,Vec3 start,Vec3 end,float startRadius,float endRadius,unsigned sides,Color color){const Vec3 axis=Normalize(end-start);if(Length(axis)<.0001f)return;sides=(std::max)(3u,sides);const Vec3 reference=std::abs(axis.y)<.92f?Vec3{0,1,0}:Vec3{1,0,0},right=Normalize(Cross(reference,axis)),forward=Normalize(Cross(axis,right));const uint32_t base=uint32_t(mesh.vertices.size()),packedColor=PackColorRgba8(color);mesh.vertices.reserve(mesh.vertices.size()+size_t(sides)*2);mesh.indices.reserve(mesh.indices.size()+size_t(sides)*6);for(unsigned ring=0;ring<2;++ring){const Vec3 center=ring?end:start;const float radius=ring?endRadius:startRadius;for(unsigned side=0;side<sides;++side){const float angle=2.f*Pi*float(side)/sides;const Vec3 radial=right*std::cos(angle)+forward*std::sin(angle),normal=Normalize(radial+axis*((startRadius-endRadius)/(std::max)(.001f,Length(end-start))));const Vec3 point=center+radial*radius;mesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal),packedColor,float(side)/sides,float(ring)});}}for(unsigned side=0;side<sides;++side){const uint32_t next=(side+1)%sides,a=base+side,b=base+next,c=base+sides+side,d=base+sides+next;mesh.indices.insert(mesh.indices.end(),{a,b,c,b,d,c});}}
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
    slices=(std::max)(6u,slices);stacks=(std::max)(4u,stacks);Mesh3D mesh;const std::array<Vec3,7> positions={Vec3{0,.02f,0},Vec3{-.52f,-.12f,.12f},Vec3{.5f,-.08f,.18f},Vec3{.12f,.12f,-.48f},Vec3{-.2f,.28f,.36f},Vec3{.34f,.32f,-.14f},Vec3{-.38f,.18f,-.3f}};const std::array<Vec3,7> scales={Vec3{1.05f,.86f,.92f},Vec3{.72f,.62f,.66f},Vec3{.76f,.65f,.7f},Vec3{.78f,.7f,.62f},Vec3{.64f,.58f,.62f},Vec3{.62f,.56f,.58f},Vec3{.58f,.54f,.56f}};for(size_t index=0;index<positions.size();++index)AppendTransformed(mesh,CreateIrregularLeafLobe(slices,stacks,unsigned(index+1)),positions[index],scales[index],index%2?Color{.86f,1.f,.8f,1}:Color{.72f,.94f,.68f,1});
    return mesh;
}

Mesh3D CreateTreeLodMesh3D(unsigned sides){
    return CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Far,(std::max)(3u,sides));
}

Mesh3D CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D detail,unsigned variationSeed){const bool isNearDetail=detail==EnvironmentMeshDetail3D::Near;const unsigned branchSides=isNearDetail?8u:5u,leafSlices=isNearDetail?9u:5u,leafStacks=isNearDetail?5u:3u;Mesh3D mesh;const Color bark{.42f,.25f,.12f,1};const float leanX=(Variation(variationSeed,1)-.5f)*.12f,leanZ=(Variation(variationSeed,2)-.5f)*.1f;const Vec3 trunk0{0,0,0},trunk1{leanX*.25f,.72f,leanZ*.2f},trunk2{leanX*.62f,1.42f,leanZ*.55f},trunk3{leanX,2.05f,leanZ};AppendBranch(mesh,trunk0,trunk1,.19f,.155f,branchSides,bark);AppendBranch(mesh,trunk1,trunk2,.155f,.115f,branchSides,bark);AppendBranch(mesh,trunk2,trunk3,.115f,.065f,branchSides,bark);std::vector<Vec3> tips={trunk3};const std::array<Vec3,5> directions={Vec3{-.72f,.52f,.18f},Vec3{.68f,.48f,.24f},Vec3{-.28f,.5f,-.66f},Vec3{.38f,.58f,-.54f},Vec3{.1f,.68f,.48f}};const size_t branchCount=isNearDetail?directions.size():size_t{3};for(size_t index=0;index<branchCount;++index){const Vec3 origin=index<2?trunk2:trunk1+Vec3{0,.25f,0},direction=Normalize(directions[index]+Vec3{(Variation(variationSeed,unsigned(index+10))-.5f)*.18f,0,(Variation(variationSeed,unsigned(index+20))-.5f)*.18f}),tip=origin+direction*(isNearDetail?.9f:.72f);AppendBranch(mesh,origin,tip,isNearDetail?.075f:.085f,.025f,isNearDetail?6u:4u,bark);tips.push_back(tip);if(isNearDetail&&index<3){const Vec3 twigTip=tip+Normalize(Vec3{direction.x*.55f,.75f,direction.z*.55f})*.48f;AppendBranch(mesh,tip-direction*.22f,twigTip,.032f,.012f,4,bark);tips.push_back(twigTip);}}const Mesh3D leaf=CreateIrregularLeafLobe(leafSlices,leafStacks,variationSeed);for(size_t index=0;index<tips.size();++index){const float scale=.48f+.1f*Variation(variationSeed,unsigned(index+40));AppendTransformed(mesh,leaf,tips[index]+Vec3{0,.12f,0},{scale*(.92f+.12f*Variation(variationSeed,unsigned(index+50))),scale,scale*(.78f+.14f*Variation(variationSeed,unsigned(index+60)))},index%2?Color{.78f,.96f,.7f,1}:Color{.66f,.88f,.58f,1});}return mesh;}

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
