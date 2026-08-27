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

void AppendOriented(Mesh3D& destination,const Mesh3D& source,Vec3 position,Vec3 rotation,Vec3 scale,Color tint){
    const uint32_t base=uint32_t(destination.vertices.size());const Matrix4 matrix=Transform3D{position,rotation,scale}.Matrix(),normalMatrix=CreateNormalMatrix(matrix);destination.vertices.reserve(destination.vertices.size()+source.vertices.size());destination.indices.reserve(destination.indices.size()+source.indices.size());for(const Vertex3D& vertex:source.vertices){const Vec3 point=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});const Vec4 unpacked=UnpackNormalSnorm8(vertex.normal);const Vec3 normal=TransformNormal(normalMatrix,{unpacked.x,unpacked.y,unpacked.z});const Color sourceColor=UnpackColorRgba8(vertex.color);destination.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal,unpacked.w),PackColorRgba8({sourceColor.r*tint.r,sourceColor.g*tint.g,sourceColor.b*tint.b,sourceColor.a*tint.a}),vertex.u,vertex.v});}for(uint32_t index:source.indices)destination.indices.push_back(base+index);
}

float Variation(unsigned seed,unsigned index){uint32_t value=seed*0x9e3779b9u+index*0x85ebca6bu+0x632be59bu;value^=value>>16;value*=0x7feb352du;value^=value>>15;return float(value&0xffffu)/65535.f;}

void RebuildNormals(Mesh3D& mesh){std::vector<Vec3> sums(mesh.vertices.size());for(size_t offset=0;offset<mesh.indices.size();offset+=3){const uint32_t a=mesh.indices[offset],b=mesh.indices[offset+1],c=mesh.indices[offset+2];const auto point=[&](uint32_t index){const Vertex3D& vertex=mesh.vertices[index];return Vec3{vertex.x,vertex.y,vertex.z};};const Vec3 normal=Cross(point(b)-point(a),point(c)-point(a));sums[a]=sums[a]+normal;sums[b]=sums[b]+normal;sums[c]=sums[c]+normal;}for(size_t index=0;index<mesh.vertices.size();++index){const uint32_t semantic=mesh.vertices[index].normal&0xff000000u;mesh.vertices[index].normal=(PackNormalSnorm8(Normalize(sums[index]))&0x00ffffffu)|semantic;}}

Mesh3D CreateIrregularLeafLobe(unsigned slices,unsigned stacks,unsigned seed){slices=(std::max)(5u,slices);stacks=(std::max)(3u,stacks);Mesh3D mesh;const unsigned stride=slices+1;mesh.vertices.reserve(size_t(stride)*(stacks+1));mesh.indices.reserve(size_t(slices)*(stacks-1)*6);for(unsigned stack=0;stack<=stacks;++stack){const float t=float(stack)/stacks,y=(t-.46f)*(1.f-.12f*t),profile=std::pow((std::max)(0.f,std::sin(Pi*t)),.72f)*(1.08f-.22f*t);for(unsigned slice=0;slice<=slices;++slice){const float angle=2.f*Pi*float(slice)/slices,edge=1.f+.12f*std::sin(angle*3.f+Variation(seed,2)*Pi)+.07f*std::sin(angle*5.f+Variation(seed,7)*Pi);const float x=std::cos(angle)*profile*edge,z=std::sin(angle)*profile*(.76f+.1f*std::cos(angle*2.f+Variation(seed,5)*Pi));const Color color{.46f+.12f*t+.05f*Variation(seed,slice),.7f+.18f*t,.32f+.1f*t,1};mesh.vertices.push_back({x,y,z,PackNormalSnorm8({},1.f),PackColorRgba8(color),float(slice)/slices,t});}}for(unsigned stack=0;stack<stacks;++stack)for(unsigned slice=0;slice<slices;++slice){const uint32_t a=stack*stride+slice,b=a+stride,c=b+1,d=a+1;if(stack)mesh.indices.insert(mesh.indices.end(),{a,b,d});if(stack+1<stacks)mesh.indices.insert(mesh.indices.end(),{d,b,c});}RebuildNormals(mesh);return mesh;}

