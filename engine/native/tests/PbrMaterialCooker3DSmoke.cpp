#include <Aevrynd/PbrMaterialCooker3D.h>
#include <array>
#include <iostream>
#include <string>

using namespace Aevrynd;
namespace {
constexpr const char* Hash="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

TextureResolutionTier Tier(uint32_t size){if(size>=4096)return TextureResolutionTier::Hero;if(size>=2048)return TextureResolutionTier::Near;if(size>=1024)return TextureResolutionTier::General;return TextureResolutionTier::Far;}
AssetManifestEntry Texture(std::string id,TextureUsage usage,uint32_t width,uint32_t height){AssetManifestEntry entry;entry.id=std::move(id);entry.path="textures/"+entry.id+".dds";entry.source={"https://example.com/"+entry.id,Hash};entry.texture={usage,Tier((std::max)(width,height)),width,height,true};return entry;}
AssetManifestEntry Material(std::string id,const std::array<std::string,3>& textureIds){AssetManifestEntry entry;entry.id=std::move(id);entry.path="materials/"+entry.id+".material";entry.kind=AssetKind::Material;entry.source={"https://example.com/"+entry.id,Hash};entry.material.layers[0]={textureIds[0],textureIds[1],textureIds[2]};entry.material.resolutionTier=TextureResolutionTier::Hero;entry.material.textureScale=.5f;entry.material.useUv=true;entry.material.castsShadow=false;entry.material.materialClass=uint8_t(MaterialClass3D::Terrain);entry.pbr.roughnessFactor=.72f;entry.pbr.metallicFactor=.18f;return entry;}
AssetManifest OneMaterial(uint32_t width=4096,uint32_t height=4096){AssetManifest manifest;manifest.entries.push_back(Texture("base",TextureUsage::BaseColor,width,height));manifest.entries.push_back(Texture("rough",TextureUsage::Roughness,width,height));manifest.entries.push_back(Texture("normal",TextureUsage::Normal,width,height));manifest.entries.push_back(Material("terrain",{"base","rough","normal"}));return manifest;}
bool Preserved(const PbrMaterialCookPlan3D& plan){return plan.materials.size()==1&&plan.materials[0].name=="sentinel"&&plan.estimatedResidentBytes==123&&plan.referencedTextures==7&&plan.uniqueTextureSets==9&&plan.maximumResolution==77;}
PbrMaterialCookPlan3D Sentinel(){PbrMaterialCookPlan3D plan;plan.materials.push_back({"sentinel"});plan.estimatedResidentBytes=123;plan.referencedTextures=7;plan.uniqueTextureSets=9;plan.maximumResolution=77;return plan;}
}

