#include "MistwatchSupplyCrate.h"
#include "MistwatchWorld.h"
#include <iostream>
int main() {
    using namespace Aevrynd;
    const auto crate=Mistwatch::CreateSupplyCrate();
    const auto repeated=Mistwatch::CreateSupplyCrate();
    if(!crate.wood.Valid()||!crate.metal.Valid())return 1;
    if(crate.wood.indices.size()/3!=360||crate.metal.indices.size()/3!=236)return 2;
    unsigned checked{};
    for(const auto* mesh:{&crate.wood,&crate.metal}) {
        for(const auto& v:mesh->vertices) {
            if(!Mistwatch::Session::Finite({v.x,v.y,v.z})||std::abs(v.x)>.40001f||
                std::abs(v.z)>.40001f||v.y<-.00001f||v.y>.80001f)return 3;
            const auto n=UnpackNormalSnorm8(v.normal);
            if(std::abs(Length(Vec3{n.x,n.y,n.z})-1.f)>.02f)return 4;
        }
        for(std::size_t i=0;i<mesh->indices.size();i+=3) {
            const auto& a=mesh->vertices[mesh->indices[i]];
            const auto& b=mesh->vertices[mesh->indices[i+1]];
            const auto& c=mesh->vertices[mesh->indices[i+2]];
            const Vec3 cross=Cross(Vec3{b.x-a.x,b.y-a.y,b.z-a.z},Vec3{c.x-a.x,c.y-a.y,c.z-a.z});
            if(Length(cross)<.0000001f)return 5;
            const auto n=UnpackNormalSnorm8(a.normal);
            // 幾何面方向與頂點法線一致，避免單面材質剔除反向的鉚釘面。
            if(Dot(cross,{n.x,n.y,n.z})<=0) {
                std::cerr<<"FIRST_DIVERGENCE: face_normal_winding mesh="<<(mesh==&crate.wood?"wood":"metal")
                    <<" triangle="<<i/3<<" dot="<<Dot(cross,{n.x,n.y,n.z})<<'\n';
                return 6;
            }
            ++checked;
        }
    }
    if(crate.wood.indices!=repeated.wood.indices||crate.metal.indices!=repeated.metal.indices)return 7;
    const auto woodBounds=crate.wood.Bounds(),metalBounds=crate.metal.Bounds();
    if((metalBounds.center.y+metalBounds.extents.y)-(woodBounds.center.y+woodBounds.extents.y)<.0059f||
        (woodBounds.center.y-woodBounds.extents.y)-(metalBounds.center.y-metalBounds.extents.y)<.0059f)return 13;
    float bandFront=0,rivetFront=0;
    for(std::size_t i=0;i<96;++i)bandFront=(std::min)(bandFront,crate.metal.vertices[i].z);
    for(std::size_t i=96;i<288;++i)rivetFront=(std::min)(rivetFront,crate.metal.vertices[i].z);
    if(bandFront-rivetFront<.0029f)return 14;
    const auto terrain=Mistwatch::CreateTerrain();
    for(unsigned i=0;i<3;++i) {
        const auto p=Mistwatch::Session::supplies[i];
        for(const float x:{-.4f,0.f,.4f})for(const float z:{-.4f,0.f,.4f})
            if(std::abs(terrain.SampleHeight(p.x+x,p.z+z)-p.y)>.00001f)return 8;
        Mistwatch::Session game;
        game.feet=p+Vec3{0,0,-2.51f};
        if(game.Collect(i))return 9;
        game.feet=p+Vec3{0,0,-2.5f};
        if(!game.Collect(i)||game.Collect(i)||game.collected!=(1u<<i))return 10;
        Mistwatch::Session loaded;
        if(!loaded.Restore(game.Serialize())||loaded.collected!=game.collected)return 11;
        loaded.Damage(100);
        if(loaded.Collect((i+1)%3))return 12;
    }
    std::cout<<"Supply crate PASS; wood_parts=30 metal_triangles=236 total_triangles="<<checked
        <<" grounded_sites=3 collection_and_save=passed cap_clearance=6mm rivet_clearance=3mm\n";
}
