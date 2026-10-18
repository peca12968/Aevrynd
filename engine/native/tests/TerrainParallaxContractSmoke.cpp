// 舊編碼精確不變；新高度必須有明確能力與有界設定，不能把舊 alpha 誤當高度。
#include <Aevrynd/Renderer.h>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Aevrynd;
namespace {
unsigned checks{};
void Need(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
}
int main(){try{
    static_assert(static_cast<unsigned>(MaterialSurfaceMapEncoding3D::RoughnessR)==0);
    static_assert(static_cast<unsigned>(MaterialSurfaceMapEncoding3D::PackedOrm)==1);
    static_assert(static_cast<unsigned>(MaterialSurfaceMapEncoding3D::PackedOrmHeight)==2);
    for(unsigned rough=0;rough<256;++rough)for(unsigned height=0;height<256;++height){const std::array<uint8_t,4> pixel{255,uint8_t(rough),0,uint8_t(height)};
        Need(CanonicalMaterialSurfaceTexel3D(pixel,MaterialSurfaceMapEncoding3D::PackedOrmHeight)==pixel,"HEIGHT_ALPHA_NOT_PRESERVED");
        Need(CanonicalMaterialSurfaceTexel3D(pixel,MaterialSurfaceMapEncoding3D::PackedOrm)==std::array<uint8_t,4>{255,uint8_t(rough),0,uint8_t(rough)},"OLD_ORM_CHANGED");
        Need(CanonicalMaterialSurfaceTexel3D(pixel,MaterialSurfaceMapEncoding3D::RoughnessR)==std::array<uint8_t,4>{255,255,255,255},"OLD_ROUGHNESS_CHANGED");}
    MaterialTextureSetDefinition3D ordinary;Need(ValidMaterialSurfaceEncodingDefinition3D(ordinary),"OLD_UNSEALED_DEFINITION_CHANGED");
    ordinary.surfaceEncoding=static_cast<MaterialSurfaceMapEncoding3D>(99);Need(!ValidMaterialSurfaceEncodingDefinition3D(ordinary),"UNKNOWN_ENCODING_ACCEPTED");
    MaterialTextureSetDefinition3D sealed;sealed.baseColorPath="color.jpg";sealed.roughnessPath="height.png";sealed.normalPath="normal.jpg";sealed.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    Need(!ValidMaterialSurfaceEncodingDefinition3D(sealed),"UNSEALED_HEIGHT_ACCEPTED");
    for(auto& seal:sealed.integrity)seal={1,std::string(64,'a')};Need(ValidMaterialSurfaceEncodingDefinition3D(sealed),"SEALED_HEIGHT_REJECTED");
    auto bad=sealed;bad.alphaMode=MaterialAlphaMode3D::Mask;Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"HEIGHT_AS_ALPHA_MASK_ACCEPTED");
    bad=sealed;bad.additionalLayers[0].surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrmHeight;Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"UNIMPLEMENTED_LAYER_HEIGHT_ACCEPTED");
    bad=sealed;bad.integrity[1].expectedSha256[2]='z';Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"BAD_HEIGHT_SEAL_ACCEPTED");
    bad=sealed;bad.normalPath.clear();Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"MISSING_HEIGHT_NORMAL_ACCEPTED");
    auto both=sealed;auto& leaf=both.additionalLayers[0];
    leaf={"leaf-color.png","leaf-height.png","leaf-normal.png",MaterialSurfaceMapEncoding3D::PackedOrmHeight,sealed.integrity};
    Need(ValidMaterialSurfaceEncodingDefinition3D(both)&&MaterialHeightLayerMask3D(both)==3,"SEALED_TWO_HEIGHT_LAYERS_REJECTED");
    auto onlyLeaf=both;onlyLeaf.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    Need(ValidMaterialSurfaceEncodingDefinition3D(onlyLeaf)&&MaterialHeightLayerMask3D(onlyLeaf)==2,"SEALED_LEAF_ONLY_REJECTED");
    Need(MaterialHeightLayerMask3D(sealed)==1,"SEALED_BASE_MASK_WRONG");
    Need(MaterialHeightLayerMask3D(ordinary)==0,"INVALID_DEFINITION_HAS_HEIGHT_MASK");
    for(unsigned slot=0;slot<3;++slot){
        bad=both;bad.additionalLayers[0].integrity[slot]={};Need(!ValidMaterialSurfaceEncodingDefinition3D(bad)&&MaterialHeightLayerMask3D(bad)==0,"UNSEALED_LEAF_ACCEPTED");
        bad=both;bad.additionalLayers[0].integrity[slot].expectedSha256[1]='z';Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"BAD_LEAF_SEAL_ACCEPTED");
        bad=both;bad.additionalLayers[0].integrity[slot].expectedBytes=0;Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"ZERO_LEAF_SIZE_ACCEPTED");
    }
    bad=both;bad.additionalLayers[0].roughnessPath.clear();Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"MISSING_LEAF_PATH_ACCEPTED");
    bad=both;bad.additionalLayers[1]=leaf;Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"ROCK_HEIGHT_ACCEPTED");
    bad=both;bad.alphaMode=MaterialAlphaMode3D::Blend;Need(!ValidMaterialSurfaceEncodingDefinition3D(bad),"LEAF_BLEND_HEIGHT_ACCEPTED");
    TerrainParallaxSettings3D settings;Need(ValidTerrainParallaxSettings3D(settings)&&!settings.enabled&&FingerprintTerrainParallaxSettings3D(settings)==0,"DEFAULT_NOT_OFF");
    settings.enabled=true;Need(!NormalizeTerrainParallaxSettings3D(settings).enabled,"ZERO_DEPTH_ENABLED");
    settings.depthMeters=.015f;Need(NormalizeTerrainParallaxSettings3D(settings)==settings&&FingerprintTerrainParallaxSettings3D(settings)!=0,"VALID_POM_REJECTED");
    DrawList draw;Need(!draw.TerrainParallax().enabled,"DRAW_DEFAULT_POM_ENABLED");draw.SetTerrainParallax(settings);Need(draw.TerrainParallax()==settings,"DRAW_POM_SETTINGS_NOT_STORED");
    auto reject=[&](TerrainParallaxSettings3D value){Need(!ValidTerrainParallaxSettings3D(value)&&!NormalizeTerrainParallaxSettings3D(value).enabled&&FingerprintTerrainParallaxSettings3D(value)==0,"BAD_PARALLAX_NOT_FAIL_CLOSED");};
    auto invalid=settings;invalid.depthMeters=std::nextafter(.02f,1.f);reject(invalid);
    invalid=settings;invalid.depthMeters=-.001f;reject(invalid);
    invalid=settings;invalid.fadeEnd=invalid.fadeStart;reject(invalid);
    invalid=settings;invalid.minimumNormalY=.89f;reject(invalid);
    invalid=settings;invalid.minimumViewCosine=.01f;reject(invalid);
    invalid=settings;invalid.maximumUvOffset=.2f;reject(invalid);
    invalid=settings;invalid.depthMeters=std::numeric_limits<float>::quiet_NaN();reject(invalid);
    invalid=settings;invalid.fadeStart=std::numeric_limits<float>::infinity();reject(invalid);
    draw.SetTerrainParallax(invalid);Need(!draw.TerrainParallax().enabled,"DRAW_INVALID_POM_RETAINED");
    std::cout<<"TERRAIN_PARALLAX_CONTRACT_CPU_PASSED checks="<<checks<<" GPU_verified=0 runtime_pixels_verified=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
