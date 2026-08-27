#include <Aevrynd/SkinnedRenderable3D.h>
#include <array>
#include <iostream>

using namespace Aevrynd;

namespace {
SkinPaletteJoint3D IdentityJoint(){
    return{Matrix4::Identity(),{Vec4{1,0,0,0},Vec4{0,1,0,0},Vec4{0,0,1,0}}};
}
}

int main(){
    DrawList draw(DrawListCapacityProfile::Minimal2D);
    const std::array<SkinPaletteJoint3D,1> joints{IdentityJoint()};
    const SkinPaletteRange3D palette=draw.AddSkinPalette(joints);
    const SkinnedMeshHandle3D mesh{7};
    if(draw.SkinnedMeshInstance(mesh,palette,Matrix4::Identity(),{1,1,1,1},{},{},.25f,true,MaterialClass3D::Surface,true,DirectionalShadowAllCascades3D,0,3,CharacterShadingModel3D::Default,MaterialAlphaMode3D::Mask,.5f,false)||draw.SkinnedMeshInstance(mesh,palette,Matrix4::Identity(),{1,1,1,1},{},{9},.25f,false,MaterialClass3D::Surface,true,DirectionalShadowAllCascades3D,0,3,CharacterShadingModel3D::Default,MaterialAlphaMode3D::Mask,.5f,false)||!draw.skinnedMeshInstances.empty()){std::cerr<<"Direct MASK validation was not atomic\n";return 11;}
    const std::array<SkinnedSubmesh3D,2> submeshes{{{0,3,1,0},{3,6,0,0}}};
    std::array<Material3D,2> materials{};
    materials[0].tint={1,0,0,1};materials[0].textures={11};materials[0].shadingModel=CharacterShadingModel3D::Skin;
    materials[1].tint={0,1,0,1};materials[1].textures={22};materials[1].useUv=true;materials[1].materialClass=MaterialClass3D::Foliage;materials[1].shadingModel=CharacterShadingModel3D::Hair;materials[1].alphaMode=MaterialAlphaMode3D::Mask;materials[1].alphaCutoff=.42f;materials[1].doubleSided=true;
    if(!SubmitSkinnedRenderable3D(draw,mesh,palette,Matrix4::Identity(),9,submeshes,materials)||
        draw.skinnedMeshInstances.size()!=2||draw.skinPaletteJoints.size()!=1||
        draw.skinnedMeshInstances[0].textures.value!=22||draw.skinnedMeshInstances[0].firstIndex||draw.skinnedMeshInstances[0].shadingModel!=CharacterShadingModel3D::Hair||draw.skinnedMeshInstances[0].alphaMode!=MaterialAlphaMode3D::Mask||std::abs(draw.skinnedMeshInstances[0].alphaCutoff-.42f)>.0001f||!draw.skinnedMeshInstances[0].doubleSided||
        draw.skinnedMeshInstances[1].textures.value!=11||draw.skinnedMeshInstances[1].firstIndex!=3||
        draw.skinnedMeshInstances[1].indexCount!=6||draw.skinnedMeshInstances[1].shadingModel!=CharacterShadingModel3D::Skin){std::cerr<<"Multi-material submission failed\n";return 1;}
    const uint32_t skinPacked=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,.25f,CharacterShadingModel3D::Skin),hairPacked=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,.25f,CharacterShadingModel3D::Hair),eyePacked=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,.25f,CharacterShadingModel3D::Eye),defaultPacked=PackSkinnedMeshInstanceData3D(false,false,MaterialClass3D::Surface,.25f),invalidPacked=PackSkinnedMeshInstanceData3D(false,false,MaterialClass3D::Surface,.25f,CharacterShadingModel3D(7));if(UnpackSkinnedCharacterShadingModel3D(skinPacked)!=CharacterShadingModel3D::Skin||UnpackSkinnedCharacterShadingModel3D(hairPacked)!=CharacterShadingModel3D::Hair||UnpackSkinnedCharacterShadingModel3D(eyePacked)!=CharacterShadingModel3D::Eye||UnpackSkinnedCharacterShadingModel3D(defaultPacked)!=CharacterShadingModel3D::Default||UnpackSkinnedCharacterShadingModel3D(invalidPacked)!=CharacterShadingModel3D::Default||(skinPacked&3u)!=3u){std::cerr<<"Skinned shading model packing failed\n";return 7;}
    const uint32_t alphaPacked=PackSkinnedPaletteMaterialData3D(4095,MaterialAlphaMode3D::Mask,.42f,true);if(UnpackSkinnedPaletteBase3D(alphaPacked)!=4095||!UnpackSkinnedAlphaMasked3D(alphaPacked)||!UnpackSkinnedDoubleSided3D(alphaPacked)||std::abs(UnpackSkinnedAlphaCutoff3D(alphaPacked)-.42f)>1.f/255.f){std::cerr<<"Skinned alpha packing failed\n";return 8;}
    const std::size_t accepted=draw.skinnedMeshInstances.size();
    auto expectAtomicFailure=[&](std::span<const SkinnedSubmesh3D> ranges,std::span<const Material3D> bindings,std::uint32_t indices){
        const bool acceptedInvalid=SubmitSkinnedRenderable3D(draw,mesh,palette,Matrix4::Identity(),indices,ranges,bindings);
        return !acceptedInvalid&&draw.skinnedMeshInstances.size()==accepted&&draw.skinPaletteJoints.size()==1;
    };
    const std::array<SkinnedSubmesh3D,2> gap{{{0,3,0,0},{6,3,1,0}}},overlap{{{0,6,0,0},{3,6,1,0}}},badSlot{{{0,3,2,0},{3,6,0,0}}},badAlignment{{{0,4,0,0},{4,5,1,0}}};
    if(!expectAtomicFailure(gap,materials,9)||!expectAtomicFailure(overlap,materials,9)||
        !expectAtomicFailure(badSlot,materials,9)||!expectAtomicFailure(badAlignment,materials,9)){
        std::cerr<<"Range or slot failure was not atomic\n";return 2;
    }
    materials[1].materialClass=MaterialClass3D::Water;
    if(!expectAtomicFailure(submeshes,materials,9)){std::cerr<<"Unsupported material was accepted\n";return 3;}
    materials[1].materialClass=MaterialClass3D::Foliage;
    materials[1].alphaMode=MaterialAlphaMode3D::Blend;
    if(!expectAtomicFailure(submeshes,materials,9)){std::cerr<<"BLEND material was accepted by opaque skinned submission\n";return 9;}
    materials[1].alphaMode=MaterialAlphaMode3D::Mask;materials[1].textures={};
    if(!expectAtomicFailure(submeshes,materials,9)){std::cerr<<"MASK material without a base texture was accepted\n";return 10;}
    materials[1].textures={22};
    DrawList bounded(DrawListCapacityProfile::Minimal2D);const SkinPaletteRange3D boundedPalette=bounded.AddSkinPalette(joints);
    const std::array<SkinnedSubmesh3D,1> one{{{0,3,0,0}}};
    for(std::size_t index=0;index<DrawList::MaximumSkinnedMeshInstancesPerFrame-1;++index)
        if(!SubmitSkinnedRenderable3D(bounded,mesh,boundedPalette,Matrix4::Identity(),3,one,std::span<const Material3D>{materials.data(),1}))return 4;
    const std::array<SkinnedSubmesh3D,2> two{{{0,3,0,0},{3,3,1,0}}};
    const std::size_t before=bounded.skinnedMeshInstances.size();
    if(SubmitSkinnedRenderable3D(bounded,mesh,boundedPalette,Matrix4::Identity(),6,two,materials)||
        bounded.skinnedMeshInstances.size()!=before||bounded.skinPaletteJoints.size()!=1){
        std::cerr<<"Bounded submission was partial\n";return 5;
    }
    if(!SubmitSkinnedRenderable3D(bounded,mesh,boundedPalette,Matrix4::Identity(),3,one,std::span<const Material3D>{materials.data(),1})||
        bounded.skinnedMeshInstances.size()!=DrawList::MaximumSkinnedMeshInstancesPerFrame||
        bounded.skinPaletteJoints.size()!=1){std::cerr<<"Final bounded slot failed\n";return 6;}
    std::cout<<"submeshes=2 bounded="<<bounded.skinnedMeshInstances.size()<<" paletteJoints="<<bounded.skinPaletteJoints.size()<<'\n';
}
