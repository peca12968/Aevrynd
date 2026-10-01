#include "MistwatchLandscape.h"
#include <iostream>
int main() {
    using namespace Aevrynd;
    const auto core=Mistwatch::CreateTerrain();
    const auto before=core.CreateMesh(Mistwatch::GroundMeshSettings());
    const auto landscape=Mistwatch::CreateLandscapeMesh(core),repeat=Mistwatch::CreateLandscapeMesh(core);
    if(!landscape.Valid()||landscape.vertices.size()>30000||landscape.indices.size()>180000)return 1;
    if(landscape.indices!=repeat.indices||landscape.vertices.size()!=repeat.vertices.size())return 2;
    unsigned seam{};
    for(std::size_t i=0;i<landscape.vertices.size();++i) {
        const auto& v=landscape.vertices[i];const auto& r=repeat.vertices[i];
        if(v.x!=r.x||v.y!=r.y||v.z!=r.z||v.normal!=r.normal||v.color!=r.color||!std::isfinite(v.y))return 3;
        const Vec4 normal=UnpackNormalSnorm8(v.normal);
        if(normal.y<=0||std::abs(Length(Vec3{normal.x,normal.y,normal.z})-1.f)>.02f)return 4;
        if(core.Contains(v.x,v.z)) {
            if(v.x!=-50&&v.x!=50&&v.z!=-20&&v.z!=100)return 5;
            const unsigned x=unsigned((v.x+50)*2),z=unsigned((v.z+20)*2);
            const auto& original=before.vertices[std::size_t(z)*201+x];
            if(v.y!=original.y||v.normal!=original.normal||v.color!=original.color)return 6;
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
    for(std::size_t i=0;i<trees.size();++i) {
        const auto& tree=trees[i];
        if(Length(tree.position-again[i].position)!=0||tree.rotation.y!=again[i].rotation.y||tree.scale.x!=again[i].scale.x)return 11;
        if(core.Contains(tree.position.x,tree.position.z)||tree.position.y!=Mistwatch::LandscapeHeight(core,tree.position.x,tree.position.z))return 12;
        if(tree.scale.x<.5f||tree.scale.x>.85f)return 13;
        for(std::size_t j=0;j<i;++j)if(std::hypot(tree.position.x-trees[j].position.x,tree.position.z-trees[j].position.z)<5.f)return 14;
    }
    const auto after=core.CreateMesh(Mistwatch::GroundMeshSettings());
    for(std::size_t i=0;i<before.vertices.size();++i)if(before.vertices[i].y!=after.vertices[i].y)return 15;
    std::cout<<"Landscape PASS; seam="<<seam<<" vertices="<<landscape.vertices.size()<<" triangles="<<landscape.indices.size()/3<<" background_trees="<<trees.size()<<'\n';
}
