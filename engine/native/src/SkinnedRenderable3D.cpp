#include <Aevrynd/SkinnedRenderable3D.h>
#include <limits>

namespace Aevrynd {

bool SubmitSkinnedRenderable3D(DrawList& draw,SkinnedMeshHandle3D mesh,
    SkinPaletteRange3D palette,Matrix4 transform,std::uint32_t totalIndexCount,
    std::span<const SkinnedSubmesh3D> submeshes,
    std::span<const Material3D> materialBindings){
    if(!mesh||!palette||!totalIndexCount||totalIndexCount%3||submeshes.empty()||
        materialBindings.empty()||submeshes.size()>DrawList::MaximumSkinnedMeshInstancesPerFrame-
            (std::min)(draw.skinnedMeshInstances.size(),DrawList::MaximumSkinnedMeshInstancesPerFrame)||
        palette.firstJoint>draw.skinPaletteJoints.size()||
        palette.jointCount>draw.skinPaletteJoints.size()-palette.firstJoint)return false;
    std::uint32_t expectedFirst{};
    for(const SkinnedSubmesh3D& submesh:submeshes){
        if(!submesh.indexCount||submesh.firstIndex!=expectedFirst||submesh.firstIndex%3||
            submesh.indexCount%3||submesh.indexCount>totalIndexCount-submesh.firstIndex||
            submesh.materialSlot>=materialBindings.size())return false;
        const Material3D& material=materialBindings[submesh.materialSlot];
        const MaterialClass3D materialClass=NormalizeMaterialClass3D(material.materialClass);
        if(materialClass==MaterialClass3D::Water||materialClass==MaterialClass3D::Terrain||
            NormalizeMaterialAlphaMode3D(material.alphaMode)==MaterialAlphaMode3D::Blend||
            (NormalizeMaterialAlphaMode3D(material.alphaMode)==MaterialAlphaMode3D::Mask&&(!material.textures||!material.useUv))||
            !std::isfinite(material.textureScale)||material.textureScale<=0||!std::isfinite(material.alphaCutoff))return false;
        expectedFirst+=submesh.indexCount;
    }
    if(expectedFirst!=totalIndexCount)return false;
    const std::size_t originalSize=draw.skinnedMeshInstances.size();
    for(const SkinnedSubmesh3D& submesh:submeshes){
        const Material3D& material=materialBindings[submesh.materialSlot];
        if(!draw.SkinnedMeshInstance(mesh,palette,transform,material.tint,material.surface,
            material.textures,material.textureScale,material.useUv,material.materialClass,
            material.castsShadow,DirectionalShadowAllCascades3D,submesh.firstIndex,
            submesh.indexCount,material.shadingModel,material.alphaMode,material.alphaCutoff,material.doubleSided)){
            draw.skinnedMeshInstances.resize(originalSize);
            return false;
        }
    }
    return true;
}

}
