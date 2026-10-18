#include "MistwatchForestSurfaceProfile.h"
#include "MistwatchEntranceRoadMaterial.h"
#include "MistwatchSettlementSurface.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {
using namespace Aevrynd;
namespace Policy=Mistwatch::ForestSurfaceProfile;
using Settings=TerrainDistanceMaterialSettings3D;
std::uint64_t checks{},negatives{};
void Need(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
bool Bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
constexpr std::array<float Settings::*,12> Fields{
    &Settings::detailFadeStart,&Settings::detailFadeEnd,&Settings::macroVariationScale,&Settings::macroVariationStrength,
    &Settings::normalDetailStrength,&Settings::farRoughnessBoost,&Settings::warpScale,&Settings::warpStrength,
    &Settings::grassLayerScale,&Settings::soilLayerScale,&Settings::rockLayerScale,&Settings::chromaVariationStrength};
bool SameSettings(const Settings& a,const Settings& b){
    if(a.enabled!=b.enabled||a.decorrelationEnabled!=b.decorrelationEnabled||a.mesoPartitionEnabled!=b.mesoPartitionEnabled||a.rockVariationEnabled!=b.rockVariationEnabled)return false;
    for(auto field:Fields)if(!Bits(a.*field,b.*field))return false;return true;
}
template<class F>void Reject(F&& action,std::string_view reason){
    bool rejected{};try{std::forward<F>(action)();}
    catch(const std::invalid_argument& error){Need(std::string_view(error.what())==reason,"NEGATIVE_WRONG_REASON");rejected=true;}
    Need(rejected,"NEGATIVE_ACCEPTED");++negatives;
}
bool SameLayer(const MaterialTextureLayerDefinition3D& a,const MaterialTextureLayerDefinition3D& b) {
    if(a.baseColorPath!=b.baseColorPath||a.roughnessPath!=b.roughnessPath||a.normalPath!=b.normalPath||a.surfaceEncoding!=b.surfaceEncoding)return false;
    for(std::size_t channel=0;channel<3;++channel)
        if(a.integrity[channel].expectedBytes!=b.integrity[channel].expectedBytes||a.integrity[channel].expectedSha256!=b.integrity[channel].expectedSha256)return false;
    return true;
}
bool SameMaterial(const Material3D& a,const Material3D& b) {
    return Bits(a.tint.r,b.tint.r)&&Bits(a.tint.g,b.tint.g)&&Bits(a.tint.b,b.tint.b)&&Bits(a.tint.a,b.tint.a)&&
        Bits(a.surface.roughness,b.surface.roughness)&&Bits(a.surface.metallic,b.surface.metallic)&&
        Bits(a.surface.emissive,b.surface.emissive)&&Bits(a.surface.occlusion,b.surface.occlusion)&&
        a.castsShadow==b.castsShadow&&a.textures.value==b.textures.value&&a.textures.semanticIdentity==b.textures.semanticIdentity&&
        Bits(a.textureScale,b.textureScale)&&a.useUv==b.useUv&&a.materialClass==b.materialClass&&a.shadingModel==b.shadingModel&&
        a.alphaMode==b.alphaMode&&Bits(a.alphaCutoff,b.alphaCutoff)&&a.doubleSided==b.doubleSided&&
        Bits(a.normalScale,b.normalScale)&&Bits(a.baseTextureScale,b.baseTextureScale)&&Bits(a.detailTextureScale,b.detailTextureScale);
}
bool SameDefinition(const MaterialAssetDefinition3D& a,const MaterialAssetDefinition3D& b) {
    const auto base=[](const MaterialTextureSetDefinition3D& t){return MaterialTextureLayerDefinition3D{t.baseColorPath,t.roughnessPath,t.normalPath,t.surfaceEncoding,t.integrity};};
    return a.name==b.name&&SameMaterial(a.material,b.material)&&a.textures.maximumResolution==b.textures.maximumResolution&&
        a.textures.alphaMode==b.textures.alphaMode&&Bits(a.textures.alphaCutoff,b.textures.alphaCutoff)&&
        SameLayer(base(a.textures),base(b.textures))&&SameLayer(a.textures.additionalLayers[0],b.textures.additionalLayers[0])&&
        SameLayer(a.textures.additionalLayers[1],b.textures.additionalLayers[1]);
}
// 期望值獨立列出；不從待測 helper 或其 CanonicalOriginal 回填 source／default bits。
MaterialAssetDefinition3D ExpectedOriginal(){
    MaterialAssetDefinition3D out;out.name="ground";auto& t=out.textures;auto& m=out.material;
    t.baseColorPath="ambientcg/ground037/1k/Ground037_1K-JPG_Color.jpg";t.roughnessPath="ambientcg/ground037/1k/Ground037_1K-JPG_Roughness.jpg";t.normalPath="ambientcg/ground037/1k/Ground037_1K-JPG_NormalDX.jpg";
    t.maximumResolution=2048;t.alphaMode=MaterialAlphaMode3D::Opaque;t.alphaCutoff=.5f;t.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    t.integrity={MaterialTextureIntegrity3D{2187764,"d92bec8a35b9cb16ec79b421ca0f572a7400286eae109beafa266629ef49a6cf"},MaterialTextureIntegrity3D{694944,"3786b088e981aae562f06b6477972d379cffd09a5cc542ab42f9ffd94f9bcb0e"},MaterialTextureIntegrity3D{2474498,"0e2d613d117d9231aa0aa61f39b19755f891b4ed1d1599a004cb018a8224e039"}};
    auto& leaf=t.additionalLayers[0];leaf.baseColorPath="poly-haven/forest_leaves_04/2k/forest_leaves_04_diff_2k.png";leaf.roughnessPath="poly-haven/forest_leaves_04/2k/forest_leaves_04_rough_2k.png";leaf.normalPath="poly-haven/forest_leaves_04/2k/forest_leaves_04_nor_dx_2k.png";leaf.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    leaf.integrity={MaterialTextureIntegrity3D{10402792,"40b1786afa17c9930ccae93a9ca75880fc52fc34b96453489c767b2cc82d7ee9"},MaterialTextureIntegrity3D{6083648,"713874257d143af49c482ce8b7648cb7c5c748ca6540db0e44563a04a9fe09ce"},MaterialTextureIntegrity3D{11539651,"19bbaa0eb67eda146129946b9605cfc9b472a302ba1f75eb8ea0b48aa21baa33"}};
    auto& rock=t.additionalLayers[1];rock.baseColorPath="poly-haven/rock_surface/1k/base-color.jpg";rock.roughnessPath="poly-haven/rock_surface/1k/roughness.jpg";rock.normalPath="poly-haven/rock_surface/1k/normal-dx.jpg";rock.surfaceEncoding=MaterialSurfaceMapEncoding3D::RoughnessR;
    rock.integrity={MaterialTextureIntegrity3D{886291,"07edda2bbe4715b01aae2e4bac9f21f204cf4aae37b4733ef01de33281e5e60e"},MaterialTextureIntegrity3D{642680,"ca710d895b01cf323bf00f53194c43504a39d2c964fbfc343c4f45c7d41b10dc"},MaterialTextureIntegrity3D{1161843,"1e1aa5b06f74d22734318cd93ecf49ffe7095c4b2307c5c9b3a4550207c6b1b7"}};
    m.tint={1,1,1,1};m.surface={.92f,0,0,.8f};m.castsShadow=false;m.textures={};m.textureScale=.25f;m.useUv=false;m.materialClass=MaterialClass3D::Terrain;m.shadingModel=CharacterShadingModel3D::Default;m.alphaMode=MaterialAlphaMode3D::Opaque;m.alphaCutoff=.5f;m.doubleSided=false;m.normalScale=.65f;m.baseTextureScale=0;m.detailTextureScale=0;
    return out;
}
MaterialAssetDefinition3D ExpectedCandidate(){
    auto out=ExpectedOriginal();auto& t=out.textures;t.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    t.roughnessPath="moss-height/Ground037_SurfaceOrmHeight_Level0.png";t.integrity[1]={1531546,"012d2378aa6e62cca05ad66d69b512966d5894694b7b24d39afd0ca7ca655f38"};
    auto& leaf=t.additionalLayers[0];leaf.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrmHeight;
    leaf.roughnessPath="leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png";leaf.integrity[1]={9102955,"9f3259f4772f294edca1de03fa4e8d7ed65c0924238b1a6d0b27b8fed7c0ef85"};
    return out;
}
void RejectApply(MaterialAssetDefinition3D ground,Settings settings,Policy::Selection selection,std::string_view reason){
    const auto beforeGround=ground;const auto beforeSettings=settings;
    Reject([&]{Policy::Apply(ground,settings,selection);},reason);
    Need(SameDefinition(ground,beforeGround)&&SameSettings(settings,beforeSettings),"ATOMIC_REJECT_MUTATED_INPUT");
}
void ResolveMatrix(){
    for(unsigned flags=0;flags<8;++flags){
        const bool manual=(flags&1u)!=0,capture=(flags&2u)!=0,replay=(flags&4u)!=0;const bool conflict=unsigned(manual)+unsigned(capture)+unsigned(replay)>1u;
        for(const auto token:std::array<std::wstring_view,3>{L"",L"original",L"forest-moss-leaf-metric-v1"}){
            if(conflict){Reject([&]{static_cast<void>(Policy::Resolve(manual,capture,replay,token));},"FOREST_SURFACE_MODE_CONFLICT");continue;}
            if(manual&&token==L"original"){Reject([&]{static_cast<void>(Policy::Resolve(manual,capture,replay,token));},"FOREST_SURFACE_MANUAL_TOKEN_CONFLICT");continue;}
            const auto value=Policy::Resolve(manual,capture,replay,token);
            const auto expectedMode=manual?Policy::Mode::Manual:capture?Policy::Mode::Capture:replay?Policy::Mode::Replay:Policy::Mode::Normal;
            const auto expectedProfile=manual||token==L"forest-moss-leaf-metric-v1"||(flags==0u&&token.empty())?Policy::Profile::ForestMossLeafMetricV1:Policy::Profile::Original;
            Need(value.mode==expectedMode&&value.profile==expectedProfile,"RESOLVED_CONTEXT_OR_PROFILE_WRONG");
        }
        for(const auto raw:std::array<std::wstring_view,9>{L"forest-moss-leaf-metric",L"forest-moss-leaf-metric-v2",L"Forest-moss-leaf-metric-v1",L" forest-moss-leaf-metric-v1",L"forest-moss-leaf-metric-v1 ",L"Original",L"x",std::wstring_view(L"forest-moss-leaf-metric-v1\0x",28),std::wstring_view(L"original\0",9)})
            Reject([&]{static_cast<void>(Policy::Resolve(manual,capture,replay,raw));},conflict?"FOREST_SURFACE_MODE_CONFLICT":"FOREST_SURFACE_TOKEN_INVALID");
    }
    Need(Policy::Name(Policy::Profile::Original)=="original"&&Policy::Name(Policy::Profile::ForestMossLeafMetricV1)=="forest-moss-leaf-metric-v1","PROFILE_NAMES");
    Need(Policy::ModeName(Policy::Mode::Normal)=="normal"&&Policy::ModeName(Policy::Mode::Manual)=="manual"&&Policy::ModeName(Policy::Mode::Capture)=="capture"&&Policy::ModeName(Policy::Mode::Replay)=="replay","MODE_NAMES");
    Reject([&]{static_cast<void>(Policy::Name(static_cast<Policy::Profile>(255)));},"FOREST_SURFACE_PROFILE_INVALID");
    Reject([&]{static_cast<void>(Policy::ModeName(static_cast<Policy::Mode>(255)));},"FOREST_SURFACE_MODE_INVALID");
    for(const auto item:std::array{std::pair{Policy::Selection{Policy::Mode::Manual,Policy::Profile::Original},"FOREST_SURFACE_SELECTION_INVALID"},
        std::pair{Policy::Selection{static_cast<Policy::Mode>(255),Policy::Profile::Original},"FOREST_SURFACE_MODE_INVALID"},
        std::pair{Policy::Selection{Policy::Mode::Normal,static_cast<Policy::Profile>(255)},"FOREST_SURFACE_PROFILE_INVALID"}}){
        RejectApply(ExpectedOriginal(),Settings{},item.first,item.second);
        Reject([&]{static_cast<void>(Policy::HeightSettings(item.first));},item.second);
    }
}
void HeightContract(Policy::Selection selection){
    const auto height=Policy::HeightSettings(selection);const bool enabled=selection.profile==Policy::Profile::ForestMossLeafMetricV1;
    Need(height.enabled==enabled&&Bits(height.depthMeters,enabled?.015f:0.f)&&Bits(height.fadeStart,12.f)&&Bits(height.fadeEnd,18.f)&&
        Bits(height.minimumNormalY,.96f)&&Bits(height.minimumViewCosine,.15f)&&Bits(height.maximumUvOffset,.08f),"HEIGHT_DEFAULT_OR_CANDIDATE_BITS");
}
void Candidate(Policy::Selection selection,Settings before){
    auto ground=Mistwatch::HabitatGroundDefinition(true,true);const auto canonical=ExpectedOriginal();
    Need(SameDefinition(ground,canonical),"UPSTREAM_CANONICAL_DEFAULT_CHANGED");
    const auto road=Mistwatch::EntranceRoadMaterialDefinition(ground),gravel=Mistwatch::SettlementSurface::MaterialDefinition(ground);
    const auto roadBefore=road,gravelBefore=gravel;auto settings=before;
    Policy::Apply(ground,settings,selection);auto expectedSettings=before;expectedSettings.soilLayerScale=1.75f;
    Need(SameDefinition(ground,ExpectedCandidate())&&SameSettings(settings,expectedSettings),"CANDIDATE_NOT_EXACT_BOUNDED_CHANGE");
    Need(SameMaterial(ground.material,canonical.material)&&SameLayer(ground.textures.additionalLayers[1],canonical.textures.additionalLayers[1]),"MATERIAL_OR_ROCK_CHANGED");
    Need(ValidMaterialSurfaceEncodingDefinition3D(ground.textures)&&MaterialHeightLayerMask3D(ground.textures)==3u,"DUAL_HEIGHT_MASK_NOT_THREE");
    Need(SameDefinition(road,roadBefore)&&SameDefinition(gravel,gravelBefore)&&road.textures.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrm&&gravel.textures.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrm,"DOMAIN_CANONICAL_CHANGED");
    Need(Mistwatch::HabitatGroundLayerSource(ground)=="unrecognized","OLD_HABITAT_HELPER_RELAXED");
    constexpr std::uint32_t mask=31u<<15;const auto packedBefore=PackTerrainDecorrelationSettings3D(before),packedAfter=PackTerrainDecorrelationSettings3D(settings);
    Need(((packedAfter>>15)&31u)==31u&&(packedBefore&~mask)==(packedAfter&~mask),"SOIL_OR_OTHER_PACKED_BITS_CHANGED");
    Need(Bits(TerrainDistanceMaterialMode3D(before),TerrainDistanceMaterialMode3D(settings))&&NormalizeTerrainDistanceMaterialSettings3D(settings)==settings,"MODE_OR_EXISTING_BOUNDS_CHANGED");
    HeightContract(selection);
    RejectApply(ground,settings,selection,"FOREST_SURFACE_SOURCE_INVALID");
}
// 普通預設是明確產品合約；先檢查此分歧，再執行原有完整來源與設定閘門。
void OrdinaryNormalDefault(){
    const auto ordinary=Policy::Resolve(false,false,false,L"");
    // 允許同一測試連結舊標頭做因果紅測，不讓最佳化器預先判定後續路徑不可到達。
    const volatile auto observedProfile=ordinary.profile;
    Need(ordinary.mode==Policy::Mode::Normal&&observedProfile==Policy::Profile::ForestMossLeafMetricV1,"ORDINARY_NORMAL_SURFACE_DEFAULT_NOT_ENHANCED");
    Candidate(ordinary,Settings{});
    const auto control=Policy::Resolve(false,false,false,L"original");
    Need(control.mode==Policy::Mode::Normal&&control.profile==Policy::Profile::Original,"ORDINARY_EXPLICIT_ORIGINAL_LOST");
    HeightContract(control);
}
void OriginalAndModes(){
    for(const auto selection:std::array{Policy::Resolve(false,false,false,L"original"),Policy::Resolve(false,true,false,L"original"),Policy::Resolve(false,false,true,L"")}){
        auto ground=ExpectedOriginal();ground.name="original-invalid-diagnostic";ground.material.normalScale=std::bit_cast<float>(0x7fc12345u);ground.material.textureScale=-0.f;ground.textures.integrity[1]={};
        Settings settings;settings.soilLayerScale=std::bit_cast<float>(0x7fc54321u);settings.enabled=false;const auto before=ground;const auto oldSettings=settings;
        Policy::Apply(ground,settings,selection);Need(SameDefinition(ground,before)&&SameSettings(settings,oldSettings),"ORIGINAL_NOT_FULL_BITWISE_NOOP");HeightContract(selection);
    }
    for(const auto selection:std::array{Policy::Resolve(true,false,false,L""),Policy::Resolve(false,true,false,L"forest-moss-leaf-metric-v1"),Policy::Resolve(false,false,true,L"forest-moss-leaf-metric-v1")})
    for(bool rock:{false,true})for(bool meso:{false,true}){
        Settings settings;settings.rockVariationEnabled=rock;settings.mesoPartitionEnabled=meso;
        Need(((PackTerrainDecorrelationSettings3D(settings)>>15)&31u)==18u,"ORIGINAL_SOIL_CODE_CHANGED");Candidate(selection,settings);
    }
    const auto manual=Policy::Resolve(true,false,false,L"");Need(manual.mode==Policy::Mode::Manual&&manual.mode!=Policy::Mode::Capture&&manual.mode!=Policy::Mode::Replay,"MANUAL_FAKED_CAPTURE_CONTEXT");
}
std::array<std::filesystem::path*,9> Paths(MaterialTextureSetDefinition3D& t){return {&t.baseColorPath,&t.roughnessPath,&t.normalPath,&t.additionalLayers[0].baseColorPath,&t.additionalLayers[0].roughnessPath,&t.additionalLayers[0].normalPath,&t.additionalLayers[1].baseColorPath,&t.additionalLayers[1].roughnessPath,&t.additionalLayers[1].normalPath};}
std::array<MaterialTextureIntegrity3D*,9> Seals(MaterialTextureSetDefinition3D& t){return {&t.integrity[0],&t.integrity[1],&t.integrity[2],&t.additionalLayers[0].integrity[0],&t.additionalLayers[0].integrity[1],&t.additionalLayers[0].integrity[2],&t.additionalLayers[1].integrity[0],&t.additionalLayers[1].integrity[1],&t.additionalLayers[1].integrity[2]};}
void SourceNegatives(){
    const auto selection=Policy::Resolve(true,false,false,L"");
    for(std::size_t channel=0;channel<9;++channel)for(unsigned kind=0;kind<5;++kind){
        auto bad=ExpectedOriginal();const auto paths=Paths(bad.textures);const auto seals=Seals(bad.textures);
        if(kind==0)*paths[channel]="bad/source.png";if(kind==1)++seals[channel]->expectedBytes;if(kind==2)seals[channel]->expectedSha256=std::string(64,'0');
        if(kind==3)*seals[channel]={};if(kind==4)std::ranges::transform(seals[channel]->expectedSha256,seals[channel]->expectedSha256.begin(),[](char c){return c>='a'&&c<='f'?char(c-'a'+'A'):c;});
        RejectApply(bad,Settings{},selection,"FOREST_SURFACE_SOURCE_INVALID");
    }
    for(unsigned slot=0;slot<3;++slot){auto bad=ExpectedOriginal();if(slot==0)bad.textures.surfaceEncoding=static_cast<MaterialSurfaceMapEncoding3D>(255);else bad.textures.additionalLayers[slot-1].surfaceEncoding=static_cast<MaterialSurfaceMapEncoding3D>(255);RejectApply(bad,Settings{},selection,"FOREST_SURFACE_SOURCE_INVALID");}
    for(unsigned slot=0;slot<2;++slot){auto bad=ExpectedOriginal();const auto candidate=ExpectedCandidate();if(slot==0){bad.textures.surfaceEncoding=candidate.textures.surfaceEncoding;bad.textures.roughnessPath=candidate.textures.roughnessPath;bad.textures.integrity[1]=candidate.textures.integrity[1];}else bad.textures.additionalLayers[0]=candidate.textures.additionalLayers[0];RejectApply(bad,Settings{},selection,"FOREST_SURFACE_SOURCE_INVALID");}
    for(unsigned kind=0;kind<23;++kind){auto bad=ExpectedOriginal();auto& m=bad.material;auto& t=bad.textures;
        switch(kind){
        case 0:bad.name="entrance-road";break;case 1:t.maximumResolution=2049;break;case 2:t.maximumResolution=2047;break;
        case 3:m.useUv=true;break;case 4:m.normalScale=std::nextafter(.65f,1.f);break;case 5:m.normalScale=-15.46875f;break;
        case 6:m.materialClass=MaterialClass3D::Surface;break;case 7:m.textureScale=.54f;break;case 8:m.baseTextureScale=1.f;break;case 9:m.detailTextureScale=1.f;break;
        case 10:m.alphaMode=MaterialAlphaMode3D::Mask;break;case 11:m.alphaMode=MaterialAlphaMode3D::Blend;break;case 12:m.alphaMode=static_cast<MaterialAlphaMode3D>(255);break;
        case 13:t.alphaMode=MaterialAlphaMode3D::Mask;break;case 14:t.alphaMode=MaterialAlphaMode3D::Blend;break;case 15:t.alphaMode=static_cast<MaterialAlphaMode3D>(255);break;
        case 16:m.alphaCutoff=std::nextafter(.5f,1.f);break;case 17:t.alphaCutoff=std::nextafter(.5f,1.f);break;case 18:m.castsShadow=true;break;case 19:m.doubleSided=true;break;
        case 20:m.tint.r=std::nextafter(1.f,0.f);break;case 21:m.surface.occlusion=std::nextafter(.8f,1.f);break;case 22:m.textures.semanticIdentity=1;break;
        default:throw std::runtime_error("NEGATIVE_MATRIX_INTERNAL");
        }
        RejectApply(bad,Settings{},selection,"FOREST_SURFACE_SOURCE_INVALID");
    }
    const auto canonical=ExpectedOriginal();RejectApply(Mistwatch::EntranceRoadMaterialDefinition(canonical),Settings{},selection,"FOREST_SURFACE_SOURCE_INVALID");
    RejectApply(Mistwatch::SettlementSurface::MaterialDefinition(canonical),Settings{},selection,"FOREST_SURFACE_SOURCE_INVALID");
}
void SettingsNegativesAndBounds(){
    const auto selection=Policy::Resolve(false,true,false,L"forest-moss-leaf-metric-v1");
    Settings bad;bad.enabled=false;RejectApply(ExpectedOriginal(),bad,selection,"FOREST_SURFACE_SETTINGS_INVALID");
    bad=Settings{};bad.decorrelationEnabled=false;RejectApply(ExpectedOriginal(),bad,selection,"FOREST_SURFACE_SETTINGS_INVALID");
    for(auto field:Fields)for(float invalid:std::array{std::bit_cast<float>(0x7fc12345u),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}){
        bad=Settings{};bad.*field=invalid;RejectApply(ExpectedOriginal(),bad,selection,"FOREST_SURFACE_SETTINGS_INVALID");
    }
    constexpr std::array<std::pair<float,float>,12> ranges{{{0.f,100000.f},{48.f+.01f,100001.f},{.00001f,4.f},{0.f,.5f},{0.f,2.f},{0.f,.5f},{.0001f,.05f},{0.f,8.f},{.5f,1.75f},{.5f,1.75f},{.5f,1.75f},{0.f,.2f}}};
    for(std::size_t index=0;index<Fields.size();++index){
        Settings basis;if(index==0)basis.detailFadeEnd=100001.f;const auto [minimum,maximum]=ranges[index];const auto field=Fields[index];
        for(float boundary:{minimum,maximum}){auto valid=basis;valid.*field=boundary;Candidate(selection,valid);}
        auto low=basis;low.*field=std::nextafter(minimum,-std::numeric_limits<float>::infinity());RejectApply(ExpectedOriginal(),low,selection,"FOREST_SURFACE_SETTINGS_INVALID");
        auto high=basis;high.*field=std::nextafter(maximum,std::numeric_limits<float>::infinity());RejectApply(ExpectedOriginal(),high,selection,"FOREST_SURFACE_SETTINGS_INVALID");
    }
}
}
int main()try{
    OrdinaryNormalDefault();ResolveMatrix();OriginalAndModes();SourceNegatives();SettingsNegativesAndBounds();
    std::cout<<"FOREST_SURFACE_PROFILE_CPU_PASS checks="<<checks<<" negatives="<<negatives<<" dual_height_mask=3 soil_code=31 manual_fake_capture=0 GPU_or_art_pass=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FOREST_SURFACE_PROFILE_FIRST_DIVERGENCE "<<error.what()<<'\n';return 2;}
