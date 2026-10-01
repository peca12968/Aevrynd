#include "MistwatchWorld.h"
#include <Aevrynd/CharacterController3D.h>
#include <iostream>
#include <limits>
namespace {
bool EqualMesh(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b) {
    if(a.indices!=b.indices || a.vertices.size()!=b.vertices.size())return false;
    for(std::size_t i=0;i<a.vertices.size();++i) {
        const auto& x=a.vertices[i];const auto& y=b.vertices[i];
        if(x.x!=y.x||x.y!=y.y||x.z!=y.z||x.normal!=y.normal||x.color!=y.color||x.u!=y.u||x.v!=y.v)return false;
    }
    return true;
}
bool SoundMesh(const Aevrynd::Mesh3D& mesh) {
    using namespace Aevrynd;
    if(!mesh.Valid())return false;
    for(const auto& v:mesh.vertices) {
        const auto n=UnpackNormalSnorm8(v.normal);
        const float length=Length(Vec3{n.x,n.y,n.z});
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||v.y<0||length<.85f||length>1.15f) {
            std::cerr<<"Invalid vertex y="<<v.y<<" normal length="<<length<<'\n';return false;
        }
    }
    for(std::size_t i=0;i<mesh.indices.size();i+=3) {
        const auto point=[&](unsigned index){const auto& v=mesh.vertices[index];return Vec3{v.x,v.y,v.z};};
        const auto cross=Cross(point(mesh.indices[i+1])-point(mesh.indices[i]),point(mesh.indices[i+2])-point(mesh.indices[i]));
        if(Length(cross)<.000001f){std::cerr<<"Degenerate triangle "<<i/3<<'\n';return false;}
        const auto packed=UnpackNormalSnorm8(mesh.vertices[mesh.indices[i]].normal);
        // 葉尖三角形面積小於通用 Normalize 的截斷值；已驗證非退化後明確正規化。
        const float agreement=Dot(cross*(1.f/Length(cross)),Vec3{packed.x,packed.y,packed.z});
        if(agreement<.2f){std::cerr<<"Winding/normal mismatch triangle "<<i/3<<" dot="<<agreement<<" area="<<Length(cross)
            <<" normal="<<packed.x<<","<<packed.y<<","<<packed.z<<'\n';return false;}
    }
    return true;
}
}
int main() {
    using namespace Aevrynd;
    for(unsigned seed=0;seed<32;++seed) {
        const auto fernNear=CreateFernMesh3D(EnvironmentMeshDetail3D::Near,seed);
        const auto fernFar=CreateFernMesh3D(EnvironmentMeshDetail3D::Far,seed);
        if(!SoundMesh(fernNear)||!SoundMesh(fernFar)) {std::cerr<<"Fern topology/ground anchoring failed seed="<<seed<<'\n';return 1;}
        if(!EqualMesh(fernNear,CreateFernMesh3D(EnvironmentMeshDetail3D::Near,seed))||
            !EqualMesh(fernFar,CreateFernMesh3D(EnvironmentMeshDetail3D::Far,seed)))return 2;
        if(fernNear.vertices.size()>1800||fernNear.indices.size()/3>1300||fernFar.indices.size()>=fernNear.indices.size()/3)return 3;
        const auto bounds=fernNear.Bounds();
        if(bounds.extents.x<.7f||bounds.extents.z<.7f||bounds.extents.y<.25f||bounds.extents.y>.5f)return 4;
    }
    if(EqualMesh(CreateFernMesh3D(EnvironmentMeshDetail3D::Near,17),CreateFernMesh3D(EnvironmentMeshDetail3D::Near,18)))return 5;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto trees=Mistwatch::TreePositions();
    const auto props=Mistwatch::ForestProps(terrain,trees);
    const auto plants=Mistwatch::CreateUnderstory(terrain,trees,props),repeat=Mistwatch::CreateUnderstory(terrain,trees,props);
    if(plants.ferns.size()<220||plants.ferns.size()>420||plants.saplings.size()<30||plants.saplings.size()>42) {
        std::cerr<<"Insufficient layered coverage: ferns="<<plants.ferns.size()<<" saplings="<<plants.saplings.size()<<'\n';return 6;
    }
    const auto solids=Mistwatch::WorldSolids(false);
    unsigned trunkChecks{};
    for(bool sapling:{false,true}) {
        const auto& collection=sapling?plants.saplings:plants.ferns;
        const auto& again=sapling?repeat.saplings:repeat.ferns;
        if(collection.size()!=again.size())return 7;
        for(std::size_t i=0;i<collection.size();++i) {
            const auto& plant=collection[i];const auto a=plant.Matrix(),b=again[i].Matrix();
            for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)if(a[row][column]!=b[row][column])return 8;
            if(std::abs(plant.position.y-terrain.SampleHeight(plant.position.x,plant.position.z))>.0001f)return 9;
            const float radius=sapling?1.4f:.9f;
            if(!Mistwatch::PropClearsObjectives({plant.position,{radius,1,radius}}))return 10;
            if(sapling) {
                const auto trunk=Mistwatch::SaplingCollider(plant);
                bool found=false;
                for(const auto& solid:solids)found|=Length(solid.center-trunk.center)<.0001f&&Length(solid.extents-trunk.extents)<.0001f;
                if(!found)return 11;
                if(std::abs(plant.position.x)<10.f) {
                    CharacterController3D player;
                    player.Reset(plant.position+Vec3{-1.5f,0,0});
                    const std::array<BoxCollider3D,1> obstacle{trunk};
                    for(unsigned tick=0;tick<60;++tick)player.Step({{1,0,0},false,1},obstacle,terrain,1.f/60.f);
                    if(player.Position().x>trunk.center.x-trunk.extents.x-player.Settings().radius+.02f)return 12;
                    ++trunkChecks;
                }
            }
            for(const auto& prop:props)if(std::abs(plant.position.x-prop.collider.center.x)<prop.collider.extents.x+radius &&
                std::abs(plant.position.z-prop.collider.center.z)<prop.collider.extents.z+radius)return 13;
            for(std::size_t j=0;j<i;++j)if(std::hypot(plant.position.x-collection[j].position.x,plant.position.z-collection[j].position.z)<(sapling?3.f:1.f))return 14;
        }
    }
    if(!trunkChecks)return 15;
    float previous=1;
    for(unsigned distance=0;distance<=400;++distance) {
        const float value=Mistwatch::UnderstoryVisibility(float(distance)*.1f);
        if(value<0||value>1||value>previous)return 16;
        previous=value;
    }
    if(Mistwatch::UnderstoryVisibility(24)!=1||Mistwatch::UnderstoryVisibility(38)!=0||
        Mistwatch::UnderstoryVisibility(std::numeric_limits<float>::quiet_NaN())!=0||
        Mistwatch::UnderstoryVisibility(30,40,20)!=0)return 17;
    const auto mesh=CreateFernMesh3D(EnvironmentMeshDetail3D::Near,17);
    std::cout<<"Understory PASS; ferns="<<plants.ferns.size()<<" saplings="<<plants.saplings.size()
        <<" native fern triangles="<<mesh.indices.size()/3<<" trunk controller checks="<<trunkChecks<<'\n';
}
