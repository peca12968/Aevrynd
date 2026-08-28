#include <Aevrynd/SkinnedRenderable3D.h>
#include <limits>
#include <new>

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
    Matrix4 previousTransform,bool motionValid,std::uint32_t submissionGroupId){
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
            previousPalette,previousTransform,motionValid,material.normalScale,material.baseTextureScale,material.detailTextureScale,submissionGroupId)){
            draw.skinnedMeshInstances.resize(originalSize);
            return false;
        }
    }
    return true;
}

bool SubmitSkinnedRenderableGroup3D(DrawList& draw,SkinPaletteRange3D palette,
    std::span<const SkinnedRenderablePart3D> parts,SkinPaletteRange3D previousPalette,
    bool motionValid,SkinnedRenderableGroupStats3D* stats){
    auto reject=[&](SkinnedRenderableGroupFailure3D failure){if(stats){++stats->rejectedGroups;stats->lastFailure=failure;switch(failure){case SkinnedRenderableGroupFailure3D::UnknownSkeleton:++stats->unknownSkeletonRejections;break;case SkinnedRenderableGroupFailure3D::SkeletonMismatch:++stats->skeletonMismatchRejections;break;case SkinnedRenderableGroupFailure3D::CurrentPaletteMismatch:++stats->currentPaletteMismatchRejections;break;case SkinnedRenderableGroupFailure3D::PreviousPaletteMismatch:++stats->previousPaletteMismatchRejections;break;case SkinnedRenderableGroupFailure3D::Range:++stats->rangeRejections;break;case SkinnedRenderableGroupFailure3D::Material:++stats->materialRejections;break;case SkinnedRenderableGroupFailure3D::Capacity:++stats->capacityRejections;break;default:break;}}return false;};
    if(parts.empty())return reject(SkinnedRenderableGroupFailure3D::Empty);
    if(!palette)return reject(SkinnedRenderableGroupFailure3D::UnknownSkeleton);
    if(!draw.ContainsSkinPalette(palette))return reject(SkinnedRenderableGroupFailure3D::CurrentPaletteMismatch);
    if(!palette.skeletonCompatibilityId)return reject(SkinnedRenderableGroupFailure3D::UnknownSkeleton);
    if(motionValid&&(!previousPalette||!draw.ContainsSkinPalette(previousPalette)||!previousPalette.skeletonCompatibilityId))return reject(SkinnedRenderableGroupFailure3D::PreviousPaletteMismatch);
    if(motionValid&&(previousPalette.skeletonCompatibilityId!=palette.skeletonCompatibilityId||previousPalette.jointCount!=palette.jointCount))return reject(SkinnedRenderableGroupFailure3D::PreviousPaletteMismatch);
    const auto finiteMatrix=[](const Matrix4& matrix){for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return false;return true;};
    std::size_t requiredInstances{};
    for(const SkinnedRenderablePart3D& part:parts){
        if(!part.mesh||!part.mesh.skeletonCompatibilityId||!part.mesh.jointCount)return reject(SkinnedRenderableGroupFailure3D::UnknownSkeleton);
        if(!finiteMatrix(part.transform)||(motionValid&&!finiteMatrix(part.previousTransform)))return reject(SkinnedRenderableGroupFailure3D::Range);
        if(part.mesh.skeletonCompatibilityId!=palette.skeletonCompatibilityId||part.mesh.jointCount!=palette.jointCount)return reject(SkinnedRenderableGroupFailure3D::SkeletonMismatch);
        if(!part.totalIndexCount||part.totalIndexCount%3||part.submeshes.empty()||part.materialBindings.empty())return reject(SkinnedRenderableGroupFailure3D::Range);
        if(part.submeshes.size()>DrawList::MaximumSkinnedMeshInstancesPerFrame-requiredInstances)return reject(SkinnedRenderableGroupFailure3D::Capacity);
        requiredInstances+=part.submeshes.size();std::uint32_t expectedFirst{};
        for(const SkinnedSubmesh3D& submesh:part.submeshes){
            if(!submesh.indexCount||submesh.firstIndex!=expectedFirst||submesh.firstIndex%3||submesh.indexCount%3||submesh.firstIndex>part.totalIndexCount||submesh.indexCount>part.totalIndexCount-submesh.firstIndex||submesh.materialSlot>=part.materialBindings.size())return reject(SkinnedRenderableGroupFailure3D::Range);
            const Material3D& material=part.materialBindings[submesh.materialSlot];const MaterialClass3D materialClass=NormalizeMaterialClass3D(material.materialClass);const MaterialAlphaMode3D alphaMode=NormalizeMaterialAlphaMode3D(material.alphaMode);
            if(materialClass==MaterialClass3D::Water||materialClass==MaterialClass3D::Terrain||alphaMode==MaterialAlphaMode3D::Blend||(alphaMode==MaterialAlphaMode3D::Mask&&(!material.textures||!material.useUv))||!std::isfinite(material.textureScale)||material.textureScale<=0||!std::isfinite(material.baseTextureScale)||material.baseTextureScale<0||!std::isfinite(material.detailTextureScale)||material.detailTextureScale<0||!std::isfinite(material.normalScale)||!std::isfinite(material.alphaCutoff))return reject(SkinnedRenderableGroupFailure3D::Material);
            expectedFirst+=submesh.indexCount;
        }
        if(expectedFirst!=part.totalIndexCount)return reject(SkinnedRenderableGroupFailure3D::Range);
    }
    if(requiredInstances>DrawList::MaximumSkinnedMeshInstancesPerFrame-(std::min)(draw.skinnedMeshInstances.size(),DrawList::MaximumSkinnedMeshInstancesPerFrame))return reject(SkinnedRenderableGroupFailure3D::Capacity);
    const std::size_t originalSize=draw.skinnedMeshInstances.size(),requiredTotal=originalSize+requiredInstances;try{if(draw.skinnedMeshInstances.capacity()<requiredTotal){const std::size_t doubled=(std::max)(std::size_t(16),(std::min)(DrawList::MaximumSkinnedMeshInstancesPerFrame,draw.skinnedMeshInstances.capacity()*2));draw.skinnedMeshInstances.reserve((std::max)(requiredTotal,doubled));}}catch(const std::bad_alloc&){return reject(SkinnedRenderableGroupFailure3D::Capacity);}const std::uint32_t submissionGroupId=draw.AllocateSkinnedSubmissionGroupId3D();
    for(const SkinnedRenderablePart3D& part:parts)if(!SubmitSkinnedRenderable3D(draw,part.mesh,palette,part.transform,part.totalIndexCount,part.submeshes,part.materialBindings,previousPalette,part.previousTransform,motionValid,submissionGroupId)){draw.skinnedMeshInstances.resize(originalSize);return reject(SkinnedRenderableGroupFailure3D::Capacity);}
    if(stats){++stats->acceptedGroups;stats->acceptedParts+=parts.size();stats->acceptedInstances+=requiredInstances;stats->lastFailure=SkinnedRenderableGroupFailure3D::None;}
    return true;
}

}
