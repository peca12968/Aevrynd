#include "MistwatchRangerHut.h"
#include "MistwatchCamera.h"
#include <iostream>
namespace {
bool HitsRoof(Aevrynd::Vec3 from,Aevrynd::Vec3 to,const Aevrynd::Mesh3D& mesh,bool roofOnly=true) {
    using namespace Aevrynd;
    const auto direction=to-from;
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
        const auto& va=mesh.vertices[mesh.indices[i]];
        const auto& vb=mesh.vertices[mesh.indices[i+1]];
        const auto& vc=mesh.vertices[mesh.indices[i+2]];
        if(roofOnly&&(std::min)({va.y,vb.y,vc.y})<2.8499f)continue;
        const Vec3 a{va.x,va.y,va.z},b{vb.x,vb.y,vb.z},c{vc.x,vc.y,vc.z};
        const auto edge=b-a,other=c-a,h=Cross(direction,other);
        const float det=Dot(edge,h);
        if(std::abs(det)<.000001f)continue;
        const float inverse=1.f/det;
        const auto s=from-a;
        const float u=inverse*Dot(s,h);
        if(u<0||u>1)continue;
        const auto q=Cross(s,edge);
        const float v=inverse*Dot(direction,q),t=inverse*Dot(other,q);
        if(v>=0&&u+v<=1&&t>.0001f&&t<.9999f)return true;
    }
    return false;
}
}
int main() {
    using namespace Aevrynd;
    const auto hut=Mistwatch::CreateRangerHut();
    const auto bounds=hut.Bounds();
    if(bounds.center.y+bounds.extents.y<3.7f) {
        std::cerr<<"FIRST_DIVERGENCE: ranger hut remains a flat collision-box roof; peak="
            <<bounds.center.y+bounds.extents.y<<'\n';
        return 1;
    }
    const auto repeated=Mistwatch::CreateRangerHut();
    if(!hut.Valid()||hut.indices!=repeated.indices||hut.vertices.size()!=repeated.vertices.size())return 2;
    for(std::size_t i=0;i<hut.vertices.size();++i) {
        const auto& v=hut.vertices[i];const auto& r=repeated.vertices[i];
        if(!Mistwatch::Session::Finite({v.x,v.y,v.z})||std::abs(v.x)>2.5001f||
            std::abs(v.z)>3.0001f||v.y<-.00001f||v.y>3.8501f)return 3;
        if(v.x!=r.x||v.y!=r.y||v.z!=r.z||v.normal!=r.normal||v.color!=r.color||v.u!=r.u||v.v!=r.v)return 4;
        const auto normal=UnpackNormalSnorm8(v.normal);
        if(std::abs(Length(Vec3{normal.x,normal.y,normal.z})-1.f)>.02f)return 5;
        // 入口與窗戶嚴格內部沒有新增木板，原有立柱邊界不算窗孔。
        if(v.x<-2.2999f&&v.y<2.5499f&&std::abs(v.z)<2.7999f)return 6;
        if(std::abs(v.z)>2.7999f&&v.y>.9001f&&v.y<2.1999f&&
            ((v.x>-2.1499f&&v.x<-.1501f)||(v.x>.1501f&&v.x<2.1499f)))return 7;
    }
    for(std::size_t i=0;i<hut.indices.size();i+=3) {
        const auto& a=hut.vertices[hut.indices[i]],b=hut.vertices[hut.indices[i+1]],c=hut.vertices[hut.indices[i+2]];
        const auto cross=Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z});
        const auto n=UnpackNormalSnorm8(a.normal);
        if(Length(cross)<.0000001f||Dot(cross,{n.x,n.y,n.z})<=0)return 8;
    }
    unsigned sealRays{};
    for(const float side:{-1.f,1.f})for(const float x:{.2f,.6f,1.2f,1.8f,2.4f}) {
        const float underside=3.76f-.85f*x/2.48f-.05f/std::cos(std::atan(.85f/2.48f));
        for(unsigned seam=0;seam<11;++seam) {
            const float z=-2.5f+.5f*float(seam);
            if(!HitsRoof({side*x,4,z},{side*x,underside-.003f,z},hut,false))return 11;
            ++sealRays;
        }
        for(const float end:{-1.f,1.f}) {
            if(!HitsRoof({side*x,underside-.001f,end*3.05f},{side*x,underside-.001f,end*2.9f},hut,false))return 12;
            ++sealRays;
        }
    }
    for(const float end:{-1.f,1.f})for(const float x:{-1.15f,1.15f})for(const float y:{1.1f,1.55f,2.f})
        if(HitsRoof({x,y,end*3.05f},{x,y,end*2.7f},hut,false))return 13;
    for(const float z:{-2.f,0.f,2.f})for(const float y:{.4f,1.3f,2.3f})
        if(HitsRoof({-2.6f,y,z},{-2.2f,y,z},hut,false))return 14;
    const auto solids=Mistwatch::WorldSolids(true);
    const auto terrain=Mistwatch::CreateTerrain();
    const std::array cameraOnly{Mistwatch::RangerHutCameraRoofSolid()};
    unsigned checked{},oldHits{};
    for(const Vec3 offset:{Vec3{},Vec3{-3,0,0},Vec3{4,0,0},Vec3{0,0,-4},Vec3{0,0,4},
        Vec3{-1,0,-2.5f},Vec3{-1,0,2.5f},Vec3{-4,0,-4},Vec3{-4,0,4}}) {
        auto feet=Mistwatch::Session::rangerLog+offset;feet.y=terrain.SampleHeight(feet.x,feet.z);
        for(unsigned yaw=0;yaw<36;++yaw)for(const float pitch:{-.15f,.3f,1.1f})for(const float distance:{2.5f,6.f,10.f}) {
            Mistwatch::OrbitCamera orbit;orbit.yaw=float(yaw)*6.2831853f/36.f;orbit.pitch=pitch;orbit.distance=distance;
            const auto old=Mistwatch::BuildPlayerCamera(feet,1.8f,orbit,solids,terrain);
            oldHits+=HitsRoof(old.target-Mistwatch::Session::rangerLog,old.position-Mistwatch::Session::rangerLog,hut);
            const auto camera=Mistwatch::BuildPlayerCamera(feet,1.8f,orbit,solids,terrain,cameraOnly);
            if(HitsRoof(camera.target-Mistwatch::Session::rangerLog,camera.position-Mistwatch::Session::rangerLog,hut)) {
                std::cerr<<"FIRST_DIVERGENCE: added roof crossed by player camera; yaw="<<yaw<<" pitch="<<pitch<<" distance="<<distance<<'\n';
                return 9;
            }
            ++checked;
        }
    }
    if(!oldHits)return 10;
    std::cout<<"Ranger hut PASS; triangles="<<hut.indices.size()/3<<" camera_sweeps="<<checked
        <<" old_camera_roof_intersections="<<oldHits<<" corrected_intersections=0 windows_and_west_entry=preserved"
        <<" roof_seal_rays="<<sealRays<<'\n';
    return 0;
}