void AppendBranch(Mesh3D& mesh,Vec3 start,Vec3 end,float startRadius,float endRadius,unsigned sides,Color color){const Vec3 axis=Normalize(end-start);if(Length(axis)<.0001f)return;sides=(std::max)(3u,sides);const Vec3 reference=std::abs(axis.y)<.92f?Vec3{0,1,0}:Vec3{1,0,0},right=Normalize(Cross(reference,axis)),forward=Normalize(Cross(axis,right));const uint32_t base=uint32_t(mesh.vertices.size()),packedColor=PackColorRgba8(color);mesh.vertices.reserve(mesh.vertices.size()+size_t(sides)*2);mesh.indices.reserve(mesh.indices.size()+size_t(sides)*6);for(unsigned ring=0;ring<2;++ring){const Vec3 center=ring?end:start;const float radius=ring?endRadius:startRadius;for(unsigned side=0;side<sides;++side){const float angle=2.f*Pi*float(side)/sides;const Vec3 radial=right*std::cos(angle)+forward*std::sin(angle),normal=Normalize(radial+axis*((startRadius-endRadius)/(std::max)(.001f,Length(end-start))));const Vec3 point=center+radial*radius;mesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal),packedColor,float(side)/sides,float(ring)});}}for(unsigned side=0;side<sides;++side){const uint32_t next=(side+1)%sides,a=base+side,b=base+next,c=base+sides+side,d=base+sides+next;mesh.indices.insert(mesh.indices.end(),{a,b,c,b,d,c});}}

void AppendLeafCard(Mesh3D& mesh,Vec3 center,Vec3 right,Vec3 up,float halfWidth,float halfHeight,Color color){
    right=Normalize(right)*halfWidth;up=Normalize(up)*halfHeight;const Vec3 normal=Normalize(Cross(right,up));
    if(Length(normal)<.5f)return;
    const std::array<Vec3,6> points={center-up,center+right*.64f-up*.28f,center+right*.92f+up*.08f,center+up,center-right*.7f+up*.24f,center-right*.78f-up*.18f};const uint32_t base=uint32_t(mesh.vertices.size()),packedColor=PackColorRgba8(color);
    mesh.vertices.reserve(mesh.vertices.size()+12);mesh.indices.reserve(mesh.indices.size()+24);
    for(Vec3 point:points)mesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal,1.f),packedColor});
    for(Vec3 point:points)mesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal*-1.f,1.f),packedColor});
    mesh.indices.insert(mesh.indices.end(),{base,base+1,base+2,base,base+2,base+3,base,base+3,base+4,base,base+4,base+5,base+6,base+8,base+7,base+6,base+9,base+8,base+6,base+10,base+9,base+6,base+11,base+10});
}

void AppendLeafSpray(Mesh3D& mesh,Vec3 center,unsigned seed,unsigned leafCount){
    for(unsigned leaf=0;leaf<leafCount;++leaf){const float yaw=2.f*Pi*Variation(seed,leaf*7+1),tilt=(Variation(seed,leaf*7+2)-.5f)*1.15f,radial=.1f+.27f*Variation(seed,leaf*7+3);const Vec3 outward{std::cos(yaw)*std::cos(tilt),std::sin(tilt),std::sin(yaw)*std::cos(tilt)},leafCenter=center+Vec3{outward.x*radial,outward.y*radial*.72f,outward.z*radial};const Vec3 right=Normalize(Vec3{-std::sin(yaw),.18f*(Variation(seed,leaf*7+4)-.5f),std::cos(yaw)}),up=Normalize(Cross(outward,right));const float width=.07f+.042f*Variation(seed,leaf*7+5),height=.13f+.062f*Variation(seed,leaf*7+6);const float shade=Variation(seed,leaf*7+7);AppendLeafCard(mesh,leafCenter,right,up,width,height,{.2f+.1f*shade,.49f+.18f*shade,.1f+.07f*shade,1});}
}
}

