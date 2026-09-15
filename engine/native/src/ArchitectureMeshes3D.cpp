#include "Aevrynd/ArchitectureMeshes3D.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace Aevrynd {
namespace {

float Variation(std::uint32_t seed,std::uint32_t index){std::uint32_t value=seed*0x9e3779b9u+index*0x85ebca6bu+0x632be59bu;value^=value>>16;value*=0x7feb352du;value^=value>>15;return float(value&0xffffu)/65535.f;}

void AppendMesh(Mesh3D& destination,const Mesh3D& source,const Transform3D& transform,Color tint){
    const std::uint32_t base=std::uint32_t(destination.vertices.size());
    const Matrix4 matrix=transform.Matrix(),normalMatrix=CreateNormalMatrix(matrix);
    destination.vertices.reserve(destination.vertices.size()+source.vertices.size());
    destination.indices.reserve(destination.indices.size()+source.indices.size());
    for(const Vertex3D& vertex:source.vertices){const Vec3 position=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});const Vec4 packedNormal=UnpackNormalSnorm8(vertex.normal);const Vec3 normal=TransformNormal(normalMatrix,{packedNormal.x,packedNormal.y,packedNormal.z});const Color sourceColor=UnpackColorRgba8(vertex.color);destination.vertices.push_back({position.x,position.y,position.z,PackNormalSnorm8(normal,packedNormal.w),PackColorRgba8({sourceColor.r*tint.r,sourceColor.g*tint.g,sourceColor.b*tint.b,sourceColor.a*tint.a}),vertex.u,vertex.v});}
    for(std::uint32_t index:source.indices)destination.indices.push_back(base+index);
}

void AppendBox(Mesh3D& mesh,Vec3 center,Vec3 size,Color color){AppendMesh(mesh,Mesh3D::CreateUnitBox(),{center,{},size},color);}

void AppendFace(Mesh3D& mesh,std::array<Vec3,4> points,Color color){
    const std::uint32_t base=std::uint32_t(mesh.vertices.size());
    const Vec3 normal=Normalize(Cross(points[2]-points[0],points[1]-points[0]));
    const std::uint32_t packedNormal=PackNormalSnorm8(normal),packedColor=PackColorRgba8(color);
    mesh.vertices.push_back({points[0].x,points[0].y,points[0].z,packedNormal,packedColor,0,1});
    mesh.vertices.push_back({points[1].x,points[1].y,points[1].z,packedNormal,packedColor,1,1});
    mesh.vertices.push_back({points[2].x,points[2].y,points[2].z,packedNormal,packedColor,1,0});
    mesh.vertices.push_back({points[3].x,points[3].y,points[3].z,packedNormal,packedColor,0,0});
    mesh.indices.insert(mesh.indices.end(),{base,base+2,base+1,base,base+3,base+2});
}

void AppendGableRoof(Mesh3D& mesh,float width,float depth,float wallHeight,float roofHeight,float overhang,Color color){
    const float halfWidth=width*.5f+overhang,halfDepth=depth*.5f+overhang,ridge=wallHeight+roofHeight,eave=wallHeight;
    AppendFace(mesh,{Vec3{-halfWidth,eave,-halfDepth},Vec3{-halfWidth,eave,halfDepth},Vec3{0,ridge,halfDepth},Vec3{0,ridge,-halfDepth}},color);
    AppendFace(mesh,{Vec3{0,ridge,-halfDepth},Vec3{0,ridge,halfDepth},Vec3{halfWidth,eave,halfDepth},Vec3{halfWidth,eave,-halfDepth}},color);
    AppendFace(mesh,{Vec3{-halfWidth,eave,-halfDepth},Vec3{0,ridge,-halfDepth},Vec3{halfWidth,eave,-halfDepth},Vec3{halfWidth,eave-.08f,-halfDepth}},color);
    AppendFace(mesh,{Vec3{halfWidth,eave,halfDepth},Vec3{0,ridge,halfDepth},Vec3{-halfWidth,eave,halfDepth},Vec3{-halfWidth,eave-.08f,halfDepth}},color);
}

