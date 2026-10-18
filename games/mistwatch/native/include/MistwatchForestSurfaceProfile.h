#pragma once
#include "MistwatchHabitatLeafMaterial.h"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <type_traits>
#include <utility>

namespace Mistwatch::ForestSurfaceProfile {
enum class Mode : std::uint8_t { Normal,Manual,Capture,Replay };
enum class Profile : std::uint8_t { Original,ForestMossLeafMetricV1 };
struct Selection {Mode mode{Mode::Normal};Profile profile{Profile::Original};};
inline constexpr std::wstring_view CandidateToken=L"forest-moss-leaf-metric-v1";
inline std::string_view Name(Profile profile) {
    switch(profile){case Profile::Original:return "original";case Profile::ForestMossLeafMetricV1:return "forest-moss-leaf-metric-v1";}
    throw std::invalid_argument("FOREST_SURFACE_PROFILE_INVALID");
}
inline std::string_view ModeName(Mode mode) {
    switch(mode){case Mode::Normal:return "normal";case Mode::Manual:return "manual";case Mode::Capture:return "capture";case Mode::Replay:return "replay";}
    throw std::invalid_argument("FOREST_SURFACE_MODE_INVALID");
}
inline void RequireSelection(Selection selection) {
    static_cast<void>(Name(selection.profile));static_cast<void>(ModeName(selection.mode));
    if(selection.mode==Mode::Manual&&selection.profile!=Profile::ForestMossLeafMetricV1)
        throw std::invalid_argument("FOREST_SURFACE_SELECTION_INVALID");
}
inline Selection Resolve(bool manual,bool capture,bool replay,std::wstring_view raw) {
    if(unsigned(manual)+unsigned(capture)+unsigned(replay)>1u)throw std::invalid_argument("FOREST_SURFACE_MODE_CONFLICT");
    if(!raw.empty()&&raw!=L"original"&&raw!=CandidateToken)throw std::invalid_argument("FOREST_SURFACE_TOKEN_INVALID");
    if(manual){if(raw==L"original")throw std::invalid_argument("FOREST_SURFACE_MANUAL_TOKEN_CONFLICT");return {Mode::Manual,Profile::ForestMossLeafMetricV1};}
    const Mode mode=capture?Mode::Capture:replay?Mode::Replay:Mode::Normal;
    // 一般遊玩預設採雙高度地表；明確 original 與驗證模式仍保留各自契約。
    if(raw==CandidateToken||(mode==Mode::Normal&&raw.empty()))return {mode,Profile::ForestMossLeafMetricV1};
    return {mode,Profile::Original};
}
namespace Detail {
inline bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
inline bool CanonicalOriginal(const Aevrynd::MaterialAssetDefinition3D& definition) {
    using namespace Aevrynd;
    const auto& t=definition.textures;const auto& m=definition.material;
    if(definition.name!="ground"||t.maximumResolution!=2048||t.alphaMode!=MaterialAlphaMode3D::Opaque||!Bits(t.alphaCutoff,.5f)||
       t.surfaceEncoding!=MaterialSurfaceMapEncoding3D::RoughnessR||t.additionalLayers[0].surfaceEncoding!=MaterialSurfaceMapEncoding3D::RoughnessR||
       t.additionalLayers[1].surfaceEncoding!=MaterialSurfaceMapEncoding3D::RoughnessR)return false;
    // 固定原九通道 literal；不借會變動的作者預設作為放寬來源閘門的依據。
    constexpr std::array<std::string_view,9> paths{
        "ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg","ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg","ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg",
        "poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png","poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png","poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png",
        "poly-haven/rock_surface/1k/base-color.jpg","poly-haven/rock_surface/1k/roughness.jpg","poly-haven/rock_surface/1k/normal-dx.jpg"};
    constexpr std::array<std::uint64_t,9> bytes{2187764,694944,2474498,10402792,6083648,11539651,886291,642680,1161843};
    constexpr std::array<std::string_view,9> sha{
        "d92bec8a35b9cb16ec79b421ca0f572a7400286eae109beafa266629ef49a6cf","3786b088e981aae562f06b6477972d379cffd09a5cc542ab42f9ffd94f9bcb0e","0e2d613d117d9231aa0aa61f39b19755f891b4ed1d1599a004cb018a8224e039",
        "40b1786afa17c9930ccae93a9ca75880fc52fc34b96453489c767b2cc82d7ee9","713874257d143af49c482ce8b7648cb7c5c748ca6540db0e44563a04a9fe09ce","19bbaa0eb67eda146129946b9605cfc9b472a302ba1f75eb8ea0b48aa21baa33",
        "07edda2bbe4715b01aae2e4bac9f21f204cf4aae37b4733ef01de33281e5e60e","ca710d895b01cf323bf00f53194c43504a39d2c964fbfc343c4f45c7d41b10dc","1e1aa5b06f74d22734318cd93ecf49ffe7095c4b2307c5c9b3a4550207c6b1b7"};
    const std::array<const std::filesystem::path*,9> actualPaths{&t.baseColorPath,&t.roughnessPath,&t.normalPath,
        &t.additionalLayers[0].baseColorPath,&t.additionalLayers[0].roughnessPath,&t.additionalLayers[0].normalPath,
        &t.additionalLayers[1].baseColorPath,&t.additionalLayers[1].roughnessPath,&t.additionalLayers[1].normalPath};
    const std::array<const MaterialTextureIntegrity3D*,9> seals{&t.integrity[0],&t.integrity[1],&t.integrity[2],
        &t.additionalLayers[0].integrity[0],&t.additionalLayers[0].integrity[1],&t.additionalLayers[0].integrity[2],
        &t.additionalLayers[1].integrity[0],&t.additionalLayers[1].integrity[1],&t.additionalLayers[1].integrity[2]};
    for(std::size_t i=0;i<paths.size();++i)if(*actualPaths[i]!=paths[i]||seals[i]->expectedBytes!=bytes[i]||seals[i]->expectedSha256!=sha[i])return false;
    return m.materialClass==MaterialClass3D::Terrain&&!m.useUv&&!m.castsShadow&&!m.doubleSided&&
        m.shadingModel==CharacterShadingModel3D::Default&&m.alphaMode==MaterialAlphaMode3D::Opaque&&!m.textures.value&&!m.textures.semanticIdentity&&
        Bits(m.tint.r,1.f)&&Bits(m.tint.g,1.f)&&Bits(m.tint.b,1.f)&&Bits(m.tint.a,1.f)&&
        Bits(m.surface.roughness,.92f)&&Bits(m.surface.metallic,0.f)&&Bits(m.surface.emissive,0.f)&&Bits(m.surface.occlusion,.8f)&&
        Bits(m.textureScale,.25f)&&Bits(m.normalScale,.65f)&&Bits(m.baseTextureScale,0.f)&&Bits(m.detailTextureScale,0.f)&&Bits(m.alphaCutoff,.5f);
}
inline bool ValidSettings(const Aevrynd::TerrainDistanceMaterialSettings3D& source) {
    if(!source.enabled||!source.decorrelationEnabled)return false;
    for(float value:std::array{source.detailFadeStart,source.detailFadeEnd,source.macroVariationScale,source.macroVariationStrength,
        source.normalDetailStrength,source.farRoughnessBoost,source.warpScale,source.warpStrength,source.grassLayerScale,
        source.soilLayerScale,source.rockLayerScale,source.chromaVariationStrength})if(!std::isfinite(value))return false;
    return Aevrynd::NormalizeTerrainDistanceMaterialSettings3D(source)==source;
}
}
inline void Apply(Aevrynd::MaterialAssetDefinition3D& ground,Aevrynd::TerrainDistanceMaterialSettings3D& settings,Selection selection) {
    RequireSelection(selection);if(selection.profile==Profile::Original)return;
    if(!Detail::CanonicalOriginal(ground))throw std::invalid_argument("FOREST_SURFACE_SOURCE_INVALID");
    if(!Detail::ValidSettings(settings))throw std::invalid_argument("FOREST_SURFACE_SETTINGS_INVALID");
    auto candidate=ground;auto candidateSettings=settings;
    candidate.textures.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    candidate.textures.roughnessPath="moss-height/Ground037_SurfaceOrmHeight_Level0.png";
    candidate.textures.integrity[1]={1531546,"012d2378aa6e62cca05ad66d69b512966d5894694b7b24d39afd0ca7ca655f38"};
    auto& leaf=candidate.textures.additionalLayers[0];leaf.surfaceEncoding=Aevrynd::MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    leaf.roughnessPath="leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png";
    leaf.integrity[1]={9102955,"9f3259f4772f294edca1de03fa4e8d7ed65c0924238b1a6d0b27b8fed7c0ef85"};
    candidateSettings.soilLayerScale=1.75f;
    if(!Detail::ValidSettings(candidateSettings))throw std::invalid_argument("FOREST_SURFACE_SETTINGS_INVALID");
    if(!Aevrynd::ValidMaterialSurfaceEncodingDefinition3D(candidate.textures)||Aevrynd::MaterialHeightLayerMask3D(candidate.textures)!=3u)
        throw std::invalid_argument("FOREST_SURFACE_DERIVED_SEMANTICS_INVALID");
    // 完整 copy 與封印字串先備妥，最後才以 noexcept swap 同時提交兩個輸入。
    static_assert(std::is_nothrow_swappable_v<Aevrynd::MaterialAssetDefinition3D>);
    static_assert(std::is_nothrow_swappable_v<Aevrynd::TerrainDistanceMaterialSettings3D>);
    using std::swap;swap(ground,candidate);swap(settings,candidateSettings);
}
inline Aevrynd::TerrainParallaxSettings3D HeightSettings(Selection selection) {
    RequireSelection(selection);Aevrynd::TerrainParallaxSettings3D settings;
    if(selection.profile==Profile::ForestMossLeafMetricV1){settings.enabled=true;settings.depthMeters=.015f;}
    return settings;
}
}
