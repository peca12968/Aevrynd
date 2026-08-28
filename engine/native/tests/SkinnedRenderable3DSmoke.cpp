#include <Aevrynd/SkinnedRenderable3D.h>
#include <array>
#include <iostream>
#include <limits>

using namespace Aevrynd;

namespace {
SkinPaletteJoint3D IdentityJoint(){
    return{Matrix4::Identity(),{Vec4{1,0,0,0},Vec4{0,1,0,0},Vec4{0,0,1,0}}};
}
}

int main(){
    DrawList draw(DrawListCapacityProfile::Minimal2D);
    const std::array<SkinPaletteJoint3D,1> joints{IdentityJoint()};
    constexpr SkeletonCompatibilityId3D compatibility{0x8d5b7f140d11a931ull};
    const SkinPaletteRange3D palette=draw.AddSkinPalette(joints,compatibility);
    const SkinnedMeshHandle3D mesh{7,compatibility};
    if(draw.SkinnedMeshInstance(mesh,palette,Matrix4::Identity(),{1,1,1,1},{},{},.25f,true,MaterialClass3D::Surface,true,DirectionalShadowAllCascades3D,0,3,CharacterShadingModel3D::Default,MaterialAlphaMode3D::Mask,.5f,false)||draw.SkinnedMeshInstance(mesh,palette,Matrix4::Identity(),{1,1,1,1},{},{9},.25f,false,MaterialClass3D::Surface,true,DirectionalShadowAllCascades3D,0,3,CharacterShadingModel3D::Default,MaterialAlphaMode3D::Mask,.5f,false)||!draw.skinnedMeshInstances.empty()){std::cerr<<"Direct MASK validation was not atomic\n";return 11;}
    const std::array<SkinnedSubmesh3D,2> submeshes{{{0,3,1,0},{3,6,0,0}}};
    std::array<Material3D,2> materials{};
    materials[0].tint={1,0,0,1};materials[0].textures={11};materials[0].normalScale=.625f;materials[0].shadingModel=CharacterShadingModel3D::Skin;
    materials[1].tint={0,1,0,1};materials[1].textures={22};materials[1].normalScale=-2.5f;materials[1].baseTextureScale=1.f;materials[1].detailTextureScale=6.f;materials[1].useUv=true;materials[1].materialClass=MaterialClass3D::Foliage;materials[1].shadingModel=CharacterShadingModel3D::Hair;materials[1].alphaMode=MaterialAlphaMode3D::Mask;materials[1].alphaCutoff=.42f;materials[1].doubleSided=true;
    if(!SubmitSkinnedRenderable3D(draw,mesh,palette,Matrix4::Identity(),9,submeshes,materials)||
        draw.skinnedMeshInstances.size()!=2||draw.skinPaletteJoints.size()!=1||
        draw.skinnedMeshInstances[0].textures.value!=22||draw.skinnedMeshInstances[0].firstIndex||draw.skinnedMeshInstances[0].shadingModel!=CharacterShadingModel3D::Hair||draw.skinnedMeshInstances[0].alphaMode!=MaterialAlphaMode3D::Mask||std::abs(draw.skinnedMeshInstances[0].alphaCutoff-.42f)>.0001f||std::abs(draw.skinnedMeshInstances[0].normalScale+2.5f)>.0001f||draw.skinnedMeshInstances[0].baseTextureScale!=1.f||draw.skinnedMeshInstances[0].detailTextureScale!=6.f||!draw.skinnedMeshInstances[0].doubleSided||
        draw.skinnedMeshInstances[1].textures.value!=11||draw.skinnedMeshInstances[1].firstIndex!=3||
        draw.skinnedMeshInstances[1].indexCount!=6||draw.skinnedMeshInstances[1].shadingModel!=CharacterShadingModel3D::Skin){std::cerr<<"Multi-material submission failed\n";return 1;}
    const uint32_t skinPacked=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,1.f,4.f,CharacterShadingModel3D::Skin),hairPacked=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,1.f,2.f,CharacterShadingModel3D::Hair),eyePacked=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,1.f,1.f,CharacterShadingModel3D::Eye),clothPacked=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,1.f,6.f,CharacterShadingModel3D::Cloth),defaultPacked=PackSkinnedMeshInstanceData3D(false,false,MaterialClass3D::Surface,.25f),invalidPacked=PackSkinnedMeshInstanceData3D(false,false,MaterialClass3D::Surface,.25f,CharacterShadingModel3D(7));if(UnpackSkinnedCharacterShadingModel3D(skinPacked)!=CharacterShadingModel3D::Skin||UnpackSkinnedCharacterShadingModel3D(hairPacked)!=CharacterShadingModel3D::Hair||UnpackSkinnedCharacterShadingModel3D(eyePacked)!=CharacterShadingModel3D::Eye||UnpackSkinnedCharacterShadingModel3D(clothPacked)!=CharacterShadingModel3D::Cloth||UnpackSkinnedCharacterShadingModel3D(defaultPacked)!=CharacterShadingModel3D::Default||UnpackSkinnedCharacterShadingModel3D(invalidPacked)!=CharacterShadingModel3D::Default||(skinPacked&3u)!=3u||std::abs(UnpackSkinnedTextureScale3D(skinPacked,16)-1.f)>16.f/255.f||std::abs(UnpackSkinnedTextureScale3D(skinPacked,24)-4.f)>16.f/255.f||std::abs(UnpackSkinnedTextureScale3D(clothPacked,24)-6.f)>16.f/255.f){std::cerr<<"Skinned shading model or split texture scale packing failed\n";return 7;}
    for(uint32_t code=0;code<256;++code){const float scale=float(code)*(16.f/255.f);const uint32_t packed=PackSkinnedMeshInstanceData3D(true,true,MaterialClass3D::Surface,scale,16.f-scale,CharacterShadingModel3D::Cloth);if(((packed>>16)&255u)!=code||((packed>>24)&255u)!=(255u-code)){std::cerr<<"Split texture scale exhaustive packing failed\n";return 18;}}
    const uint32_t alphaPacked=PackSkinnedPaletteMaterialData3D(4095,MaterialAlphaMode3D::Mask,.42f,true,-2.5f),defaultNormalPacked=PackSkinnedPaletteMaterialData3D(0,MaterialAlphaMode3D::Opaque,.5f,false);if(UnpackSkinnedPaletteBase3D(alphaPacked)!=4095||!UnpackSkinnedAlphaMasked3D(alphaPacked)||!UnpackSkinnedDoubleSided3D(alphaPacked)||std::abs(UnpackSkinnedAlphaCutoff3D(alphaPacked)-.42f)>1.f/255.f||UnpackSkinnedNormalScale3D(alphaPacked)!=-2.5f||UnpackSkinnedNormalScale3D(defaultNormalPacked)!=1.f){std::cerr<<"Skinned material packing failed\n";return 8;}
    for(uint32_t code=0;code<1024;++code){const int32_t signedCode=code>=512?int32_t(code)-1024:int32_t(code);const float decoded=float(signedCode)/32.f;const uint32_t repacked=PackSkinnedPaletteMaterialData3D(0,MaterialAlphaMode3D::Opaque,.5f,false,decoded);if(((repacked>>SkinnedNormalScaleShift3D)&1023u)!=code||UnpackSkinnedNormalScale3D(repacked)!=decoded){std::cerr<<"Skinned normal scale exhaustive packing failed\n";return 16;}}
    const uint32_t lowPacked=PackSkinnedPaletteMaterialData3D(0,MaterialAlphaMode3D::Opaque,.5f,false,-99.f),highPacked=PackSkinnedPaletteMaterialData3D(0,MaterialAlphaMode3D::Opaque,.5f,false,99.f),nanPacked=PackSkinnedPaletteMaterialData3D(0,MaterialAlphaMode3D::Opaque,.5f,false,(std::numeric_limits<float>::quiet_NaN)());if(UnpackSkinnedNormalScale3D(lowPacked)!=-16.f||UnpackSkinnedNormalScale3D(highPacked)!=15.96875f||UnpackSkinnedNormalScale3D(nanPacked)!=1.f){std::cerr<<"Skinned normal scale saturation failed\n";return 17;}
    const std::size_t accepted=draw.skinnedMeshInstances.size();
    const SkinPaletteRange3D incompatible=draw.AddSkinPalette(joints,{compatibility.value+1});
    if(SubmitSkinnedRenderable3D(draw,mesh,incompatible,Matrix4::Identity(),9,submeshes,materials)||
        SubmitSkinnedRenderable3D(draw,{mesh.value,{}},palette,Matrix4::Identity(),9,submeshes,materials)||
        SubmitSkinnedRenderable3D(draw,mesh,palette,Matrix4::Identity(),9,submeshes,materials,incompatible,Matrix4::Identity(),true)||
        draw.skinnedMeshInstances.size()!=accepted){std::cerr<<"Skeleton compatibility rejection was not atomic\n";return 19;}
    const std::size_t acceptedPaletteJoints=draw.skinPaletteJoints.size();
    auto expectAtomicFailure=[&](std::span<const SkinnedSubmesh3D> ranges,std::span<const Material3D> bindings,std::uint32_t indices){
        const bool acceptedInvalid=SubmitSkinnedRenderable3D(draw,mesh,palette,Matrix4::Identity(),indices,ranges,bindings);
        return !acceptedInvalid&&draw.skinnedMeshInstances.size()==accepted&&draw.skinPaletteJoints.size()==acceptedPaletteJoints;
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
    DrawList bounded(DrawListCapacityProfile::Minimal2D);const SkinPaletteRange3D boundedPalette=bounded.AddSkinPalette(joints,compatibility);
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
    SkinnedMotionHistory3D history;DrawList firstFrame(DrawListCapacityProfile::Minimal2D);const auto firstCurrent=firstFrame.AddSkinPalette(joints,compatibility);const Matrix4 firstTransform=CreateTranslationMatrix({.1f,0,0});const auto firstHistory=history.Advance(firstFrame,firstCurrent,firstTransform);if(!history.Valid()||firstHistory.motionValid||firstHistory.previousPalette!=firstCurrent||history.MemoryStats().retainedCapacityBytes<sizeof(SkinPaletteJoint3D)){std::cerr<<"Initial skinned history fallback failed\n";return 12;}
    DrawList secondFrame(DrawListCapacityProfile::Minimal2D);auto movedJoints=joints;movedJoints[0].skin=CreateTranslationMatrix({.2f,0,0});const auto secondCurrent=secondFrame.AddSkinPalette(movedJoints,compatibility);const Matrix4 secondTransform=CreateTranslationMatrix({.3f,0,0});const auto secondHistory=history.Advance(secondFrame,secondCurrent,secondTransform);if(!secondHistory.motionValid||secondHistory.previousPalette==secondCurrent||secondFrame.skinPaletteJoints.size()!=2||std::abs(secondFrame.skinPaletteJoints[secondHistory.previousPalette.firstJoint].skin[0][3])>.0001f||std::abs(secondHistory.previousTransform[0][3]-.1f)>.0001f||!SubmitSkinnedRenderable3D(secondFrame,mesh,secondCurrent,secondTransform,3,one,std::span<const Material3D>{materials.data(),1},secondHistory.previousPalette,secondHistory.previousTransform,secondHistory.motionValid)||!secondFrame.skinnedMeshInstances.back().motionValid){std::cerr<<"Continuous skinned history submission failed\n";return 13;}
    const auto stalePalette=secondCurrent;secondFrame.Clear();if(history.Advance(secondFrame,stalePalette,Matrix4::Identity()).previousPalette){std::cerr<<"Stale skinned history palette was accepted\n";return 14;}history.Invalidate();const std::size_t reclaimable=history.MemoryStats().reclaimableCapacityBytes;if(!reclaimable||history.TrimUnusedMemory()!=reclaimable||history.MemoryStats().retainedCapacityBytes){std::cerr<<"Skinned history memory trim failed\n";return 15;}
    std::cout<<"submeshes=2 bounded="<<bounded.skinnedMeshInstances.size()<<" paletteJoints="<<bounded.skinPaletteJoints.size()<<'\n';
}