void AppendWindow(Mesh3D& mesh,float x,float y,float z,float width,float height){
    const Color timber{.29f,.16f,.075f,1},glass{.34f,.58f,.69f,1};
    AppendBox(mesh,{x,y,z-.035f},{width,height,.07f},glass);
    constexpr float frame=.09f;
    AppendBox(mesh,{x-width*.5f,y,z-.09f},{frame,height+.18f,.12f},timber);AppendBox(mesh,{x+width*.5f,y,z-.09f},{frame,height+.18f,.12f},timber);
    AppendBox(mesh,{x,y-height*.5f,z-.09f},{width+.18f,frame,.12f},timber);AppendBox(mesh,{x,y+height*.5f,z-.09f},{width+.18f,frame,.12f},timber);
    AppendBox(mesh,{x,y,z-.1f},{frame,height,.13f},timber);AppendBox(mesh,{x,y,z-.1f},{width,frame,.13f},timber);
}

void AppendSideWindow(Mesh3D& mesh,float x,float y,float z,float width,float height){
    const Color timber{.29f,.16f,.075f,1},glass{.34f,.58f,.69f,1};constexpr float frame=.09f;
    AppendBox(mesh,{x,y,z},{.07f,height,width},glass);
    AppendBox(mesh,{x,y,z-width*.5f},{.12f,height+.18f,frame},timber);AppendBox(mesh,{x,y,z+width*.5f},{.12f,height+.18f,frame},timber);
    AppendBox(mesh,{x,y-height*.5f,z},{.12f,frame,width+.18f},timber);AppendBox(mesh,{x,y+height*.5f,z},{.12f,frame,width+.18f},timber);
    AppendBox(mesh,{x,y,z},{.13f,height,frame},timber);AppendBox(mesh,{x,y,z},{.13f,frame,width},timber);
}

void AppendRoofCourses(Mesh3D& mesh,const CottageMeshSettings3D& settings,Color roof){
    const float halfWidth=settings.width*.5f+settings.roofOverhang,halfDepth=settings.depth*.5f+settings.roofOverhang;
    constexpr std::uint32_t courses=5;for(std::uint32_t course=0;course<courses;++course){const float start=float(course)/float(courses),end=float(course+1)/float(courses),lift=.018f+.004f*float(course&1u);Color tile=roof;const float shade=.88f+.12f*Variation(settings.seed,31+course);tile.r*=shade;tile.g*=shade;tile.b*=shade;
        const float leftStart=-halfWidth+halfWidth*start,leftEnd=-halfWidth+halfWidth*end,leftY=settings.wallHeight+settings.roofHeight*start,rightY=settings.wallHeight+settings.roofHeight*end;
        AppendFace(mesh,{Vec3{leftStart,leftY+lift,-halfDepth},Vec3{leftStart,leftY+lift,halfDepth},Vec3{leftEnd,rightY+lift,halfDepth},Vec3{leftEnd,rightY+lift,-halfDepth}},tile);
        AppendFace(mesh,{Vec3{-leftEnd,rightY+lift,-halfDepth},Vec3{-leftEnd,rightY+lift,halfDepth},Vec3{-leftStart,leftY+lift,halfDepth},Vec3{-leftStart,leftY+lift,-halfDepth}},tile);
    }
}

void AppendEaveJoinery(Mesh3D& mesh,const CottageMeshSettings3D& settings,Color timber){
    const float halfWidth=settings.width*.5f+settings.roofOverhang,halfDepth=settings.depth*.5f+settings.roofOverhang;
    // 簷底板與牆體、外側封簷重疊，讓屋頂挑出部分有連續承接面。
    for(float side:{-1.f,1.f})AppendBox(mesh,{side*(settings.width*.5f+settings.roofOverhang*.5f),settings.wallHeight-.065f,0},{settings.roofOverhang+.12f,.13f,halfDepth*2.f},timber);
    // 山牆斜封邊共用屋頂坡度；每片只用兩個三角形維持近景預算。
    for(float side:{-1.f,1.f}){
        const float z=side*(halfDepth+.006f),eave=settings.wallHeight+.018f,ridge=eave+settings.roofHeight;
        std::array<Vec3,4> left={Vec3{-halfWidth,eave-.14f,z},Vec3{0,ridge-.14f,z},Vec3{0,ridge,z},Vec3{-halfWidth,eave,z}};
        std::array<Vec3,4> right={Vec3{0,ridge-.14f,z},Vec3{halfWidth,eave-.14f,z},Vec3{halfWidth,eave,z},Vec3{0,ridge,z}};
        if(side>0){std::reverse(left.begin(),left.end());std::reverse(right.begin(),right.end());}
        AppendFace(mesh,left,timber);AppendFace(mesh,right,timber);
    }
}

}