Mesh3D CreateGrassClumpMesh3D(unsigned bladeCount,unsigned segments){
    bladeCount=(std::max)(3u,bladeCount);segments=(std::clamp)(segments,2u,5u);Mesh3D mesh;
    mesh.vertices.reserve(std::size_t(bladeCount)*(segments+1)*2);mesh.indices.reserve(std::size_t(bladeCount)*segments*12);
    for(unsigned blade=0;blade<bladeCount;++blade){
        const float angle=2.f*Pi*float(blade)/float(bladeCount)+std::sin(float(blade)*12.9898f)*.34f;
        const Vec3 right{std::cos(angle),0,-std::sin(angle)},forward{std::sin(angle),0,std::cos(angle)};
        const float radial=.06f+.27f*Variation(73,blade*5+1),height=.61f+.43f*Variation(73,blade*5+2),width=.035f+.065f*Variation(73,blade*5+3),sideBias=(Variation(73,blade*5+4)-.5f)*.18f,bendStrength=.1f+.2f*Variation(73,blade*5+5);
        const uint32_t base=uint32_t(mesh.vertices.size());
        for(unsigned segment=0;segment<=segments;++segment){
            const float t=float(segment)/float(segments),taper=std::pow(1.f-t,.72f)*.94f+.035f,bend=t*t*bendStrength,lateral=std::sin(t*Pi)*sideBias;
            const Vec3 center=forward*(radial+bend)+right*lateral+Vec3{0,t*height,0},tangent=Normalize(Vec3{0,height,0}+forward*(2.f*bendStrength*t)+right*(sideBias*Pi*std::cos(t*Pi)));
            const Vec3 normal=Normalize(Cross(right,tangent));const float bladeShade=.82f+.2f*Variation(91,blade+3),sunlit=.1f*t;
            const uint32_t color=PackColorRgba8({(.48f+sunlit)*bladeShade,(.72f+.16f*t)*bladeShade,(.28f+.12f*t)*bladeShade,1});
            const float leftWidth=width*taper*(1.f+.12f*Variation(37,blade+1)),rightWidth=width*taper*(.88f+.16f*Variation(43,blade+2));for(float offset:{-leftWidth,rightWidth}){const Vec3 point=center+right*offset;mesh.vertices.push_back({point.x,point.y,point.z,PackNormalSnorm8(normal,1.f-t),color});}
        }
        for(unsigned segment=0;segment<segments;++segment){const uint32_t a=base+segment*2,b=a+1,c=a+2,d=a+3;mesh.indices.insert(mesh.indices.end(),{a,c,b,b,c,d,b,c,a,b,d,c});}
    }
    return mesh;
}

Mesh3D CreateBroadleafCrownMesh3D(unsigned slices,unsigned stacks){
    slices=(std::max)(6u,slices);stacks=(std::max)(4u,stacks);Mesh3D mesh;const std::array<Vec3,7> positions={Vec3{0,.02f,0},Vec3{-.52f,-.15f,.12f},Vec3{.5f,-.06f,.18f},Vec3{.12f,.14f,-.48f},Vec3{-.2f,.34f,.36f},Vec3{.34f,.27f,-.14f},Vec3{-.38f,.2f,-.3f}};const std::array<Vec3,7> scales={Vec3{1.05f,.9f,.92f},Vec3{.72f,.66f,.66f},Vec3{.76f,.7f,.7f},Vec3{.78f,.74f,.62f},Vec3{.64f,.62f,.62f},Vec3{.62f,.61f,.58f},Vec3{.58f,.58f,.56f}};for(size_t index=0;index<positions.size();++index){const Vec3 rotation{(Variation(113,unsigned(index)*3)-.5f)*.48f,(Variation(113,unsigned(index)*3+1)-.5f)*1.1f,(Variation(113,unsigned(index)*3+2)-.5f)*.42f};AppendOriented(mesh,CreateIrregularLeafLobe(slices,stacks,unsigned(index+1)),positions[index],rotation,scales[index],index%2?Color{.72f,.9f,.62f,1}:Color{.58f,.8f,.49f,1});}
    return mesh;
}

Mesh3D CreateTreeLodMesh3D(unsigned sides){
    return CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D::Far,(std::max)(3u,sides));
}

