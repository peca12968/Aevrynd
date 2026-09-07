#include <Aevrynd/CharacterShape3D.h>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <source_location>
using namespace Aevrynd;
int main(){
    Mesh3D base;base.vertices={{0,0,0,0,0},{1,0,0,0,0},{0,1,0,0,0}};base.indices={0,1,2};
    CharacterShapeDelta3D target{"jaw",{{0,0,0},{0,0,0},{0,0,.5f}},{0,1,2},-1,1};
    CharacterShapeWeight3D weight{"jaw",0};CharacterShapeResult3D output;std::string error;
    auto apply=[&](){return ApplyCharacterShape3D(base,std::span(&target,1),std::span(&weight,1),output,&error);};
    auto require=[](bool ok,const std::source_location at=std::source_location::current()){if(!ok)throw std::runtime_error("CharacterShape3D assertion failed at line "+std::to_string(at.line()));};
    try{
        base.vertices[0].normal=PackNormalSnorm8({0,.1f,.995f});
        require(apply()&&output.mesh.vertices[2].z==0&&output.requiresTangentRebuild);
        require(output.mesh.vertices[0].normal==base.vertices[0].normal);
        weight.value=1;require(apply()&&output.mesh.vertices[2].z==.5f);
        // 此頂點帶 authored 傾斜，預期為切平面逆轉置，不能用純幾何法線覆蓋。
        const auto normal=UnpackNormalSnorm8(output.mesh.vertices[0].normal);
        const auto authored=UnpackNormalSnorm8(base.vertices[0].normal);
        const float root=std::sqrt(1.25f);
        const auto expected=Normalize(Vec3{authored.x,authored.y/1.25f-.5f*authored.z/root,.5f*authored.y/1.25f+authored.z/root});
        require(std::abs(normal.y-expected.y)<.01f&&std::abs(normal.z-expected.z)<.01f);
        const auto geometric=UnpackNormalSnorm8(output.mesh.vertices[1].normal);require(geometric.y<-.4f&&geometric.z>.8f);
        require(apply()&&output.mesh.vertices[2].z==.5f);
        weight.value=-1;require(apply()&&output.mesh.vertices[2].z==-.5f);
        weight.value=std::numeric_limits<float>::quiet_NaN();require(!apply()&&output.mesh.vertices[2].z==-.5f);
        weight.value=2;require(!apply());weight.value=0;
        target.topology={0,2,1};require(!apply());target.topology=base.indices;
        target.offsets[0].x=std::numeric_limits<float>::infinity();require(!apply());target.offsets[0].x=0;
        target.offsets.pop_back();require(!apply());target.offsets.push_back({0,0,.5f});
        base.indices[2]=9;require(!apply());base.indices[2]=2;
        require(apply()&&output.mesh.vertices[2].z==0);
        auto second=target;second.name="waist";second.offsets[2]={0,.25f,0};
        std::array<CharacterShapeDelta3D,2> pair{target,second};
        std::array<CharacterShapeWeight3D,2> settings{{{"jaw",1},{"waist",1}}};
        require(ApplyCharacterShape3D(base,pair,settings,output,&error)&&output.mesh.vertices[2].z==.5f&&output.mesh.vertices[2].y==1.25f);
        settings[1].name="jaw";require(!ApplyCharacterShape3D(base,pair,settings,output,&error));
        target.offsets[2]={0,-1,0};weight.value=1;require(!apply()&&output.mesh.vertices[2].y==1.25f);
        require(!ApplyCharacterShape3D(output.mesh,{}, {},output,&error));
        const auto sphere=Mesh3D::CreateUnitSphere();require(ApplyCharacterShape3D(sphere,{}, {},output,&error));
        for(size_t i=0;i<sphere.vertices.size();++i)require(output.mesh.vertices[i].normal==sphere.vertices[i].normal);
        base.vertices={{0,0,0,0,0},{1,0,0,0,0},{0,1,0,0,0},{0,0,0,0,0},{0,1,0,0,0},{0,0,1,0,0}};base.indices={0,1,2,3,4,5};
        for(auto& vertex:base.vertices)vertex.normal=PackNormalSnorm8({.70710678f,0,.70710678f},.25f);
        CharacterShapeDelta3D move{"move",std::vector<Vec3>(6,{0,0,.125f}),base.indices};CharacterShapeWeight3D moveWeight{"move",1};
        require(ApplyCharacterShape3D(base,std::span(&move,1),std::span(&moveWeight,1),output,&error));
        for(size_t i=0;i<base.vertices.size();++i)require(output.mesh.vertices[i].normal==base.vertices[i].normal);
        base.vertices.resize(3);base.indices={0,1,2};
        std::array<CharacterShapeDelta3D,3> orderedTargets{{{"a",std::vector<Vec3>(3,{0,0,1e8f}),base.indices},{"b",std::vector<Vec3>(3,{0,0,-1e8f}),base.indices},{"c",std::vector<Vec3>(3,{0,0,1}),base.indices}}};
        std::array<CharacterShapeWeight3D,3> orderedWeights{{{"a",1},{"b",1},{"c",1}}};
        require(ApplyCharacterShape3D(base,orderedTargets,orderedWeights,output,&error)&&output.mesh.vertices[0].z==1);
        std::swap(orderedWeights[1],orderedWeights[2]);require(ApplyCharacterShape3D(base,orderedTargets,orderedWeights,output,&error)&&output.mesh.vertices[0].z==1);
        Mesh3D tinyMesh=base;tinyMesh.vertices[1].x=.001f;tinyMesh.vertices[2].y=.001f;
        require(ApplyCharacterShape3D(tinyMesh,{}, {},output,&error));
        CharacterShapeDelta3D smallDelta{"small",{{0,0,0},{0,0,0},{0,0,.0001f}},tinyMesh.indices};CharacterShapeWeight3D smallWeight{"small",1};
        require(ApplyCharacterShape3D(tinyMesh,std::span(&smallDelta,1),std::span(&smallWeight,1),output,&error));
        std::cout<<"CharacterShape3D passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<" "<<error<<"\n";return 1;}
}
