#include "MistwatchWorld.h"
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/MeshLoader.h>
#include <filesystem>
#include <iostream>
int main() {
    using namespace Aevrynd;
    auto loaded=LoadAevryndMeshFile(std::filesystem::path(MISTWATCH_SOURCE_ROOT)/
        "engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01"/Mistwatch::BoulderMeshName);
    if(!loaded){std::cerr<<"FIRST_DIVERGENCE: boulder asset "<<loaded.error<<'\n';return 11;}
    const auto original=loaded.mesh;
    Mistwatch::RootBoulderMesh(loaded.mesh);
    if(loaded.mesh.vertices.size()!=56938||loaded.mesh.indices.size()!=79833||original.indices!=loaded.mesh.indices)return 12;
    for(std::size_t i=0;i<original.vertices.size();++i) {
        const auto& a=original.vertices[i];const auto& b=loaded.mesh.vertices[i];
        if(a.normal!=b.normal||a.color!=b.color||a.u!=b.u||a.v!=b.v||
            std::abs(b.x-(a.x-Mistwatch::BoulderRoot.x)*Mistwatch::BoulderScale)>.000001f||
            std::abs(b.y-(a.y-Mistwatch::BoulderRoot.y)*Mistwatch::BoulderScale)>.000001f||
            std::abs(b.z-(a.z-Mistwatch::BoulderRoot.z)*Mistwatch::BoulderScale)>.000001f)return 13;
    }
    const auto actual=loaded.mesh.Bounds(),profile=Mistwatch::BoulderBounds();
    if(Length(actual.center-profile.center)>.00001f||Length(actual.extents-profile.extents)>.00001f)return 14;
    const auto terrain=Mistwatch::CreateTerrain();
    const auto trees=Mistwatch::TreePositions();
    const auto props=Mistwatch::ForestProps(terrain,trees),repeat=Mistwatch::ForestProps(terrain,trees);
    if(props.size()!=repeat.size() || props.size()<20){std::cerr<<"FIRST_DIVERGENCE: forest props count="<<props.size()<<'\n';return 1;}
    unsigned logs=0,rocks=0,vertices=0,blocked=0,bottomChecks=0,floating=0;
    float maximumFloat=-100.f,maximumBurial=0;
    const auto solids=Mistwatch::WorldSolids(false);
    for(std::size_t index=0;index<props.size();++index) {
        const auto& p=props[index];const auto& other=repeat[index];
        if(p.mesh!=other.mesh || Length(p.collider.center-other.collider.center)>0 || Length(p.collider.extents-other.collider.extents)>0)return 2;
        const auto matrix=p.transform.Matrix(),again=other.transform.Matrix();
        for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)if(matrix[row][column]!=again[row][column])return 3;
        const auto& mesh=p.FallenLog()?Mistwatch::ForestPropLogMesh():loaded.mesh;
        if(!mesh.Valid() || !Mistwatch::PropClearsObjectives(p.collider))return 4;
        if(p.FallenLog())++logs;else ++rocks;
        for(const auto& vertex:mesh.vertices) {
            const auto world=TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});
            const auto delta=world-p.collider.center;
            if(std::abs(delta.x)>p.collider.extents.x+.001f || std::abs(delta.y)>p.collider.extents.y+.001f || std::abs(delta.z)>p.collider.extents.z+.001f)return 5;
            ++vertices;
            // 檢查掃描底殼，不以包圍盒中心高度代替真實表面接地。
            if(!p.FallenLog()&&vertex.y<=.035f) {
                const float gap=world.y-terrain.SampleHeight(world.x,world.z);
                maximumFloat=std::max(maximumFloat,gap);
                maximumBurial=std::max(maximumBurial,-gap);
                ++bottomChecks;
                if(gap>.05f||gap<-.25f)++floating;
            }
        }
        bool shared=false;
        for(const auto& collider:solids)shared|=Length(collider.center-p.collider.center)<.0001f && Length(collider.extents-p.collider.extents)<.0001f;
        if(!shared)return 6;
        const float ground=terrain.SampleHeight(p.transform.position.x,p.transform.position.z);
        if(p.collider.center.y-p.collider.extents.y>ground+.01f || p.collider.center.y+p.collider.extents.y<ground)return 7;
        // 平坦地上的大型石塊須阻擋角色，並縮短穿越石塊的鏡頭射線。
        if(!p.FallenLog() && p.collider.extents.y>.4f && std::abs(p.collider.center.x)+p.collider.extents.x<9.f) {
            CharacterController3D player;
            const Vec3 start{p.collider.center.x-p.collider.extents.x-2,ground,p.collider.center.z};
            player.Reset(start);
            const std::array<BoxCollider3D,1> obstacle{p.collider};
            for(unsigned tick=0;tick<90;++tick)player.Step({{1,0,0},false,1},obstacle,terrain,1.f/60.f);
            if(player.Position().x>p.collider.center.x-p.collider.extents.x-player.Settings().radius+.02f || player.Stats().collisions==0)return 8;
            Camera3D camera;
            const Vec3 target{start.x,p.collider.center.y,start.z};
            camera.position={p.collider.center.x+p.collider.extents.x+2,p.collider.center.y,start.z};
            camera.target=target;
            const auto constrained=ConstrainThirdPersonCamera(camera,target,obstacle);
            if(constrained.position.x>=p.collider.center.x-p.collider.extents.x)return 9;
            ++blocked;
        }
    }
    if(floating||!bottomChecks) {
        std::cerr<<"FIRST_DIVERGENCE: scanned boulder bottom not grounded; violations="<<floating
            <<" samples="<<bottomChecks<<" maximum_float="<<maximumFloat<<" maximum_burial="<<maximumBurial<<'\n';
        return 15;
    }
    if(logs<2 || rocks<12 || !blocked) {
        std::cerr<<"FIRST_DIVERGENCE: insufficient authored coverage; rocks="<<rocks<<" logs="<<logs<<" controller/camera checks="<<blocked<<'\n';
        return 10;
    }
    if(rocks!=32)return 16;
    std::cout<<"Forest props PASS; rocks="<<rocks<<" fallen logs="<<logs<<" contained vertices="<<vertices
        <<" controller/camera checks="<<blocked<<" bottom checks="<<bottomChecks
        <<" maximum float="<<maximumFloat<<" maximum burial="<<maximumBurial<<'\n';
}
