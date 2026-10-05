#include "MistwatchLandscape.h"
#include <iostream>
int main() {
    using namespace Aevrynd;
    const auto core=Mistwatch::CreateTerrain();
    // 溪谷切槽不能被邊界夾取一路拖到遠山；24 公尺外只保留未切槽的丘陵邊界。
    const auto smooth=[](float value){const auto t=std::clamp(value,0.f,1.f);return t*t*t*(t*(t*6.f-15.f)+10.f);};
    const auto legacyHeight=[&](float x,float z) {
        const float ex=std::clamp(x,-50.f,50.f),ez=std::clamp(z,-20.f,100.f),d=std::hypot(x-ex,z-ez);
        const float ridge=24.f+18.f*std::sin(x*.023f+z*.007f)+14.f*std::cos(z*.027f-x*.009f)+8.f*std::sin(x*.057f+z*.041f)+.11f*d;
        const float edge=core.SampleHeight(ex,ez);return edge+(ridge-edge)*smooth(d/60.f);
    };
    for(float sign:{-1.f,1.f})for(float d:{6.f,12.f,24.f,30.f,36.f,48.f,60.f})for(float z:{25.f,30.f,35.f,40.f}) {
        const float edgeX=sign*50.f,x=edgeX+sign*d;
        const float hills=12.f*(1.f+.3f*std::sin(z*.13f)+.2f*std::cos(edgeX*.19f));
        const float incision=(std::max)(0.f,hills-core.SampleHeight(edgeX,z));
        const float expected=legacyHeight(x,z)+incision*smooth(d/24.f)*(1.f-smooth(d/60.f));
        const float actual=Mistwatch::LandscapeHeight(core,x,z);
        if(std::abs(actual-expected)>.00005f) {
            std::cerr<<"River boundary incision persists: x="<<x<<" z="<<z<<" actual="<<actual<<" expected="<<expected<<'\n';
            return 16;
        }
    }
    // 加寬河谷後按實際邊界切槽判定，不把固定 z 當成永遠乾燥的岸。
    // 核心、真正乾岸與60公尺以外仍須等於原遠山；影響區只允許原24公尺解槽式。
    unsigned released{},unchanged{};
    for(float x:{-150.f,-110.f,-80.f,-50.f,0.f,50.f,80.f,110.f,150.f})for(float z:{-20.f,20.f,60.f,100.f}) {
        const float ex=std::clamp(x,-50.f,50.f),ez=std::clamp(z,-20.f,100.f),d=std::hypot(x-ex,z-ez);
        const float hills=Mistwatch::HillElevation(ex,ez),incision=(std::max)(0.f,hills-core.SampleHeight(ex,ez));
        float expected=legacyHeight(x,z);
        if(x!=ex&&d<60.f&&incision>0){expected+=incision*smooth(std::abs(x-ex)/24.f)*(1.f-smooth(d/60.f));++released;}
        else ++unchanged;
        if(std::abs(Mistwatch::LandscapeHeight(core,x,z)-expected)>.00005f)return 17;
    }
    if(!released||!unchanged)return 23;
    const auto before=core.CreateMesh(Mistwatch::GroundMeshSettings());
    const auto landscape=Mistwatch::CreateLandscapeMesh(core),repeat=Mistwatch::CreateLandscapeMesh(core);
    const auto surfaceBaseline=Mistwatch::CreateLandscapeMesh(core,false);
    if(!landscape.Valid()||landscape.vertices.size()>30000||landscape.indices.size()>180000)return 1;
    if(landscape.indices!=repeat.indices||landscape.vertices.size()!=repeat.vertices.size())return 2;
    if(surfaceBaseline.indices!=landscape.indices||surfaceBaseline.vertices.size()!=landscape.vertices.size())return 21;
    unsigned seam{};
    for(std::size_t i=0;i<landscape.vertices.size();++i) {
        const auto& v=landscape.vertices[i];const auto& r=repeat.vertices[i];
        if(v.x!=r.x||v.y!=r.y||v.z!=r.z||v.normal!=r.normal||v.color!=r.color||!std::isfinite(v.y))return 3;
        const auto& baseline=surfaceBaseline.vertices[i];
        if(baseline.x!=v.x||baseline.y!=v.y||baseline.z!=v.z||baseline.normal!=v.normal||
           (baseline.color>>24)!=255u||(baseline.color&0x00ffffffu)!=(v.color&0x00ffffffu))return 22;
        const auto expectedAlpha=unsigned(std::lround(std::clamp((v.y-core.MinimumHeight())/
            (core.MaximumHeight()-core.MinimumHeight()),0.f,1.f)*255.f));
        if((v.color>>24)!=expectedAlpha) {
            std::cerr<<"FIRST_DIVERGENCE: landscape must share core height alpha; vertex="<<i
                     <<" actual="<<(v.color>>24)<<" expected="<<expectedAlpha<<'\n';return 20;
        }
        const Vec4 normal=UnpackNormalSnorm8(v.normal);
        if(normal.y<=0||std::abs(Length(Vec3{normal.x,normal.y,normal.z})-1.f)>.02f)return 4;
        if(core.Contains(v.x,v.z)) {
            if(v.x!=-50&&v.x!=50&&v.z!=-20&&v.z!=100)return 5;
            const unsigned x=unsigned((v.x+50)*2),z=unsigned((v.z+20)*2);
            const auto& original=before.vertices[std::size_t(z)*201+x];
            if(v.y!=original.y||v.normal!=original.normal||(v.color&0x00ffffffu)!=(original.color&0x00ffffffu))return 6;
            ++seam;
        }
    }
    if(seam!=880)return 7;
    for(unsigned i=0;i<landscape.indices.size();i+=3) {
        const auto& a=landscape.vertices[landscape.indices[i]];const auto& b=landscape.vertices[landscape.indices[i+1]];const auto& c=landscape.vertices[landscape.indices[i+2]];
        if(Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z}).y<=0)return 8;
        if(core.Contains((a.x+b.x+c.x)/3,(a.z+b.z+c.z)/3))return 9;
    }
    const auto trees=Mistwatch::LandscapeTrees(core),again=Mistwatch::LandscapeTrees(core);
    if(trees.size()!=520||trees.size()!=again.size())return 10;
    float maximumRootError{};
    for(std::size_t i=0;i<trees.size();++i) {
        const auto& tree=trees[i];
        if(Length(tree.position-again[i].position)!=0||tree.rotation.y!=again[i].rotation.y||tree.scale.x!=again[i].scale.x)return 11;
        if(core.Contains(tree.position.x,tree.position.z))return 12;
        if(tree.scale.x<.5f||tree.scale.x>.85f)return 13;
        for(std::size_t j=0;j<i;++j)if(std::hypot(tree.position.x-trees[j].position.x,tree.position.z-trees[j].position.z)<5.f)return 14;
        // 獨立搜尋實際提交的三角面，而非再把解析高度函數當作貼地證據。
        bool found{};
        for(std::size_t triangle=0;triangle<landscape.indices.size();triangle+=3) {
            const auto& a=landscape.vertices[landscape.indices[triangle]];
            const auto& b=landscape.vertices[landscape.indices[triangle+1]];
            const auto& c=landscape.vertices[landscape.indices[triangle+2]];
            if(tree.position.x<(std::min)({a.x,b.x,c.x})||tree.position.x>(std::max)({a.x,b.x,c.x})||
               tree.position.z<(std::min)({a.z,b.z,c.z})||tree.position.z>(std::max)({a.z,b.z,c.z}))continue;
            const float determinant=(b.z-c.z)*(a.x-c.x)+(c.x-b.x)*(a.z-c.z);
            const float wa=((b.z-c.z)*(tree.position.x-c.x)+(c.x-b.x)*(tree.position.z-c.z))/determinant;
            const float wb=((c.z-a.z)*(tree.position.x-c.x)+(a.x-c.x)*(tree.position.z-c.z))/determinant,wc=1.f-wa-wb;
            if(wa<-.00001f||wb<-.00001f||wc<-.00001f)continue;
            const float surface=wa*a.y+wb*b.y+wc*c.y;
            const float error=std::abs(tree.position.y-surface);maximumRootError=(std::max)(maximumRootError,error);
            if(error>.00005f){std::cerr<<"Landscape tree off actual mesh: index="<<i<<" error="<<error<<'\n';return 18;}
            found=true;break;
        }
        if(!found)return 19;
    }
    const auto after=core.CreateMesh(Mistwatch::GroundMeshSettings());
    for(std::size_t i=0;i<before.vertices.size();++i)if(before.vertices[i].y!=after.vertices[i].y)return 15;
    std::cout<<"Landscape PASS; seam="<<seam<<" vertices="<<landscape.vertices.size()<<" triangles="<<landscape.indices.size()/3<<" background_trees="<<trees.size()<<" max_root_mesh_error="<<maximumRootError<<'\n';
}
