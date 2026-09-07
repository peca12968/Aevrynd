#define main CharacterShapePreviewApplicationMain
#include "CharacterShapePreviewD3D12.cpp"
#undef main

int main(){
    DrawList draw;
    const std::array<SkinPaletteJoint3D,1> joints{};
    const auto palette=draw.AddSkinPalette(joints);
    Material3D material;
    material.shadingModel=CharacterShadingModel3D::Skin;
    material.normalScale=.37f;
    material.textureScale=1.f;
    material.baseTextureScale=2.f;
    material.detailTextureScale=7.f;
    material.textures={1};material.useUv=true;
    material.alphaMode=MaterialAlphaMode3D::Mask;material.alphaCutoff=.42f;material.doubleSided=true;
    if(!SubmitPreviewCharacterPart(draw,SkinnedMeshHandle3D{1,{},1},palette,Matrix4::Identity(),material,3,6))return 1;
    if(draw.skinnedMeshInstances.size()!=1||!draw.meshInstances.empty()){
        std::cerr<<"preview_character_submission_is_static: Skin shadingModel never reaches skinned submission\n";
        return 2;
    }
    const auto& instance=draw.skinnedMeshInstances.front();
    if(instance.shadingModel!=material.shadingModel||instance.normalScale!=.37f||
        instance.baseTextureScale!=2.f||instance.detailTextureScale!=7.f||
        instance.alphaMode!=MaterialAlphaMode3D::Mask||instance.alphaCutoff!=.42f||
        !instance.doubleSided||instance.firstIndex!=3||instance.indexCount!=6){
        std::cerr<<"preview_character_material_contract_lost\n";return 3;
    }
    std::cout<<"Preview GPU skinned material submission passed\n";
}
