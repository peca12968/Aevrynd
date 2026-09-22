#include "GameplayRules3D.h"
#ifndef AEVRYND_CHARACTER_BUILD_FINGERPRINT
#define AEVRYND_CHARACTER_BUILD_FINGERPRINT "gameplay-rules-test"
#endif
#include "GameplayLabGame.h"
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <iterator>
#include <optional>
#include <sstream>
using namespace GameplayLab;using namespace Aevrynd;
namespace {
bool ValidateSwordTipConsumerSmoke(){
    CharacterSwordGeometry3D geometry;geometry.available=true;
    const Matrix4 handWorld=CreateTransformMatrix({2,3,4},{.2f,-.4f,.1f},{1.2f,.8f,1.1f});
    Vec3 tip{};
    if(!ResolveGameplaySwordTipWorld3D(geometry,handWorld,tip)||Length(tip-TransformPoint(handWorld,geometry.bladeTip))>.000001f||
       Length(tip-TransformPoint(handWorld,{0,-.72f,0}))<.1f)return false;
    const Vec3 sentinel{7,8,9};tip=sentinel;geometry.available=false;
    if(ResolveGameplaySwordTipWorld3D(geometry,handWorld,tip)||Length(tip-sentinel)!=0.f)return false;
    geometry.available=true;geometry.bladeTip.y=std::numeric_limits<float>::quiet_NaN();
    if(ResolveGameplaySwordTipWorld3D(geometry,handWorld,tip)||Length(tip-sentinel)!=0.f)return false;
    geometry.bladeTip=CharacterSwordGeometry3D{}.bladeTip;
    Matrix4 invalidWorld=handWorld;invalidWorld[1][1]=std::numeric_limits<float>::infinity();
    return !ResolveGameplaySwordTipWorld3D(geometry,invalidWorld,tip)&&Length(tip-sentinel)==0.f;
}
}
namespace {struct CanonicalFloatTextRoundTripSmoke{CanonicalFloatTextRoundTripSmoke(){const float representative=1.44839f;const float adjacent=std::nextafter(representative,std::numeric_limits<float>::infinity());if(std::bit_cast<std::uint32_t>(representative)==std::bit_cast<std::uint32_t>(adjacent))throw std::runtime_error("canonical float ULP fixture collapsed");for(const float value:{representative,adjacent}){std::ostringstream text;text<<std::setprecision(std::numeric_limits<float>::max_digits10)<<value;std::istringstream parser(text.str());float decoded{};parser>>decoded;if(!parser||std::bit_cast<std::uint32_t>(decoded)!=std::bit_cast<std::uint32_t>(value))throw std::runtime_error("canonical diagnostic float text did not round-trip raw bits");}}} canonicalFloatTextRoundTripSmoke;}
static_assert(GameplayTemporalCaptureWarmupFrames3D>=3);
constexpr auto MinimalHudClosed=BuildGameplayHudPresentationPolicy3D(false,false);
constexpr auto MinimalHudOpen=BuildGameplayHudPresentationPolicy3D(false,true);
constexpr auto DiagnosticHudClosed=BuildGameplayHudPresentationPolicy3D(true,false);
constexpr auto DiagnosticHudOpen=BuildGameplayHudPresentationPolicy3D(true,true);
static_assert(!MinimalHudClosed.instructions&&!MinimalHudClosed.palette&&!MinimalHudClosed.combatStatus&&!MinimalHudClosed.interactionDiagnostics);
static_assert(!MinimalHudOpen.instructions&&MinimalHudOpen.palette&&!MinimalHudOpen.combatStatus&&!MinimalHudOpen.interactionDiagnostics);
static_assert(DiagnosticHudClosed.instructions&&!DiagnosticHudClosed.palette&&DiagnosticHudClosed.combatStatus&&DiagnosticHudClosed.interactionDiagnostics);
static_assert(DiagnosticHudOpen.instructions&&DiagnosticHudOpen.palette&&DiagnosticHudOpen.combatStatus&&DiagnosticHudOpen.interactionDiagnostics);
static_assert(GameplayHudShowsCompactInstructions3D(MinimalHudClosed));
static_assert(!GameplayHudShowsCompactInstructions3D(DiagnosticHudClosed));
static_assert(!GameplayHudShowsCompactInstructions3D(MinimalHudClosed,true));
static_assert(GameplayTemporalWarmupScheduleTick3D(240)==240);
static_assert(GameplayPresentationCaptureRequestDue3D(true,GameplayPresentationCaptureWarmupFrames3D,0));
static_assert(GameplayPresentationCaptureRequestDue3D(true,GameplayPresentationCaptureWarmupFrames3D,1));
static_assert(GameplayPresentationCaptureRequestDue3D(true,GameplayPresentationCaptureWarmupFrames3D,17));
static_assert(GameplayPresentationCaptureRequestDue3D(true,GameplayPresentationCaptureWarmupFrames3D,100));
static_assert(!GameplayPresentationCaptureRequestDue3D(false,GameplayPresentationCaptureWarmupFrames3D,0));
static_assert(!GameplayPresentationCaptureRequestDue3D(true,GameplayPresentationCaptureWarmupFrames3D-1,120));
static_assert(!GameplayPresentationCaptureRequestDue3D(true,GameplayPresentationCaptureWarmupFrames3D+1,0));
constexpr auto FullCaptureScopeMask=[] {std::array<bool,11> value{};value.fill(true);return value;}();
constexpr auto TargetedCaptureScopeMask=[] {std::array<bool,11> value{};value[1]=true;return value;}();
constexpr auto MissingTargetedCaptureScopeMask=[] {return std::array<bool,11>{};}();
constexpr auto ExtraTargetedCaptureScopeMask=[] {auto value=TargetedCaptureScopeMask;value[2]=true;return value;}();
constexpr auto MissingFullCaptureScopeMask=[] {auto value=FullCaptureScopeMask;value[10]=false;return value;}();
static_assert(ValidateGameplayCaptureEvidenceScopeMask3D(GameplayCaptureEvidenceScope3D::Full,FullCaptureScopeMask));
static_assert(ValidateGameplayCaptureEvidenceScopeMask3D(GameplayCaptureEvidenceScope3D::ThirdDayTargeted,TargetedCaptureScopeMask));
static_assert(!ValidateGameplayCaptureEvidenceScopeMask3D(GameplayCaptureEvidenceScope3D::ThirdDayTargeted,MissingTargetedCaptureScopeMask));
static_assert(!ValidateGameplayCaptureEvidenceScopeMask3D(GameplayCaptureEvidenceScope3D::ThirdDayTargeted,ExtraTargetedCaptureScopeMask));
static_assert(!ValidateGameplayCaptureEvidenceScopeMask3D(GameplayCaptureEvidenceScope3D::Full,MissingFullCaptureScopeMask));
static_assert(GameplayCottageFacadeExpectedTriangles3D(false,true)==72);
static_assert(GameplayCottageFacadeExpectedTriangles3D(true,true)==84);
static_assert(GameplayCottageFacadeExpectedTriangles3D(false,false)==48);
static_assert(GameplayCottageFacadeExpectedTriangles3D(true,false)==60);
static_assert(GameplayCottageFacadeExpectedTriangles3D(false,false)!=36);
static_assert(GameplayCottageFacadeExpectedTriangles3D(true,false)!=48);
static_assert(GameplayCottageWallNormalScale3D==.9f);
static_assert(!GameplayCottageMaterialUsesUv3D(false));
static_assert(GameplayCottageMaterialUsesUv3D(true));
static_assert(GameplayCottageBaseMaterialRole3D("procedural/cottage-foundation")==GameplayCottageMaterialRole3D::Brick);
static_assert(GameplayCottageBaseMaterialRole3D("procedural/cottage-front")==GameplayCottageMaterialRole3D::WoodSiding);
static_assert(GameplayCottageBaseMaterialRole3D("procedural/cottage-pitched-roof")==GameplayCottageMaterialRole3D::RoofTile);
static_assert(GameplayCottageDescriptorMaterialAsset3D(GameplayCottageMaterialRole3D::Brick)=="ambientcg/Bricks100/cooked-pbr");
static_assert(GameplayCottageDescriptorMaterialAsset3D(GameplayCottageMaterialRole3D::WoodSiding)=="ambientcg/WoodSiding006/material-pbr");
static_assert(GameplayCottageDescriptorMaterialAsset3D(GameplayCottageMaterialRole3D::RoofTile)=="ambientcg/RoofingTiles014B/cooked-pbr");
const bool ValidateGameplayCottageGlassGapSmoke=[](){if(!GameplayCottageGlassGapValid3D(27.74f,27.84f,.06f)||GameplayCottageGlassGapValid3D(27.74f,27.83f,.06f))throw std::runtime_error("cottage glass gap oracle failed");return true;}();
namespace {
std::optional<float> GameplayCottageSourceFloat3D(
    std::string_view source,std::string_view name){
  const auto namePosition=source.find(name);
  if(namePosition==std::string_view::npos)return std::nullopt;
  const auto equals=source.find('=',namePosition+name.size());
  const auto suffix=equals==std::string_view::npos?std::string_view::npos:
      source.find("f;",equals+1);
  if(equals==std::string_view::npos||suffix==std::string_view::npos)return std::nullopt;
  try{
    std::size_t consumed{};
    const std::string token(source.substr(equals+1,suffix-equals-1));
    const float value=std::stof(token,&consumed);
    if(consumed!=token.size()||!std::isfinite(value))return std::nullopt;
    return value;
  }catch(...){return std::nullopt;}
}
std::optional<std::uint32_t> GameplaySourceUnsigned3D(
    std::string_view source,std::string_view name){
  const auto namePosition=source.find(name);
  if(namePosition==std::string_view::npos)return std::nullopt;
  const auto equals=source.find('=',namePosition+name.size());
  const auto suffix=equals==std::string_view::npos?std::string_view::npos:
      source.find(';',equals+1);
  if(equals==std::string_view::npos||suffix==std::string_view::npos)return std::nullopt;
  try{
    std::size_t consumed{};
    const std::string token(source.substr(equals+1,suffix-equals-1));
    const auto value=std::stoul(token,&consumed);
    if(consumed!=token.size()||value>std::numeric_limits<std::uint32_t>::max())
      return std::nullopt;
    return std::uint32_t(value);
  }catch(...){return std::nullopt;}
}
bool ValidateGameplayCottageMaterialSource3D(std::string_view source){
  const auto woodRoughness=GameplayCottageSourceFloat3D(source,"GameplayCottageWoodRoughness3D");
  const auto woodNormal=GameplayCottageSourceFloat3D(source,"GameplayCottageWoodNormalScale3D");
  const auto woodScale=GameplayCottageSourceFloat3D(source,"GameplayCottageWoodTextureScale3D");
  const auto roofRoughness=GameplayCottageSourceFloat3D(source,"GameplayCottageRoofRoughness3D");
  const auto roofNormal=GameplayCottageSourceFloat3D(source,"GameplayCottageRoofNormalScale3D");
  const auto roofScale=GameplayCottageSourceFloat3D(source,"GameplayCottageRoofTextureScale3D");
  const auto brickRoughness=GameplayCottageSourceFloat3D(source,"GameplayCottageBrickRoughness3D");
  const auto brickNormal=GameplayCottageSourceFloat3D(source,"GameplayCottageBrickNormalScale3D");
  const auto brickScale=GameplayCottageSourceFloat3D(source,"GameplayCottageBrickTextureScale3D");
  if(!woodRoughness||!woodNormal||!woodScale||!roofRoughness||!roofNormal||
     !roofScale||!brickRoughness||!brickNormal||!brickScale)return false;
  const auto in=[](float value,float minimum,float maximum){
    return std::isfinite(value)&&value>=minimum&&value<=maximum;
  };
  if(!in(*woodRoughness,.60f,.76f)||!in(*roofRoughness,.70f,.88f)||
     !in(*brickRoughness,.72f,.90f)||!in(*woodNormal,.50f,.70f)||
     !in(*roofNormal,.55f,.80f)||!in(*brickNormal,.55f,.80f)||
     !in(*woodScale,.16f,.26f)||!in(*roofScale,.10f,.32f)||
     *roofScale!=.16f||
     !in(*brickScale,.20f,.32f))return false;
  // 正式 helper 必須在寫入前拒絕非有限值、越界回應與錯誤 UV 角色；
  // submission 及煙囪磚材都只能走同一個 fail-closed 入口。
  constexpr std::array<std::string_view,12> required{
      "!std::isfinite(response.roughness)",
      "!std::isfinite(response.normalScale)",
      "!std::isfinite(response.textureScale)",
      "response.useUv!=expectedUseUv",
      "GameplayCottageRoofTextureScale3D,true};",
      "in(response.textureScale,.10f,.32f)",
      "GameplayCottageBrickTextureScale3D,false};",
      "GameplayCottageWoodTextureScale3D,false};",
      "material.surface.roughness=response.roughness;",
      "material.useUv=response.useUv;",
      "if(!ApplyGameplayCottageMaterialResponse3D(material,role))break;",
      "if(!ApplyGameplayCottageMaterialResponse3D(brick,GameplayCottageMaterialRole3D::Brick))break;"};
  return std::ranges::all_of(required,[&](std::string_view token){
    return source.find(token)!=std::string_view::npos;
  });
}
bool GameplayCottageReplaceOnce3D(std::string& source,std::string_view from,
                                  std::string_view to){
  const auto position=source.find(from);
  if(position==std::string::npos||source.find(from,position+from.size())!=std::string::npos)
    return false;
  source.replace(position,from.size(),to);
  return true;
}
const bool ValidateGameplayCottageMaterialResponseSmoke=[](){
  const auto path=std::filesystem::path(AEVRYND_SOURCE_DIR)/"games/gameplay_lab/native/src/GameplayLabGame.cpp";
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("cottage material response source missing");
  const std::string source{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
  if(source.empty()||!ValidateGameplayCottageMaterialSource3D(source))
    throw std::runtime_error("cottage material response source contract failed");
  const auto rejects=[&](std::string_view from,std::string_view to){
    auto mutation=source;
    return GameplayCottageReplaceOnce3D(mutation,from,to)&&
           !ValidateGameplayCottageMaterialSource3D(mutation);
  };
  if(!rejects("GameplayCottageWoodRoughness3D=.68f","GameplayCottageWoodRoughness3D=.59f")||
     !rejects("GameplayCottageRoofRoughness3D=.78f","GameplayCottageRoofRoughness3D=.89f")||
     !rejects("GameplayCottageRoofNormalScale3D=.72f","GameplayCottageRoofNormalScale3D=.81f")||
     !rejects("GameplayCottageWoodTextureScale3D=.20f","GameplayCottageWoodTextureScale3D=.15f")||
      !rejects("GameplayCottageRoofTextureScale3D=.16f","GameplayCottageRoofTextureScale3D=.33f")||
      !rejects("in(response.textureScale,.10f,.32f)","in(response.textureScale,.20f,.32f)")||
     !rejects("GameplayCottageBrickTextureScale3D=.26f","GameplayCottageBrickTextureScale3D=.33f")||
     !rejects("GameplayCottageWoodNormalScale3D=.62f","GameplayCottageWoodNormalScale3D=NAN" )||
     !rejects("GameplayCottageRoofTextureScale3D,true};","GameplayCottageRoofTextureScale3D,false};")||
     !rejects("GameplayCottageWoodTextureScale3D,false};","GameplayCottageWoodTextureScale3D,true};")||
     !rejects("response.useUv!=expectedUseUv","false"))
    throw std::runtime_error("cottage material fail-closed mutation oracle failed");
  return true;
}();

bool ValidateGameplayR43CottageRoofTintSource3D(std::string_view source){
  constexpr std::array<std::string_view,8> contract{
      "GameplayCottageRoofTint3D",
      "{.54f,.31f,.19f,1.f}",
      "GameplayCottageMaterialRole3D::RoofTile?GameplayCottageRoofTint3D",
      "material.tint=",
      "GameplayCottageRoofTintLuminanceGate3D(tint)",
      "GameplayCottageRoofTintChromaGate3D(tint)",
      "std::isfinite(material.tint.r)",
      "value.a==1.f"};
  return std::ranges::all_of(contract,[&](std::string_view token){
    return source.find(token)!=std::string_view::npos;
  });
}

const bool ValidateGameplayR43CottageRoofTintSmoke=[](){
  const auto path=std::filesystem::path(AEVRYND_SOURCE_DIR)/
      "games/gameplay_lab/native/src/GameplayLabGame.cpp";
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("r43 cottage roof tint source missing");
  const std::string source{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
  if(!ValidateGameplayR43CottageRoofTintSource3D(source))
    throw std::runtime_error("r43 cottage roof tint contract failed");
  return true;
}();

bool ValidateGameplayR44CottageWindowLightingSource3D(std::string_view source){
  constexpr std::array<std::string_view,16> contract{
      "ApplyGameplayCottageWindowResponse3D",
      "LightingPhase::Day",
      "LightingPhase::Dusk",
      "LightingPhase::Night",
      "{.12f,.16f,.20f,1.f},.10f,.04f,0.f,1.f",
      "{.16f,.09f,.035f,1.f},.14f,.03f,.018f,1.f",
      "{.20f,.11f,.035f,1.f},.16f,.02f,.060f,1.f",
      "response.tint.r<0.f||response.tint.r>.25f",
      "response.tint.g<0.f||response.tint.g>.20f",
      "response.tint.b<0.f||response.tint.b>.25f",
      "response.emissive>=0.f&&response.emissive<=.08f",
      "response.opacity!=1.f",
      "material.surface.emissive=response.emissive;",
      "material.alphaMode=MaterialAlphaMode3D::Opaque;",
      "material.castsShadow=false;",
      "ApplyGameplayCottageWindowResponse3D(glass,rules_.Lighting())"};
  return std::ranges::all_of(contract,[&](std::string_view token){
    return source.find(token)!=std::string_view::npos;
  });
}

const bool ValidateGameplayR44CottageWindowLightingSmoke=[](){
  const auto path=std::filesystem::path(AEVRYND_SOURCE_DIR)/
      "games/gameplay_lab/native/src/GameplayLabGame.cpp";
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("r44 cottage window lighting source missing");
  const std::string source{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
  if(!ValidateGameplayR44CottageWindowLightingSource3D(source))
    throw std::runtime_error("r44 cottage window lighting contract failed");
  return true;
}();

bool ValidateGameplayR45CottageChimneyStoneSource3D(std::string_view source){
  constexpr std::array<std::string_view,9> contract{
      "ApplyGameplayCottageChimneyStoneResponse3D",
      "GameplayCottageChimneyStoneTint3D",
      "GameplayCottageChimneyStoneLuminanceGate3D",
      "GameplayCottageChimneyStoneChromaGate3D",
      "material.surface.roughness=.86f;",
      "material.surface.metallic=.015f;",
      "material.normalScale=.76f;",
      "material.textureScale=.22f;",
      "ApplyGameplayCottageChimneyStoneResponse3D(brick)"};
  return std::ranges::all_of(contract,[&](std::string_view token){
    return source.find(token)!=std::string_view::npos;
  });
}

const bool ValidateGameplayR45CottageChimneyStoneSmoke=[](){
  const auto path=std::filesystem::path(AEVRYND_SOURCE_DIR)/
      "games/gameplay_lab/native/src/GameplayLabGame.cpp";
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("r45 cottage chimney stone source missing");
  const std::string source{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
  if(!ValidateGameplayR45CottageChimneyStoneSource3D(source))
    throw std::runtime_error("r45 cottage chimney stone contract failed");
  return true;
}();

bool ValidateGameplayR46CottageFoundationStoneSource3D(std::string_view source){
  constexpr std::array<std::string_view,12> contract{
      "ApplyGameplayCottageFoundationStoneResponse3D",
      "GameplayCottageFoundationStoneTint3D",
      "GameplayCottageFoundationStoneLuminanceGate3D",
      "GameplayCottageFoundationStoneChromaGate3D",
      "material.surface.roughness=.88f;",
      "material.surface.metallic=0.f;",
      "material.normalScale=.72f;",
      "material.textureScale=.24f;",
      "role==GameplayCottageMaterialRole3D::Brick&&",
      "ApplyGameplayCottageFoundationStoneResponse3D(material)",
      "ApplyGameplayCottageFoundationStoneResponse3D(thresholdStone)",
      "detail.brick?thresholdStone:showcaseWoodMaterial_"};
  return std::ranges::all_of(contract,[&](std::string_view token){
    return source.find(token)!=std::string_view::npos;
  });
}

const bool ValidateGameplayR46CottageFoundationStoneSmoke=[](){
  const auto path=std::filesystem::path(AEVRYND_SOURCE_DIR)/
      "games/gameplay_lab/native/src/GameplayLabGame.cpp";
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("r46 cottage foundation stone source missing");
  const std::string source{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
  if(!ValidateGameplayR46CottageFoundationStoneSource3D(source))
    throw std::runtime_error("r46 cottage foundation stone contract failed");
  return true;
}();

bool ValidateGameplayR34CottageAndPaletteSource3D(std::string_view source){
  const auto block=[&](std::string_view begin,std::string_view end){
    const auto first=source.find(begin);
    const auto last=first==std::string_view::npos?std::string_view::npos:
        source.find(end,first+begin.size());
    return first==std::string_view::npos||last==std::string_view::npos?
        std::string_view{}:source.substr(first,last+end.size()-first);
  };
  const auto cottage=block("R34_COTTAGE_LIFE_BEGIN","R34_COTTAGE_LIFE_END");
  const auto palette=block("R34_PALETTE_BUILDING_BEGIN","R34_PALETTE_BUILDING_END");
  if(cottage.empty()||palette.empty())return false;

  // Exactly twelve opaque, render-only details share loaded cottage materials.
  constexpr std::array<std::string_view,20> cottageContract{
      "std::array<CottageLifeDetail3D,12>","showcaseCylinderMesh_",
      "boxMesh_","showcaseWoodMaterial_","*brickSource",
      "scene_.Mesh(detail.cylindrical?showcaseCylinderMesh_:boxMesh_",
      "renderIdentity","{-15.60f,.22f,19.72f}","{-15.60f,.22f,20.08f}",
      "{-15.35f,.50f,19.90f}","{-15.02f,.22f,19.72f}",
      "{-15.02f,.22f,20.08f}","{-8.85f,.58f,19.78f}",
      "{-14.55f,.42f,19.92f}","{-13.86f,.32f,20.00f}",
      "{-12.f,.12f,19.72f}","lifeDetails.size()!=12",
      "TransformBounds(resource.bounds,detail.transform)",
      "Cottage life detail exceeded cottage footprint",
      "scene_.MeshCount()!=lifeDrawsBefore+lifeDetails.size()"};
  if(!std::ranges::all_of(cottageContract,[&](std::string_view token){
       return cottage.find(token)!=std::string_view::npos;
     })||cottage.find("physics_")!=std::string_view::npos||
     cottage.find("MaterialBox")!=std::string_view::npos||
     cottage.find("textures={}")!=std::string_view::npos)return false;

  // The palette implementation itself must route body/preview yaw through one
  // shared-mesh builder, retain identity and retain valid/invalid feedback.
  constexpr std::array<std::string_view,13> paletteContract{
      "submitPaletteBuilding","showcaseWoodMaterial_",
      "GameplayPaletteBuildingTransform3D(center,yaw,transform)",
      "scene_.Mesh(showcaseConeHighMesh_,transform,wood,{},identity)",
      "scene_.MeshCount()==before+GameplayPaletteBuildingDrawCap3D",
      "placed.category==PaletteCategory::Building",
      "body->settings.transform.rotation.y",
      "preview.category==PaletteCategory::Building","preview.rotation",
      "preview.valid ? Color{.72f,.92f,.72f,1.f}",
      "Color{1.f,.60f,.55f,1.f}","{{GameplayPlacedIdentityHigh,placed.id},1}",
      "{{GameplayPreviewIdentityHigh,1},1}"};
  constexpr std::array<std::string_view,5> mergedMeshContract{
      "CreateGameplayPaletteBuildingMesh3D()",
      "mesh.vertices.size()!=138","mesh.indices.size()!=204",
      "mesh.indices.size()/3!=68",
      "Palette building merged mesh bounds/topology gate failed"};
  constexpr std::array<std::string_view,15> captureGhostContract{
      "GameplayPaletteCaptureGhostMinimumSeparation3D=1.5f",
      "GameplayPaletteCaptureGhostsDistinct3D(*captureValidGhost_,*invalidPoint)",
      "const auto reservation=collisions_.CreateBox",
      "minimumCapturePaletteRayDistance=4.f",
      "camera_,collisions_,minimumCapturePaletteRayDistance",
      "collisions_.Destroy(reservation)",
      "!palette_->PreviewAt(*invalidPoint,PaletteCategory::Building)",
      "palette_->PreviewAt(*invalidPoint, PaletteCategory::Building)",
      "GameplayPaletteCaptureGhostsDistinct3D(*captureValidGhost_,*captureInvalidGhost_)",
      "!collisions_.Destroy(captureEvidenceCollider_)",
      "captureEvidenceCollider_={}","captureValidGhost_.reset()",
      "captureInvalidGhost_.reset()","captureEvidenceColliderReleased_=true",
      "evidenceColliderHandleCleared"};
  if(!std::ranges::all_of(paletteContract,[&](std::string_view token){
       return palette.find(token)!=std::string_view::npos;
     })||!std::ranges::all_of(mergedMeshContract,[&](std::string_view token){
       return source.find(token)!=std::string_view::npos;
     })||!std::ranges::all_of(captureGhostContract,[&](std::string_view token){
       return source.find(token)!=std::string_view::npos;
     })||palette.find("CreateMeshResource3D")!=std::string_view::npos||
     palette.find("CreateTexture")!=std::string_view::npos)return false;
  std::size_t identityUses{};
  for(auto at=palette.find(",identity)");at!=std::string_view::npos;
      at=palette.find(",identity)",at+1))++identityUses;
  if(identityUses!=1||source.find("GameplayPaletteBuildingGeometryRuntimeGate3D")==
      std::string_view::npos)return false;

  // Parse the implementation's actual builder constants, then execute its
  // footprint mathematics across all 15-degree palette yaw steps. This makes
  // bounds, finite transforms, yaw propagation and the per-building draw cap
  // behavioral gates rather than source-token presence checks.
  const auto offsetX=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingOffsetX3D");
  const auto offsetY=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingOffsetY3D");
  const auto offsetZ=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingOffsetZ3D");
  const auto scaleX=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingScaleX3D");
  const auto scaleY=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingScaleY3D");
  const auto scaleZ=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingScaleZ3D");
  const auto minimumX=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingLocalMinimumX3D");
  const auto minimumY=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingLocalMinimumY3D");
  const auto minimumZ=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingLocalMinimumZ3D");
  const auto maximumX=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingLocalMaximumX3D");
  const auto maximumY=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingLocalMaximumY3D");
  const auto maximumZ=GameplayCottageSourceFloat3D(source,"GameplayPaletteBuildingLocalMaximumZ3D");
  const auto drawCap=GameplaySourceUnsigned3D(source,"GameplayPaletteBuildingDrawCap3D");
  if(!offsetX||!offsetY||!offsetZ||!scaleX||!scaleY||!scaleZ||!drawCap||
     !minimumX||!minimumY||!minimumZ||!maximumX||!maximumY||!maximumZ||
     *drawCap!=1||std::uint64_t(*drawCap)*256u!=256u)return false;
  const float builtWidthX=(*maximumX-*minimumX)**scaleX;
  const float builtWidthZ=(*maximumZ-*minimumZ)**scaleZ;
  const float builtHeight=(*maximumY-*minimumY)**scaleY;
  if(builtWidthX<.65f||builtWidthZ<.65f||builtHeight<.75f||
     builtWidthX>1.f||builtWidthZ>1.f||builtHeight>1.f)return false;
  float zeroX{},quarterX{};
  for(unsigned step=0;step<24;++step){
    const float yaw=float(step)*.2617993878f;
    const float sine=std::sin(yaw),cosine=std::cos(yaw);
    const float x=cosine**offsetX+sine**offsetZ;
    const float z=-sine**offsetX+cosine**offsetZ;
    const float localExtentX=(*maximumX-*minimumX)*.5f;
    const float localExtentZ=(*maximumZ-*minimumZ)*.5f;
    const float extentX=std::abs(cosine)*localExtentX**scaleX+
        std::abs(sine)*localExtentZ**scaleZ;
    const float extentZ=std::abs(sine)*localExtentX**scaleX+
        std::abs(cosine)*localExtentZ**scaleZ;
    const float builtMinimumY=*offsetY+*minimumY**scaleY;
    const float builtMaximumY=*offsetY+*maximumY**scaleY;
    for(float component:{x,*offsetY,z,yaw,extentX,extentZ,builtMinimumY,builtMaximumY})
      if(!std::isfinite(component))return false;
    if(std::abs(x)+extentX>.50001f||std::abs(z)+extentZ>.50001f||
       builtMinimumY<-.50001f||builtMaximumY>.50001f)return false;
    if(step==0)zeroX=x;if(step==6)quarterX=x;
  }
  return std::abs(zeroX-quarterX)>.01f;
}
const bool ValidateGameplayR34CottageAndPaletteSmoke=[](){
  const auto path=std::filesystem::path(AEVRYND_SOURCE_DIR)/
      "games/gameplay_lab/native/src/GameplayLabGame.cpp";
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("r34 cottage/palette source missing");
  const std::string source{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
  if(!ValidateGameplayR34CottageAndPaletteSource3D(source))
    throw std::runtime_error("r34 cottage/palette geometry or identity contract failed");
  return true;
}();

bool ValidateGameplayR42CottageLanternSource3D(std::string_view source){
  const auto begin=source.find("R42_COTTAGE_LANTERN_BEGIN");
  const auto end=begin==std::string_view::npos?std::string_view::npos:
      source.find("R42_COTTAGE_LANTERN_END",begin);
  if(begin==std::string_view::npos||end==std::string_view::npos)return false;
  const auto lantern=source.substr(begin,end+std::string_view{
      "R42_COTTAGE_LANTERN_END"}.size()-begin);
  constexpr std::array<std::string_view,18> contract{
      "GameplayCottagePorchLanternCenter3D",
      "GameplayCottageLanternEmissive3D",
      "case LightingPhase::Day:return 0.f;",
      "case LightingPhase::Dusk:return .72f;",
      "case LightingPhase::Night:return 1.75f;",
      "GameplayCottageLanternEmissive3D(rules_.Lighting())",
      "std::array<CottageLanternPart3D,7>",
      "lanternMetal.textures={}","lanternGlass.textures={}",
      "lanternGlass.castsShadow=false","showcaseCylinderMesh_",
      "boxMesh_","renderIdentity","part.glass?lanternGlass:lanternMetal",
      "lanternParts.size()!=7","scene_.MeshCount()!=lanternDrawsBefore+lanternParts.size()",
      "Cottage lantern draw count gate failed","Cottage lantern part exceeded fixture bounds"};
  return std::ranges::all_of(contract,[&](std::string_view token){
    return lantern.find(token)!=std::string_view::npos;
  })&&lantern.find("lanternGlass.surface={.12f,.04f,1.75f,.92f}")==
          std::string_view::npos&&
      lantern.find("CreateTexture") == std::string_view::npos&&
      lantern.find("physics_") == std::string_view::npos;
}

const bool ValidateGameplayR42CottageLanternSmoke=[](){
  const auto path=std::filesystem::path(AEVRYND_SOURCE_DIR)/
      "games/gameplay_lab/native/src/GameplayLabGame.cpp";
  std::ifstream stream(path,std::ios::binary);
  if(!stream)throw std::runtime_error("r42 cottage lantern source missing");
  const std::string source{std::istreambuf_iterator<char>(stream),
                           std::istreambuf_iterator<char>()};
  if(!ValidateGameplayR42CottageLanternSource3D(source))
    throw std::runtime_error("r42 cottage lantern geometry or identity contract failed");
  return true;
}();
}
namespace {struct CanonicalStateMutationSmoke{CanonicalStateMutationSmoke(){GameplayCanonicalCaptureStateInput3D state{};state.captureIndex=2;state.entities={{9,PaletteCategory::Props,{1,2,3},.2f,true,true},{3,PaletteCategory::Building,{4,5,6},.4f,false,true}};state.controllerPosition={7,8,9};state.yaw=.3f;state.pitch=-.2f;state.camera=CameraMode::ThirdPerson;state.lighting=LightingPhase::Dusk;state.paletteCategory=PaletteCategory::Building;state.female=true;state.paletteOpen=true;state.grabbedEntity=9;state.cameraHash=41;state.animationHash=42;state.physicsHash=43;const auto canonical=BuildGameplayCanonicalCaptureState3D(state);if(!canonical.valid||canonical.entityDiagnostics.size()!=2||canonical.entityDiagnostics[0].stableId!=3||canonical.entityDiagnostics[1].stableId!=9)throw std::runtime_error("canonical entity diagnostic contract failed");auto permuted=state;std::ranges::reverse(permuted.entities);if(canonical.canonical!=BuildGameplayCanonicalCaptureState3D(permuted).canonical)throw std::runtime_error("canonical entity permutation changed state");auto signedZero=state;signedZero.entities[0].position.x=-0.f;auto positiveZero=signedZero;positiveZero.entities[0].position.x=0.f;if(BuildGameplayCanonicalCaptureState3D(signedZero).canonical!=BuildGameplayCanonicalCaptureState3D(positiveZero).canonical)throw std::runtime_error("canonical signed zero was not normalized");const auto changed=[&](auto mutate){auto value=state;mutate(value);const auto receipt=BuildGameplayCanonicalCaptureState3D(value);return receipt.valid&&receipt.canonical!=canonical.canonical;};if(!changed([](auto&v){v.entities[0].id++;})||!changed([](auto&v){v.entities[0].category=PaletteCategory::Lights;})||!changed([](auto&v){v.entities[0].position.x=std::nextafter(v.entities[0].position.x,std::numeric_limits<float>::infinity());})||!changed([](auto&v){v.entities[0].position.z+=2.f;})||!changed([](auto&v){v.controllerPosition.y+=.01f;})||!changed([](auto&v){v.yaw+=.01f;})||!changed([](auto&v){v.pitch+=.01f;})||!changed([](auto&v){v.lighting=LightingPhase::Night;})||!changed([](auto&v){v.cameraHash++;})||!changed([](auto&v){v.paletteOpen=false;})||!changed([](auto&v){v.animationHash++;})||!changed([](auto&v){v.physicsHash++;}))throw std::runtime_error("canonical state mutation was not observed");for(const float invalid:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){auto value=state;value.entities[0].position.x=invalid;if(BuildGameplayCanonicalCaptureState3D(value).valid)throw std::runtime_error("canonical non-finite entity was accepted");value=state;value.entities[0].rotation=invalid;if(BuildGameplayCanonicalCaptureState3D(value).valid)throw std::runtime_error("canonical non-finite rotation was accepted");}}} canonicalStateMutationSmoke;}
static_assert(GameplayTemporalCaptureReady3D(true,true,0));
static_assert(!GameplayTemporalCaptureReady3D(false,true,0)&&!GameplayTemporalCaptureReady3D(true,false,0)&&!GameplayTemporalCaptureReady3D(true,true,1));
static_assert(GameplayCaptureWarmupMayBegin3D(true,0));
static_assert(!GameplayCaptureWarmupMayBegin3D(false,0));
static_assert(!GameplayCaptureWarmupMayBegin3D(true,1));
static_assert(!GameplayCaptureWarmupMayBegin3D(true,2));
// Engine 可在單一 outer frame 內先做多次 fixed update，再只做一次 render。
// 不論 preparation 後是立刻 render，或先多做一次 update，兩條排程都必須
// 等到新 route 的 scene submission 被觀察後，才能啟動 warmup。
static_assert([]{
  bool submitted{},warmup{};
  if(GameplayCaptureWarmupMayBegin3D(submitted,0))warmup=true; // 額外 fixed update
  if(warmup)return false;
  submitted=true; // 唯一 render / scene submission
  if(GameplayCaptureWarmupMayBegin3D(submitted,1))warmup=true;
  if(warmup)return false;
  if(GameplayCaptureWarmupMayBegin3D(submitted,0))warmup=true;
  return warmup;
}());
static_assert([]{
  bool submitted{},warmup{};
  submitted=true; // preparation 後直接 render
  if(GameplayCaptureWarmupMayBegin3D(submitted,0))warmup=true;
  return warmup;
}());
static_assert(GameplayUseFrozenPose3D(true,true,false,false,false));
static_assert(!GameplayUseFrozenPose3D(false,true,true,true,true)&&!GameplayUseFrozenPose3D(true,false,false,false,false));
static_assert(GameplayGodCameraRequiresCaptureLayout3D(true,GameplayCaptureCamera3D::God));
static_assert(!GameplayGodCameraRequiresCaptureLayout3D(false,GameplayCaptureCamera3D::God)&&!GameplayGodCameraRequiresCaptureLayout3D(true,GameplayCaptureCamera3D::First));
static_assert((GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::CameraMode)|GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::Character)|GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::PoseState)|GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::ViewProjection)|GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::DrawIdentity)|GameplayTemporalReasonMask3D(GameplayTemporalReseedReason3D::CaptureShot))==63u);
static_assert(GameplayReferenceBuildingRemovalThreshold3D==.08f);
static_assert(GameplayReferenceBuildingOverridePlan(.08f).mode==GameplayReferenceBuildingOverridePlan3D::Mode::NoOverride&&GameplayReferenceBuildingOverridePlan(.08f).RequestedCount()==0);
static_assert(GameplayReferenceBuildingOverridePlan(.08001f).mode==GameplayReferenceBuildingOverridePlan3D::Mode::RemoveAll&&GameplayReferenceBuildingOverridePlan(.08001f).RequestedCount()==6&&GameplayReferenceBuildingOverridePlan(.08001f).opacity==0.f);
constexpr bool ValidateGameplayProductionRouteMatrix(){std::array<std::array<bool,3>,3> covered{};for(std::size_t index=0;index<9;++index){const auto route=BuildGameplayCaptureProductionRoute3D(GameplayCaptureExpectations3D[index]);const auto camera=std::size_t(route.camera),lighting=std::size_t(route.lighting);if(camera>=3||lighting>=3||covered[camera][lighting]||(route.camera==CameraMode::GodView)!=(route.projection==ProjectionMode::Orthographic))return false;covered[camera][lighting]=true;}for(const auto& row:covered)for(bool value:row)if(!value)return false;return true;}
static_assert(ValidateGameplayProductionRouteMatrix());
constexpr bool RejectWrongGameplayProjectionRoute(){auto wrong=BuildGameplayCaptureProductionRoute3D(GameplayCaptureExpectations3D[2]);wrong.projection=ProjectionMode::Perspective;return !ValidateGameplayCaptureProductionRoute3D(wrong);}
static_assert(RejectWrongGameplayProjectionRoute());
bool ValidateGameplayProductionProjectionSmoke(){for(std::size_t index=0;index<9;++index){const auto route=BuildGameplayCaptureProductionRoute3D(GameplayCaptureExpectations3D[index]);Camera3D camera{{0,3,-8},{0,1,0},{0,1,0},{}};camera.projection.orthographicHeight=route.projection==ProjectionMode::Orthographic?26.f:0.f;if(!ApplyGameplayCaptureProductionProjection3D(camera,route))return false;const auto matrix=CreateViewProjectionMatrix(camera,1280.f/720.f);auto wrongMatrix=matrix;wrongMatrix[0][0]+=.01f;if(!ValidateGameplayCaptureProductionProjectionMatrix3D(camera,route,1280.f/720.f,matrix)||ValidateGameplayCaptureProductionProjectionMatrix3D(camera,route,1280.f/720.f,Matrix4{})||ValidateGameplayCaptureProductionProjectionMatrix3D(camera,route,1280.f/720.f,wrongMatrix))return false;auto wrongRoute=route;wrongRoute.projection=route.projection==ProjectionMode::Orthographic?ProjectionMode::Perspective:ProjectionMode::Orthographic;if(ValidateGameplayCaptureProductionProjectionMatrix3D(camera,wrongRoute,1280.f/720.f,matrix))return false;if(route.projection==ProjectionMode::Orthographic){auto wrongHeight=camera;wrongHeight.projection.orthographicHeight=0;if(ValidateGameplayCaptureProductionProjectionMatrix3D(wrongHeight,route,1280.f/720.f,CreateViewProjectionMatrix(wrongHeight,1280.f/720.f)))return false;}}return true;}
const bool GameplayPostureCameraCacheContract=[](){
  const auto root=std::filesystem::path(AEVRYND_SOURCE_DIR)/"games/gameplay_lab/native";
  const auto read=[](const std::filesystem::path& path){std::ifstream stream(path,std::ios::binary);return std::string(std::istreambuf_iterator<char>(stream),std::istreambuf_iterator<char>());};
  const std::string header=read(root/"include/GameplayLabGame.h");
  const std::string source=read(root/"src/GameplayLabGame.cpp");
  constexpr std::string_view member="GameplayLab::PostureState cameraBuildCachePosture_{GameplayLab::PostureState::Standing}";
  constexpr std::string_view comparison="rules_.Posture()!=cameraBuildCachePosture_";
  constexpr std::string_view update="cameraBuildCachePosture_=rules_.Posture()";
  const auto occurrences=[&](std::string_view token){std::size_t count{},position{};while((position=source.find(token,position))!=std::string::npos){++count;position+=token.size();}return count;};
  if(header.find(member)==std::string::npos||occurrences(comparison)!=2||occurrences(update)!=1||
     source.find("cameraBuildCacheCrouched_")!=std::string::npos)
    throw std::runtime_error("posture camera cache must distinguish standing, crouched and prone");
  return true;
}();

const bool GameplayPaletteCameraRouteRegression=[](){Rules3D rules;TickInput toggle{};toggle.togglePalette=true;const auto ticks=rules.GetStats().ticks;if(!rules.ApplyPalettePresentationInput(toggle)||!rules.PaletteOpen()||rules.GetStats().ticks!=ticks)throw std::runtime_error("palette camera route did not open without simulation");if(rules.ApplyPalettePresentationInput({})||!rules.PaletteOpen())throw std::runtime_error("palette camera route accepted missing toggle");auto mixed=toggle;mixed.attack=true;if(rules.ApplyPalettePresentationInput(mixed)||!rules.PaletteOpen())throw std::runtime_error("palette camera route accepted mixed simulation input");if(!rules.ApplyPalettePresentationInput(toggle)||rules.PaletteOpen()||rules.GetStats().ticks!=ticks)throw std::runtime_error("palette camera route did not close without simulation");return true;}();
const bool GameplayGodConstraintClosureRegression=[](){
  GameplayShowcaseEnvironment3D environment;if(!environment.Initialize(0x4a17u))throw std::runtime_error("god resolver fixture environment failed");
  const auto grass=CreateGrassClumpMesh3D(11,3).Bounds();const auto fixture=Mesh3D::CreateUnitBox();const Vec3 focus{-16,1.4f,8.73334f};const Matrix4 world=CreateTransformMatrix(focus-Vec3{0,.4f,0},{},{.7f,2.f,.5f});
  const auto selected=SelectGameplayGodCameraLayout3D(focus,environment.Descriptors(),grass,fixture.vertices,fixture.indices,world);const auto geometry=LegacyGameplayGodProductionGeometry3D(grass);const auto identity=[](Camera3D camera){return camera;};
  const auto resolved=ResolveGameplayGodCameraLayout3D(focus,selected,environment.Descriptors(),geometry,fixture.vertices,fixture.indices,world,identity,identity);const auto repeated=ResolveGameplayGodCameraLayout3D(focus,resolved,environment.Descriptors(),geometry,fixture.vertices,fixture.indices,world,identity,identity);
  if(!resolved.valid||!repeated.valid||resolved.camera.position.x!=repeated.camera.position.x||resolved.camera.position.y!=repeated.camera.position.y||resolved.camera.position.z!=repeated.camera.position.z||resolved.camera.projection.orthographicHeight!=repeated.camera.projection.orthographicHeight)throw std::runtime_error("god resolver deterministic closure failed");
  // 非冪等約束模擬舊版候選每次重驗都漂移；固定點契約必須精確拒絕，不能靠放寬容差通過。
  const auto drifting=[](Camera3D camera){camera.position.x+=.01f;camera.target.x+=.01f;return camera;};if(ResolveGameplayGodCameraLayout3D(focus,selected,environment.Descriptors(),geometry,fixture.vertices,fixture.indices,world,drifting,identity).valid)throw std::runtime_error("god resolver accepted drifting constraint mutation");return true;
}();
int main(){if(!ValidateSwordTipConsumerSmoke())return 90;if(!ValidateGameplayProductionProjectionSmoke())return 66;Rules3D r;r.SetCamera(CameraMode::FirstPerson);r.SetCamera(CameraMode::ThirdPerson);r.SetCamera(CameraMode::GodView);r.SetHeadroom(false);r.Tick({{},false,false,true});r.Tick({{},false,false,true});if(!r.Crouched()||r.GetStats().headroomRejected!=1)return 1;r.SetHeadroom(true);r.Tick({{},false,false,true});for(int i=0;i<60;++i)r.Tick({{0,0,1},true,i==0});if(r.CharacterPosition().z<6||r.GetStats().jumps!=1)return 2;for(unsigned c=0;c<6;++c){r.SetPaletteCategory(PaletteCategory(c));if(!r.Place({float(c)+.3f,0,float(c)+.7f},c==1))return 3;}if(r.Entities().size()!=6||!r.Undo()||!r.Redo())return 4;if(!r.BeginGrab(r.Entities()[1].id,7.9f)||r.BeginGrab(r.Entities()[1].id,1)||!r.Grabbed())return 5;r.EndGrab();if(r.BeginGrab(r.Entities()[1].id,8.1f))return 6;r.SetLighting(LightingPhase::Day);auto d=r.EvaluateLighting();r.SetLighting(LightingPhase::Night);auto n=r.EvaluateLighting();if(d.intensity<=n.intensity||n.exposure>=d.exposure)return 8;for(std::size_t i=r.Entities().size();i<Rules3D::MaximumEntities;++i)if(!r.Place({float(i%16),0,float(i/16)}))return 9;if(r.Place({0,0,0})||r.GetStats().placementRejected!=1)return 10;
if(GameplayCaptureProjectionRecordingActive3D(false,0,11)||!GameplayCaptureProjectionRecordingActive3D(true,0,11)||GameplayCaptureProjectionRecordingActive3D(true,11,11))return 58;Rules3D captureStaging;captureStaging.Reset();PrepareGameplayCaptureStaging3D(captureStaging,true);if(captureStaging.Camera()!=CameraMode::FirstPerson)return 59;Rules3D interactiveStaging;interactiveStaging.Reset();const auto interactiveCamera=interactiveStaging.Camera();PrepareGameplayCaptureStaging3D(interactiveStaging,false);if(interactiveStaging.Camera()!=interactiveCamera)return 60;ProductionCameraCandidate3D settleCandidate{};settleCandidate.candidateId={7,9};settleCandidate.yawOffsetRadians=-.25f;settleCandidate.pitchRadians=.28f;settleCandidate.verticalElevation=.45f;settleCandidate.springArmDistance=3.8f;CameraOcclusionPolicyOutput3D settled{};settled.valid=true;settled.candidateId=settleCandidate.candidateId;settled.yawOffsetRadians=settleCandidate.yawOffsetRadians;settled.pitchRadians=settleCandidate.pitchRadians;settled.verticalElevation=settleCandidate.verticalElevation;settled.arm=settleCandidate.springArmDistance;if(!GameplayCameraSelectionSettled3D(settled,settleCandidate)||GameplayCameraSelectionSettled3D(CameraOcclusionPolicyOutput3D{},settleCandidate)){return 61;}settled.yawOffsetRadians+=.002f;if(GameplayCameraSelectionSettled3D(settled,settleCandidate))return 62;
if(!ValidateGameplayCaptureExpectationTable3D()||GameplayCaptureExpectations3D.size()!=11)return 11;std::size_t terrainViews{},transitionViews{},firstViews{};for(const auto& expectation:GameplayCaptureExpectations3D){if(expectation.camera==GameplayCaptureCamera3D::First){++firstViews;if(expectation.requiredIdentityCount!=2||expectation.requiredIdentities[0]!=GameplayCaptureFirstPersonArmsIdentity3D||expectation.requiredIdentities[1]!=GameplayCaptureWeaponIdentity3D||expectation.forbiddenIdentityCount!=2)return 12;}else if(expectation.requiredIdentities[0]!=GameplayCapturePlayerIdentity3D||expectation.forbiddenIdentities[0]!=GameplayCaptureForbiddenPlayerGeneration3D)return 12;transitionViews+=expectation.lodExpectation==GameplayCaptureLodExpectation3D::Transition?1u:0u;for(std::size_t index=0;index<expectation.requiredIdentityCount;++index)terrainViews+=expectation.requiredIdentities[index]==GameplayCaptureTerrainIdentity3D?1u:0u;}if(terrainViews!=8||transitionViews!=11||firstViews!=3)return 13;const auto byName=[](std::wstring_view name)->const GameplayCaptureExpectation3D*{const auto found=std::ranges::find(GameplayCaptureExpectations3D,name,&GameplayCaptureExpectation3D::name);return found==GameplayCaptureExpectations3D.end()?nullptr:&*found;};const auto* godDay=byName(L"god-day.png");const auto* godDusk=byName(L"god-dusk.png");const auto* godNight=byName(L"god-night.png");const auto* firstDay=byName(L"first-day.png");const auto* melee=byName(L"melee-combat.png");if(!godDay||!godDusk||!godNight||!firstDay||!melee||godDay->lodExpectation!=GameplayCaptureLodExpectation3D::Transition||godDusk->lodExpectation!=GameplayCaptureLodExpectation3D::Transition||godNight->lodExpectation!=GameplayCaptureLodExpectation3D::Transition||firstDay->lodExpectation!=GameplayCaptureLodExpectation3D::Transition||melee->lodExpectation!=GameplayCaptureLodExpectation3D::Transition)return 12;
auto third=GameplayCaptureExpectations3D[1];third.lodExpectation=GameplayCaptureLodExpectation3D::Stable;std::uint32_t required{},forbidden{},conflicts{};ObjectIdCapture3D semantic{};semantic.tokenMap={GameplayCapturePlayerIdentity3D,GameplayCaptureTerrainIdentity3D};semantic.semanticCoverage={{GameplayCapturePlayerIdentity3D,40,.4},{GameplayCaptureTerrainIdentity3D,60,.6}};if(!EvaluateGameplayCaptureSemantic3D(third,semantic,GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Day,required,forbidden,conflicts)||required!=2||forbidden||conflicts)return 14;semantic.semanticCoverage[1].nonzeroPixelCount=0;if(EvaluateGameplayCaptureSemantic3D(third,semantic,GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Day,required,forbidden,conflicts)||required!=1)return 15;semantic.tokenMap[1]=GameplayCaptureForbiddenPlayerGeneration3D;semantic.semanticCoverage[1]={GameplayCaptureForbiddenPlayerGeneration3D,0,0};if(EvaluateGameplayCaptureSemantic3D(third,semantic,GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Day,required,forbidden,conflicts)||forbidden!=1||conflicts!=1)return 16;if(EvaluateGameplayCaptureSemantic3D(third,semantic,GameplayCaptureCamera3D::First,GameplayCaptureLighting3D::Day,required,forbidden,conflicts))return 17;semantic.tokenMap={GameplayCapturePlayerIdentity3D,GameplayCaptureTerrainIdentity3D,RenderObjectIdentity3D{GameplayCaptureTerrainIdentity3D.id,9}};semantic.semanticCoverage={{GameplayCapturePlayerIdentity3D,40,.4},{GameplayCaptureTerrainIdentity3D,60,.6},{semantic.tokenMap[2],5,.05}};if(EvaluateGameplayCaptureSemantic3D(third,semantic,GameplayCaptureCamera3D::Third,GameplayCaptureLighting3D::Day,required,forbidden,conflicts)||conflicts!=1)return 20;
GameplayCaptureScenePhaseLedger3D ledger{};const ReadbackCaptureSequenceToken3D token{7};Scene3DStats phase{};phase.submissionSerial=11;phase.drawListRevision=12;phase.acceptedIdentityOrderFingerprint=13;if(!ledger.Begin(token,2,3)||!ledger.Record(token,2,0,phase)||ledger.Record(token,2,0,phase)||ledger.Record({8},2,1,phase)||ledger.Record(token,3,1,phase)||ledger.Resolve(token,2,1)||ledger.Resolve({8},2,0)!=nullptr||ledger.Resolve(token,2,0)->submissionSerial!=11)return 21;phase.submissionSerial=14;phase.drawListRevision=16910598885440962772ull;if(!ledger.Record(token,2,1,phase)||ledger.Resolve(token,2,1)->submissionSerial!=14||ledger.Resolve(token,2,0)->submissionSerial!=11)return 22;ObjectIdCaptureRequestMetadata3D meleeMetadata{7547354437417462245ull,31,32,ReadbackEmptyReason3D::None,{token,1},33};if(!ledger.BindRequest(token,2,1,meleeMetadata)||meleeMetadata.drawListRevision!=16910598885440962772ull)return 27;auto invalidMetadata=meleeMetadata;invalidMetadata.sequence.index=0;if(ledger.BindRequest(token,2,1,invalidMetadata)||ledger.BindRequest({8},2,1,meleeMetadata)||ledger.BindRequest(token,3,1,meleeMetadata))return 28;invalidMetadata=meleeMetadata;invalidMetadata.cameraHash=0;if(ledger.BindRequest(token,2,1,invalidMetadata))return 29;if(!GameplayCapturePhaseRequestsAtomic3D(true,true,true)||GameplayCapturePhaseRequestsAtomic3D(false,true,true)||GameplayCapturePhaseRequestsAtomic3D(true,false,true)||GameplayCapturePhaseRequestsAtomic3D(true,true,false))return 30;ledger.Clear();if(ledger.Resolve(token,2,0)||ledger.BindRequest(token,2,1,meleeMetadata))return 23;
const auto materialEvidence=BuildGameplayCharacterMaterialEvidence3D(9,9);if(!materialEvidence.Complete()||materialEvidence.femaleCharacterMaterialCount!=9||materialEvidence.maleCharacterMaterialCount!=9||materialEvidence.characterMaterialCount!=18||BuildGameplayCharacterMaterialEvidence3D(0,9).Complete()||BuildGameplayCharacterMaterialEvidence3D((std::numeric_limits<std::uint32_t>::max)(),1).Complete())return 30;
if(!GameplayCaptureQueuedPhaseActive3D(true,true)||GameplayCaptureQueuedPhaseActive3D(false,true)||GameplayCaptureQueuedPhaseActive3D(true,false))return 31;DrawList queuedRaw;for(std::uint64_t stableLow=151;stableLow<=156;++stableLow){MeshInstance3D instance{};instance.identity={{0x454e5649524f4e4dull,stableLow},1};instance.occlusionOpacity=1.f;queuedRaw.meshInstances.push_back(instance);}auto queuedEffective=queuedRaw;const auto effectiveUpdate=queuedEffective.SetObjectOcclusionOpacity3D({{0x454e5649524f4e4dull,154},1},.35f);auto queuedNoOverrideReference=queuedEffective,queuedRemoveAllReference=queuedEffective;const auto noOverride=GameplayReferenceBuildingOverridePlan(.08f),removeAll=GameplayReferenceBuildingOverridePlan(.08001f);std::size_t referenceMatches{};if(noOverride.Removes())return 32;if(removeAll.Removes())for(std::uint64_t stableLow=151;stableLow<=156;++stableLow){const auto update=queuedRemoveAllReference.SetObjectOcclusionOpacity3D({{0x454e5649524f4e4dull,stableLow},1},removeAll.opacity);referenceMatches+=update.matchedStatic+update.matchedMultipart+update.matchedSkinned;}const auto fingerprints=[](const DrawList& draw){std::uint64_t excluding=14695981039346656037ull,including=excluding,order=excluding;const auto mix=[](std::uint64_t& hash,const void* data,std::size_t bytes){const auto* value=static_cast<const std::uint8_t*>(data);for(std::size_t index=0;index<bytes;++index){hash^=value[index];hash*=1099511628211ull;}};for(const auto& instance:draw.meshInstances){mix(excluding,&instance.identity,sizeof(instance.identity));mix(including,&instance.identity,sizeof(instance.identity));mix(order,&instance.identity,sizeof(instance.identity));mix(including,&instance.occlusionOpacity,sizeof(instance.occlusionOpacity));}return std::array{excluding,including,order};};const auto rawFingerprint=fingerprints(queuedRaw),effectiveFingerprint=fingerprints(queuedEffective),noOverrideFingerprint=fingerprints(queuedNoOverrideReference),removeAllFingerprint=fingerprints(queuedRemoveAllReference);if(!effectiveUpdate||effectiveUpdate.matchedStatic!=1||referenceMatches!=6||rawFingerprint[0]!=effectiveFingerprint[0]||effectiveFingerprint[0]!=noOverrideFingerprint[0]||effectiveFingerprint[0]!=removeAllFingerprint[0]||rawFingerprint[2]!=effectiveFingerprint[2]||effectiveFingerprint[2]!=noOverrideFingerprint[2]||effectiveFingerprint[2]!=removeAllFingerprint[2]||rawFingerprint[1]==effectiveFingerprint[1]||effectiveFingerprint[1]!=noOverrideFingerprint[1]||effectiveFingerprint[1]==removeAllFingerprint[1])return 32;
const auto simulateCaptureScenario=[](std::uint32_t gpuDelay){GameplayCaptureScenarioClock3D clock{};std::array<std::uint64_t,5> hashes{};hashes.fill(14695981039346656037ull);const auto mix=[](std::uint64_t& hash,std::uint64_t value){for(unsigned byte=0;byte<8;++byte){hash^=std::uint8_t(value>>(byte*8));hash*=1099511628211ull;}};while(clock.phase!=GameplayCaptureScenarioClock3D::Phase::Complete){while(clock.AdvancesSimulation()){const bool ready=clock.scenario!=10||clock.simulationTick+1>=240;if(!clock.RecordSimulationStep(ready))return std::array<std::uint64_t,5>{};mix(hashes[0],clock.simulationTick);mix(hashes[0],clock.scenario);}if(!clock.ReadyToStage())return std::array<std::uint64_t,5>{};const auto frozenTick=clock.simulationTick,frozenScenario=clock.scenario;mix(hashes[1],frozenTick*17+frozenScenario);mix(hashes[2],frozenTick*31+frozenScenario);mix(hashes[3],frozenTick*43+frozenScenario);mix(hashes[4],frozenTick*59+frozenScenario);if(!clock.BeginReadback(frozenScenario))return std::array<std::uint64_t,5>{};for(std::uint32_t wait=0;wait<gpuDelay;++wait)if(clock.simulationTick!=frozenTick||clock.scenario!=frozenScenario||!clock.WaitingForReadback())return std::array<std::uint64_t,5>{};if(!clock.CompleteReadback(frozenScenario,11))return std::array<std::uint64_t,5>{};}return hashes;};const auto scenario0=simulateCaptureScenario(0),scenario1=simulateCaptureScenario(1),scenario17=simulateCaptureScenario(17),scenario120=simulateCaptureScenario(120);if(scenario0!=scenario1||scenario0!=scenario17||scenario0!=scenario120||std::ranges::any_of(scenario0,[](std::uint64_t value){return value==0;}))return 66;GameplayCaptureScenarioClock3D cancelled{};while(cancelled.AdvancesSimulation())if(!cancelled.RecordSimulationStep(true))return 67;const auto cancelledTick=cancelled.simulationTick,cancelledScenario=cancelled.scenario;if(!cancelled.BeginReadback(cancelledScenario)||!cancelled.FailReadback(cancelledScenario)||cancelled.simulationTick!=cancelledTick||cancelled.scenario!=cancelledScenario||cancelled.phase!=GameplayCaptureScenarioClock3D::Phase::Failed||cancelled.CompleteReadback(cancelledScenario,11))return 68;
GameplayAuthoritativeTrajectoryEvidence3D trajectoryA{},trajectoryB{},motionChanged{},timingChanged{},windowChanged{};
for(std::uint64_t tick=1;tick<=6;++tick){
  const Attack3D attack=Attack3D((tick-1)/2+1);const bool window=(tick%2)==0;const float attackPhase=float(tick%2)*.25f;
  const Vec3 player{float(tick),0,1},hand{float(tick),1.25f,1.1f},tip{float(tick),.55f,1.15f};
  if(!trajectoryA.Record(tick,attack,window,attackPhase,player,hand,tip)||!trajectoryB.Record(tick,attack,window,attackPhase,player,hand,tip)||!motionChanged.Record(tick,attack,window,attackPhase,player,hand,tip+Vec3{tick==4?.02f:0.f,0,0})||!timingChanged.Record(tick+(tick>=4?1:0),attack,window,attackPhase,player,hand,tip)||!windowChanged.Record(tick,attack,tick==4?!window:window,attackPhase,player,hand,tip))return 33;
}
if(trajectoryA.authoritativeHash!=trajectoryB.authoritativeHash||trajectoryA.perAttackHash!=trajectoryB.perAttackHash||trajectoryA.checkpointCount!=6||trajectoryA.perAttackCheckpointCount!=std::array<std::uint32_t,3>{2,2,2}||std::ranges::any_of(trajectoryA.perAttackHash,[](std::uint64_t hash){return hash==GameplayAuthoritativeTrajectoryEvidence3D::Seed;}))return 34;
if(trajectoryA.authoritativeHash==motionChanged.authoritativeHash||trajectoryA.authoritativeHash==timingChanged.authoritativeHash||trajectoryA.authoritativeHash==windowChanged.authoritativeHash||trajectoryA.perAttackHash[1]==motionChanged.perAttackHash[1]||trajectoryA.perAttackHash[1]==timingChanged.perAttackHash[1]||trajectoryA.perAttackHash[1]==windowChanged.perAttackHash[1])return 35;
GameplayAuthoritativeTrajectoryEvidence3D invalid{};if(invalid.Record(1,Attack3D::None,false,0,{},{},{})||!invalid.Record(1,Attack3D::Attack1,false,0,{},{},{})||invalid.Record(1,Attack3D::Attack1,false,0,{},{},{})||invalid.Record(2,Attack3D::Attack1,false,std::numeric_limits<float>::infinity(),{},{},{}))return 36;
ReadbackRasterProvenance3D contactRaster{};contactRaster.frameSerial=77;contactRaster.viewProjectionHash=2;contactRaster.tokenGeneration=3;contactRaster.temporalHistoryReadGeneration=4;std::vector<std::uint32_t> contactIds(16*16);std::vector<float> contactDepth(16*16,1.f);for(unsigned y=6;y<=9;++y)for(unsigned x:{4u,5u,10u,11u}){contactIds[y*16+x]=1;contactDepth[y*16+x]=.5f;}for(unsigned y=10;y<16;++y)for(unsigned x=0;x<16;++x){contactIds[y*16+x]=2;contactDepth[y*16+x]=.505f;}const auto contact=BuildGameplayGpuContactArtifact3D(16,16,contactIds,contactDepth,1,2,contactRaster,99);if(!contact.Complete(16,16)||!contact.leftFootPixels||!contact.rightFootPixels||!contact.groundSupportPixels||!contact.contactShadowPixels||contact.leftGapPixels||contact.rightGapPixels||std::abs(contact.leftDepthDelta-.005f)>.00001f||std::abs(contact.rightDepthDelta-.005f)>.00001f)return 37;auto airborneIds=contactIds;for(unsigned y=10;y<16;++y)for(unsigned x=0;x<16;++x)airborneIds[y*16+x]=0;if(BuildGameplayGpuContactArtifact3D(16,16,airborneIds,contactDepth,1,2,contactRaster,99).Complete(16,16))return 38;auto invalidDepth=contactDepth;invalidDepth[9*16+5]=std::numeric_limits<float>::quiet_NaN();if(BuildGameplayGpuContactArtifact3D(16,16,contactIds,invalidDepth,1,2,contactRaster,99).Complete(16,16)||BuildGameplayGpuContactArtifact3D(16,16,contactIds,contactDepth,1,2,{},99).Complete(16,16)||BuildGameplayGpuContactArtifact3D(16,16,contactIds,contactDepth,1,2,contactRaster,0).Complete(16,16))return 39;
const Vec3 showcasePlayer{3,0,4};for(float yaw:{0.f,.7f,-1.2f}){const Vec3 spawn=GameplayShowcaseMonsterSpawn3D(showcasePlayer,yaw),delta=spawn-showcasePlayer;const Vec3 forward{std::sin(yaw),0,std::cos(yaw)},right{std::cos(yaw),0,-std::sin(yaw)};if(std::abs(Dot(delta,right)-1.95f)>.0001f||std::abs(Dot(delta,forward)-.2f)>.0001f||Length(delta)<=1.8f||std::abs(Dot(delta,right))>2.f||std::abs(Dot(delta,forward)-.7f)>1.8f)return 40;}
for(float yaw:{0.f,.7f,-1.2f}){const Vec3 corpse{2,0,5};if(Length(GameplayShowcaseMonsterPresentationPosition3D(corpse,yaw,true)-corpse)>.0001f||Length(GameplayShowcaseMonsterPresentationPosition3D(corpse,yaw,false)-corpse)>.0001f)return 47;}
{const Vec3 monster{2,0,5},player{-1,0,4};const auto transform=GameplayShowcaseMonsterTransform3D(monster,player);if(std::abs(transform.rotation.y-1.2490458f)>.0001f||transform.scale.x!=.98f||transform.scale.y!=.98f||transform.scale.z!=.98f)return 81;if(GameplayShowcaseMonsterFacingYaw3D(monster,monster)!=0.f)return 82;}
{const Vec3 monster{2,0,5},player{-1,0,4};const auto normal=GameplayShowcaseMonsterTransform3D(monster,player,.7f,false),god=GameplayShowcaseMonsterTransform3D(monster,player,.7f,true);const auto normalPolicy=GameplayShowcaseMonsterStage3DForCapture(false),godPolicy=GameplayShowcaseMonsterStage3DForCapture(true);if(normal.scale.x!=.98f||Length(normal.position-monster)>.0001f||normalPolicy.uniformScale!=.98f||normalPolicy.playerRightOffset!=0.f||god.scale.x!=1.45f||god.scale.y!=1.45f||god.scale.z!=1.45f||godPolicy.uniformScale!=1.45f||godPolicy.playerRightOffset!=.25f||std::abs(Length(god.position-monster)-.25f)>.0001f)return 107;}
std::vector<std::uint32_t> separatedPixels(100*40);for(unsigned y=8;y<32;++y){for(unsigned x=10;x<20;++x)separatedPixels[y*100+x]=1;for(unsigned x=25;x<35;++x)separatedPixels[y*100+x]=2;}const auto separated=EvaluateGameplayActorScreenSeparation3D(100,40,separatedPixels,1,2);if(!separated.complete||!separated.safe||separated.gapPixels!=5||separated.playerPixels!=240||separated.companionPixels!=240)return 41;for(unsigned y=8;y<32;++y)for(unsigned x=18;x<28;++x)separatedPixels[y*100+x]=2;const auto overlapping=EvaluateGameplayActorScreenSeparation3D(100,40,separatedPixels,1,2);if(!overlapping.complete||overlapping.safe||overlapping.gapPixels)return 42;
GameplayShowcaseEnvironment3D cameraEnvironment;if(!cameraEnvironment.Initialize(0x4a17u))return 43;const auto grassBounds=CreateGrassClumpMesh3D(11,3).Bounds();const Vec3 godFocus{-16,1.4f,8.73334f};const auto godFixture=Mesh3D::CreateUnitBox();const Matrix4 godFixtureWorld=CreateTransformMatrix(godFocus-Vec3{0,.4f,0},{},{.7f,2.f,.5f});const auto searchStart=std::chrono::steady_clock::now();const auto godLayout=SelectGameplayGodCameraLayout3D(godFocus,cameraEnvironment.Descriptors(),grassBounds,godFixture.vertices,godFixture.indices,godFixtureWorld);const auto searchTime=std::chrono::steady_clock::now()-searchStart;if(!godLayout.valid||godLayout.distance<18.f||godLayout.distance>30.f||godLayout.playerCoverage<GameplayGodMinimumPlayerCoverage3D||!godLayout.lodTransitions||godLayout.camera.projection.mode!=ProjectionMode::Orthographic||!std::isfinite(godLayout.camera.projection.orthographicHeight)||godLayout.camera.projection.orthographicHeight<4.f||godLayout.camera.projection.orthographicHeight>180.f||godLayout.elevation<.52f||godLayout.elevation>1.31f||godLayout.buildingUnion>.22f||godLayout.maximumBuildingPart>.08f||godLayout.roofCoverage>.05f||godLayout.roofTouchesTop||godLayout.visibleWorldDescriptors<20){std::cerr<<"god invalid distance="<<godLayout.distance<<" azimuth="<<godLayout.azimuth<<" elevation="<<godLayout.elevation<<" fov="<<godLayout.camera.projection.verticalFieldOfView<<" ortho="<<godLayout.camera.projection.orthographicHeight<<" coverage="<<godLayout.playerCoverage<<" transitions="<<godLayout.lodTransitions<<" union="<<godLayout.buildingUnion<<" maxPart="<<godLayout.maximumBuildingPart<<" roof="<<godLayout.roofCoverage<<" roofTop="<<godLayout.roofTouchesTop<<" world="<<godLayout.visibleWorldDescriptors<<'\n';return 44;}
GameplayCaptureProjectionEvidence3D firstProjection{ProjectionMode::Perspective,1.04719755f,0};GameplayCaptureProjectionEvidence3D godProjection{ProjectionMode::Orthographic,godLayout.camera.projection.verticalFieldOfView,godLayout.camera.projection.orthographicHeight,godLayout.playerCoverage,godLayout.buildingUnion,godLayout.maximumBuildingPart,godLayout.roofCoverage,godLayout.visibleWorldDescriptors,godLayout.lodTransitions};if(!firstProjection.Complete(GameplayCaptureCamera3D::First)||!godProjection.Complete(GameplayCaptureCamera3D::God))return 53;auto invalidProjection=firstProjection;invalidProjection.projectionMode=ProjectionMode::Orthographic;if(invalidProjection.Complete(GameplayCaptureCamera3D::First))return 54;invalidProjection=firstProjection;invalidProjection.verticalFovRadians=std::numeric_limits<float>::infinity();if(invalidProjection.Complete(GameplayCaptureCamera3D::Third))return 55;invalidProjection=firstProjection;invalidProjection.godActualTriangleCoverage=GameplayGodMinimumPlayerCoverage3D;if(invalidProjection.Complete(GameplayCaptureCamera3D::First))return 56;auto invalidGod=godProjection;invalidGod.projectionMode=ProjectionMode::Perspective;if(invalidGod.Complete(GameplayCaptureCamera3D::God))return 57;invalidGod=godProjection;invalidGod.godLodTransitions=0;if(invalidGod.Complete(GameplayCaptureCamera3D::God))return 57;
std::uint32_t searchCount=1,cacheHits{};const auto validationStart=std::chrono::steady_clock::now();for(unsigned tick=0;tick<120;++tick){const Vec3 translatedFocus=godFocus+Vec3{float(tick%4)*.01f,0,0};auto translatedCamera=godLayout.camera;const Vec3 delta=translatedFocus-godFocus;translatedCamera.position=translatedCamera.position+delta;translatedCamera.target=translatedCamera.target+delta;const Matrix4 translatedPlayerWorld=CreateTranslationMatrix(delta)*godFixtureWorld;const auto validation=ValidateGameplayGodCameraLayout3D(translatedFocus,translatedCamera,cameraEnvironment.Descriptors(),grassBounds,godLayout.distance,godLayout.azimuth,godLayout.elevation,godFixture.vertices,godFixture.indices,translatedPlayerWorld);if(!validation.valid)return 51;++cacheHits;}const auto validationTime=std::chrono::steady_clock::now()-validationStart;if(searchCount!=1||cacheHits!=120||validationTime>=searchTime||validationTime>std::chrono::seconds(2))return 52;
CollisionWorld3D placementCollision;if(!placementCollision.CreateBox({{{-16,.5f,8},{2,1,2}}}))return 48;const auto placementPointer=FindGameplayClearPlacementPointer3D(godLayout.camera,placementCollision);if(!placementPointer)return 49;const auto placementRay=ScreenPointToRay(godLayout.camera,{1280,720},*placementPointer);const float placementT=-placementRay.origin.y/placementRay.direction.y;auto placementPoint=placementRay.origin+placementRay.direction*placementT;placementPoint.x=std::round(placementPoint.x*.5f)*2;placementPoint.z=std::round(placementPoint.z*.5f)*2;placementPoint.y=.5f;if(placementCollision.OverlapsBox(placementPoint,{.48f,.48f,.48f}))return 50;const auto distantPlacementPointer=FindGameplayClearPlacementPointer3D(godLayout.camera,placementCollision,4.f);if(!distantPlacementPointer||FindGameplayClearPlacementPointer3D(godLayout.camera,placementCollision,201.f))return 108;const auto distantPlacementRay=ScreenPointToRay(godLayout.camera,{1280,720},*distantPlacementPointer);const float distantPlacementT=-distantPlacementRay.origin.y/distantPlacementRay.direction.y;if(!std::isfinite(distantPlacementT)||distantPlacementT<4.f)return 109;const Vec3 placementAnchor{-16,0,8.73334f};const auto separatedPlacementPointer=FindGameplayClearPlacementPointer3D(godLayout.camera,placementCollision,0.f,placementAnchor,7.f);if(!separatedPlacementPointer)return 110;const auto separatedPlacementRay=ScreenPointToRay(godLayout.camera,{1280,720},*separatedPlacementPointer);const float separatedPlacementT=-separatedPlacementRay.origin.y/separatedPlacementRay.direction.y;auto separatedPlacementPoint=separatedPlacementRay.origin+separatedPlacementRay.direction*separatedPlacementT;separatedPlacementPoint.x=std::round(separatedPlacementPoint.x*.5f)*2;separatedPlacementPoint.z=std::round(separatedPlacementPoint.z*.5f)*2;const float separatedDx=separatedPlacementPoint.x-placementAnchor.x,separatedDz=separatedPlacementPoint.z-placementAnchor.z;if(separatedDx*separatedDx+separatedDz*separatedDz<49.f)return 111;if(FindGameplayClearPlacementPointer3D(godLayout.camera,placementCollision,0.f,placementAnchor,std::numeric_limits<float>::infinity()))return 112;
const Camera3D firstCamera=MakeFirstPersonCamera({},0,0);for(const auto sex:{GameplayViewmodelSex3D::Female,GameplayViewmodelSex3D::Male}){const auto modelRoot=std::filesystem::path(AEVRYND_SOURCE_DIR)/"games/character_lab/assets/model-packs";const auto source=LoadSkinnedModelAssetFileDetailed(modelRoot/(sex==GameplayViewmodelSex3D::Female?"realistic-female/realistic-female.aeskin":"realistic-male/realistic-male.aeskin"));HumanoidSkeletonMap3D rig{};CharacterEquipmentAsset3D weapon{};if(!source||!ResolveCharacterRig3D(source.asset,rig)||!CreateCharacterEquipmentAsset3D(source.asset,rig,CharacterEquipmentShape3D::Sword,weapon))return 143;const auto geometry=BuildGameplayFirstPersonWeaponProjectionGeometry3D(weapon);if(!geometry.Valid())return 144;const auto layout=GameplayFirstPersonViewmodelLayout(sex);const Matrix4 desiredGrip=GameplayFirstPersonDesiredGripTransform3D(layout),handSocket=desiredGrip*CreateTranslationMatrix({0,-.105f,-.02f});const auto evidence=EvaluateGameplayFirstPersonViewmodelProjection3D(firstCamera,handSocket,layout,geometry);if(!evidence.actualWeaponGeometryMeasured||evidence.Complete()){std::cerr<<"first viewmodel accepted without actual posed arms sex="<<unsigned(sex)<<'\n';return 45;}auto wrong=handSocket;wrong[0][3]=.30f;if(EvaluateGameplayFirstPersonViewmodelProjection3D(firstCamera,wrong,layout,geometry).Complete())return 63;wrong=handSocket;wrong[1][3]=-.42f;if(EvaluateGameplayFirstPersonViewmodelProjection3D(firstCamera,wrong,layout,geometry).Complete())return 64;wrong=handSocket;wrong[2][3]=firstCamera.projection.nearPlane+.005f;if(EvaluateGameplayFirstPersonViewmodelProjection3D(firstCamera,wrong,layout,geometry).Complete())return 65;}
ReadbackRasterProvenance3D raster{};raster.frameSerial=42;raster.viewProjectionHash=1;raster.tokenGeneration=1;raster.temporalHistoryReadGeneration=1;ObjectIdCaptureRequestMetadata3D frameRequest{7,42,9,ReadbackEmptyReason3D::None,{{5},0},11};FrameBoundDeterminismEvidence3D frameEvidence{};frameEvidence.frameSerial=42;frameEvidence.integrity={DeterminismIntegrityMode3D::SealedCapture,DeterminismIntegrityMode3D::SealedCapture,DeterminismIntegrityRejectionReason3D::None,0,0,0,true};frameEvidence.canonical={1,1,2,3,4,5,6,7};frameEvidence.render={42,7,8,9,10,11,12,13};if(!ValidateGameplayFrameBoundRenderPair3D(frameEvidence,frameEvidence,raster,raster,frameRequest,frameRequest))return 76;auto wrongFrameEvidence=frameEvidence;++wrongFrameEvidence.render.frameSerial;if(ValidateGameplayFrameBoundRenderPair3D(frameEvidence,wrongFrameEvidence,raster,raster,frameRequest,frameRequest))return 77;auto wrongSceneEvidence=frameEvidence;++wrongSceneEvidence.render.sceneDrawRevision;if(ValidateGameplayFrameBoundRenderPair3D(wrongSceneEvidence,wrongSceneEvidence,raster,raster,frameRequest,frameRequest))return 78;auto wrongRaster=raster;++wrongRaster.frameSerial;if(ValidateGameplayFrameBoundRenderPair3D(frameEvidence,frameEvidence,raster,wrongRaster,frameRequest,frameRequest))return 79;auto wrongRequest=frameRequest;++wrongRequest.drawListRevision;if(ValidateGameplayFrameBoundRenderPair3D(frameEvidence,frameEvidence,raster,raster,frameRequest,wrongRequest))return 80;DirectionalShadowFrameProvenance3D shadow{};shadow.frameSerial=42;shadow.resourceGeneration=7;shadow.activeCascades=2;shadow.casters=3;shadow.drawCalls=4;shadow.cascadeDrawCalls=4;shadow.cascadeFingerprints[0]=11;shadow.cascadeFingerprints[1]=12;shadow.rendered[0]=1;shadow.reused[1]=1;if(!EvaluateGameplayCaptureShadowEvidence3D(shadow,raster))return 18;shadow.frameSerial=41;if(EvaluateGameplayCaptureShadowEvidence3D(shadow,raster))return 24;shadow.frameSerial=42;shadow.cascadeFingerprints[1]=0;if(EvaluateGameplayCaptureShadowEvidence3D(shadow,raster))return 25;shadow.cascadeFingerprints[1]=12;shadow.rendered[1]=1;if(EvaluateGameplayCaptureShadowEvidence3D(shadow,raster))return 26;if(!EvaluateGameplayCaptureLodEvidence3D(*godDay,42,42,1,1,1,0)||EvaluateGameplayCaptureLodEvidence3D(*godDay,41,42,1,1,1,0)||EvaluateGameplayCaptureLodEvidence3D(*godDay,42,42,1,0,1,0)||EvaluateGameplayCaptureLodEvidence3D(*godDay,42,42,1,1,0,0)||EvaluateGameplayCaptureLodEvidence3D(*godDay,42,42,1,1,1,1)||!EvaluateGameplayCaptureLodEvidence3D(third,42,42,1,0,0,0)||EvaluateGameplayCaptureLodEvidence3D(third,42,42,1,1,0,0)||EvaluateGameplayCaptureLodEvidence3D(third,42,42,1,0,1,0)||EvaluateGameplayCaptureLodEvidence3D(third,42,42,0,0,0,0))return 19;
AnimationPose3D hashPose{};hashPose.globalMatrices.push_back(Matrix4::Identity());const auto poseHash=HashGameplayCapturePose3D(hashPose);auto negativeZeroPose=hashPose;negativeZeroPose.globalMatrices[0][0][3]=-0.f;if(!poseHash||HashGameplayCapturePose3D(negativeZeroPose)!=poseHash)return 69;auto changedPose=hashPose;changedPose.globalMatrices[0][0][3]=std::nextafter(0.f,1.f);if(HashGameplayCapturePose3D(changedPose)==poseHash)return 70;auto invalidPose=hashPose;invalidPose.globalMatrices[0][0][3]=std::numeric_limits<float>::infinity();if(HashGameplayCapturePose3D(invalidPose)!=0)return 71;
FootPlacementResult3D footHashFixture{};footHashFixture.lod=FootPlacementLod3D::Full;footHashFixture.left.source=FootContactSource3D::Collider;footHashFixture.left.collider={3};footHashFixture.left.userData=0xabc;footHashFixture.left.contactWorld={1,2,3};footHashFixture.left.targetWorld={1,2.1f,3};footHashFixture.left.normalWorld={0,1,0};footHashFixture.left.weight=1;footHashFixture.right=footHashFixture.left;footHashFixture.right.collider={4};footHashFixture.right.userData=0xdef;footHashFixture.applied=true;footHashFixture.ikSolves=2;const auto footHash=HashGameplayCaptureFootPlacement3D(footHashFixture);auto reorderedHandles=footHashFixture;reorderedHandles.left.collider={99};reorderedHandles.right.collider={1};if(!footHash||HashGameplayCaptureFootPlacement3D(reorderedHandles)!=footHash)return 72;auto changedFoot=footHashFixture;changedFoot.left.targetWorld.x=std::nextafter(changedFoot.left.targetWorld.x,2.f);if(HashGameplayCaptureFootPlacement3D(changedFoot)==footHash)return 73;changedFoot=footHashFixture;changedFoot.left.userData++;if(HashGameplayCaptureFootPlacement3D(changedFoot)==footHash)return 74;changedFoot=footHashFixture;changedFoot.left.weight=std::numeric_limits<float>::quiet_NaN();if(HashGameplayCaptureFootPlacement3D(changedFoot)!=0)return 75;
std::cout<<"ticks="<<r.GetStats().ticks<<" entities="<<r.Entities().size()<<" captureExpectations="<<GameplayCaptureExpectations3D.size()<<" godDistance="<<godLayout.distance<<" godAzimuth="<<godLayout.azimuth<<" godElevation="<<godLayout.elevation<<" godCoverage="<<godLayout.playerCoverage<<" godTransitions="<<godLayout.lodTransitions<<" godBuildingUnion="<<godLayout.buildingUnion<<" godMaxPart="<<godLayout.maximumBuildingPart<<" godRoof="<<godLayout.roofCoverage<<" godWorld="<<godLayout.visibleWorldDescriptors<<"\n";return 0;}
