#include <Aevrynd/GroundConform3D.h>
#include <iostream>
#include <limits>
#include <string_view>

namespace {
using namespace Aevrynd;
unsigned checks{},negatives{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void Reject(F&& action,std::string_view reason){try{action();}catch(const std::invalid_argument& e){
    Need(e.what()==reason,"unexpected rejection");++negatives;return;}throw std::runtime_error("negative accepted");}
Mesh3D Blade(){Mesh3D mesh;for(const auto p:std::array<Vec3,6>{{{-.5f,0,0},{.5f,0,0},{-.5f,.035f,0},{.5f,.035f,0},{-.5f,.6f,0},{.5f,.6f,0}}})
    mesh.vertices.push_back({p.x,p.y,p.z,PackNormalSnorm8({0,0,1},.75f),0xffaabbccu,p.x,p.y});
    mesh.indices={0,1,2,1,3,2,2,3,4,3,5,4};return mesh;}
bool Same(const Vertex3D& a,const Vertex3D& b){return std::bit_cast<std::uint32_t>(a.x)==std::bit_cast<std::uint32_t>(b.x)&&
    std::bit_cast<std::uint32_t>(a.y)==std::bit_cast<std::uint32_t>(b.y)&&std::bit_cast<std::uint32_t>(a.z)==std::bit_cast<std::uint32_t>(b.z)&&
    a.u==b.u&&a.v==b.v&&a.normal==b.normal&&a.color==b.color;}
void Controls(){
    const auto source=Blade();const Transform3D pose{};
    const auto flat=[](float,float){return std::optional<double>{0.};};
    const auto tilted=[](float x,float z){return std::optional<double>{.25*double(x)+.125*double(z)};};
    const auto baseline=CreateGroundConformedMesh3D(source,pose,flat);
    Need(baseline.movedVertices==0&&baseline.rebuiltNormals==0&&baseline.mesh.indices==source.indices,"flat identity");
    for(unsigned i=0;i<source.vertices.size();++i)Need(Same(source.vertices[i],baseline.mesh.vertices[i]),"flat exact vertex identity");
    for(const float scale:{.5f,1.f,2.f})for(const float yaw:{0.f,.7f,2.3f}){
        Transform3D p{{0,0,0},{0,yaw,0},{scale,scale,scale}};GroundConformSettings3D settings;settings.maximumGroundDelta=.5f;
        const auto result=CreateGroundConformedMesh3D(source,p,tilted,settings),again=CreateGroundConformedMesh3D(source,p,tilted,settings);
        Need(result.mesh.indices==source.indices&&result.movedVertices>0&&result.affectedFaces>0&&result.rebuiltNormals>0,"actual lower geometry changed");
        for(unsigned i=0;i<source.vertices.size();++i){const auto& a=source.vertices[i];const auto& b=result.mesh.vertices[i];
            Need(Same(b,again.mesh.vertices[i]),"repeat deterministic");
            Need(a.x==b.x&&a.z==b.z&&a.u==b.u&&a.v==b.v&&a.color==b.color&&(a.normal&0xff000000u)==(b.normal&0xff000000u),"material and wind identity");
            const auto n=UnpackNormalSnorm8(b.normal);Need(Length({n.x,n.y,n.z})>.95f&&Length({n.x,n.y,n.z})<1.05f,"rebuilt packed normal length");
            const double relative=double(a.y)*scale;
            if(relative>=settings.fadeHeight)Need(a.y==b.y,"upper geometry fixed");
            if(relative<=settings.fullContactBand){const auto world=TransformPoint(p.Matrix(),{b.x,b.y,b.z});
                const auto expected=tilted(world.x,world.z);Need(std::abs(double(world.y)-*expected-relative)<.000001,"source contact band follows real plane");}
        }
    }
    auto seam=source;seam.vertices.push_back(source.vertices[0]);seam.indices[0]=6;
    const auto welded=CreateGroundConformedMesh3D(seam,pose,tilted);
    Need(welded.mesh.vertices[0].y==welded.mesh.vertices[6].y,"UV seam stays welded");
    auto pivot=source;for(auto& vertex:pivot.vertices)vertex.y-=.2f;
    Transform3D offset;offset.position.y=.2f;
    const auto shifted=CreateGroundConformedMesh3D(pivot,offset,tilted);
    Need(shifted.sourceMinimumY==-.2f&&std::abs(shifted.root.y)<.000001f,"negative local pivot supported");
    auto bad=source;bad.indices.pop_back();Reject([&]{(void)CreateGroundConformedMesh3D(bad,pose,flat);},"GROUND_CONFORM_TOPOLOGY");
    bad=source;bad.vertices[0].x=std::numeric_limits<float>::quiet_NaN();Reject([&]{(void)CreateGroundConformedMesh3D(bad,pose,flat);},"GROUND_CONFORM_SOURCE_VERTEX");
    bad=source;bad.vertices[0].normal=0;Reject([&]{(void)CreateGroundConformedMesh3D(bad,pose,flat);},"GROUND_CONFORM_SOURCE_NORMAL");
    bad=source;bad.indices[1]=bad.indices[0];Reject([&]{(void)CreateGroundConformedMesh3D(bad,pose,flat);},"GROUND_CONFORM_SOURCE_FACE");
    auto p=pose;p.scale.y=2;Reject([&]{(void)CreateGroundConformedMesh3D(source,p,flat);},"GROUND_CONFORM_POSE");
    p=pose;p.rotation.x=.1f;Reject([&]{(void)CreateGroundConformedMesh3D(source,p,flat);},"GROUND_CONFORM_POSE");
    p=pose;p.position.y=.1f;Reject([&]{(void)CreateGroundConformedMesh3D(source,p,flat);},"GROUND_CONFORM_ROOT_NOT_ATTACHED");
    auto settings=GroundConformSettings3D{};settings.fadeHeight=settings.fullContactBand;
    Reject([&]{(void)CreateGroundConformedMesh3D(source,pose,flat,settings);},"GROUND_CONFORM_SETTINGS");
    settings={};settings.maximumGroundDelta=.01f;Reject([&]{(void)CreateGroundConformedMesh3D(source,pose,tilted,settings);},"GROUND_CONFORM_GROUND_DELTA");
    const auto missing=[](float,float){return std::optional<double>{};};
    Reject([&]{(void)CreateGroundConformedMesh3D(source,pose,missing);},"GROUND_CONFORM_SURFACE_MISSING");
    const auto hole=[](float x,float){return x==0?std::optional<double>{0.}:std::optional<double>{};};
    Reject([&]{(void)CreateGroundConformedMesh3D(source,pose,hole);},"GROUND_CONFORM_SURFACE_MISSING");
    const auto invalid=[](float,float){return std::optional<double>{std::numeric_limits<double>::infinity()};};
    Reject([&]{(void)CreateGroundConformedMesh3D(source,pose,invalid);},"GROUND_CONFORM_SURFACE_MISSING");
    p=pose;p.position.y=GroundConformSettings3D{}.rootTolerance;
    Need(CreateGroundConformedMesh3D(source,p,flat).movedVertices==0,"reference alignment exact tolerance included");
    p.position.y=std::nextafter(p.position.y,std::numeric_limits<float>::infinity());
    Reject([&]{(void)CreateGroundConformedMesh3D(source,p,flat);},"GROUND_CONFORM_ROOT_NOT_ATTACHED");
    auto tall=source;tall.vertices[4].y=tall.vertices[5].y=4.f;p=pose;
    p.scale={std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()};
    Reject([&]{(void)CreateGroundConformedMesh3D(tall,p,flat);},"GROUND_CONFORM_WORLD_VERTEX");
    auto shortBlade=source;shortBlade.vertices[4].y=shortBlade.vertices[5].y=.06f;
    settings={};settings.fadeHeight=.05f;
    const auto strain=[](float x,float){return std::optional<double>{.4*double(x)};};
    Reject([&]{(void)CreateGroundConformedMesh3D(shortBlade,pose,strain,settings);},"GROUND_CONFORM_FACE_ORIENTATION");
}
}
int main(){try{Controls();std::cout<<"GROUND_CONFORM_CPU_PASS checks="<<checks<<" negatives="<<negatives<<'\n';return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
