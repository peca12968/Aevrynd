#include <Aevrynd/SkinnedRenderable3D.h>
#include <limits>

namespace Aevrynd {

SkinnedMotionHistorySample3D SkinnedMotionHistory3D::Advance(DrawList& draw,
    SkinPaletteRange3D currentPalette,Matrix4 currentTransform,bool reset){
    if(!draw.ContainsSkinPalette(currentPalette))return{};
    if(reset)Invalidate();
    SkinnedMotionHistorySample3D result{currentPalette,currentTransform,false};
    if(valid_&&previousJoints_.size()==currentPalette.jointCount&&previousCompatibilityId_&&previousCompatibilityId_==currentPalette.skeletonCompatibilityId){
        const SkinPaletteRange3D previousPalette=draw.AddSkinPalette(previousJoints_,previousCompatibilityId_);
        if(previousPalette){result.previousPalette=previousPalette;result.previousTransform=previousTransform_;result.motionValid=true;}
    }
    const auto first=draw.skinPaletteJoints.begin()+currentPalette.firstJoint;
    previousJoints_.assign(first,first+currentPalette.jointCount);
    previousTransform_=currentTransform;
    previousCompatibilityId_=currentPalette.skeletonCompatibilityId;
    valid_=true;
    return result;
}

ReusableMemoryStats SkinnedMotionHistory3D::MemoryStats()const noexcept{
    const std::size_t bytes=previousJoints_.capacity()*sizeof(SkinPaletteJoint3D);
    return{bytes,valid_?0:bytes,lastReleasedBytes_,trimCount_};
}

std::size_t SkinnedMotionHistory3D::TrimUnusedMemory(){
    if(valid_){lastReleasedBytes_=0;return 0;}
    const std::size_t before=previousJoints_.capacity()*sizeof(SkinPaletteJoint3D);
    std::vector<SkinPaletteJoint3D>{}.swap(previousJoints_);
    lastReleasedBytes_=before;
    ++trimCount_;
    return before;
}

bool SubmitSkinnedRenderable3D(DrawList& draw,SkinnedMeshHandle3D mesh,
    SkinPaletteRange3D palette,Matrix4 transform,std::uint32_t totalIndexCount,
    std::span<const SkinnedSubmesh3D> submeshes,
    std::span<const Material3D> materialBindings,SkinPaletteRange3D previousPalette,
    Matrix4 previousTransform,bool motionValid){
    if(!mesh||!mesh.skeletonCompatibilityId||!palette||!palette.skeletonCompatibilityId||mesh.skeletonCompatibilityId!=palette.skeletonCompatibilityId||!totalIndexCount||totalIndexCount%3||submeshes.empty()||
        materialBindings.empty()||submeshes.size()>DrawList::MaximumSkinnedMeshInstancesPerFrame-
            (std::min)(draw.skinnedMeshInstances.size(),DrawList::MaximumSkinnedMeshInstancesPerFrame)||
        palette.firstJoint>draw.skinPaletteJoints.size()||
        palette.jointCount>draw.skinPaletteJoints.size()-palette.firstJoint||
        (motionValid&&(!previousPalette.skeletonCompatibilityId||previousPalette.skeletonCompatibilityId!=palette.skeletonCompatibilityId)))return false;
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
            !std::isfinite(material.textureScale)||material.textureScale<=0||!std::isfinite(material.baseTextureScale)||!std::isfinite(material.detailTextureScale)||material.baseTextureScale<0||material.detailTextureScale<0||!std::isfinite(material.normalScale)||!std::isfinite(material.alphaCutoff))return false;
        expectedFirst+=submesh.indexCount;
    }
    if(expectedFirst!=totalIndexCount)return false;
    const std::size_t originalSize=draw.skinnedMeshInstances.size();
    for(const SkinnedSubmesh3D& submesh:submeshes){
        const Material3D& material=materialBindings[submesh.materialSlot];
        if(!draw.SkinnedMeshInstance(mesh,palette,transform,material.tint,material.surface,
            material.textures,material.textureScale,material.useUv,material.materialClass,
            material.castsShadow,DirectionalShadowAllCascades3D,submesh.firstIndex,
            submesh.indexCount,material.shadingModel,material.alphaMode,material.alphaCutoff,material.doubleSided,
            previousPalette,previousTransform,motionValid,material.normalScale,material.baseTextureScale,material.detailTextureScale)){
            draw.skinnedMeshInstances.resize(originalSize);
            return false;
        }
    }
    return true;
}

}