Mesh3D CreateCottageMesh3D(const CottageMeshSettings3D& settings){
    if(!std::isfinite(settings.width)||!std::isfinite(settings.depth)||!std::isfinite(settings.wallHeight)||!std::isfinite(settings.roofHeight)||settings.width<2.f||settings.depth<2.f||settings.wallHeight<1.8f||settings.roofHeight<.4f||settings.wallThickness<=0||settings.wallThickness>settings.width*.25f||settings.roofOverhang<0||settings.roofOverhang>2.f)return {};
    const bool detailed=settings.detail==ArchitectureMeshDetail3D::Near;Mesh3D mesh;
    const float stoneShift=(Variation(settings.seed,3)-.5f)*.06f;const Color plaster{.76f+stoneShift,.69f+stoneShift,.55f+stoneShift,1},roof{.42f+.08f*Variation(settings.seed,5),.16f,.09f,1},timber{.28f,.15f,.07f,1};
    AppendBox(mesh,{0,settings.wallHeight*.5f,0},{settings.width,settings.wallHeight,settings.depth},plaster);
    AppendGableRoof(mesh,settings.width,settings.depth,settings.wallHeight,settings.roofHeight,settings.roofOverhang,roof);
    const float chimneySide=Variation(settings.seed,11)<.5f?-1.f:1.f,chimneyX=chimneySide*settings.width*(.24f+.06f*Variation(settings.seed,12)),chimneyZ=(Variation(settings.seed,13)-.5f)*settings.depth*.18f;
    if(!detailed){const float front=-settings.depth*.5f-.012f,doorWidth=(std::min)(1.15f,settings.width*.22f),doorHeight=(std::min)(2.15f,settings.wallHeight*.78f),windowY=settings.wallHeight*.57f,windowOffset=settings.width*.31f;AppendFace(mesh,{Vec3{-doorWidth*.5f,0,front},Vec3{doorWidth*.5f,0,front},Vec3{doorWidth*.5f,doorHeight,front},Vec3{-doorWidth*.5f,doorHeight,front}},{.3f,.17f,.08f,1});for(float x:{-windowOffset,windowOffset})AppendFace(mesh,{Vec3{x-.46f,windowY-.52f,front-.002f},Vec3{x+.46f,windowY-.52f,front-.002f},Vec3{x+.46f,windowY+.52f,front-.002f},Vec3{x-.46f,windowY+.52f,front-.002f}},{.3f,.52f,.62f,1});AppendBox(mesh,{chimneyX,settings.wallHeight+settings.roofHeight*.72f,chimneyZ},{.42f,settings.roofHeight*.72f,.5f},{.32f,.24f,.19f,1});return mesh;}
    const float front=-settings.depth*.5f-.07f,doorWidth=(std::min)(1.15f,settings.width*.22f),doorHeight=(std::min)(2.15f,settings.wallHeight*.78f);
    // 突出的石材牆基切斷大面積灰泥白盒輪廓，遠距離版本仍維持單純量體。
    AppendBox(mesh,{0,.19f,0},{settings.width+.12f,.38f,settings.depth+.12f},{.34f,.32f,.28f,1});
    AppendBox(mesh,{0,doorHeight*.5f,front},{doorWidth,doorHeight,.14f},{.3f,.17f,.08f,1});
    AppendBox(mesh,{-doorWidth*.58f,doorHeight*.5f+.1f,front-.06f},{.13f,doorHeight+.2f,.16f},timber);AppendBox(mesh,{doorWidth*.58f,doorHeight*.5f+.1f,front-.06f},{.13f,doorHeight+.2f,.16f},timber);AppendBox(mesh,{0,doorHeight+.07f,front-.06f},{doorWidth+.28f,.14f,.16f},timber);
    const float windowOffset=settings.width*.31f,windowY=settings.wallHeight*.57f;AppendWindow(mesh,-windowOffset,windowY,front,.92f,1.05f);AppendWindow(mesh,windowOffset,windowY,front,.92f,1.05f);
    const float sideOffset=settings.width*.5f+.07f;AppendSideWindow(mesh,-sideOffset,windowY,0,.92f,1.05f);AppendSideWindow(mesh,sideOffset,windowY,0,.92f,1.05f);
    for(float x:{-settings.width*.47f,settings.width*.47f})AppendBox(mesh,{x,settings.wallHeight*.5f,front-.04f},{.16f,settings.wallHeight,.16f},timber);
    for(float x:{-settings.width*.47f,settings.width*.47f})AppendBox(mesh,{x,settings.wallHeight*.5f,settings.depth*.5f+.04f},{.16f,settings.wallHeight,.16f},timber);
    AppendBox(mesh,{0,settings.wallHeight-.18f,front-.04f},{settings.width-.22f,.17f,.16f},timber);
    for(float x:{-sideOffset,sideOffset})AppendBox(mesh,{x,settings.wallHeight-.18f,0},{.16f,.17f,settings.depth-.22f},timber);
    // 門廊立柱從石階頂面連至雨遮底面，避免短支撐懸空；台階本身仍落在地面。
    AppendBox(mesh,{0,.08f,front-.35f},{doorWidth+.35f,.16f,.82f},{.37f,.34f,.29f,1});
    AppendBox(mesh,{0,doorHeight+.38f,front-.33f},{doorWidth+.55f,.15f,.68f},timber);
    const float porchBase=.16f,porchTop=doorHeight+.305f;
    for(float x:{-doorWidth*.58f,doorWidth*.58f})AppendBox(mesh,{x,(porchBase+porchTop)*.5f,front-.5f},{.14f,porchTop-porchBase,.14f},timber);
    for(float x:{-windowOffset,windowOffset})AppendBox(mesh,{x,windowY-.62f,front-.13f},{1.14f,.13f,.24f},timber);for(float x:{-sideOffset,sideOffset})AppendBox(mesh,{x+(x<0?-.12f:.12f),windowY-.62f,0},{.24f,.13f,1.14f},timber);
    AppendBox(mesh,{0,settings.wallHeight+.02f,-settings.depth*.5f-settings.roofOverhang*.62f},{settings.width+settings.roofOverhang*1.7f,.13f,.16f},timber);
    const float roofHalfWidth=settings.width*.5f+settings.roofOverhang,roofDepth=settings.depth+settings.roofOverhang*2.f;AppendBox(mesh,{0,settings.wallHeight+settings.roofHeight+.045f,0},{.16f,.13f,roofDepth+.12f},{.3f,.13f,.07f,1});for(float x:{-roofHalfWidth,roofHalfWidth})AppendBox(mesh,{x,settings.wallHeight+.01f,0},{.13f,.15f,roofDepth+.08f},timber);
    AppendBox(mesh,{chimneyX,settings.wallHeight+settings.roofHeight*.72f,chimneyZ},{.48f,settings.roofHeight*.78f,.58f},{.34f,.25f,.2f,1});
    AppendBox(mesh,{chimneyX,settings.wallHeight+settings.roofHeight+.08f,chimneyZ},{.64f,.14f,.72f},{.28f,.2f,.16f,1});
    // 屋瓦以少量坡面層次表現重疊瓦列，避免為每片瓦建立昂貴立體幾何。
    AppendRoofCourses(mesh,settings,roof);
    AppendEaveJoinery(mesh,settings,timber);
    return mesh;
}

}