Mesh3D CreateBroadleafTreeMesh3D(EnvironmentMeshDetail3D detail,unsigned variationSeed){const bool isNearDetail=detail==EnvironmentMeshDetail3D::Near;const unsigned branchSides=isNearDetail?8u:5u;Mesh3D mesh;const Color bark{.42f,.25f,.12f,1};const float leanX=(Variation(variationSeed,1)-.5f)*.12f,leanZ=(Variation(variationSeed,2)-.5f)*.1f;const Vec3 trunk0{0,0,0},trunk1{leanX*.25f,.72f,leanZ*.2f},trunk2{leanX*.62f,1.42f,leanZ*.55f},trunk3{leanX,2.05f,leanZ};AppendBranch(mesh,trunk0,trunk1,.19f,.155f,branchSides,bark);AppendBranch(mesh,trunk1,trunk2,.155f,.115f,branchSides,bark);AppendBranch(mesh,trunk2,trunk3,.115f,.065f,branchSides,bark);std::vector<Vec3> tips={trunk3};const std::array<Vec3,7> directions={Vec3{-.72f,.52f,.18f},Vec3{.68f,.48f,.24f},Vec3{-.28f,.5f,-.66f},Vec3{.38f,.58f,-.54f},Vec3{.1f,.68f,.48f},Vec3{-.56f,.62f,-.32f},Vec3{.54f,.64f,.04f}};const size_t branchCount=isNearDetail?directions.size():size_t{3};for(size_t index=0;index<branchCount;++index){const Vec3 origin=index<2?trunk2:trunk1+Vec3{0,.25f,0},direction=Normalize(directions[index]+Vec3{(Variation(variationSeed,unsigned(index+10))-.5f)*.18f,0,(Variation(variationSeed,unsigned(index+20))-.5f)*.18f}),tip=origin+direction*(isNearDetail?.9f:.72f);AppendBranch(mesh,origin,tip,isNearDetail?.075f:.085f,.025f,isNearDetail?6u:4u,bark);tips.push_back(tip);if(isNearDetail&&index<5){const Vec3 twigTip=tip+Normalize(Vec3{direction.x*.55f,.75f,direction.z*.55f})*.48f;AppendBranch(mesh,tip-direction*.22f,twigTip,.032f,.012f,4,bark);tips.push_back(twigTip);}}
    if(isNearDetail){for(size_t index=0;index<tips.size();++index)AppendLeafSpray(mesh,tips[index]+Vec3{0,.1f,0},variationSeed+unsigned(index)*101u,7u);}
    else {const Mesh3D leaf=CreateIrregularLeafLobe(5,3,variationSeed);for(size_t index=0;index<tips.size();++index){const float scale=.46f+.11f*Variation(variationSeed,unsigned(index+40)),yaw=2.f*Pi*Variation(variationSeed,unsigned(index+70)),tilt=(Variation(variationSeed,unsigned(index+80))-.5f)*.58f;const Vec3 primaryScale{scale*(.88f+.16f*Variation(variationSeed,unsigned(index+50))),scale*(1.f+.16f*Variation(variationSeed,unsigned(index+55))),scale*(.7f+.18f*Variation(variationSeed,unsigned(index+60)))};const Color primary=index%2?Color{.65f,.84f,.54f,1}:Color{.52f,.74f,.43f,1};AppendOriented(mesh,leaf,tips[index]+Vec3{0,.11f+.08f*Variation(variationSeed,unsigned(index+90)),0},{tilt,yaw,-tilt*.45f},primaryScale,primary);
        // 第二個交錯葉團沿另一方向傾斜，以很低面數補出高度層次與不規則遠景剪影。
        const float secondaryScale=scale*.68f;const Vec3 offset{std::cos(yaw)*scale*.34f,.19f+.08f*Variation(variationSeed,unsigned(index+100)),std::sin(yaw)*scale*.34f};AppendOriented(mesh,leaf,tips[index]+offset,{-tilt*.7f,yaw+Pi*.63f,tilt*.5f},{secondaryScale*.9f,secondaryScale*1.08f,secondaryScale*.72f},index%2?Color{.48f,.7f,.39f,1}:Color{.61f,.79f,.48f,1});}}
    return mesh;}

