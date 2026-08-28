#include <Aevrynd/SkinnedRenderable3D.h>
#include <Aevrynd/Engine.h>
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
    bool motionValid,SkinnedRenderableGroupStats3D* stats,SkinnedSubmissionReceipt3D* receipt){
    auto reject=[&](SkinnedRenderableGroupFailure3D failure){if(stats){++stats->rejectedGroups;stats->lastFailure=failure;switch(failure){case SkinnedRenderableGroupFailure3D::InvalidPartKey:++stats->invalidPartKeyRejections;break;case SkinnedRenderableGroupFailure3D::DuplicatePartKey:++stats->duplicatePartKeyRejections;break;case SkinnedRenderableGroupFailure3D::UnknownSkeleton:++stats->unknownSkeletonRejections;break;case SkinnedRenderableGroupFailure3D::SkeletonMismatch:++stats->skeletonMismatchRejections;break;case SkinnedRenderableGroupFailure3D::CurrentPaletteMismatch:++stats->currentPaletteMismatchRejections;break;case SkinnedRenderableGroupFailure3D::PreviousPaletteMismatch:++stats->previousPaletteMismatchRejections;break;case SkinnedRenderableGroupFailure3D::Range:++stats->rangeRejections;break;case SkinnedRenderableGroupFailure3D::Material:++stats->materialRejections;break;case SkinnedRenderableGroupFailure3D::Capacity:++stats->capacityRejections;break;default:break;}}return false;};
    if(parts.empty())return reject(SkinnedRenderableGroupFailure3D::Empty);
    if(!palette)return reject(SkinnedRenderableGroupFailure3D::UnknownSkeleton);
    if(!draw.ContainsSkinPalette(palette))return reject(SkinnedRenderableGroupFailure3D::CurrentPaletteMismatch);
    if(!palette.skeletonCompatibilityId)return reject(SkinnedRenderableGroupFailure3D::UnknownSkeleton);
    bool anyMotion=motionValid;for(const auto& part:parts)anyMotion=anyMotion||part.motionValid;
    if(anyMotion&&(!previousPalette||!draw.ContainsSkinPalette(previousPalette)||!previousPalette.skeletonCompatibilityId))return reject(SkinnedRenderableGroupFailure3D::PreviousPaletteMismatch);
    if(anyMotion&&(previousPalette.skeletonCompatibilityId!=palette.skeletonCompatibilityId||previousPalette.jointCount!=palette.jointCount))return reject(SkinnedRenderableGroupFailure3D::PreviousPaletteMismatch);
    const auto finiteMatrix=[](const Matrix4& matrix){for(const auto& row:matrix.values)for(float value:row)if(!std::isfinite(value))return false;return true;};
    std::size_t requiredInstances{};
    for(std::size_t partIndex=0;partIndex<parts.size();++partIndex){const SkinnedRenderablePart3D& part=parts[partIndex];
        if(!part.mesh||!part.mesh.skeletonCompatibilityId||!part.mesh.jointCount)return reject(SkinnedRenderableGroupFailure3D::UnknownSkeleton);
        if(!finiteMatrix(part.transform)||((motionValid||part.motionValid)&&!finiteMatrix(part.previousTransform)))return reject(SkinnedRenderableGroupFailure3D::Range);
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
    const std::size_t originalSize=draw.skinnedMeshInstances.size(),requiredTotal=originalSize+requiredInstances;try{if(draw.skinnedMeshInstances.capacity()<requiredTotal){const std::size_t doubled=(std::max)(std::size_t(16),(std::min)(DrawList::MaximumSkinnedMeshInstancesPerFrame,draw.skinnedMeshInstances.capacity()*2));draw.skinnedMeshInstances.reserve((std::max)(requiredTotal,doubled));}}catch(const std::bad_alloc&){return reject(SkinnedRenderableGroupFailure3D::Capacity);}const std::uint64_t submissionNonce=draw.AllocateSkinnedSubmissionNonce3D();if(!submissionNonce)return reject(SkinnedRenderableGroupFailure3D::Capacity);const std::uint32_t submissionGroupId=draw.AllocateSkinnedSubmissionGroupId3D();
    for(const SkinnedRenderablePart3D& part:parts)if(!SubmitSkinnedRenderable3D(draw,part.mesh,palette,part.transform,part.totalIndexCount,part.submeshes,part.materialBindings,previousPalette,part.previousTransform,motionValid||part.motionValid,submissionGroupId)){draw.skinnedMeshInstances.resize(originalSize);return reject(SkinnedRenderableGroupFailure3D::Capacity);}
    uint64_t resourceIdentity=0xcbf29ce484222325ull;for(const auto& part:parts){resourceIdentity^=part.mesh.resourceIdentity;resourceIdentity*=0x100000001b3ull;}const SkinnedSubmissionReceipt3D completedReceipt{submissionNonce,draw.Generation3D(),submissionGroupId,resourceIdentity?resourceIdentity:1};for(std::size_t index=originalSize;index<draw.skinnedMeshInstances.size();++index){draw.skinnedMeshInstances[index].submissionDrawGeneration=completedReceipt.drawGeneration;draw.skinnedMeshInstances[index].submissionNonce=completedReceipt.submissionNonce;draw.skinnedMeshInstances[index].submissionResourceIdentity=completedReceipt.resourceIdentity;}if(receipt)*receipt=completedReceipt;
    if(stats){++stats->acceptedGroups;stats->acceptedParts+=parts.size();stats->acceptedInstances+=requiredInstances;stats->lastFailure=SkinnedRenderableGroupFailure3D::None;}
    return true;
}

bool SkinnedRenderableGroupHistory3D::Submit(DrawList& draw,SkinPaletteRange3D currentPalette,
    std::span<const SkinnedRenderablePart3D> parts,bool reset,SkinnedRenderableGroupStats3D* stats){
    if(reset){motionReset_=true;if(pending_){invalidateRequested_=true;return false;}}
    if(pending_)return false;
    const auto reject=[&](SkinnedRenderableGroupFailure3D failure){if(stats){++stats->rejectedGroups;stats->lastFailure=failure;switch(failure){case SkinnedRenderableGroupFailure3D::InvalidPartKey:++stats->invalidPartKeyRejections;break;case SkinnedRenderableGroupFailure3D::DuplicatePartKey:++stats->duplicatePartKeyRejections;break;case SkinnedRenderableGroupFailure3D::UnknownSkeleton:++stats->unknownSkeletonRejections;break;case SkinnedRenderableGroupFailure3D::SkeletonMismatch:++stats->skeletonMismatchRejections;break;case SkinnedRenderableGroupFailure3D::CurrentPaletteMismatch:++stats->currentPaletteMismatchRejections;break;case SkinnedRenderableGroupFailure3D::PreviousPaletteMismatch:++stats->previousPaletteMismatchRejections;break;case SkinnedRenderableGroupFailure3D::Range:++stats->rangeRejections;break;case SkinnedRenderableGroupFailure3D::Material:++stats->materialRejections;break;case SkinnedRenderableGroupFailure3D::Capacity:++stats->capacityRejections;break;default:break;}}return false;};
    if(parts.empty())return reject(SkinnedRenderableGroupFailure3D::Empty);
    if(parts.size()>MaximumParts)return reject(SkinnedRenderableGroupFailure3D::Capacity);
    if(!draw.ContainsSkinPalette(currentPalette)||!currentPalette.skeletonCompatibilityId)return reject(SkinnedRenderableGroupFailure3D::CurrentPaletteMismatch);
    std::array<SkinnedRenderablePart3D,MaximumParts> submitParts{};
    for(std::size_t index=0;index<parts.size();++index){
        if(!parts[index].key)return reject(SkinnedRenderableGroupFailure3D::InvalidPartKey);
        for(std::size_t prior=0;prior<index;++prior)if(parts[prior].key==parts[index].key)return reject(SkinnedRenderableGroupFailure3D::DuplicatePartKey);
        pendingParts_[index]={parts[index].key,parts[index].mesh,parts[index].transform};
        submitParts[index]=parts[index];submitParts[index].motionValid=false;
    }
    try{const auto first=draw.skinPaletteJoints.begin()+currentPalette.firstJoint;pendingJoints_.assign(first,first+currentPalette.jointCount);}catch(const std::bad_alloc&){return reject(SkinnedRenderableGroupFailure3D::Capacity);}
    const auto paletteCheckpoint=draw.SkinPaletteCheckpoint();const std::size_t instanceSize=draw.skinnedMeshInstances.size();
    SkinPaletteRange3D previousPalette{};
    const bool paletteCompatible=!motionReset_&&valid_&&previousCompatibilityId_==currentPalette.skeletonCompatibilityId&&previousJoints_.size()==currentPalette.jointCount;
    if(paletteCompatible){try{previousPalette=draw.AddSkinPalette(previousJoints_,previousCompatibilityId_);}catch(const std::bad_alloc&){draw.RestoreSkinPaletteCheckpoint(paletteCheckpoint);return reject(SkinnedRenderableGroupFailure3D::Capacity);}if(!previousPalette)return reject(SkinnedRenderableGroupFailure3D::Capacity);}
    for(std::size_t index=0;index<parts.size();++index)if(paletteCompatible){
        for(std::size_t old=0;old<partCount_;++old)if(parts_[old].key==parts[index].key){
            if(parts_[old].mesh==parts[index].mesh){submitParts[index].previousTransform=parts_[old].transform;submitParts[index].motionValid=true;}
            break;
        }
    }
    SkinnedSubmissionReceipt3D receipt{};if(!SubmitSkinnedRenderableGroup3D(draw,currentPalette,std::span<const SkinnedRenderablePart3D>{submitParts.data(),parts.size()},previousPalette,false,stats,&receipt)){
        draw.skinnedMeshInstances.resize(instanceSize);draw.RestoreSkinPaletteCheckpoint(paletteCheckpoint);return false;
    }
    pendingPartCount_=parts.size();pendingCompatibilityId_=currentPalette.skeletonCompatibilityId;pendingReceipt_=receipt;pending_=true;return true;
}

bool SkinnedRenderableGroupHistory3D::CommitSubmittedInternal()noexcept{
    if(!pending_)return false;previousJoints_.swap(pendingJoints_);parts_=pendingParts_;partCount_=pendingPartCount_;previousCompatibilityId_=pendingCompatibilityId_;valid_=true;pending_=false;motionReset_=false;invalidateRequested_=false;return true;
}

SkinnedSubmissionResult3D SkinnedRenderableGroupHistory3D::ResolveSubmitted(const D3D12Renderer& renderer)noexcept{
    if(!pending_)return SkinnedSubmissionResult3D::Unknown;const SkinnedSubmissionResult3D result=renderer.QuerySkinnedSubmission(pendingReceipt_);if(invalidateRequested_&&(result==SkinnedSubmissionResult3D::Accepted||result==SkinnedSubmissionResult3D::Rejected||result==SkinnedSubmissionResult3D::Expired))DiscardSubmitted();else if(result==SkinnedSubmissionResult3D::Accepted)CommitSubmittedInternal();else if(result==SkinnedSubmissionResult3D::Rejected||result==SkinnedSubmissionResult3D::Expired)DiscardSubmitted();return result;
}
SkinnedSubmissionResult3D SkinnedRenderableGroupHistory3D::ResolveSubmitted(const Engine& engine)noexcept{
    if(!pending_)return SkinnedSubmissionResult3D::Unknown;const SkinnedSubmissionResult3D result=engine.QuerySkinnedSubmission(pendingReceipt_);if(invalidateRequested_&&(result==SkinnedSubmissionResult3D::Accepted||result==SkinnedSubmissionResult3D::Rejected||result==SkinnedSubmissionResult3D::Expired))DiscardSubmitted();else if(result==SkinnedSubmissionResult3D::Accepted)CommitSubmittedInternal();else if(result==SkinnedSubmissionResult3D::Rejected||result==SkinnedSubmissionResult3D::Expired)DiscardSubmitted();return result;
}

ReusableMemoryStats SkinnedRenderableGroupHistory3D::MemoryStats()const noexcept{
    const std::size_t bytes=(previousJoints_.capacity()+pendingJoints_.capacity())*sizeof(SkinPaletteJoint3D);
    return{bytes,(valid_||pending_)?0:bytes,lastReleasedBytes_,trimCount_};
}

std::size_t SkinnedRenderableGroupHistory3D::TrimUnusedMemory(){
    if(valid_||pending_){lastReleasedBytes_=0;return 0;}const std::size_t before=(previousJoints_.capacity()+pendingJoints_.capacity())*sizeof(SkinPaletteJoint3D);std::vector<SkinPaletteJoint3D>{}.swap(previousJoints_);std::vector<SkinPaletteJoint3D>{}.swap(pendingJoints_);lastReleasedBytes_=before;++trimCount_;return before;
}

}
