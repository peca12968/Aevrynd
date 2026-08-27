#include <Aevrynd/PbrMaterialCooker3D.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>

namespace Aevrynd {
namespace {
void SetError(PbrMaterialCookError3D* error,PbrMaterialCookErrorCode code,size_t entry,std::string material,std::string message){if(error)*error={code,entry,std::move(material),std::move(message)};}
struct TextureSetKey {
    std::array<std::string,9> textures{};
    uint32_t maximumResolution{};
    bool operator<(const TextureSetKey& other)const{return std::tie(textures,maximumResolution)<std::tie(other.textures,other.maximumResolution);}
};
uint32_t ScaledDimension(uint32_t width,uint32_t height,uint32_t maximumResolution,bool returnWidth){const uint32_t longest=(std::max)(width,height);if(!longest)return 0;const double scale=(std::min)(1.,double(maximumResolution)/double(longest));return(std::max)(1u,uint32_t(std::lround(double(returnWidth?width:height)*scale)));}
}

uint32_t PbrMaterialCooker3D::QualityMaximumResolution(MaterialQualityTier3D quality){switch(quality){case MaterialQualityTier3D::Low:return 1024;case MaterialQualityTier3D::Medium:case MaterialQualityTier3D::High:return 2048;case MaterialQualityTier3D::Ultra:return 4096;}return 0;}
size_t PbrMaterialCooker3D::EstimateBcMipBytes(uint32_t width,uint32_t height,uint32_t maximumResolution,size_t blockBytes){
    if(!width||!height||!maximumResolution||(blockBytes!=8&&blockBytes!=16))return 0;
    const uint32_t sourceWidth=width,sourceHeight=height;
    width=ScaledDimension(sourceWidth,sourceHeight,maximumResolution,true);
    height=ScaledDimension(sourceWidth,sourceHeight,maximumResolution,false);
    size_t bytes{};
    for(;;){
        const size_t blocksWide=(width+3)/4,blocksHigh=(height+3)/4;
        if(blocksWide&&blocksHigh>(std::numeric_limits<size_t>::max)()/blocksWide/blockBytes)return(std::numeric_limits<size_t>::max)();
        const size_t level=blocksWide*blocksHigh*blockBytes;
        if(bytes>(std::numeric_limits<size_t>::max)()-level)return(std::numeric_limits<size_t>::max)();
        bytes+=level;
        if(width==1&&height==1)break;
        width=(std::max)(1u,width/2);height=(std::max)(1u,height/2);
    }
    return bytes;
}

bool PbrMaterialCooker3D::Cook(const AssetManifest& manifest,PbrMaterialCookPlan3D& output,PbrMaterialCookError3D* error,const PbrMaterialCookOptions3D& options){
    if(error)*error={};const uint32_t qualityMaximum=QualityMaximumResolution(options.quality);if(!qualityMaximum){SetError(error,PbrMaterialCookErrorCode::InvalidOptions,0,{},"Material quality tier is invalid");return false;}AssetManifest normalized;AssetManifestError manifestError;if(!AssetManifestValidator::ValidateAndNormalize(manifest,normalized,&manifestError)){SetError(error,PbrMaterialCookErrorCode::InvalidManifest,manifestError.entryIndex,{},manifestError.message);return false;}
    std::map<std::string,const AssetManifestEntry*,std::less<>> entries;size_t materialCount{};for(const AssetManifestEntry& entry:normalized.entries){entries.emplace(entry.id,&entry);if(entry.kind==AssetKind::Material)++materialCount;}if(materialCount>options.maximumMaterials){SetError(error,PbrMaterialCookErrorCode::MaterialLimitExceeded,0,{},"Material count exceeds the cook limit");return false;}
    PbrMaterialCookPlan3D temporary;temporary.materials.reserve(materialCount);temporary.maximumResolution=qualityMaximum;std::set<std::string,std::less<>> referencedTextureIds;std::map<TextureSetKey,size_t> uniqueTextureSets;
    for(size_t entryIndex=0;entryIndex<normalized.entries.size();++entryIndex){const AssetManifestEntry& materialEntry=normalized.entries[entryIndex];if(materialEntry.kind!=AssetKind::Material)continue;const auto& layers=materialEntry.material.layers;if(layers[0].baseColorTexture.empty()||layers[0].roughnessTexture.empty()||layers[0].normalTexture.empty()){SetError(error,PbrMaterialCookErrorCode::InvalidMaterial,entryIndex,materialEntry.id,"The primary PBR layer requires base color, roughness, and DirectX normal textures");return false;}
        MaterialAssetDefinition3D definition{};definition.name=materialEntry.id;definition.material.surface.roughness=materialEntry.pbr.roughnessFactor;definition.material.surface.metallic=materialEntry.pbr.metallicFactor;definition.material.textureScale=(std::max)(.001f,materialEntry.material.textureScale);definition.material.useUv=materialEntry.material.useUv;definition.material.castsShadow=materialEntry.material.castsShadow;definition.material.materialClass=MaterialClass3D(materialEntry.material.materialClass);TextureSetKey setKey{};uint32_t setMaximum=0;size_t setBytes{};bool foundEmptyLayer{};
        for(size_t layerIndex=0;layerIndex<layers.size();++layerIndex){const AssetMaterialLayerMetadata& layer=layers[layerIndex];const bool empty=layer.baseColorTexture.empty()&&layer.roughnessTexture.empty()&&layer.normalTexture.empty();if(empty){foundEmptyLayer=true;continue;}if(foundEmptyLayer||layer.baseColorTexture.empty()||layer.roughnessTexture.empty()||layer.normalTexture.empty()){SetError(error,PbrMaterialCookErrorCode::InvalidMaterial,entryIndex,materialEntry.id,"PBR layers must be complete and contiguous");return false;}const std::array<std::pair<std::string_view,TextureUsage>,3> references={{{layer.baseColorTexture,TextureUsage::BaseColor},{layer.roughnessTexture,TextureUsage::Roughness},{layer.normalTexture,TextureUsage::Normal}}};std::array<const AssetManifestEntry*,3> textures{};
            for(size_t channel=0;channel<references.size();++channel){const auto found=entries.find(references[channel].first);if(found==entries.end()||found->second->kind!=AssetKind::Texture){SetError(error,PbrMaterialCookErrorCode::MissingTexture,entryIndex,materialEntry.id,"Material references a missing texture asset");return false;}textures[channel]=found->second;if(textures[channel]->texture.usage!=references[channel].second){SetError(error,PbrMaterialCookErrorCode::TextureUsageMismatch,entryIndex,materialEntry.id,"Material texture usage does not match its PBR channel");return false;}if(references[channel].second==TextureUsage::Normal&&!textures[channel]->texture.directXNormal){SetError(error,PbrMaterialCookErrorCode::NormalConventionUnsupported,entryIndex,materialEntry.id,"Only DirectX normal maps are accepted");return false;}referencedTextureIds.insert(textures[channel]->id);setKey.textures[layerIndex*3+channel]=textures[channel]->path;setMaximum=(std::max)(setMaximum,(std::max)(textures[channel]->texture.width,textures[channel]->texture.height));}
            const uint32_t materialMaximum=AssetManifestValidator::TierMaximumDimension(materialEntry.material.resolutionTier),layerMaximum=(std::min)({qualityMaximum,materialMaximum,setMaximum});setBytes+=EstimateBcMipBytes(textures[0]->texture.width,textures[0]->texture.height,layerMaximum,8);setBytes+=EstimateBcMipBytes(textures[1]->texture.width,textures[1]->texture.height,layerMaximum,8);setBytes+=EstimateBcMipBytes(textures[2]->texture.width,textures[2]->texture.height,layerMaximum,16);
            if(layerIndex){definition.textures.additionalLayers[layerIndex-1]={textures[0]->path,textures[1]->path,textures[2]->path};}
            else{definition.textures.baseColorPath=textures[0]->path;definition.textures.roughnessPath=textures[1]->path;definition.textures.normalPath=textures[2]->path;}
        }
        definition.textures.maximumResolution=(std::min)({qualityMaximum,AssetManifestValidator::TierMaximumDimension(materialEntry.material.resolutionTier),setMaximum});setKey.maximumResolution=definition.textures.maximumResolution;if(!uniqueTextureSets.contains(setKey)){if(setBytes>options.maximumResidentBytes-temporary.estimatedResidentBytes){SetError(error,PbrMaterialCookErrorCode::ResidentBudgetExceeded,entryIndex,materialEntry.id,"Cooked PBR textures exceed the resident memory budget");return false;}uniqueTextureSets.emplace(setKey,setBytes);temporary.estimatedResidentBytes+=setBytes;}temporary.materials.push_back(std::move(definition));
    }
    temporary.referencedTextures=referencedTextureIds.size();temporary.uniqueTextureSets=uniqueTextureSets.size();output=std::move(temporary);return true;
}
}