Mesh3D CreateSkyDomeMesh3D(unsigned slices,unsigned stacks){
    Mesh3D mesh=Mesh3D::CreateUnitSphere((std::max)(8u,slices),(std::max)(6u,stacks));for(Vertex3D& vertex:mesh.vertices){const Vec4 source=UnpackNormalSnorm8(vertex.normal);const float height=Clamp(vertex.y*2.f,0.f,1.f),haze=Clamp(1.f-std::abs(vertex.y)*4.f,0.f,1.f);const Color horizon{.46f,.67f,.86f,1},zenith{.065f,.19f,.39f,1};const Color color{horizon.r+(zenith.r-horizon.r)*height+.1f*haze,horizon.g+(zenith.g-horizon.g)*height+.055f*haze,horizon.b+(zenith.b-horizon.b)*height,1};vertex.normal=PackNormalSnorm8({-source.x,-source.y,-source.z},-1.f);vertex.color=PackColorRgba8(color);}for(std::size_t index=0;index<mesh.indices.size();index+=3)std::swap(mesh.indices[index+1],mesh.indices[index+2]);return mesh;
}

Mesh3D CreateMountainMesh3D(unsigned radialSegments,unsigned rings,unsigned variationSeed){
    radialSegments=(std::clamp)(radialSegments,6u,64u);rings=(std::clamp)(rings,2u,16u);Mesh3D mesh;mesh.vertices.reserve(1+std::size_t(radialSegments)*rings);mesh.indices.reserve(std::size_t(radialSegments)*(1+(rings-1)*2)*3);const float ridgeAngle=2.f*Pi*Variation(variationSeed,2),peakX=std::sin(ridgeAngle)*.09f,peakZ=std::cos(ridgeAngle)*.09f;mesh.vertices.push_back({peakX,1.04f,peakZ,PackNormalSnorm8({0,1,0}),PackColorRgba8({.72f,.74f,.72f,1}),.5f,0});
    for(unsigned ring=1;ring<=rings;++ring){const float t=float(ring)/rings,baseHeight=std::pow(1.f-t,1.38f),centerWeight=1.f-t,centerX=peakX*centerWeight,centerZ=peakZ*centerWeight;for(unsigned segment=0;segment<radialSegments;++segment){const float angle=2.f*Pi*float(segment)/radialSegments,largeRidge=.15f*std::sin(angle*3.f+Variation(variationSeed,4)*Pi),smallRidge=.065f*std::sin(angle*7.f+Variation(variationSeed,9)*Pi),secondaryAlignment=(std::max)(0.f,std::cos(angle-ridgeAngle-Pi*.72f)),secondaryPeak=.17f*secondaryAlignment*secondaryAlignment*std::sin(Pi*t)*(1.f-.35f*t),erosion=.022f*std::sin(t*Pi*float(3u+(variationSeed%3u))+angle*2.f);const float radius=t*(.48f+largeRidge+smallRidge*t),height=(std::max)(0.f,baseHeight+secondaryPeak+erosion+(Variation(variationSeed,ring*radialSegments+segment)-.5f)*.1f*t*(1.f-t));const float stratum=.035f*float((ring+segment/3u)&1u),sun=.035f*std::cos(angle-ridgeAngle);const Color rock{.43f+.19f*height+sun+stratum,.45f+.17f*height+sun*.7f+stratum,.44f+.15f*height+sun*.45f+stratum*.75f,1};mesh.vertices.push_back({centerX+std::sin(angle)*radius,height,centerZ+std::cos(angle)*radius,PackNormalSnorm8({0,1,0}),PackColorRgba8(rock),float(segment)/radialSegments,t});}}
    for(unsigned segment=0;segment<radialSegments;++segment){const std::uint32_t current=1+segment,next=1+(segment+1)%radialSegments;mesh.indices.insert(mesh.indices.end(),{0,next,current});}
    for(unsigned ring=1;ring<rings;++ring){const std::uint32_t inner=1+(ring-1)*radialSegments,outer=inner+radialSegments;for(unsigned segment=0;segment<radialSegments;++segment){const std::uint32_t next=(segment+1)%radialSegments,a=inner+segment,b=inner+next,c=outer+segment,d=outer+next;mesh.indices.insert(mesh.indices.end(),{a,b,c,b,d,c});}}
    RebuildNormals(mesh);return mesh;
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