int main(){
    static_assert(CanonicalMaterialSurfaceTexel3D({12,34,56,78},MaterialSurfaceMapEncoding3D::RoughnessR)==std::array<uint8_t,4>{255,12,255,12});
    static_assert(CanonicalMaterialSurfaceTexel3D({12,34,56,78},MaterialSurfaceMapEncoding3D::PackedOrm)==std::array<uint8_t,4>{12,34,56,34});
    if(PbrMaterialCooker3D::EstimateBcMipBytes(0,1,1,8)||PbrMaterialCooker3D::EstimateBcMipBytes(1,1,1,7)||PbrMaterialCooker3D::EstimateBcMipBytes(1,1,1,8)!=8||PbrMaterialCooker3D::EstimateBcMipBytes(4,4,4,8)!=24||PbrMaterialCooker3D::EstimateBcMipBytes(5,3,5,8)!=32||PbrMaterialCooker3D::EstimateBcMipBytes(8,8,8,8)!=56)return 1;
    if(PbrMaterialCooker3D::EstimateBcMipBytes(2048,1024,1024,8)!=349544||PbrMaterialCooker3D::EstimateBcMipBytes(2048,1024,1024,16)!=699088)return 2;
    if(PbrMaterialCooker3D::QualityMaximumResolution(MaterialQualityTier3D::Low)!=1024||PbrMaterialCooker3D::QualityMaximumResolution(MaterialQualityTier3D::Medium)!=2048||PbrMaterialCooker3D::QualityMaximumResolution(MaterialQualityTier3D::High)!=2048||PbrMaterialCooker3D::QualityMaximumResolution(MaterialQualityTier3D::Ultra)!=4096)return 3;

    const AssetManifest manifest=OneMaterial();PbrMaterialCookPlan3D plan;PbrMaterialCookError3D error;const std::array<size_t,4> expected={3495320,13981080,13981080,55924120};
    for(size_t index=0;index<expected.size();++index){PbrMaterialCookOptions3D options;options.quality=MaterialQualityTier3D(index);if(!PbrMaterialCooker3D::Cook(manifest,plan,&error,options)||plan.estimatedResidentBytes!=expected[index]||plan.maximumResolution!=PbrMaterialCooker3D::QualityMaximumResolution(options.quality)||error)return 4;}
    const MaterialAssetDefinition3D& cooked=plan.materials[0];if(cooked.name!="terrain"||cooked.textures.baseColorPath!="textures/base.dds"||cooked.textures.roughnessPath!="textures/rough.dds"||cooked.textures.normalPath!="textures/normal.dds"||cooked.textures.maximumResolution!=4096||cooked.material.materialClass!=MaterialClass3D::Terrain||cooked.material.textureScale!=.5f||!cooked.material.useUv||cooked.material.castsShadow||cooked.material.surface.roughness!=.72f||cooked.material.surface.metallic!=.18f)return 5;

    PbrMaterialCookOptions3D options;options.quality=MaterialQualityTier3D::Low;options.maximumResidentBytes=3495320;if(!PbrMaterialCooker3D::Cook(manifest,plan,&error,options))return 6;options.maximumResidentBytes=3495319;plan=Sentinel();if(PbrMaterialCooker3D::Cook(manifest,plan,&error,options)||error.code!=PbrMaterialCookErrorCode::ResidentBudgetExceeded||!Preserved(plan))return 7;
    options={};options.quality=MaterialQualityTier3D(255);plan=Sentinel();if(PbrMaterialCooker3D::Cook(manifest,plan,&error,options)||error.code!=PbrMaterialCookErrorCode::InvalidOptions||!Preserved(plan))return 8;

    AssetManifest invalid=manifest;invalid.entries.back().material.layers[0].normalTexture="missing";plan=Sentinel();if(PbrMaterialCooker3D::Cook(invalid,plan,&error)||error.code!=PbrMaterialCookErrorCode::MissingTexture||!Preserved(plan))return 9;
    invalid=manifest;invalid.entries[2].texture.usage=TextureUsage::Roughness;if(PbrMaterialCooker3D::Cook(invalid,plan,&error)||error.code!=PbrMaterialCookErrorCode::TextureUsageMismatch||!Preserved(plan))return 10;
    invalid=manifest;invalid.entries[2].texture.directXNormal=false;if(PbrMaterialCooker3D::Cook(invalid,plan,&error)||error.code!=PbrMaterialCookErrorCode::NormalConventionUnsupported||!Preserved(plan))return 11;
    invalid=manifest;invalid.entries.back().material.layers[2]={"base","rough","normal"};if(PbrMaterialCooker3D::Cook(invalid,plan,&error)||error.code!=PbrMaterialCookErrorCode::InvalidMaterial||!Preserved(plan))return 12;

    AssetManifest shared=manifest;AssetManifestEntry alias=Material("terrain-alias",{"base","rough","normal"});shared.entries.push_back(std::move(alias));if(!PbrMaterialCooker3D::Cook(shared,plan,&error)||plan.materials.size()!=2||plan.referencedTextures!=3||plan.uniqueTextureSets!=1||plan.estimatedResidentBytes!=55924120)return 13;
    AssetManifest layered=OneMaterial(1024,1024);for(size_t layer=1;layer<3;++layer){const std::string suffix=std::to_string(layer);layered.entries.push_back(Texture("base"+suffix,TextureUsage::BaseColor,1024,1024));layered.entries.push_back(Texture("rough"+suffix,TextureUsage::Roughness,1024,1024));layered.entries.push_back(Texture("normal"+suffix,TextureUsage::Normal,1024,1024));layered.entries[3].material.layers[layer]={"base"+suffix,"rough"+suffix,"normal"+suffix};}if(!PbrMaterialCooker3D::Cook(layered,plan,&error)||plan.referencedTextures!=9||plan.uniqueTextureSets!=1||plan.estimatedResidentBytes!=10485960||plan.materials[0].textures.additionalLayers[1].normalPath!="textures/normal2.dds")return 14;
    AssetManifest orm=manifest;orm.entries[1].texture.usage=TextureUsage::PackedOrm;if(!PbrMaterialCooker3D::Cook(orm,plan,&error)||plan.materials[0].textures.surfaceEncoding!=MaterialSurfaceMapEncoding3D::PackedOrm)return 15;
    std::cout<<"materials="<<plan.materials.size()<<" textures="<<plan.referencedTextures<<" bytes="<<plan.estimatedResidentBytes<<" quality=ultra\n";return 0;
}
