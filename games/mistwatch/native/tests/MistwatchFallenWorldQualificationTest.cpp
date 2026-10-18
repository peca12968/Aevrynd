#include "fallen/MistwatchFallenSeatSupport.h"
#include "fallen/MistwatchQualificationRuntime.h"
#include "fallen/MistwatchAuthoredWorldQualification.h"
#include "fallen/MistwatchAuthoredNativeExpected.h"
#include "fallen/MistwatchCurrentWorldExpected.h"
#include "MistwatchFallenLogProfile.h"
#include "MistwatchFallenLogAssets.h"
#include "MistwatchMiddleLayer.h"
#include "MistwatchCanopyPopulation.h"
#include "MistwatchCanopyCollision.h"
#include "MistwatchRootStemTerrain.h"
#include "MistwatchTrailLitter.h"
#include "MistwatchCamera.h"
#include "MistwatchRangerHut.h"
#include "MistwatchEncounter.h"
#include "MistwatchLoadPreparation.h"
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/WorldDeltaPersistence3D.h>
#include <Windows.h>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>

namespace W=::Mistwatch::FallenWorldSeat;
namespace S=::Mistwatch::FullSourceSupport;
namespace {
constexpr std::size_t MaximumAssetBytes=33554432,MaximumOutputBytes=67108864;
struct Handle {HANDLE value{INVALID_HANDLE_VALUE};explicit Handle(HANDLE item):value(item){};Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;~Handle(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
std::string Hex(std::span<const std::byte> bytes){std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto value:bytes)out<<std::setw(2)<<std::to_integer<unsigned>(value);return out.str();}
std::string Sha(std::span<const std::byte> bytes){std::array<std::byte,32> digest{};W::Gate(Aevrynd::WorldDeltaSha2563D(bytes,digest),"SHA_FAILED");return Hex(digest);}
std::string Sha(const std::string& value){return Sha(std::as_bytes(std::span(value.data(),value.size())));}
std::vector<std::byte> Read(HANDLE file){LARGE_INTEGER size{};W::Gate(GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<=static_cast<LONGLONG>(MaximumAssetBytes),"READ_FILE_DOMAIN");std::vector<std::byte> raw(static_cast<std::size_t>(size.QuadPart));DWORD count{};W::Gate(ReadFile(file,raw.data(),static_cast<DWORD>(raw.size()),&count,nullptr)&&count==raw.size(),"READ_FILE_FAILED");return raw;}
void WriteNew(const std::filesystem::path& path,const std::string& value){W::Gate(value.size()<=MaximumOutputBytes,"OUTPUT_LIMIT_UNRESOLVED");Handle file(CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr));W::Gate(file.value!=INVALID_HANDLE_VALUE,"OUTPUT_REFUSES_OVERWRITE");DWORD count{};W::Gate(WriteFile(file.value,value.data(),static_cast<DWORD>(value.size()),&count,nullptr)&&count==value.size()&&FlushFileBuffers(file.value),"OUTPUT_WRITE_FAILED");}
std::string IntegerText(const S::Kernel::Integer& value){if(!value.Sign())return"0x0";std::ostringstream out;if(value.negative)out<<'-';out<<"0x"<<std::hex;for(std::size_t k=value.words.size();k>0;--k){if(k!=value.words.size())out<<std::setw(8)<<std::setfill('0');out<<value.words[k-1];}return out.str();}
std::string Qtext(const W::Q& value){return IntegerText(value.numerator)+"/"+IntegerText(value.denominator);}
void Vector(std::ostream& out,W::V value){out<<std::hexfloat<<value.x<<','<<value.y<<','<<value.z<<std::defaultfloat;}
void BoxText(std::ostream& out,const W::Box& value){Vector(out,value.center);out<<" extent=";Vector(out,value.extents);}
void MeshRawMatches(const Aevrynd::Mesh3D& mesh,std::span<const std::byte> raw){static_assert(sizeof(Aevrynd::Vertex3D)==28&&std::endian::native==std::endian::little);std::array<std::uint32_t,4> header{};W::Gate(raw.size()>=16,"MESH_HEADER_SHORT");std::memcpy(header.data(),raw.data(),16);const std::size_t vertices=mesh.vertices.size()*28,indices=mesh.indices.size()*4;W::Gate(header[0]==0x334d4541u&&header[1]==1&&header[2]==mesh.vertices.size()&&header[3]==mesh.indices.size()&&raw.size()==16+vertices+indices,"MESH_HEADER_COUNT_CHANGED");W::Gate(std::memcmp(raw.data()+16,mesh.vertices.data(),vertices)==0&&std::memcmp(raw.data()+16+vertices,mesh.indices.data(),indices)==0,"RAW_RENDER_MESH_CHANGED");}
Aevrynd::Mesh3D LoadAsset(const std::filesystem::path& path,std::ostream& log){Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));W::Gate(file.value!=INVALID_HANDLE_VALUE,"RETAINED_ASSET_PIN_OPEN_FAILED");const auto raw=Read(file.value);auto loaded=Aevrynd::LoadAevryndMeshFile(path);W::Gate(static_cast<bool>(loaded),"RETAINED_ASSET_NATIVE_LOAD_FAILED");MeshRawMatches(loaded.mesh,raw);log<<"ASSET "<<std::quoted(path.generic_string())<<" bytes="<<raw.size()<<" sha256="<<Sha(raw)<<" vertices="<<loaded.mesh.vertices.size()<<" triangles="<<loaded.mesh.indices.size()/3<<'\n';return std::move(loaded.mesh);}
struct Input {std::filesystem::path root,source,assets,output,generatedRootStem;std::size_t bytes{},vertices{},indices{};std::string sha;
    std::optional<Mistwatch::QualificationRuntime::Profile> runtimeProfile;
    bool authoredWorld{},surfaceOnly{},currentWorld{};
};
Input Arguments(int argc,wchar_t** argv){
    W::Gate(argc==7||argc==9||argc==11||argc==13,"USAGE: MistwatchFallenWorldQualificationTest --source-root ABS --runtime-assets ABS --out FRESH_ABS [--runtime-profile REVIEWED_PROFILE] [--generated-root-stem ABS] [--world-cohort REVIEWED_WORLD]");
    static_assert(sizeof(void*)==8);W::Gate(std::string(MISTWATCH_FALLEN_CONFIGURATION)=="Release","FORMAL_CPU_RELEASE_CONFIGURATION_REQUIRED");
    std::map<std::wstring,std::wstring> arguments;
    for(int k=1;k<argc;k+=2){
        const std::wstring name=argv[k];
        W::Gate(name==L"--source-root"||name==L"--runtime-assets"||name==L"--out"||name==L"--runtime-profile"||name==L"--generated-root-stem"||name==L"--world-cohort","CLI_UNKNOWN");
        W::Gate(arguments.emplace(name,argv[k+1]).second,"CLI_DUPLICATE");
    }
    const auto take=[&](const wchar_t* name){const auto found=arguments.find(name);W::Gate(found!=arguments.end(),"CLI_MISSING");return found->second;};
    Input input;input.root=std::filesystem::path(take(L"--source-root")).lexically_normal();input.assets=std::filesystem::path(take(L"--runtime-assets")).lexically_normal();input.output=std::filesystem::path(take(L"--out")).lexically_normal();
    if(const auto profile=arguments.find(L"--runtime-profile");profile!=arguments.end()){
        std::string name;for(const auto c:profile->second){W::Gate(c>0&&c<128,"CLI_RUNTIME_PROFILE_ASCII");name.push_back(static_cast<char>(c));}
        input.runtimeProfile=Mistwatch::QualificationRuntime::ParseProfile(name);
    }
    if(const auto generated=arguments.find(L"--generated-root-stem");generated!=arguments.end()){
        W::Gate(input.runtimeProfile.has_value(),"CLI_GENERATED_REQUIRES_RUNTIME_PROFILE");
        input.generatedRootStem=std::filesystem::path(generated->second).lexically_normal();
        W::Gate(input.generatedRootStem.is_absolute(),"CLI_GENERATED_DOMAIN");
    }
    if(const auto world=arguments.find(L"--world-cohort");world!=arguments.end()){
        W::Gate(world->second==L"legacy-four-v1"||world->second==L"entrance-authored-habitat-v1"||world->second==L"entrance-authored-surface-only-v1"||
            world->second==L"entrance-current-forest-surface-v1","CLI_WORLD_COHORT_UNKNOWN");
        input.authoredWorld=world->second==L"entrance-authored-habitat-v1";
        input.surfaceOnly=world->second==L"entrance-authored-surface-only-v1";
        input.currentWorld=world->second==L"entrance-current-forest-surface-v1";
        if(input.authoredWorld)W::Gate(input.runtimeProfile.has_value()&&*input.runtimeProfile!=Mistwatch::QualificationRuntime::Profile::LegacyFullV1,"CLI_AUTHORED_REQUIRES_HABITAT_RUNTIME");
    }
    const bool surfaceRuntime=input.runtimeProfile&&*input.runtimeProfile==Mistwatch::QualificationRuntime::Profile::AuthoredForestSurfaceFullV4;
    W::Gate(input.surfaceOnly==surfaceRuntime,"CLI_SURFACE_PROFILE_EXPLICIT_SURFACE_ONLY_COHORT_REQUIRED");
    const bool currentRuntime=input.runtimeProfile&&*input.runtimeProfile==Mistwatch::QualificationRuntime::Profile::AuthoredCurrentForestFullV5;
    W::Gate(input.currentWorld==currentRuntime,"CLI_CURRENT_PROFILE_EXPLICIT_CURRENT_COHORT_REQUIRED");
    W::Gate(input.root.is_absolute()&&input.assets.is_absolute()&&input.output.is_absolute(),"CLI_DOMAIN");
    W::Gate(std::filesystem::equivalent(input.root,std::filesystem::path(MISTWATCH_FALLEN_SOURCE_ROOT)),"CLI_COMPILED_SOURCE_ROOT_MISMATCH");
    input.source=input.root/"engine/assets/mistwatch-fallen-logs/meshes"/Mistwatch::FallenLogAssets::MeshName;
    input.bytes=Mistwatch::FallenLogMeshBytes;input.vertices=Mistwatch::FallenLogVertexCount;input.indices=Mistwatch::FallenLogIndexCount;input.sha=Mistwatch::FallenLogAssets::MeshSha;
    W::Gate(input.bytes==434012&&input.bytes<=MaximumAssetBytes&&input.vertices==8956&&input.vertices<=10000&&input.indices==45807&&input.indices<=50000&&input.sha=="b7f6a05bf928787082dea92a0051557496e45894a9c2b1a0af1e51110abd0d46","CLI_CURRENT_STRAIGHT_SOURCE_IDENTITY");
    return input;
}
// 獨立預期值逐欄取自 m1974 真 stdout，不直接以 producer 常數驗證自己。
constexpr std::array<std::array<std::uint32_t,15>,4> ExpectedPoseAndBox{{
    {{0xc1060985u,0x3d1525b4u,0xc131bcbau,0x00000000u,0xbfc90fdbu,0x00000000u,0x3fa9233du,0x3fa9233du,0x3fa9233du,0xc106b75au,0x3e43f30cu,0xc132085cu,0x3e3b8d40u,0x3e43f30cu,0x4000fb90u}},
    {{0x4113bf6du,0x3d040ba5u,0xbd9f3eb2u,0x00000000u,0xbfb77fb5u,0x00000000u,0x3f95be63u,0x3f95be63u,0x3f95be63u,0x4113651au,0x3e2d7b2bu,0xbdb45cf0u,0x3eb8b240u,0x3e2d7b2bu,0x3fe29000u}},
    {{0xc0feac9bu,0x3d05f17au,0x42ae27a8u,0x00000000u,0x3fcd1229u,0x00000000u,0x3f97e555u,0x3f97e555u,0x3f97e555u,0xc0fd692cu,0x3e2ff972u,0x42ae2fb4u,0x3e5290e0u,0x3e2ff972u,0x3fe790c0u}},
    {{0xc0e0365cu,0x3d27f121u,0x4281de2du,0x00000000u,0xc042839fu,0x00000000u,0x3fbe738cu,0x3fbe738cu,0x3fbe738cu,0xc0dfb60fu,0x3e5ca44bu,0x4281cc31u,0x40109812u,0x3e5ca44bu,0x3ec93400u}}
}};
constexpr std::array<std::array<std::uint32_t,3>,4> ExpectedAnchors{{
    {{0xc106b75au,0x3e4f6d47u,0xc132085cu}},{{0x41131dd0u,0x3e5b02afu,0xbdb5e268u}},
    {{0xc0fd7048u,0x3e4c83a5u,0x42ae2f88u}},{{0xc0dfb4e9u,0x3e4937c5u,0x4281c4c0u}}
}};
std::array<std::uint32_t,15> PlacementWords(const Mistwatch::FallenLogPlacement& p){
    return{{std::bit_cast<std::uint32_t>(p.pose.position.x),std::bit_cast<std::uint32_t>(p.pose.position.y),std::bit_cast<std::uint32_t>(p.pose.position.z),
        std::bit_cast<std::uint32_t>(p.pose.rotation.x),std::bit_cast<std::uint32_t>(p.pose.rotation.y),std::bit_cast<std::uint32_t>(p.pose.rotation.z),
        std::bit_cast<std::uint32_t>(p.pose.scale.x),std::bit_cast<std::uint32_t>(p.pose.scale.y),std::bit_cast<std::uint32_t>(p.pose.scale.z),
        std::bit_cast<std::uint32_t>(p.collider.center.x),std::bit_cast<std::uint32_t>(p.collider.center.y),std::bit_cast<std::uint32_t>(p.collider.center.z),
        std::bit_cast<std::uint32_t>(p.collider.extents.x),std::bit_cast<std::uint32_t>(p.collider.extents.y),std::bit_cast<std::uint32_t>(p.collider.extents.z)}};
}
std::vector<W::Placement> FixedStraightCandidate(const Aevrynd::Mesh3D& raw,const Aevrynd::HeightField3D& terrain,
    std::span<const Mistwatch::ForestProp> original,std::span<const W::V> roots,std::span<const W::Solid> retained,
    W::Budget& budget,const W::Audit& audit){
    S::CheckFloatingEnvironment();W::Gate(roots.size()==100&&retained.size()<=12000,"ONE_CANDIDATE_FORMAL_POPULATION_DOMAIN");W::VerifyCurrentTerrain(terrain);W::VerifyCurrentProps(original);
    Mistwatch::ValidateFallenLogRawMesh(raw);Mistwatch::ValidateFallenLogSourceRoots(terrain,roots);const auto& fixed=Mistwatch::FallenLogPlacements();W::Gate(fixed.size()==4,"ACTIVE_COHORT_IDENTITY_FAILED");std::vector<W::Placement> result;result.reserve(4);
    for(std::size_t at=0;at<fixed.size();++at){const auto& p=fixed[at];budget.Time();try{
        W::Gate(p.propId==W::CurrentLogIds[at]&&p.propId<original.size()&&PlacementWords(p)==ExpectedPoseAndBox[at],"FIXED_PRODUCER_15_WORD_IDENTITY");
        W::Gate(std::array<std::uint32_t,3>{std::bit_cast<std::uint32_t>(p.authoredAnchor.x),std::bit_cast<std::uint32_t>(p.authoredAnchor.y),std::bit_cast<std::uint32_t>(p.authoredAnchor.z)}==ExpectedAnchors[at],"FIXED_PRODUCER_AUTHORED_ANCHOR_IDENTITY");
        W::VerifyPose(p.pose,budget);W::Placement seated;seated.propId=p.propId;seated.anchor=p.authoredAnchor;seated.pose=p.pose;seated.collider=p.collider;
        // 不找新姿態或重新坐落；原完整三角地形裁切，包含面、線、點投影域。
        const auto footprint=W::MakeFootprint(raw,seated.pose,terrain,budget);seated.finalRange=W::Evaluate(raw,seated.pose,footprint,budget);
        seated.predecessorY=std::nextafter(seated.pose.position.y,-(std::numeric_limits<float>::infinity)());auto predecessor=seated.pose;predecessor.position.y=seated.predecessorY;seated.predecessorRange=W::Evaluate(raw,predecessor,footprint,budget);seated.evaluations=2;
        W::Gate(seated.finalRange.minimum.Sign()>=0&&seated.finalRange.negative==0&&seated.predecessorRange.minimum.Sign()<0,"ADJACENT_Y_CERTIFICATE_FAILED");
        const auto points=W::ActualPoints(raw,seated.pose,budget);const auto box=W::ConservativeBox(points,budget);W::Gate(W::SameBox(box,seated.collider)&&W::ContainsActual(seated.collider,points)&&Mistwatch::FallenLogDetail::ContainsEveryVertex(raw,seated.pose,seated.collider),"FIXED_PRODUCER_ALL_VERTEX_CONSERVATIVE_BOX_CHANGED");
        if(audit)audit(seated,footprint);result.push_back(std::move(seated));W::OriginalGuards(result.back(),terrain,roots,original,result,retained,budget);
    }catch(const std::exception& error){throw std::runtime_error("PROP_FIRST_DIVERGENCE id="+std::to_string(p.propId)+" cause="+error.what());}}
    W::VerifyCurrentActiveRoles(result);for(const auto& p:result){budget.Time();try{W::OriginalGuards(p,terrain,roots,original,result,retained,budget);}catch(const std::exception& error){throw std::runtime_error("PROP_FIRST_DIVERGENCE id="+std::to_string(p.propId)+" cause="+error.what());}}
    return result;
}
// production 新 API 沒有 mapped span；此最小 adapter 僅保留舊 span 身分負控。
// 成功預檢仍完整呼叫真正 ScannedStraightV1 API，絕不以私有預檢代替 production。
struct MappedPreparationResult {
    Mistwatch::LoadPreparationError error{Mistwatch::LoadPreparationError::PreparationException};
    std::unique_ptr<Mistwatch::PreparedSavedGame> candidate;bool invalidMappedWorld{};
    explicit operator bool()const noexcept{return !invalidMappedWorld&&error==Mistwatch::LoadPreparationError::None&&bool(candidate);}
};
MappedPreparationResult PrepareWithExpectedMapped(const Mistwatch::SaveGame& saved,const Mistwatch::LoadPreparationInputs& live,
    const Aevrynd::HeightField3D& terrain,std::span<const W::Box> extras,std::span<const W::Box> mapped,
    const Mistwatch::EntranceWorldAuthor* authored=nullptr){
    const auto expected=authored?authored->WorldSolids(saved.session.bridgeRepaired):
        Mistwatch::WorldSolids(saved.session.bridgeRepaired,Mistwatch::FirLayoutSelector::ForestWood,Mistwatch::FallenLogProfile::ScannedStraightV1);
    if(expected.size()!=mapped.size())return{Mistwatch::LoadPreparationError::PreparationException,{},true};
    for(std::size_t id=0;id<expected.size();++id)if(!W::SameBox(expected[id],mapped[id]))return{Mistwatch::LoadPreparationError::PreparationException,{},true};
    auto prepared=Mistwatch::PrepareSavedGame(saved,live,terrain,{},extras,Mistwatch::FirLayoutSelector::ForestWood,Mistwatch::FallenLogProfile::ScannedStraightV1,authored);
    return{prepared.error,std::move(prepared.candidate),false};
}
template<class Action>void Reject(Action&& action,std::ostream& log,const char* label,const char* expected){bool rejected{};try{std::forward<Action>(action)();}catch(const std::exception& error){rejected=std::string_view(error.what())==expected;}W::Gate(rejected,"NEGATIVE_CONTROL_WRONG_OR_MISSING_REJECTION");log<<"CONTROL "<<label<<" rejected=true reason="<<expected<<'\n';}
Aevrynd::Mesh3D Fixture(std::array<W::V,3> points){Aevrynd::Mesh3D result;for(auto p:points)result.vertices.push_back({p.x,p.y,p.z,0,0xffffffffu,0,0});result.indices={0,1,2};return result;}
void OverlayControls(W::Budget& budget,std::ostream& log){
    Aevrynd::HeightField3D terrain({{-50,0,-20},201,241,.5f});terrain.Fill(0);W::Gate(terrain.SetHeight(100,40,1),"CONTROL_TERRAIN_EDIT_FAILED");terrain.RecalculateRange();
    const auto flat=Fixture({{{-1,0,-1},{1,0,-1},{0,0,1}}});W::Pose pose;const auto footprint=W::MakeFootprint(flat,pose,terrain,budget);pose.position.y=.5f;const auto range=W::Evaluate(flat,pose,footprint,budget);W::Gate(range.minimum.Sign()<0,"INTERIOR_TERRAIN_PEAK_NOT_REJECTED");
    for(const auto& vertex:flat.vertices)W::Gate(vertex.y+pose.position.y>=terrain.SampleHeight(vertex.x,vertex.z),"CONTROL_VERTEX_ONLY_DID_NOT_PASS");log<<"CONTROL interior_terrain_peak_vertex_only_pass_but_full_overlay_rejects=true\n";
    terrain.Fill(0);const auto vertical=Fixture({{{0,-1,0},{0,1,0},{0,0,1}}});pose={};const auto lineFootprint=W::MakeFootprint(vertical,pose,terrain,budget);W::Gate(W::Evaluate(vertical,pose,lineFootprint,budget).minimum.Sign()<0,"VERTICAL_PROJECTED_LINE_NOT_COVERED");
    const auto point=Fixture({{{0,-1,0},{0,0,0},{0,1,0}}});const auto pointFootprint=W::MakeFootprint(point,pose,terrain,budget);W::Gate(W::Evaluate(point,pose,pointFootprint,budget).minimum.Sign()<0,"PROJECTED_POINT_NOT_COVERED");log<<"CONTROL projected_point_and_line_full_parameter_domain passed=true\n";
    auto reflected=pose;reflected.scale.x=-1;Reject([&]{W::VerifyPose(reflected,budget);},log,"reflection","POSE_NOT_FINITE_PROPER_UNIFORM");auto anisotropic=pose;anisotropic.scale.y=2;Reject([&]{W::VerifyPose(anisotropic,budget);},log,"anisotropy","POSE_NOT_FINITE_PROPER_UNIFORM");
    W::Gate(!Mistwatch::PropClearsObjectives({{0,0,7},{1,1,1}}),"ROAD_GUARD_NEGATIVE_CONTROL_ACCEPTED");const std::array<W::V,2> points{{{-1,-1,-1},{1,1,1}}};W::Gate(!W::ContainsActual({{0,0,0},{.5f,1,1}},points),"UNDERSIZED_BOX_NEGATIVE_CONTROL_ACCEPTED");log<<"CONTROL original_road_guard_and_undersized_box rejected=true\n";
}

struct NativeGeometry {
    std::uint64_t canopySourceTriangles{},canopySurfaceCells{},canopyEnclosedCells{},canopyPrimitiveTriangles{};
    std::array<std::uint64_t,3> broadleafTriangles{};
    std::array<std::uint64_t,5> grassTriangles{},grassVertices{};
};
NativeGeometry nativeGeometry;
std::string nativeExpectedDocument;
// 只記錄既有作者的真結果；不重造布局、接地或包圍盒。
struct Retained {
    Mistwatch::MatureFirLayout firs;std::vector<Mistwatch::ForestProp> props;Mistwatch::ForestUnderstory understory;std::vector<Mistwatch::BroadleafTree> broadleaf;Mistwatch::MiddleLayerResult middle;Mistwatch::CanopyPopulation canopy;
    W::TypedWorld before[2];std::vector<W::Solid> fullVolumes;std::vector<W::Box> wood;std::array<Aevrynd::Mesh3D,4> fern;std::array<Mistwatch::FernColonyAsset,4> fernAssets;std::array<Mistwatch::ForestCoverAsset,5> grassAssets;Aevrynd::Mesh3D shrubLeaf;
};
void Volume(Retained& world,W::Role role,W::Id id,const Aevrynd::Mesh3D& mesh,const W::Pose& pose,W::Budget& budget){
    // 保留完整原來源範圍，不以根點或細幹 collider 冒充枝冠／植株。
    const auto matrix=pose.Matrix();std::vector<W::V> points;points.reserve(mesh.vertices.size());for(const auto& vertex:mesh.vertices){budget.Op();const auto point=Aevrynd::TransformPoint(matrix,{vertex.x,vertex.y,vertex.z});W::Gate(S::Finite(point),"RETAINED_WORLD_VERTEX_NONFINITE");points.push_back(point);}world.fullVolumes.push_back({role,id,W::ConservativeBox(points,budget)});
}
void ObserveBaseline(const Retained& world,const Aevrynd::HeightField3D& terrain,std::ostream& log){
    const auto ids=W::LogRoles(world.props);std::ostringstream out;out<<"OBSERVED_ORIGINAL_BASELINE profile=forest-wood firs="<<world.firs.firs.size()<<" roots="<<world.firs.roots.size()<<" reached="<<world.firs.reachedBudget<<" props="<<world.props.size()<<" logs="<<ids.size()<<" rocks="<<world.props.size()-ids.size()<<" saplings="<<world.understory.saplings.size()<<" ferns="<<world.understory.ferns.size()<<" broadleaf="<<world.broadleaf.size()<<" fir_fingerprint="<<world.firs.fingerprint<<" terrain_fingerprint="<<W::TerrainFingerprint(terrain)<<" prop_fingerprint="<<W::PropFingerprint(world.props)<<'\n';
    for(auto id:ids){const auto& p=world.props[id];out<<"OBSERVED_ORIGINAL_LOG id="<<id<<" position=";Vector(out,p.transform.position);out<<" rotation=";Vector(out,p.transform.rotation);out<<" scale=";Vector(out,p.transform.scale);out<<" pose_bits="<<std::hex;for(auto word:W::PoseBits(p.transform))out<<"0x"<<std::setw(8)<<std::setfill('0')<<word<<',';out<<std::dec<<'\n';}const auto text=out.str();log<<text;std::cout<<text<<std::flush;
}
void CohortIdentityControls(const Retained& baseline,const Aevrynd::HeightField3D& terrain,W::Budget& budget,std::ostream& log){
    // 負控只複製實際原作者結果，不重跑作者或加入正式世界。
    for(auto id:W::CurrentLogIds){budget.Op();auto missing=baseline.props;missing.erase(missing.begin()+static_cast<std::ptrdiff_t>(id));const auto label="missing_original_log_"+std::to_string(id);Reject([&]{W::VerifyCurrentProps(missing);},log,label.c_str(),"CURRENT_PROP_COUNT_IDENTITY_FAILED");}
    auto fifth=baseline.props;fifth.push_back(baseline.props[W::CurrentLogIds.front()]);Reject([&]{W::VerifyCurrentProps(fifth);},log,"fake_fifth_original_log","CURRENT_PROP_COUNT_IDENTITY_FAILED");
    const auto rock=std::find_if(baseline.props.begin(),baseline.props.end(),[](const auto& p){return !p.FallenLog();});W::Gate(rock!=baseline.props.end(),"CONTROL_ORIGINAL_NONLOG_MISSING");const auto rockId=static_cast<std::size_t>(rock-baseline.props.begin());auto duplicated=baseline.props;duplicated[rockId].mesh=3;Reject([&]{W::VerifyCurrentProps(duplicated);},log,"duplicate_log_role_by_nonlog_replacement","CURRENT_LOG_COHORT_IDS_FAILED");
    auto nonlog=baseline.props;nonlog[rockId].collider.extents.x=std::nextafter(nonlog[rockId].collider.extents.x,(std::numeric_limits<float>::infinity)());Reject([&]{W::VerifyCurrentProps(nonlog);},log,"original_nonlog_single_ULP","CURRENT_PROP_FINGERPRINT_FAILED");
    for(auto id:W::CurrentLogIds)for(unsigned column=0;column<9;++column){budget.Op();auto changed=baseline.props;auto& pose=changed[id].transform;const std::array<float*,9> fields{{&pose.position.x,&pose.position.y,&pose.position.z,&pose.rotation.x,&pose.rotation.y,&pose.rotation.z,&pose.scale.x,&pose.scale.y,&pose.scale.z}};*fields[column]=std::nextafter(*fields[column],(std::numeric_limits<float>::infinity)());const auto label="original_log_pose_ULP_id_"+std::to_string(id)+"_column_"+std::to_string(column);Reject([&]{W::VerifyCurrentProps(changed);},log,label.c_str(),"CURRENT_LOG_POSE_BITS_FAILED");}
    auto changedTerrain=terrain;W::Gate(changedTerrain.SetHeight(0,0,std::nextafter(changedTerrain.Height(0,0),(std::numeric_limits<float>::infinity)())),"CONTROL_TERRAIN_IDENTITY_EDIT_FAILED");changedTerrain.RecalculateRange();Reject([&]{W::VerifyCurrentTerrain(changedTerrain);},log,"original_terrain_fingerprint_single_ULP","CURRENT_TERRAIN_FINGERPRINT_FAILED");budget.Time();
}
Retained Baseline(const Input& input,const Aevrynd::HeightField3D& terrain,W::Budget& budget,std::ostream& log){
    budget.Time();Retained result;result.firs=Mistwatch::CreateMatureFirLayout(terrain,Mistwatch::FirLayoutSelector::ForestWood);result.props=Mistwatch::ForestProps(terrain,result.firs.roots);result.understory=Mistwatch::CreateUnderstory(terrain,result.firs.roots,result.props);result.broadleaf=Mistwatch::CreateBroadleafTrees(terrain,result.firs.roots,result.props,result.understory);ObserveBaseline(result,terrain,log);W::VerifyCurrentBaseline(result.firs,result.props,result.understory,result.broadleaf,terrain);CohortIdentityControls(result,terrain,budget,log);
    std::array<float,4> radii{};for(unsigned id=0;id<4;++id){result.fern[id]=LoadAsset(input.assets/"mistwatch-ferns"/"meshes"/Mistwatch::FernMeshName(id),log);Mistwatch::NormalizeFernAsset(result.fern[id]);result.fernAssets[id]=Mistwatch::FernColonyAssetFromMesh(result.fern[id]);radii[id]=result.fernAssets[id].radius;}
    for(unsigned state=0;state<2;++state){result.before[state]=W::BaseWorld(state!=0,result.firs,result.props,result.understory,result.broadleaf);const auto expected=Mistwatch::WorldSolids(state!=0,Mistwatch::FirLayoutSelector::ForestWood),actual=W::Boxes(result.before[state]);log<<"OBSERVED_ORIGINAL_WORLD bridge_state="<<state<<" formal_count="<<expected.size()<<" typed_count="<<actual.size()<<'\n';W::Gate(expected.size()==(state?224u:223u)&&actual.size()==expected.size(),"TYPED_FORMAL_WORLD_COUNT_DIVERGED");for(std::size_t id=0;id<actual.size();++id)W::Gate(W::SameBox(actual[id],expected[id]),"TYPED_FORMAL_WORLD_ORDER_OR_BYTES_DIVERGED");}
    result.middle=Mistwatch::CreateMiddleLayer(terrain,result.firs.roots,result.props,result.understory,result.broadleaf,W::Boxes(result.before[0]),24,radii);W::Gate(result.middle.reachedBudget&&result.middle.trees.size()==24,"RETAINED_MIDDLE_QUOTA");for(W::Id id=0;id<result.middle.trees.size();++id)for(auto& world:result.before)world.push_back({W::Role::Middle,id,result.middle.trees[id].trunk});
    constexpr const char* firSlots[]{"bark","twig","dead-branches","trunk-c"};std::array<Aevrynd::Mesh3D,5> firMeshes;
    for(unsigned id=0;id<4;++id)firMeshes[id]=LoadAsset(input.assets/"mistwatch-forest"/"meshes"/("fir-tree-01-c-lod1-node0-mesh0-primitive"+std::to_string(id)+"-material"+std::to_string(id)+"-fir-tree-01-"+firSlots[id]+".aemesh"),log);firMeshes[4]=LoadAsset(input.assets/"mistwatch-forest"/"meshes"/"fir-tree-01-c-lod0-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh",log);
    for(W::Id id=0;id<result.firs.firs.size();++id)for(const auto& mesh:firMeshes)Volume(result,W::Role::MatureFir,id,mesh,result.firs.firs[id].render,budget);for(W::Id id=0;id<result.understory.saplings.size();++id)for(const auto& mesh:firMeshes)Volume(result,W::Role::Sapling,id,mesh,result.understory.saplings[id],budget);
    std::array<Aevrynd::Mesh3D,3> broadleaf;for(unsigned slot=0;slot<3;++slot){broadleaf[slot]=LoadAsset(input.assets/"mistwatch-broadleaf"/"meshes"/Mistwatch::BroadleafMeshName(slot),log);if(slot==1)result.shrubLeaf=broadleaf[slot];Mistwatch::RootBroadleafMesh(broadleaf[slot]);nativeGeometry.broadleafTriangles[slot]=broadleaf[slot].indices.size()/3;}
    for(W::Id id=0;id<result.broadleaf.size();++id)for(const auto& mesh:broadleaf)Volume(result,W::Role::Broadleaf,id,mesh,result.broadleaf[id].transform,budget);for(W::Id id=0;id<result.middle.trees.size();++id)for(const auto& mesh:broadleaf)Volume(result,W::Role::Middle,id,mesh,result.middle.trees[id].transform,budget);
    for(W::Id id=0;id<result.understory.ferns.size();++id)Volume(result,W::Role::Sapling,static_cast<W::Id>(1000+id),result.fern[id%4],result.understory.ferns[id],budget);
    constexpr const char* canopyNames[]{"island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh","island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh","island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh"};std::array<Aevrynd::Mesh3D,3> canopyMeshes;for(unsigned slot=0;slot<3;++slot)canopyMeshes[slot]=LoadAsset(input.assets/"mistwatch-canopy-preview"/"meshes"/canopyNames[slot],log);
    const auto geometry=Mistwatch::MeasureCanopy(canopyMeshes);result.canopy=Mistwatch::CreateCanopyPopulation(canopyMeshes,geometry,terrain,W::Boxes(result.before[0]),result.understory,{}, {},result.fernAssets,{});W::Gate(result.canopy.reachedBudget&&result.canopy.plants.size()==4&&result.canopy.fingerprint==6584434718760703877ull,"RETAINED_CANOPY_FORMAL_IDENTITY_CHANGED");const auto stem=LoadAsset(input.assets/"mistwatch-canopy-preview"/"meshes"/"island-tree-01-lod2-trunk-root-stem.aemesh",log);
    for(W::Id id=0;id<result.canopy.plants.size();++id){budget.Time();auto& plant=result.canopy.plants[id];plant.rootedTrunk=Mistwatch::PlantDerivedRootStem(stem,geometry,plant.pose,terrain);plant.lower=Mistwatch::ExtendRootCollision(plant.lower,plant.rootedTrunk,plant.pose);Volume(result,W::Role::CanopyWood,id,plant.rootedTrunk,plant.pose,budget);Volume(result,W::Role::CanopyWood,id,canopyMeshes[1],plant.pose,budget);Volume(result,W::Role::CanopyWood,id,canopyMeshes[2],plant.pose,budget);const auto proxy=Mistwatch::CreateCanopyWoodProxy(plant.rootedTrunk,canopyMeshes[2],plant.pose);nativeGeometry.canopySourceTriangles+=proxy.sourceTriangles;nativeGeometry.canopySurfaceCells+=proxy.surfaceCells;nativeGeometry.canopyEnclosedCells+=proxy.enclosedGridCells;const auto primitive=plant.rootedTrunk.indices.size()/3+canopyMeshes[1].indices.size()/3+canopyMeshes[2].indices.size()/3;if(!id)nativeGeometry.canopyPrimitiveTriangles=primitive;else W::Gate(nativeGeometry.canopyPrimitiveTriangles==primitive,"NATIVE_EXPECTED_CANOPY_ROOTED_RECIPE_CHANGED");W::Gate(result.wood.size()+proxy.parts.size()<=9000,"RETAINED_CANOPY_WOOD_QUOTA");for(const auto& box:proxy.parts){const auto partId=static_cast<W::Id>(result.wood.size());result.wood.push_back(box);result.fullVolumes.push_back({W::Role::CanopyWood,partId,box});for(auto& world:result.before)world.push_back({W::Role::CanopyWood,partId,box});}}
    for(const auto& value:result.before[0])if(value.role!=W::Role::ForestProp&&value.role!=W::Role::CanopyWood)result.fullVolumes.push_back(value);
    for(unsigned id=0;id<5;++id){auto grass=LoadAsset(input.assets/"mistwatch-grass"/"meshes"/Mistwatch::GrassMeshName(id),log);Mistwatch::NormalizeGrassAsset(grass);nativeGeometry.grassTriangles[id]=grass.indices.size()/3;nativeGeometry.grassVertices[id]=grass.vertices.size();result.grassAssets[id]=Mistwatch::ForestCoverAssetFromMesh(grass);}
    budget.Time();log<<"RETAINED fir_fingerprint="<<result.firs.fingerprint<<" middle_fingerprint="<<Mistwatch::MiddleLayerTransformFingerprint(result.middle.trees)<<" canopy_fingerprint="<<result.canopy.fingerprint<<" full_volume_count="<<result.fullVolumes.size()<<" wood_parts="<<result.wood.size()<<" before_false="<<result.before[0].size()<<" before_true="<<result.before[1].size()<<'\n';return result;
}
void FootprintProof(const W::Placement& scan,const W::Footprint& footprint,const Aevrynd::Mesh3D& raw,const Input& input,W::Budget& budget,std::ostream& summary){
    std::ostringstream proof;proof<<"PROP "<<scan.propId<<" original_anchor=";Vector(proof,scan.anchor);proof<<" position=";Vector(proof,scan.pose.position);proof<<" rotationYXZ=";Vector(proof,scan.pose.rotation);proof<<" uniform_scale=";Vector(proof,scan.pose.scale);proof<<" new_conservative_box=";BoxText(proof,scan.collider);proof<<" predecessorY="<<std::hexfloat<<scan.predecessorY<<std::defaultfloat<<'\n';
    proof<<"COMPLETE source_faces="<<footprint.sourceFaces<<" nonempty_pairs="<<footprint.nonemptyPairs<<" constraints="<<footprint.contacts.size()<<" rational_words="<<footprint.words<<" final_min="<<Qtext(scan.finalRange.minimum)<<" final_max_all_surface="<<Qtext(scan.finalRange.maximum)<<" final_negative="<<scan.finalRange.negative<<" predecessor_min="<<Qtext(scan.predecessorRange.minimum)<<" lattice_evaluations="<<scan.evaluations<<" art=false lower_envelope_distribution_qualified=false\n";
    const auto points=W::ActualPoints(raw,scan.pose,budget);std::vector<W::Q> heights;for(auto point:points)heights.push_back(W::QF(point.y,budget));
    for(std::size_t id=0;id<footprint.contacts.size();++id){const auto& c=footprint.contacts[id];const auto gap=W::QS(W::Linear({{heights[c.sourceIds[0]],heights[c.sourceIds[1]],heights[c.sourceIds[2]]}},c.parameter,budget),c.terrainY,budget);proof<<"CONTACT "<<id<<" source_triangle="<<c.face<<" source_ids="<<c.sourceIds[0]<<','<<c.sourceIds[1]<<','<<c.sourceIds[2]<<" terrain_cell="<<c.cellX<<','<<c.cellZ<<" terrain_half="<<c.terrainHalf<<" u="<<Qtext(c.parameter.u)<<" v="<<Qtext(c.parameter.v)<<" terrainY="<<Qtext(c.terrainY)<<" actual_f32_gap="<<Qtext(gap)<<'\n';W::Gate(gap.Sign()>=0,"OUTPUT_FINAL_CONTACT_NEGATIVE");}
    for(W::Id id=0;id<points.size();++id){proof<<"ACTUAL_VERTEX "<<id<<' ';Vector(proof,points[id]);proof<<'\n';}
    const auto text=proof.str();const auto filename="footprint-prop-"+std::to_string(scan.propId)+".txt";WriteNew(input.output/filename,text);summary<<"SEATED_PROP id="<<scan.propId<<" complete_proof="<<filename<<" bytes="<<text.size()<<" sha256="<<Sha(text)<<" final_min="<<Qtext(scan.finalRange.minimum)<<" predecessor_min="<<Qtext(scan.predecessorRange.minimum)<<'\n';
}
std::uint64_t MixPlants(std::span<const Mistwatch::ForestCoverPlant> plants){std::uint64_t hash=14695981039346656037ull;for(const auto& p:plants){for(float value:{p.transform.position.x,p.transform.position.y,p.transform.position.z,p.transform.rotation.x,p.transform.rotation.y,p.transform.rotation.z,p.transform.scale.x,p.transform.scale.y,p.transform.scale.z,p.radius}){const auto word=std::bit_cast<std::uint32_t>(value);hash^=word;hash*=1099511628211ull;}hash^=p.variant;hash*=1099511628211ull;hash^=p.patch;hash*=1099511628211ull;}return hash;}
bool SameMesh(const Aevrynd::Mesh3D& a,const Aevrynd::Mesh3D& b){return a.vertices.size()==b.vertices.size()&&a.indices==b.indices&&(a.vertices.empty()||std::memcmp(a.vertices.data(),b.vertices.data(),a.vertices.size()*sizeof(Aevrynd::Vertex3D))==0);}
void FullRepeats(const Mistwatch::TrailLitterResult& a,const Mistwatch::TrailLitterResult& b,const Mistwatch::ForestCoverResult& c,const Mistwatch::ForestCoverResult& d,const Mistwatch::FernColonyResult& e,const Mistwatch::FernColonyResult& f,const Mistwatch::ShrubCanopyResult& g,const Mistwatch::ShrubCanopyResult& h){
    W::Gate(a.reachedBudget==b.reachedBudget&&a.branchCount==b.branchCount&&a.stoneCount==b.stoneCount&&a.rejectedClearance==b.rejectedClearance&&a.rejectedBudget==b.rejectedBudget&&a.patches.size()==b.patches.size()&&SameMesh(a.branches,b.branches)&&SameMesh(a.stones,b.stones),"LITTER_FULL_REPEAT_CHANGED");
    for(std::size_t id=0;id<a.patches.size();++id){const auto& x=a.patches[id];const auto& y=b.patches[id];W::Gate(W::SameV(x.centre,y.centre)&&x.halfX==y.halfX&&x.halfZ==y.halfZ&&x.branchCount==y.branchCount&&x.stoneCount==y.stoneCount,"LITTER_PATCH_REPEAT_CHANGED");}
    W::Gate(c.patches.size()==d.patches.size()&&c.plants.size()==d.plants.size()&&c.rejectedClearance==d.rejectedClearance&&c.rejectedTerrain==d.rejectedTerrain&&c.rejectedSpacing==d.rejectedSpacing,"COVER_FULL_REPEAT_METADATA_CHANGED");
    for(std::size_t id=0;id<c.patches.size();++id){const auto& x=c.patches[id];const auto& y=d.patches[id];W::Gate(W::SameV(x.centre,y.centre)&&x.radius==y.radius&&x.aspect==y.aspect&&x.yaw==y.yaw&&x.requested==y.requested&&x.accepted==y.accepted,"COVER_PATCH_REPEAT_CHANGED");}for(std::size_t id=0;id<c.plants.size();++id){const auto& x=c.plants[id];const auto& y=d.plants[id];W::Gate(W::SamePose(x.transform,y.transform)&&x.variant==y.variant&&x.patch==y.patch&&x.radius==y.radius,"COVER_PLANT_REPEAT_CHANGED");}
    W::Gate(e.colonies.size()==f.colonies.size()&&e.plants.size()==f.plants.size()&&e.rejectedClearance==f.rejectedClearance&&e.rejectedTerrain==f.rejectedTerrain&&e.rejectedExistingFern==f.rejectedExistingFern&&e.rejectedGrass==f.rejectedGrass&&e.rejectedSpacing==f.rejectedSpacing,"COLONIES_FULL_REPEAT_METADATA_CHANGED");for(std::size_t id=0;id<e.colonies.size();++id){const auto& x=e.colonies[id];const auto& y=f.colonies[id];W::Gate(W::SameV(x.centre,y.centre)&&x.radius==y.radius&&x.grassPatch==y.grassPatch&&x.requested==y.requested&&x.accepted==y.accepted,"COLONY_REPEAT_CHANGED");}for(std::size_t id=0;id<e.plants.size();++id){const auto& x=e.plants[id];const auto& y=f.plants[id];W::Gate(W::SamePose(x.transform,y.transform)&&x.variant==y.variant&&x.colony==y.colony&&x.grassPatch==y.grassPatch&&x.radius==y.radius,"COLONY_PLANT_FULL_REPEAT_CHANGED");}
    W::Gate(g.plants.size()==h.plants.size()&&g.attempted==h.attempted&&g.rejectedClearance==h.rejectedClearance&&g.rejectedTerrain==h.rejectedTerrain&&g.rejectedVegetation==h.rejectedVegetation,"SHRUB_FULL_REPEAT_METADATA_CHANGED");for(unsigned id=0;id<4;++id){const auto& x=g.variants[id];const auto& y=h.variants[id];W::Gate(SameMesh(x.stems,y.stems)&&SameMesh(x.leaves,y.leaves)&&x.radius==y.radius&&x.height==y.height&&x.minimumY==y.minimumY&&x.maximumY==y.maximumY&&x.leafCount==y.leafCount&&x.stemCount==y.stemCount&&x.attachments.size()==y.attachments.size(),"SHRUB_VARIANT_FULL_REPEAT_CHANGED");for(std::size_t at=0;at<x.attachments.size();++at)W::Gate(x.attachments[at].vertex==y.attachments[at].vertex&&W::SameV(x.attachments[at].centre,y.attachments[at].centre)&&x.attachments[at].branchRadius==y.attachments[at].branchRadius,"SHRUB_ATTACHMENT_REPEAT_CHANGED");}for(std::size_t id=0;id<g.plants.size();++id)W::Gate(W::SamePose(g.plants[id].pose,h.plants[id].pose)&&g.plants[id].variant==h.plants[id].variant&&g.plants[id].radius==h.plants[id].radius,"SHRUB_PLANT_REPEAT_CHANGED");
}

// 原生收據的 FNV-1a 是每個 uint32 的小端四 byte；原 MixPlants 原文保留作更豐富的重複性身分。
using NativeFields=std::map<std::string,std::string>;
constexpr std::uint64_t NativeFnvOffset=14695981039346656037ull,NativeFnvPrime=1099511628211ull;
void NativeMix(std::uint64_t& hash,std::uint32_t word){for(unsigned byte=0;byte<4;++byte){hash^=(word>>(byte*8))&255u;hash*=NativeFnvPrime;}}
void NativeFloat(std::uint64_t& hash,float value){W::Gate(std::isfinite(value),"NATIVE_EXPECTED_NONFINITE");NativeMix(hash,std::bit_cast<std::uint32_t>(value));}
void NativePose(std::uint64_t& hash,const W::Pose& pose,unsigned variant){
    for(float value:{pose.position.x,pose.position.y,pose.position.z,pose.rotation.x,pose.rotation.y,pose.rotation.z,pose.scale.x,pose.scale.y,pose.scale.z})NativeFloat(hash,value);NativeMix(hash,variant);
}
std::uint64_t NativeCoverHash(std::span<const Mistwatch::ForestCoverPlant> plants){
    std::uint64_t hash=NativeFnvOffset;for(const auto& p:plants){const auto root=p.transform.position;
        for(float value:{root.x,root.y,root.z,Mistwatch::ForestCoverScale(root),root.x+root.z})NativeFloat(hash,value);NativeMix(hash,p.variant);}return hash;
}
std::uint64_t NativeSolidHash(std::span<const W::Box> solids){
    std::uint64_t hash=NativeFnvOffset;for(const auto& s:solids)for(float value:{s.center.x,s.center.y,s.center.z,s.extents.x,s.extents.y,s.extents.z})NativeFloat(hash,value);return hash;
}
std::uint64_t NativeColonyHash(std::span<const Mistwatch::FernColonyPlant> plants){std::uint64_t hash=NativeFnvOffset;for(const auto& p:plants)NativePose(hash,p.transform,p.variant);return hash;}
std::uint64_t NativeOriginalFernHash(std::span<const W::Pose> plants){std::uint64_t hash=NativeFnvOffset;for(std::size_t i=0;i<plants.size();++i)NativePose(hash,plants[i],static_cast<unsigned>(i%4));return hash;}
template<class T,std::size_t N>std::string NativeCsv(const std::array<T,N>& values,bool trailing=false){std::ostringstream out;for(std::size_t i=0;i<N;++i){if(i)out<<',';out<<values[i];}if(trailing)out<<',';return out.str();}
std::string NativeJsonString(std::string_view value){
    std::ostringstream out;out<<'"';constexpr char hex[]="0123456789abcdef";
    for(char raw:value){const auto c=static_cast<unsigned char>(raw);if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);else if(c<32)out<<"\\u00"<<hex[c>>4]<<hex[c&15];else out<<static_cast<char>(c);}out<<'"';return out.str();
}
std::string NativeJsonMap(const NativeFields& values){std::ostringstream out;out<<'{';bool first=true;for(const auto& [key,value]:values){if(!first)out<<',';first=false;out<<NativeJsonString(key)<<':'<<NativeJsonString(value);}out<<'}';return out.str();}
std::string NativeUpper(std::string value){for(auto& c:value)if(c>='a'&&c<='f')c=static_cast<char>(c-'a'+'A');return value;}
void NativeField(NativeFields& fields,const char* key,const std::string& value){W::Gate(fields.emplace(key,value).second,"NATIVE_EXPECTED_DUPLICATE_FIELD");}
template<class T>void NativeNumber(NativeFields& fields,const char* key,T value){NativeField(fields,key,std::to_string(value));}
void NativeBoolean(NativeFields& fields,const char* key,bool value){NativeField(fields,key,value?"1":"0");}
NativeFields NativeContract(const Retained& retained,const W::TypedWorld& mapped,const Mistwatch::TrailLitterResult& litter,
    const Mistwatch::ForestCoverResult& cover,const Mistwatch::FernColonyResult& colonies,const Mistwatch::ShrubCanopyResult& shrubs,
    std::string_view worldProfile,std::string_view fallenProfile,bool fullSolidVolumes){
    W::Gate(worldProfile=="forest-wood"&&fallenProfile=="scanned-straight-v1","NATIVE_EXPECTED_WRONG_PROFILE");
    W::Gate(fullSolidVolumes,"NATIVE_EXPECTED_FULL_SOLID_VOLUMES_REQUIRED");
    W::Gate(cover.reachedBudget&&cover.plants.size()==3600&&colonies.reachedBudget&&colonies.plants.size()==300&&shrubs.reachedBudget&&shrubs.plants.size()==64&&retained.understory.ferns.size()==420&&retained.understory.saplings.size()==42,"NATIVE_EXPECTED_PRODUCTION_QUOTA");
    W::Gate(mapped.size()==4582&&retained.wood.size()==4335&&retained.firs.firs.size()==100&&retained.middle.trees.size()==24&&retained.canopy.plants.size()==4,"NATIVE_EXPECTED_INITIAL_WORLD_IDENTITY");
    const Mistwatch::ForestCoverSettings coverSettings{};const Mistwatch::FernColonySettings colonySettings{};
    std::array<unsigned,5> coverVariants{};std::array<unsigned,4> colonyVariants{};
    for(const auto& p:cover.plants){W::Gate(p.variant<5,"NATIVE_EXPECTED_COVER_VARIANT");++coverVariants[p.variant];}
    for(const auto& p:colonies.plants){W::Gate(p.variant<4,"NATIVE_EXPECTED_COLONY_VARIANT");++colonyVariants[p.variant];}
    std::array<std::uint64_t,4> renderLeaves{},stemIndices{},leafIndices{};std::uint64_t authorTriangles{},renderTriangles{},uploaded{};
    for(unsigned i=0;i<4;++i){const auto crown=Mistwatch::CreateShrubOpenCrown(shrubs.variants[i],i);W::Gate(crown.stems.Valid()&&crown.leaves.Valid(),"NATIVE_EXPECTED_SHRUB_RECIPE_INVALID");renderLeaves[i]=crown.renderLeafCount;stemIndices[i]=shrubs.variants[i].stems.indices.size();leafIndices[i]=crown.leaves.indices.size();uploaded+=2;}
    for(const auto& p:shrubs.plants){W::Gate(p.variant<4,"NATIVE_EXPECTED_SHRUB_VARIANT");authorTriangles+=(shrubs.variants[p.variant].stems.indices.size()+shrubs.variants[p.variant].leaves.indices.size())/3;renderTriangles+=(stemIndices[p.variant]+leafIndices[p.variant])/3;}
    NativeFields out;
    NativeField(out,"world_profile",std::string(worldProfile));NativeField(out,"forest_cover_mode","fixed_cluster_patches");
    NativeNumber(out,"ground_cover_instances",cover.plants.size());NativeNumber(out,"forest_cover_seed",coverSettings.seed);
    NativeNumber(out,"forest_cover_authored_population",coverSettings.targetPlants);NativeNumber(out,"forest_cover_maximum_patches",coverSettings.maximumPatches);
    NativeNumber(out,"forest_cover_patch_minimum_plants",coverSettings.minimumPlantsPerPatch);NativeNumber(out,"forest_cover_patch_maximum_plants",coverSettings.maximumPlantsPerPatch);
    NativeNumber(out,"forest_cover_independent_anchors",coverSettings.independentAnchors);NativeNumber(out,"forest_cover_patches",cover.patches.size());
    NativeBoolean(out,"forest_cover_reached_budget",cover.reachedBudget);NativeNumber(out,"forest_cover_fingerprint",NativeCoverHash(cover.plants));
    NativeNumber(out,"forest_cover_clearance_colliders",mapped.size());NativeNumber(out,"forest_cover_clearance_collider_fingerprint",NativeSolidHash(W::Boxes(mapped)));
    NativeField(out,"forest_cover_variants",NativeCsv(coverVariants));
    NativeNumber(out,"fern_colony_population",colonies.plants.size());NativeNumber(out,"fern_colony_authored_population",colonySettings.targetPlants);
    NativeNumber(out,"fern_colonies",colonies.colonies.size());NativeNumber(out,"fern_colony_seed",colonySettings.seed);
    NativeBoolean(out,"fern_colony_reached_budget",colonies.reachedBudget);NativeNumber(out,"fern_colony_fingerprint",NativeColonyHash(colonies.plants));
    NativeNumber(out,"fern_colony_original_fingerprint",NativeOriginalFernHash(retained.understory.ferns));NativeField(out,"fern_colony_variants",NativeCsv(colonyVariants));
    NativeNumber(out,"understory_ferns",retained.understory.ferns.size());NativeNumber(out,"understory_saplings",retained.understory.saplings.size());
    NativeBoolean(out,"shrubs_enabled",true);NativeNumber(out,"shrubs_plants",shrubs.plants.size());NativeNumber(out,"shrubs_fingerprint",shrubs.fingerprint);
    NativeNumber(out,"shrubs_author_leaf_count",shrubs.variants[0].leafCount);NativeField(out,"shrubs_render_leaf_counts",NativeCsv(renderLeaves));
    NativeNumber(out,"shrubs_author_instance_triangles",authorTriangles);NativeNumber(out,"shrubs_render_instance_triangles",renderTriangles);
    NativeNumber(out,"shrubs_uploaded_meshes",uploaded);NativeField(out,"shrubs_uploaded_stem_indices",NativeCsv(stemIndices));NativeField(out,"shrubs_uploaded_leaf_indices",NativeCsv(leafIndices));
    NativeNumber(out,"shrubs_submitted",shrubs.plants.size());NativeBoolean(out,"middle_layer_comparison",true);
    NativeNumber(out,"middle_layer_population",retained.middle.trees.size());NativeNumber(out,"middle_layer_target",24);
    NativeBoolean(out,"middle_layer_reached_budget",retained.middle.reachedBudget);NativeNumber(out,"middle_layer_attempted",retained.middle.attempted);
    NativeNumber(out,"middle_layer_transform_fingerprint",Mistwatch::MiddleLayerTransformFingerprint(retained.middle.trees));
    NativeNumber(out,"submitted_middle_layer",retained.middle.trees.size());NativeNumber(out,"submitted_middle_layer_triangles",retained.middle.trees.size()*std::accumulate(nativeGeometry.broadleafTriangles.begin(),nativeGeometry.broadleafTriangles.end(),std::uint64_t{}));
    NativeField(out,"fir_layout","forest-wood-mature");NativeNumber(out,"fir_layout_population",retained.firs.firs.size());NativeBoolean(out,"fir_layout_reached_budget",retained.firs.reachedBudget);
    NativeNumber(out,"fir_layout_fingerprint",retained.firs.fingerprint);NativeNumber(out,"fir_layout_patch_centres",retained.firs.patchCentres.size());
    NativeField(out,"canopy_collision_mode","wood-grid-conservative");NativeNumber(out,"canopy_collision_parts",retained.wood.size());NativeField(out,"canopy_collision_spacing_m","0.15");
    NativeNumber(out,"canopy_wood_source_triangles",nativeGeometry.canopySourceTriangles);NativeNumber(out,"canopy_wood_surface_cells",nativeGeometry.canopySurfaceCells);NativeNumber(out,"canopy_wood_enclosed_grid_cells",nativeGeometry.canopyEnclosedCells);
    NativeBoolean(out,"canopy_world_profile",true);NativeBoolean(out,"canopy_enabled",true);NativeNumber(out,"canopy_plants",retained.canopy.plants.size());NativeNumber(out,"canopy_population_fingerprint",retained.canopy.fingerprint);
    NativeNumber(out,"canopy_ground_checks",retained.canopy.groundChecks);NativeNumber(out,"canopy_rooted_meshes",retained.canopy.plants.size());NativeNumber(out,"canopy_primitive_triangles",nativeGeometry.canopyPrimitiveTriangles);
    NativeNumber(out,"canopy_materials",3);NativeNumber(out,"canopy_submitted_slots",retained.canopy.plants.size()*3);
    NativeBoolean(out,"trail_litter_enabled",true);NativeNumber(out,"trail_litter_patches",litter.patches.size());NativeNumber(out,"trail_litter_branches",litter.branchCount);NativeNumber(out,"trail_litter_stones",litter.stoneCount);
    NativeNumber(out,"trail_litter_fingerprint",litter.fingerprint);NativeNumber(out,"trail_litter_triangles",(litter.branches.indices.size()+litter.stones.indices.size())/3);
    W::Gate(out.size()==70,"NATIVE_EXPECTED_CONTRACT_KEY_COUNT");return out;
}
struct NativeFernSubmission {std::uint64_t plants{},triangles{};std::array<unsigned,4> variants{};};
NativeFernSubmission NativeOriginalSubmission(const Retained& retained,W::V feet){
    NativeFernSubmission result;for(std::size_t i=0;i<retained.understory.ferns.size();++i){const auto& plant=retained.understory.ferns[i];if(Mistwatch::UnderstoryVisibility(Aevrynd::Length(plant.position-feet))<=0)continue;
        const auto variant=i%4;W::Gate(retained.fern[variant].Valid(),"NATIVE_EXPECTED_ORIGINAL_FERN_MESH_INVALID");++result.plants;++result.variants[variant];result.triangles+=retained.fern[variant].indices.size()/3;}return result;
}
std::string NativeFernJson(const NativeFernSubmission& value){std::ostringstream out;out<<"{\"plants\":"<<value.plants<<",\"triangles\":"<<value.triangles<<",\"variants\":"<<NativeJsonString(NativeCsv(value.variants))<<'}';return out.str();}
void NativeExpectedControls(const Retained& retained,const Aevrynd::HeightField3D& terrain,const W::TypedWorld& mapped,
    const Mistwatch::TrailLitterResult& litter,const Mistwatch::ForestCoverResult& cover,const Mistwatch::FernColonyResult& colonies,
    const Mistwatch::ShrubCanopyResult& shrubs,const NativeFields& fields,W::Budget& budget,std::ostream& log){
    Reject([&]{(void)NativeContract(retained,mapped,litter,cover,colonies,shrubs,"forest-canopy","scanned-straight-v1",true);},log,"native_expected_wrong_world_profile","NATIVE_EXPECTED_WRONG_PROFILE");
    Reject([&]{(void)NativeContract(retained,mapped,litter,cover,colonies,shrubs,"forest-wood","legacy-cylinder",true);},log,"native_expected_wrong_fallen_profile","NATIVE_EXPECTED_WRONG_PROFILE");
    Reject([&]{(void)NativeContract(retained,mapped,litter,cover,colonies,shrubs,"forest-wood","scanned-straight-v1",false);},log,"native_expected_false_mode_export","NATIVE_EXPECTED_FULL_SOLID_VOLUMES_REQUIRED");
    auto missing=cover;missing.plants.pop_back();Reject([&]{(void)NativeContract(retained,mapped,litter,missing,colonies,shrubs,"forest-wood","scanned-straight-v1",true);},log,"native_expected_missing_cover_plant","NATIVE_EXPECTED_PRODUCTION_QUOTA");
    auto pose=Mistwatch::FallenLogPlacements().front();pose.pose.position.x=std::bit_cast<float>(std::bit_cast<std::uint32_t>(pose.pose.position.x)^1u);
    Reject([&]{W::Gate(PlacementWords(pose)==ExpectedPoseAndBox.front(),"NATIVE_EXPECTED_POSE_WORD_IDENTITY");},log,"native_expected_pose_single_bit","NATIVE_EXPECTED_POSE_WORD_IDENTITY");
    auto solid=W::Boxes(mapped);solid.front().center.x=std::bit_cast<float>(std::bit_cast<std::uint32_t>(solid.front().center.x)^1u);
    W::Gate(std::to_string(NativeSolidHash(solid))!=fields.at("forest_cover_clearance_collider_fingerprint"),"NATIVE_EXPECTED_SOLID_SINGLE_BIT_ACCEPTED");
    log<<"CONTROL native_expected_solid_single_bit identity_differs=true\n";
    std::vector<W::Box> vegetation;for(const auto& s:mapped)if(s.role!=W::Role::CanopyWood)vegetation.push_back(s.box);
    // 故意使用原 default=false 作反例，真 producer 結果須與 full-volume 身分不同；不讀 native 答案。
    const auto falseCover=Mistwatch::CreateForestCover(terrain,retained.firs.roots,retained.understory,vegetation,retained.grassAssets,{},retained.wood,false);budget.Time();
    const auto falseColony=Mistwatch::CreateFernColonies(terrain,falseCover,retained.understory.ferns,vegetation,retained.fernAssets,{},retained.wood,false);budget.Time();
    const auto falseShrub=Mistwatch::CreateShrubCanopy(retained.shrubLeaf,terrain,vegetation,retained.understory,falseCover,falseColony,retained.fernAssets,64,retained.wood,false);budget.Time();
    const bool different=NativeCoverHash(falseCover.plants)!=NativeCoverHash(cover.plants)||NativeColonyHash(falseColony.plants)!=NativeColonyHash(colonies.plants)||falseShrub.fingerprint!=shrubs.fingerprint;
    W::Gate(different,"NATIVE_EXPECTED_FALSE_PRODUCER_MODE_NOT_DETECTED");
    log<<"CONTROL native_expected_false_producer_mode identity_differs=true false_cover="<<NativeCoverHash(falseCover.plants)<<" false_colony="<<NativeColonyHash(falseColony.plants)<<" false_shrub="<<falseShrub.fingerprint<<'\n';
}

// 身分雜湊另有 128 MiB 有限讀取域；不更動原網格 loader 的 32 MiB gate。
struct NativeFileIdentity {std::string path,sha256;std::uint64_t bytes{};};
NativeFileIdentity NativeIdentity(const std::filesystem::path& path){
    const DWORD attributes=GetFileAttributesW(path.c_str());W::Gate(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)),"NATIVE_EXPECTED_IDENTITY_REPARSE_OR_MISSING");
    Handle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));W::Gate(file.value!=INVALID_HANDLE_VALUE,"NATIVE_EXPECTED_IDENTITY_OPEN");
    LARGE_INTEGER size{};W::Gate(GetFileSizeEx(file.value,&size)&&size.QuadPart>0&&size.QuadPart<=134217728,"NATIVE_EXPECTED_IDENTITY_READ_DOMAIN");
    std::vector<std::byte> raw(static_cast<std::size_t>(size.QuadPart));DWORD count{};W::Gate(ReadFile(file.value,raw.data(),static_cast<DWORD>(raw.size()),&count,nullptr)&&count==raw.size(),"NATIVE_EXPECTED_IDENTITY_READ");
    return{path.generic_string(),NativeUpper(Sha(raw)),static_cast<std::uint64_t>(raw.size())};
}
std::string NativeIdentityJson(const NativeFileIdentity& value){std::ostringstream out;out<<"{\"path\":"<<NativeJsonString(value.path)<<",\"bytes\":"<<value.bytes<<",\"sha256\":"<<NativeJsonString(value.sha256)<<'}';return out.str();}
struct NativeSourcePin {const char* path;std::uint64_t bytes;const char* sha;};
NativeFileIdentity NativePinned(const NativeSourcePin& pin){auto row=NativeIdentity(pin.path);W::Gate(row.bytes==pin.bytes&&row.sha256==NativeUpper(pin.sha),"NATIVE_EXPECTED_PIN_CHANGED");return row;}
std::string NativeRuntimeInventory(const Input& input,W::Budget& budget){
    const auto& root=input.assets;
    // 未指定版本的既有入口仍使用原 387 門；明確版本另驗完整來源映射。
    if(input.runtimeProfile){
        const auto rows=Mistwatch::QualificationRuntime::Validate(input.root,root,*input.runtimeProfile,{input.generatedRootStem});
        std::ostringstream out;out<<'[';bool first=true;
        for(const auto& row:rows){budget.Time();if(!first)out<<',';first=false;out<<NativeIdentityJson({row.path,row.sha256,row.bytes});}
        out<<']';return out.str();
    }
    for(auto p=root;!p.empty();){const auto attributes=GetFileAttributesW(p.c_str());W::Gate(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"NATIVE_EXPECTED_ASSET_ANCESTOR_REPARSE");const auto parent=p.parent_path();if(parent==p)break;p=parent;}
    std::map<std::string,std::filesystem::path> files;std::uint64_t total{};
    for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){
        budget.Time();const auto attributes=GetFileAttributesW(entry.path().c_str());W::Gate(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"NATIVE_EXPECTED_ASSET_REPARSE");if(entry.is_directory())continue;
        W::Gate(entry.is_regular_file()&&files.size()<512,"NATIVE_EXPECTED_RUNTIME_FILE_DOMAIN");
        const auto relative=entry.path().lexically_relative(root).generic_string();W::Gate(!relative.empty()&&relative.find("..")==std::string::npos&&files.emplace(relative,entry.path()).second,"NATIVE_EXPECTED_RUNTIME_MEMBER_INVALID");
    }
    W::Gate(files.size()==387,"NATIVE_EXPECTED_RUNTIME_MEMBERSHIP_COUNT");std::ostringstream out;out<<'[';bool first=true;
    for(const auto& [relative,path]:files){budget.Time();auto row=NativeIdentity(path);total+=row.bytes;W::Gate(total<=2147483648ull,"NATIVE_EXPECTED_RUNTIME_TOTAL_DOMAIN");row.path=relative;if(!first)out<<',';first=false;out<<NativeIdentityJson(row);}out<<']';return out.str();
}
// 僅紀錄本次正式來源與部署資產；舊收據不參與本次資格判定。
void NativeAncestors(const std::filesystem::path& path){
    for(auto p=path;!p.empty();){const auto attributes=GetFileAttributesW(p.c_str());W::Gate(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_REPARSE_POINT),"FORMAL_SOURCE_ANCESTOR_REPARSE");const auto parent=p.parent_path();if(parent==p)break;p=parent;}
}
std::string NativeFallenSourceAssets(const Input& input,W::Budget& budget){
    const NativeSourcePin pins[]{
        {"meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh",434012ull,"B7F6A05BF928787082DEA92A0051557496E45894A9C2B1A0AF1E51110ABD0D46"},
        {"materials/slot-0-base.dds",5592580ull,"DA9DB4EDE331A22F4E47BA2EAB34CFCFB2C6979F6F180007E28DFC61EA23410B"},
        {"materials/slot-0-arm.dds",5592580ull,"147D81E27AFEAA1D1AFC6E88D70D3814912A7D6235110CA4935EDF6FFB5838FE"},
        {"materials/slot-0-normal.dds",5592580ull,"F2F8EBD0A4C52C543047FB8131AD42391374313D595CD0901DDF5136C2B01354"},
        {"fallen-log-package-manifest.json",10663ull,"B8F6AEBD3986B3A674981F45A6A6BFA0D4262CCCA458818D518CA3F8140EB98B"},
        {"LICENSE.md",1998ull,"CE9483C1783A84DC63A77C36B376D23A44598F1ECFC34A7A689BDA5EBC405083"}
    };
    std::ostringstream out;out<<'[';bool first=true;
    for(const auto& pin:pins){budget.Time();const auto source=input.root/"engine/assets/mistwatch-fallen-logs"/pin.path,runtime=input.assets/"mistwatch-fallen-logs"/pin.path;NativeAncestors(source);NativeAncestors(runtime);
        const auto sourcePath=source.generic_string(),runtimePath=runtime.generic_string();
        auto row=NativePinned({sourcePath.c_str(),pin.bytes,pin.sha});const auto deployed=NativePinned({runtimePath.c_str(),pin.bytes,pin.sha});
        W::Gate(row.bytes==deployed.bytes&&row.sha256==deployed.sha256,"FORMAL_SOURCE_RUNTIME_FALLEN_ASSET_MISMATCH");row.path=source.lexically_relative(input.root).generic_string();
        if(!first)out<<',';first=false;out<<NativeIdentityJson(row);
    }out<<']';return out.str();
}
std::string NativeSourceInventory(const Input& input,W::Budget& budget){
    NativeAncestors(input.root);std::map<std::string,std::filesystem::path> files;
    const auto add=[&](const std::filesystem::path& path){NativeAncestors(path);const auto attributes=GetFileAttributesW(path.c_str());W::Gate(attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_DIRECTORY)&&std::filesystem::is_regular_file(path),"FORMAL_SOURCE_FILE_DOMAIN");
        const auto relative=path.lexically_relative(input.root).generic_string();W::Gate(!relative.empty()&&relative.find("..")==std::string::npos&&files.size()<4096&&files.emplace(relative,path).second,"FORMAL_SOURCE_MEMBER_INVALID");};
    for(const auto* directory:{"games/mistwatch/native/include","engine/native/include","engine/native/src","engine/native/shaders"}){
        const auto root=input.root/directory;NativeAncestors(root);W::Gate(std::filesystem::is_directory(root),"FORMAL_SOURCE_DIRECTORY_MISSING");
        for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){budget.Time();NativeAncestors(entry.path());if(entry.is_directory())continue;add(entry.path());}
    }
    for(const auto* relative:{"CMakeLists.txt","engine/native/CMakeLists.txt","games/mistwatch/native/CMakeLists.txt","games/mistwatch/native/src/main.cpp","games/mistwatch/tools/author_root_stem.py",
        "games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp","games/mistwatch/native/tests/fallen/MistwatchFallenSeatSupport.h","games/mistwatch/native/tests/fallen/MistwatchFallenSourceSupport.h",
        "games/mistwatch/native/tests/fallen/MistwatchQualificationRuntime.h","games/mistwatch/native/tests/qualification/MistwatchQualificationRuntimeTest.cpp","games/mistwatch/native/tests/qualification/runtime-profiles-v1.json",
        "games/mistwatch/native/tests/fallen/MistwatchAuthoredWorldQualification.h","games/mistwatch/native/tests/qualification/MistwatchAuthoredWorldQualificationTest.cpp",
        "games/mistwatch/native/tests/fallen/MistwatchAuthoredNativeExpected.h","games/mistwatch/native/tests/qualification/MistwatchAuthoredNativeExpectedTest.cpp",
        "games/mistwatch/tools/AuthoredNativeContract.ps1","games/mistwatch/native/tests/MistwatchAuthoredNativeReceiptTest.ps1",
        "engine/assets/mistwatch-fallen-logs/meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh","engine/assets/mistwatch-fallen-logs/materials/slot-0-base.dds","engine/assets/mistwatch-fallen-logs/materials/slot-0-arm.dds","engine/assets/mistwatch-fallen-logs/materials/slot-0-normal.dds","engine/assets/mistwatch-fallen-logs/fallen-log-package-manifest.json","engine/assets/mistwatch-fallen-logs/LICENSE.md"})add(input.root/relative);
    if(input.runtimeProfile){
        if(*input.runtimeProfile==Mistwatch::QualificationRuntime::Profile::AuthoredLeafClusterFullV3||input.surfaceOnly||input.currentWorld) {
            add(input.root/"games/mistwatch/native/tests/qualification/runtime-leaf-cluster-full-v3.json");
            for(const auto& row:Mistwatch::QualificationRuntime::LeafClusterEntries)add(input.root/row.source);
        }
        if(input.surfaceOnly||input.currentWorld){
            add(input.root/"games/mistwatch/tools/ForestSurfaceContract.ps1");
            add(input.root/"games/mistwatch/native/tests/qualification/runtime-forest-surface-full-v4.json");
            for(const auto& row:Mistwatch::QualificationRuntime::ForestSurfaceEntries)add(input.root/row.source);
        }
        if(input.currentWorld){
            for(const auto* relative:{"games/mistwatch/native/tests/fallen/MistwatchCurrentWorldExpected.h",
                "games/mistwatch/native/tests/qualification/MistwatchCurrentWorldExpectedTest.cpp",
                "games/mistwatch/native/tests/qualification/runtime-current-forest-full-v5.json",
                "games/mistwatch/tools/CurrentWorldContract.ps1","games/mistwatch/tools/CurrentWorldEvidence.ps1",
                "games/mistwatch/tools/run_current_native_ctest.ps1","games/mistwatch/tools/validate_current_native_capture.ps1",
                "games/mistwatch/tools/stage_current_runtime.py","games/mistwatch/native/tests/MistwatchCurrentWorldReceiptTest.ps1"})add(input.root/relative);
        }
        // 完整記錄新資產來源；蕁麻的來源 21 檔不等於部署 10 檔。
        for(const auto* directory:{"engine/assets/mistwatch-nettle-plant","engine/assets/material-packs/poly-haven/forest_leaves_04"}){
            const auto root=input.root/directory;NativeAncestors(root);W::Gate(std::filesystem::is_directory(root),"VERSIONED_SOURCE_DIRECTORY_MISSING");
            for(const auto& entry:std::filesystem::recursive_directory_iterator(root)){budget.Time();NativeAncestors(entry.path());if(!entry.is_directory())add(entry.path());}
        }
    }
    std::ostringstream out;out<<'[';bool first=true;std::uint64_t total{};
    for(const auto& [relative,path]:files){budget.Time();auto row=NativeIdentity(path);total+=row.bytes;W::Gate(total<=2147483648ull,"FORMAL_SOURCE_TOTAL_DOMAIN");row.path=relative;if(!first)out<<',';first=false;out<<NativeIdentityJson(row);}out<<']';return out.str();
}
NativeFileIdentity NativeExecutableIdentity(){
    std::array<wchar_t,32768> module{};const auto length=GetModuleFileNameW(nullptr,module.data(),static_cast<DWORD>(module.size()));W::Gate(length>0&&length<module.size(),"NATIVE_EXPECTED_BINARY_PATH");
    return NativeIdentity(std::filesystem::path(std::wstring(module.data(),length)));
}
struct NativeSnapshot {std::string sources,runtime,fallen,binary;};
NativeSnapshot NativeInputs(const Input& input,W::Budget& budget){
    return{NativeSourceInventory(input,budget),NativeRuntimeInventory(input,budget),NativeFallenSourceAssets(input,budget),NativeIdentityJson(NativeExecutableIdentity())};
}
std::string NativeProvenance(const Input& input,const NativeSnapshot& before,const NativeSnapshot& after){
    W::Gate(before.sources==after.sources&&before.fallen==after.fallen,"FORMAL_SOURCE_INPUT_CHANGED_DURING_CPU");
    W::Gate(before.runtime==after.runtime,"FORMAL_RUNTIME_INPUT_CHANGED_DURING_CPU");W::Gate(before.binary==after.binary,"FORMAL_CPU_BINARY_CHANGED");
    auto raw=NativeIdentity(input.source);raw.path=input.source.lexically_relative(input.root).generic_string();
    auto test=NativeIdentity(input.root/"games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp");test.path="games/mistwatch/native/tests/MistwatchFallenWorldQualificationTest.cpp";
    std::ostringstream out;out<<"{\"schema\":"<<NativeJsonString(input.currentWorld?"mistwatch.current-world.cpu-provenance.1":input.surfaceOnly?"mistwatch.forest-surface.cpu-provenance.1":input.authoredWorld?"mistwatch.authored-world.cpu-provenance.1":input.runtimeProfile?"mistwatch.runtime-versioned-legacy.native-provenance.1":"mistwatch.formal-fallen-native-provenance.1");
    if(input.runtimeProfile)out<<",\"world_cohort\":"<<NativeJsonString(input.currentWorld?"entrance-current-forest-surface-v1":input.surfaceOnly?"entrance-authored-surface-only-v1":input.authoredWorld?"entrance-authored-habitat-v1":"legacy-four-v1")<<",\"authored_world_qualified\":false,\"runtime_profile\":"
        <<NativeJsonString(Mistwatch::QualificationRuntime::ProfileName(*input.runtimeProfile))
        <<",\"generated_root_stem_override\":"<<NativeJsonString(input.generatedRootStem.generic_string());
    if(input.surfaceOnly)out<<",\"current_geometry_qualified\":false";
    out<<",\"source_root\":"<<NativeJsonString(input.root.generic_string())
        <<",\"source_files\":"<<after.sources<<",\"runtime_assets_root\":"<<NativeJsonString(input.assets.generic_string())<<",\"runtime_assets\":"<<after.runtime
        <<",\"fallen_source_assets\":"<<after.fallen<<",\"raw_mesh\":"<<NativeIdentityJson(raw)<<",\"test_source\":"<<NativeIdentityJson(test)<<",\"cpu_binary\":"<<after.binary
        <<",\"toolchain\":{\"msc_full_ver\":"<<_MSC_FULL_VER<<",\"cplusplus\":"<<_MSVC_LANG<<",\"configuration\":\"Release-x64-C++20-/W4-/WX-/MP1-/fp:strict-MultiThreadedDLL\",\"abi\":\"formal-AevryndEngine\"}"
        <<",\"source_input_unchanged\":true,\"runtime_input_unchanged\":true"
        <<",\"outer_receipt_scope\":\"Before/after selected source and runtime raw identities only. Root independently binds fresh compile/link inputs, exits, Job drain and JUnit; no self-claimed outer process or GPU qualification.\"}";return out.str();
}
std::string FormalReceipt(const Input& input,const NativeSnapshot& after,const std::string& proof,const std::string& expected){
    std::ostringstream out;out<<"{\"schema\":"<<NativeJsonString(input.runtimeProfile?"mistwatch.runtime-versioned-legacy.cpu-receipt.1":"mistwatch.formal-fallen.cpu-receipt.1");
    if(input.runtimeProfile)out<<",\"world_cohort\":\"legacy-four-v1\",\"authored_world_qualified\":false,\"runtime_profile\":"
        <<NativeJsonString(Mistwatch::QualificationRuntime::ProfileName(*input.runtimeProfile));
    out<<",\"status\":\"full-world-cpu-passed\",\"source_root\":"<<NativeJsonString(input.root.generic_string())
        <<",\"runtime_assets_root\":"<<NativeJsonString(input.assets.generic_string())<<",\"full_world_cpu_passed\":true,\"native_expected_author_passed\":true,\"source_input_unchanged\":true,\"runtime_input_unchanged\":true"
        <<",\"expected_file\":"<<NativeIdentityJson({"Expected.json",NativeUpper(Sha(expected)),static_cast<std::uint64_t>(expected.size())})
        <<",\"proof_file\":"<<NativeIdentityJson({"world-seat.txt",NativeUpper(Sha(proof)),static_cast<std::uint64_t>(proof.size())})
        <<",\"cpu_binary\":"<<after.binary
        <<",\"scope\":{\"native_qualified\":false,\"actual_mesh_query_qualified\":false,\"actual_Main_route_qualified\":false,\"mass_stability_qualified\":false,\"lower_envelope_distribution_qualified\":false,\"art_accepted\":false,\"game_adopted\":false,\"full49_qualified\":false,\"outer_process_qualified\":false}}\n";return out.str();
}
std::string AuthoredNativeExpectedCore(const Input& input,const Mistwatch::AuthoredQualification::Context& context,
    const Mistwatch::AuthoredQualification::Assets& assets,const Aevrynd::HeightField3D& terrain,
    std::span<const W::Placement> scan,const NativeFields& fields,const std::string& provenance,
    const Mistwatch::CurrentWorldExpected::Result* current=nullptr){
    namespace N=::Mistwatch::AuthoredNativeExpected;
    W::Gate((input.authoredWorld||input.currentWorld)&&input.runtimeProfile.has_value()&&input.currentWorld==bool(current),"AUTHORED_NATIVE_EXPLICIT_COHORT_REQUIRED");
    NativeFields poses;
    for(std::size_t i=0;i<scan.size();++i){
        const auto& p=scan[i];const Mistwatch::FallenLogPlacement placement{p.propId,p.anchor,p.pose,p.collider};
        const auto words=PlacementWords(placement);
        W::Gate(i<ExpectedPoseAndBox.size()&&words==ExpectedPoseAndBox[i]&&
            words==PlacementWords(Mistwatch::FallenLogPlacements()[i]),"AUTHORED_NATIVE_FIXED_15_WORD_PROOF");
        NativeField(poses,("prop_"+std::to_string(p.propId)).c_str(),NativeCsv(words,true));
    }
    W::Gate(poses.size()==4,"AUTHORED_NATIVE_FIXED_4_POSES");
    std::array<std::uint64_t,5> grassVertices{},grassTriangles{};std::array<std::uint64_t,4> fernTriangles{};
    for(unsigned i=0;i<5;++i){grassVertices[i]=assets.grass[i].vertices.size();grassTriangles[i]=assets.grass[i].indices.size()/3;
        W::Gate(grassVertices[i]==Mistwatch::GrassVertexCounts[i]&&grassTriangles[i]==Mistwatch::GrassTriangleCounts[i],"AUTHORED_NATIVE_GRASS_SOURCE_COUNTS");}
    for(unsigned i=0;i<4;++i){fernTriangles[i]=assets.fern[i].indices.size()/3;
        W::Gate(fernTriangles[i]==Mistwatch::FernTriangleCounts[i],"AUTHORED_NATIVE_FERN_SOURCE_COUNTS");}
    const auto initial=N::SceneOriginalFerns(context,terrain,assets,"initial"),river=N::SceneOriginalFerns(context,terrain,assets,"river-overlook");
    const auto scene=[&](const N::FernSubmission& value){return std::string("{\"plants\":")+NativeJsonString(std::to_string(value.plants))+
        ",\"triangles\":"+NativeJsonString(std::to_string(value.triangles))+",\"variants\":"+NativeJsonString(NativeCsv(value.variants))+"}";};
    const auto world=[&](unsigned state){return std::string("{\"count\":")+NativeJsonString(std::to_string(context.worlds[state].size()))+
        ",\"bytewise_fnv\":"+NativeJsonString(std::to_string(context.worldFingerprints[state]))+
        ",\"typed_fingerprint\":"+NativeJsonString(std::to_string(context.typedFingerprints[state]))+"}";};
    // 只序列化獨立 CPU 預期；GPU 提交、材質 handle 與釋放仍須本輪原生執行取得。
    std::ostringstream out;
    out<<"{\"schema\":"<<NativeJsonString(input.currentWorld?"mistwatch.current-world.native-expected.1":"mistwatch.authored-world.native-expected.1")
        <<",\"world_cohort\":"<<NativeJsonString(input.currentWorld?"entrance-current-forest-surface-v1":"entrance-authored-habitat-v1")<<",\"runtime_profile\":"
        <<NativeJsonString(Mistwatch::QualificationRuntime::ProfileName(*input.runtimeProfile))
        <<",\"world_profile\":\"forest-wood\",\"fallen_log_profile\":\"scanned-straight-v1\",\"production_full_solid_volumes\":true"
        <<",\"source\":{\"sha256\":"<<NativeJsonString(Mistwatch::FallenLogAssets::MeshSha)
        <<",\"bytes\":"<<NativeJsonString(std::to_string(Mistwatch::FallenLogMeshBytes))
        <<",\"vertices\":"<<NativeJsonString(std::to_string(Mistwatch::FallenLogVertexCount))
        <<",\"indices\":"<<NativeJsonString(std::to_string(Mistwatch::FallenLogIndexCount))
        <<",\"triangles\":"<<NativeJsonString(std::to_string(Mistwatch::FallenLogIndexCount/3))<<'}'
        <<",\"native_contract\":"<<NativeJsonMap(fields);
    if(current)out<<",\"legacy_native_contract\":"<<NativeJsonMap(current->legacy)<<",\"current_extras\":"<<NativeJsonMap(current->extras)
        <<",\"surface_contract\":"<<NativeJsonMap(current->surface)<<",\"art_profiles\":"<<NativeJsonMap(current->artProfiles)
        <<",\"suppression_ids\":{\"grass\":["<<Mistwatch::CurrentWorldExpected::Detail::Csv(current->suppressedGrass)
        <<"],\"broadleaf\":["<<Mistwatch::CurrentWorldExpected::Detail::Csv(current->suppressedBroadleaf)<<"]}";
    out<<",\"fallen_pose_words\":"<<NativeJsonMap(poses)
        <<",\"cpu_identity\":{\"rich_cover_word_fnv\":"<<NativeJsonString(std::to_string(MixPlants(context.retention.cover.plants)))
        <<",\"terrain_fingerprint\":"<<NativeJsonString(std::to_string(W::TerrainFingerprint(terrain)))
        <<",\"original_prop_fingerprint\":"<<NativeJsonString(std::to_string(W::PropFingerprint(context.source.props)))
        <<",\"source_original_nonlog_count\":"<<NativeJsonString(std::to_string(context.source.props.size()-W::CurrentLogIds.size()))
        <<",\"retained_nonlog_count\":"<<NativeJsonString(std::to_string(context.author.props.size()-W::CurrentLogIds.size()))
        <<",\"initial_world\":"<<world(0)<<",\"repaired_world\":"<<world(1)
        <<",\"grass_source_vertices\":"<<NativeJsonString(NativeCsv(grassVertices))<<",\"grass_source_triangles\":"<<NativeJsonString(NativeCsv(grassTriangles))
        <<",\"fern_source_triangles\":"<<NativeJsonString(NativeCsv(fernTriangles))
        <<",\"fixed_scene_original_ferns\":{\"initial\":"<<scene(initial)<<",\"river-overlook\":"<<scene(river)<<"}}"
        <<",\"provenance\":"<<provenance
        <<",\"scope\":{\"authored_world_CPU_passed\":true,\"native_expected_author_passed\":true,\"native_executed\":false,\"GPU_qualified\":false,\"art_accepted\":false,\"manual_play\":false,\"published\":false}}\n";
    return out.str();
}
std::string NativeExpectedCore(const Retained& retained,const Aevrynd::HeightField3D& terrain,const W::TypedWorld& mapped,std::span<const W::Placement> scan,
    const Mistwatch::ForestCoverResult& cover,const NativeFields& fields){
    std::array<std::uint64_t,4> fernTriangles{};for(unsigned i=0;i<4;++i){fernTriangles[i]=retained.fern[i].indices.size()/3;W::Gate(fernTriangles[i]==Mistwatch::FernTriangleCounts[i],"NATIVE_EXPECTED_ORIGINAL_FERN_SOURCE_COUNTS");}
    for(unsigned i=0;i<5;++i)W::Gate(nativeGeometry.grassTriangles[i]==Mistwatch::GrassTriangleCounts[i]&&nativeGeometry.grassVertices[i]==Mistwatch::GrassVertexCounts[i],"NATIVE_EXPECTED_GRASS_SOURCE_COUNTS");
    const auto initial=NativeOriginalSubmission(retained,{}),river=NativeOriginalSubmission(retained,{0,0,30});
    // 既有兩個原始420蕨 fixed-scene門，不以新倒木改寫或放寬。
    W::Gate(initial.plants==198&&initial.triangles==317224&&initial.variants==std::array<unsigned,4>{50,55,47,46},"NATIVE_EXPECTED_INITIAL_ORIGINAL_FERN_IDENTITY");
    W::Gate(river.plants==227&&river.triangles==359632&&river.variants==std::array<unsigned,4>{65,52,62,48},"NATIVE_EXPECTED_RIVER_ORIGINAL_FERN_IDENTITY");
    const auto repaired=W::Bind(retained.before[1],true,retained.props,scan);W::Gate(repaired.size()==4583,"NATIVE_EXPECTED_REPAIRED_WORLD_COUNT");
    NativeFields poses;for(std::size_t i=0;i<scan.size();++i){const auto& p=scan[i];const Mistwatch::FallenLogPlacement placement{p.propId,p.anchor,p.pose,p.collider};
        const auto words=PlacementWords(placement);W::Gate(words==ExpectedPoseAndBox[i]&&words==PlacementWords(Mistwatch::FallenLogPlacements()[i]),"NATIVE_EXPECTED_FIXED_15_WORD_PROOF");NativeField(poses,("prop_"+std::to_string(p.propId)).c_str(),NativeCsv(words,true));}
    W::Gate(poses.size()==4,"NATIVE_EXPECTED_FIXED_4_POSES");
    std::ostringstream out;out<<"{\"schema\":\"mistwatch.straight-log.native-expected.1\",\"world_profile\":\"forest-wood\",\"fallen_log_profile\":\"scanned-straight-v1\",\"production_full_solid_volumes\":true,"
        <<"\"source\":{\"sha256\":\""<<Mistwatch::FallenLogAssets::MeshSha<<"\",\"bytes\":"<<Mistwatch::FallenLogMeshBytes<<",\"vertices\":"<<Mistwatch::FallenLogVertexCount<<",\"indices\":"<<Mistwatch::FallenLogIndexCount<<",\"triangles\":"<<Mistwatch::FallenLogIndexCount/3<<"},"
        <<"\"native_contract\":"<<NativeJsonMap(fields)<<",\"fallen_pose_words\":"<<NativeJsonMap(poses)
        <<",\"cpu_identity\":{\"rich_cover_word_fnv\":"<<NativeJsonString(std::to_string(MixPlants(cover.plants)))
        <<",\"terrain_fingerprint\":"<<NativeJsonString(std::to_string(W::TerrainFingerprint(terrain)))
        <<",\"original_prop_fingerprint\":"<<NativeJsonString(std::to_string(W::PropFingerprint(retained.props)))
        <<",\"original_nonlog_count\":"<<retained.props.size()-W::CurrentLogIds.size()
        <<",\"initial_world\":{\"count\":"<<mapped.size()<<",\"bytewise_fnv\":"<<NativeJsonString(std::to_string(NativeSolidHash(W::Boxes(mapped))))<<'}'
        <<",\"repaired_world\":{\"count\":"<<repaired.size()<<",\"bytewise_fnv\":"<<NativeJsonString(std::to_string(NativeSolidHash(W::Boxes(repaired))))<<'}'
        <<",\"grass_source_vertices\":"<<NativeJsonString(NativeCsv(nativeGeometry.grassVertices))<<",\"grass_source_triangles\":"<<NativeJsonString(NativeCsv(nativeGeometry.grassTriangles))
        <<",\"fern_source_triangles\":"<<NativeJsonString(NativeCsv(fernTriangles))
        <<",\"fixed_scene_original_ferns\":{\"initial\":"<<NativeFernJson(initial)<<",\"river-overlook\":"<<NativeFernJson(river)<<'}'
        <<",\"negative_controls\":{\"wrong_world_profile\":true,\"wrong_fallen_profile\":true,\"false_mode_export\":true,\"missing_cover_plant\":true,\"single_bit_pose\":true,\"single_bit_solid\":true,\"false_producer_mode_identity_differs\":true}}";
    return out.str();
}
void ComposeNativeExpected(const Retained& retained,const Aevrynd::HeightField3D& terrain,const W::TypedWorld& mapped,
    std::span<const W::Placement> scan,const Mistwatch::TrailLitterResult& litter,const Mistwatch::ForestCoverResult& cover,
    const Mistwatch::FernColonyResult& colonies,const Mistwatch::ShrubCanopyResult& shrubs,W::Budget& budget,std::ostream& log){
    const auto fields=NativeContract(retained,mapped,litter,cover,colonies,shrubs,"forest-wood","scanned-straight-v1",true);
    NativeExpectedControls(retained,terrain,mapped,litter,cover,colonies,shrubs,fields,budget,log);
    nativeExpectedDocument=NativeExpectedCore(retained,terrain,mapped,scan,cover,fields);budget.Time();
    log<<"NATIVE_EXPECTED_PREPARED fields="<<fields.size()<<" poses=4 fullSolidVolumes=true independent_bytewise_fnv=true source_native_png_or_receipt_used=false\n";
}
void Downstream(const Retained& retained,const Aevrynd::HeightField3D& terrain,const W::TypedWorld& mapped,std::span<const W::Placement> scan,W::Budget& budget,std::ostream& log){
    std::vector<W::Box> vegetation;for(const auto& value:mapped)if(value.role!=W::Role::CanopyWood)vegetation.push_back(value.box);const auto full=W::Boxes(mapped);
    const auto litter=Mistwatch::CreateTrailLitter(terrain,full),litterRepeat=Mistwatch::CreateTrailLitter(terrain,full);W::Gate(litter.reachedBudget&&litter.patches.size()==48&&litter.fingerprint==litterRepeat.fingerprint,"MAPPED_TRAIL_LITTER_QUOTA_OR_REPEAT");budget.Time();
    const auto cover=Mistwatch::CreateForestCover(terrain,retained.firs.roots,retained.understory,vegetation,retained.grassAssets,{},retained.wood,true),coverRepeat=Mistwatch::CreateForestCover(terrain,retained.firs.roots,retained.understory,vegetation,retained.grassAssets,{},retained.wood,true);W::Gate(cover.reachedBudget&&cover.plants.size()==3600&&coverRepeat.reachedBudget&&MixPlants(cover.plants)==MixPlants(coverRepeat.plants),"MAPPED_FOREST_COVER_QUOTA_OR_REPEAT");budget.Time();
    const auto colonies=Mistwatch::CreateFernColonies(terrain,cover,retained.understory.ferns,vegetation,retained.fernAssets,{},retained.wood,true),colonyRepeat=Mistwatch::CreateFernColonies(terrain,cover,retained.understory.ferns,vegetation,retained.fernAssets,{},retained.wood,true);W::Gate(colonies.reachedBudget&&colonyRepeat.reachedBudget&&colonies.plants.size()==300&&colonies.plants.size()==colonyRepeat.plants.size(),"MAPPED_FERN_COLONY_QUOTA_OR_REPEAT");for(std::size_t id=0;id<colonies.plants.size();++id)W::Gate(W::SamePose(colonies.plants[id].transform,colonyRepeat.plants[id].transform)&&colonies.plants[id].variant==colonyRepeat.plants[id].variant&&colonies.plants[id].radius==colonyRepeat.plants[id].radius,"MAPPED_FERN_COLONY_REPEAT_CHANGED");budget.Time();
    const auto shrubs=Mistwatch::CreateShrubCanopy(retained.shrubLeaf,terrain,vegetation,retained.understory,cover,colonies,retained.fernAssets,64,retained.wood,true),shrubRepeat=Mistwatch::CreateShrubCanopy(retained.shrubLeaf,terrain,vegetation,retained.understory,cover,colonies,retained.fernAssets,64,retained.wood,true);W::Gate(shrubs.reachedBudget&&shrubRepeat.reachedBudget&&shrubs.plants.size()==64&&shrubs.fingerprint==shrubRepeat.fingerprint,"MAPPED_SHRUB_QUOTA_OR_REPEAT");budget.Time();
    FullRepeats(litter,litterRepeat,cover,coverRepeat,colonies,colonyRepeat,shrubs,shrubRepeat);budget.Time();
    // 新作者的完整植物體積也不得穿入新倒木；原420蕨已在 retained set 核過。
    std::vector<W::Box> scanBoxes;for(const auto& p:scan)scanBoxes.push_back(p.collider);
    for(const auto& plant:cover.plants){budget.Op();const auto& a=retained.grassAssets[plant.variant];W::Gate(Mistwatch::CanopyVolume::ClearsCanopyWood(plant.transform,Mistwatch::CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY),scanBoxes),"MAPPED_FULL_GRASS_VOLUME_INTERSECTS_SCAN");}
    for(const auto& plant:colonies.plants){budget.Op();const auto& a=retained.fernAssets[plant.variant];W::Gate(Mistwatch::CanopyVolume::ClearsCanopyWood(plant.transform,Mistwatch::CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.height),scanBoxes),"MAPPED_FULL_COLONY_VOLUME_INTERSECTS_SCAN");}
    for(const auto& plant:shrubs.plants){budget.Op();const auto& a=shrubs.variants[plant.variant];W::Gate(Mistwatch::CanopyVolume::ClearsCanopyWood(plant.pose,Mistwatch::CanopyVolume::ConservativeLocalVolume(a.radius,a.minimumY,a.maximumY),scanBoxes),"MAPPED_FULL_SHRUB_VOLUME_INTERSECTS_SCAN");}
    for(const auto& p:scan)W::Gate(Mistwatch::PropClearsObjectives(p.collider),"DOWNSTREAM_WEAKENED_SCAN_GUARD");
    ComposeNativeExpected(retained,terrain,mapped,scan,litter,cover,colonies,shrubs,budget,log);
    log<<"DOWNSTREAM trail_patches="<<litter.patches.size()<<" trail_fingerprint="<<litter.fingerprint<<" forest_cover="<<cover.plants.size()<<" forest_fingerprint="<<MixPlants(cover.plants)<<" colonies="<<colonies.plants.size()<<" shrubs="<<shrubs.plants.size()<<" shrub_fingerprint="<<shrubs.fingerprint<<" original_authors_and_quotas=true\n";
}
void CompleteRoute(const W::TypedWorld& before,const W::TypedWorld& after,const Aevrynd::HeightField3D& terrain,W::Budget& budget,std::ostream& log){
    using namespace Mistwatch;Session game;Encounter enemy;Aevrynd::CharacterController3D actor;actor.Reset({});auto solids=W::Boxes(before);const auto repaired=W::Boxes(after);unsigned ticks{};bool crouching{};OrbitCamera orbit;Aevrynd::ThirdPersonCameraMotion3D camera;
    const std::array roof{RangerHutCameraRoofSolid()};const auto tick=[&](W::V move,bool attack=false){budget.Op();budget.Time();W::Gate(++ticks<=120000,"MAPPED_ROUTE_TICK_LIMIT");actor.Step({move,false,crouching?.45f:1.f},solids,terrain,1.f/60.f);game.feet=actor.Position();if(game.feet.y< -5)game.Damage(100);if(game.CurrentStage()==Stage::Encounter){game.Damage(enemy.Tick(game.feet,1.f/60.f,attack));if(!enemy.health)game.FinishEncounter(enemy.feet);}const auto frame=BuildPlayerCameraFraming(game.feet,actor.Settings().height,orbit,solids,terrain,roof);const auto request=BuildPlayerCameraMotionRequest(game.feet,actor.Settings().height,orbit,camera.Initialized(),solids,terrain,roof);const auto motion=camera.Update(request,PlayerCameraSamples(game.feet,actor.Settings().height),solids,CameraCeilingSolids(),terrain,1.f/60.f,roof);(void)frame;W::Gate(motion.accepted&&Aevrynd::ThirdPersonCameraPathClear3D(motion.camera.position,motion.camera.position,solids,terrain,roof,.155f),"MAPPED_PLAYER_CAMERA_MOTION_OR_RENDER_PATH_FAILED");};
    const auto walk=[&](W::V target){for(unsigned step=0;step<1800;++step){auto delta=target-game.feet;delta.y=0;if(Aevrynd::Length(delta)<.15f)return game.Alive();tick(Aevrynd::Normalize(delta));if(!game.Alive())return false;}log<<"ROUTE_STALLED feet=";Vector(log,game.feet);log<<" target=";Vector(log,target);log<<'\n';return false;};
    for(unsigned id=0;id<Session::supplies.size();++id){const auto p=Session::supplies[id];W::Gate(walk({0,0,game.feet.z})&&walk({0,0,p.z})&&walk(p)&&game.Collect(id),"MAPPED_SUPPLY_ROUTE_FAILED");}
    for(unsigned id=0;id<Session::repairSites.size();++id){const auto p=Session::repairSites[id];W::Gate(walk({0,0,game.feet.z})&&walk({0,0,p.z}),"MAPPED_REPAIR_APPROACH_FAILED");crouching=id==0;const auto target=p+(crouching?W::V{.65f,0,0}:W::V{});W::Gate(actor.TrySetHeight(crouching?1.4f:1.8f,solids)&&walk(target)&&game.CollectRepairMaterial(id,crouching)&&walk({0,0,p.z}),"MAPPED_REPAIR_MATERIAL_ROUTE_FAILED");crouching=false;W::Gate(actor.TrySetHeight(1.8f,solids),"MAPPED_STAND_AFTER_SHELTER_FAILED");}
    W::Gate(walk({0,0,game.feet.z})&&walk(Session::bridge)&&game.RepairBridge(),"MAPPED_BRIDGE_ROUTE_FAILED");solids=repaired;Session restored;W::Gate(restored.Restore(game.Serialize())&&restored.CurrentStage()==Stage::Encounter,"MAPPED_BRIDGE_SESSION_RESTORE_FAILED");W::Gate(walk({0,0,45.5f}),"MAPPED_ENCOUNTER_APPROACH_FAILED");for(unsigned step=0;step<600&&game.CurrentStage()==Stage::Encounter;++step)tick({},true);W::Gate(game.Alive()&&game.guardianDefeated,"MAPPED_ENCOUNTER_FAILED");
    W::Gate(walk({0,0,Session::rangerLog.z})&&walk(Session::rangerLog)&&game.ReadFuseClue(),"MAPPED_RANGER_ROUTE_FAILED");Session clue;W::Gate(clue.Restore(game.Serialize())&&clue.fuseClueRead&&!clue.fuseDials,"MAPPED_CLUE_RESTORE_FAILED");W::Gate(walk({0,0,Session::rangerLog.z})&&walk({0,0,Session::fuse.z})&&walk(Session::fuse)&&!game.CollectFuse(),"MAPPED_FUSE_APPROACH_FAILED");for(unsigned wheel=0;wheel<3;++wheel)for(unsigned turn=0;turn<Session::fuseCode[wheel];++turn)W::Gate(game.CycleFuseDial(wheel),"MAPPED_FUSE_DIAL_FAILED");W::Gate(game.CollectFuse(),"MAPPED_FUSE_COLLECT_FAILED");
    W::Gate(walk({0,0,Session::fuse.z})&&walk({0,0,Session::generator.z})&&walk(Session::generator+W::V{-1.6f,0,0})&&!game.RepairGenerator()&&game.ToggleCircuit(0)&&game.ToggleCircuit(2)&&game.RepairGenerator()&&walk({0,0,Session::generator.z}),"MAPPED_GENERATOR_ROUTE_FAILED");W::Gate(game.CurrentStage()==Stage::Relays&&!game.LightBeacon(),"MAPPED_RELAY_STAGE_FAILED");
    for(unsigned id=0;id<3;++id){const auto p=Session::relaySites[id];W::Gate(walk({0,0,p.z})&&walk(p+W::V{p.x<0?1.15f:-1.15f,0,0}),"MAPPED_RELAY_APPROACH_FAILED");for(unsigned turn=0;turn<Session::RelayTargetSector(id);++turn)W::Gate(game.TuneRelay(id,1),"MAPPED_RELAY_TUNE_FAILED");
        // M1894_CURRENT_RELAY_CONTRACT_BEGIN: same order and actions as the formal RouteTest.
        budget.Op();budget.Time();
        const auto relayLinkedBefore=game.relayLinked,relaySectorsBefore=game.relaySectors;
        if(id==FieldRelayTask::WestRelay||id==FieldRelayTask::NorthWestRelay){
            const auto missingFieldSave=game.Serialize();
            W::Gate(!missingFieldSave.empty()&&game.Valid()&&game.Alive()&&
                game.CurrentStage()==Stage::Relays&&!(game.relayLinked&(1u<<id))&&
                game.NearRelay(id)&&game.RelaySector(id)==Session::RelayTargetSector(id)&&
                !game.RelayFieldTaskReady(id),"MAPPED_RELAY_MISSING_FIELD_CONTROL_PRECONDITION_FAILED");
            W::Gate(!game.ConnectRelay(id)&&game.Serialize()==missingFieldSave,
                "MAPPED_RELAY_MISSING_FIELD_CONNECT_NEGATIVE_CONTROL_ACCEPTED");
            log<<"RELAY_FIELD_CONTROL id="<<id<<" missing_field_connect_rejected=true serialized_state_unchanged=true\n";
        }
        if(id==FieldRelayTask::WestRelay){
            budget.Op();budget.Time();
            W::Gate(game.CycleRelayPatch(0)&&game.ConfirmRelayCircuit(),
                "MAPPED_RELAY_CIRCUIT_TASK_FAILED");
        }
        if(id==FieldRelayTask::NorthWestRelay){
            budget.Op();budget.Time();
            W::Gate(game.CycleExtractionRoute()&&game.ConfirmExtractionRoute(),
                "MAPPED_RELAY_EXTRACTION_TASK_FAILED");
        }
        W::Gate(game.relayLinked==relayLinkedBefore&&game.relaySectors==relaySectorsBefore&&
            game.RelayFieldTaskReady(id),"MAPPED_RELAY_FIELD_TASK_READY_OR_UNREQUESTED_LINK_FAILED");
        const auto relaySave=game.Serialize();Session relayRestored;
        W::Gate(!relaySave.empty()&&relayRestored.Restore(relaySave)&&
            relayRestored.Serialize()==relaySave&&relayRestored.RelayFieldTaskReady(id)&&
            relayRestored.CurrentStage()==Stage::Relays&&game.Serialize()==relaySave,
            "MAPPED_RELAY_FIELD_TASK_SESSION_ROUNDTRIP_FAILED");
        log<<"RELAY_FIELD_TASK id="<<id<<" patchA="<<game.relayPatchA<<" patchB="<<game.relayPatchB
            <<" tasks="<<game.relayFieldTasks<<" extraction_route="<<game.extractionRoute
            <<" ready=true serialize_restore_whole_roundtrip=true live_session_not_replaced=true\n";
        // M1894_CURRENT_RELAY_CONTRACT_END
        W::Gate(game.ConnectRelay(id)&&walk({0,0,p.z}),"MAPPED_RELAY_RETURN_FAILED");}
    W::Gate(walk(Session::beacon)&&game.LightBeacon()&&game.CurrentStage()==Stage::Complete,"MAPPED_BEACON_ROUTE_FAILED");budget.Time();log<<"COMPLETE_MAPPED_COLLISION_ROUTE ticks="<<ticks<<" health="<<game.health<<" both_bridge_states=true camera_motion_and_point155_render_path=true native_save_load=false\n";
}
void RouteCollisionControls(const W::TypedWorld& before,const W::TypedWorld& after,const Aevrynd::HeightField3D& terrain,W::Budget& budget,std::ostream& log){
    // 臨時路障僅用於負控，不進入候選 mapping／作者／任務世界。
    struct Attempt {bool reached{};unsigned ticks{};W::V feet{};std::uint64_t collisions{};};
    const auto attempt=[&](std::span<const W::Box> solids){Aevrynd::CharacterController3D actor;actor.Reset({0,terrain.SampleHeight(0,0),0});Attempt result;
        for(unsigned step=0;step<1800;++step){budget.Op();budget.Time();auto delta=W::V{0,0,6}-actor.Position();delta.y=0;if(Aevrynd::Length(delta)<.15f){result.reached=true;break;}actor.Step({Aevrynd::Normalize(delta),false,1.f},solids,terrain,1.f/60.f);++result.ticks;}
        result.feet=actor.Position();result.collisions=actor.Stats().collisions;return result;};
    for(unsigned state=0;state<2;++state){const auto& typed=state?after:before;const auto full=W::Boxes(typed);const auto clear=attempt(full);W::Gate(clear.reached,"MAPPED_ROUTE_CONTROL_ORIGINAL_PREFIX_BLOCKED");auto blocked=full;blocked.push_back({{0,0,3},{50,32,.25f}});const auto stopped=attempt(blocked);
        W::Gate(!stopped.reached&&stopped.ticks==1800&&stopped.collisions>0,"MAPPED_ROUTE_BLOCKER_NEGATIVE_CONTROL_ACCEPTED");W::Gate(blocked.size()==full.size()+1,"MAPPED_ROUTE_CONTROL_WORLD_MUTATED");for(std::size_t id=0;id<full.size();++id)W::Gate(W::SameBox(blocked[id],full[id]),"MAPPED_ROUTE_CONTROL_ORIGINAL_SOLID_CHANGED");
        log<<"ROUTE_COLLISION_CONTROL bridge_state="<<state<<" original_prefix_reached=true temporary_blocker_rejected=true prefix_ticks="<<clear.ticks<<" blocked_ticks="<<stopped.ticks<<" collision_count="<<stopped.collisions<<" stopped_feet=";Vector(log,stopped.feet);log<<" complete_quest_qualification=false\n";}
}
void SaveLoadControls(const Retained& retained,const W::TypedWorld& before,const W::TypedWorld& after,std::span<const W::Placement> scan,const Aevrynd::HeightField3D& terrain,W::Budget& budget,std::ostream& log,
    const Mistwatch::EntranceWorldAuthor* authored=nullptr){
    using namespace Mistwatch;Aevrynd::CharacterController3D controller;Explorer explorer;Boar boar;OrbitCamera orbit;controller.Reset({0,terrain.SampleHeight(0,6),6});const auto liveHash=controller.StateHash();
    const auto split=[](const W::TypedWorld& typed,bool extras){std::vector<W::Box> result;for(const auto& value:typed)if((value.role==W::Role::Middle||value.role==W::Role::CanopyWood)==extras)result.push_back(value.box);return result;};
    const auto prepare=[&](const SaveGame& snapshot,const std::vector<W::Box>& extras,const std::vector<W::Box>& mapped){return PrepareWithExpectedMapped(snapshot,{controller,explorer,boar,orbit},terrain,extras,mapped,authored);};
    for(unsigned state=0;state<2;++state){budget.Time();const auto& typed=state?after:before;const auto full=W::Boxes(typed),mapped=split(typed,false),extras=split(typed,true);SaveGame saved;saved.session.feet=controller.Position();if(state){saved.session.collected=7;saved.session.repairMaterials=7;saved.session.bridgeRepaired=true;}W::Gate(saved.Valid(),"MAPPED_SAVE_FIXTURE_INVALID");const auto serialized=saved.Serialize();
        auto prepared=prepare(saved,extras,mapped);W::Gate(static_cast<bool>(prepared),"MAPPED_SAVE_LOAD_PREPARE_FAILED");W::Gate(prepared.candidate->snapshot.Serialize()==serialized&&prepared.candidate->solids.size()==full.size()&&controller.StateHash()==liveHash,"MAPPED_SAVE_PREPARE_MUTATED_INPUT_OR_SOLIDS");for(std::size_t id=0;id<full.size();++id)W::Gate(W::SameBox(prepared.candidate->solids[id],full[id]),"MAPPED_SAVE_SOLID_ORDER_OR_REPLACEMENT_CHANGED");
        SaveGame restored;W::Gate(restored.Restore(serialized),"MAPPED_SAVE_SERIALIZE_RESTORE_FAILED");auto second=prepare(restored,extras,mapped);W::Gate(static_cast<bool>(second)&&second.candidate->snapshot.Serialize()==serialized,"MAPPED_RESTORED_PREPARE_FAILED");
        auto changed=mapped;changed.front().extents.x=std::nextafter(changed.front().extents.x,(std::numeric_limits<float>::infinity)());W::Gate(prepare(saved,extras,changed).invalidMappedWorld,"MAPPED_SAVE_NONLOG_MUTATION_ACCEPTED");
        const auto wrongState=split(state?before:after,false);W::Gate(prepare(saved,extras,wrongState).invalidMappedWorld,"MAPPED_SAVE_WRONG_BRIDGE_WORLD_ACCEPTED");const auto stale=split(retained.before[state],false);W::Gate(prepare(saved,extras,stale).invalidMappedWorld,"MAPPED_SAVE_OLD_LOG_COLLIDERS_ACCEPTED");
        auto blocked=saved;blocked.session.feet={scan.front().collider.center.x,terrain.SampleHeight(scan.front().collider.center.x,scan.front().collider.center.z),scan.front().collider.center.z};W::Gate(blocked.Valid()&&prepare(blocked,extras,mapped).error==LoadPreparationError::BodyOverlap,"MAPPED_SAVE_ACTUAL_SCAN_BODY_OVERLAP_NOT_REJECTED");
        Session liveSession;Encounter liveEncounter;Evasion liveEvasion;auto liveSolids=full;Aevrynd::ThirdPersonCameraMotion3D motion;Aevrynd::ThirdPersonFramingResult3D framing;Aevrynd::ThirdPersonCameraMotionResult3D result;Aevrynd::Camera3D rendered;const auto sessionBefore=liveSession.Serialize();const auto targets=SavedGameCommitTargets{liveSession,liveEncounter,liveEvasion,controller,explorer,boar,orbit,liveSolids,motion,framing,result,rendered};SwapPreparedSavedGame(*prepared.candidate,targets);W::Gate(liveSession.Serialize()==saved.session.Serialize()&&liveSolids.size()==full.size(),"MAPPED_SAVE_COMMIT_FAILED");SwapPreparedSavedGame(*prepared.candidate,targets);W::Gate(liveSession.Serialize()==sessionBefore&&controller.StateHash()==liveHash,"MAPPED_SAVE_WHOLE_INVERSE_FAILED");for(std::size_t id=0;id<full.size();++id)W::Gate(W::SameBox(liveSolids[id],full[id]),"MAPPED_SAVE_INVERSE_SOLIDS_CHANGED");
        log<<"SAVE_LOAD_CONTROL bridge_state="<<state<<" full_mapped_base_plus_original_extra_order=true span_nonlog_mutation_wrong_bridge_stale_log_rejected_by_adapter_only=true scan_body_rejected_by_production=true production_scanned_straight_api=true prepare_commit_double_inverse=true native_input_or_file_IO_qualified=false\n";
    }
}
void Operations(const W::Budget& budget,std::ostream& log){log<<"OPERATIONS charged_calls="<<budget.operations<<" terrain_triangle_pairs="<<budget.pairs<<" accumulated_constraints="<<budget.contacts<<" Y_evaluations="<<budget.evaluations<<" maximum_live_constraints="<<budget.maximumLiveContacts<<" maximum_live_rational_words="<<budget.maximumLiveWords<<" elapsed_ms="<<budget.Elapsed()<<'\n';}
void SurfaceOnlyCertificate(const Input& input,W::Budget& budget){
    // 新部署及地表配方獨立證書；不可把舊世界的 64 株灌木冒充目前世界。
    W::Gate(input.surfaceOnly&&input.runtimeProfile==Mistwatch::QualificationRuntime::Profile::AuthoredForestSurfaceFullV4,
        "SURFACE_ONLY_EXPLICIT_PROFILE_REQUIRED");
    const auto before=NativeInputs(input,budget);
    const auto fields=Mistwatch::AuthoredNativeExpected::BuildForestSurface(input.assets);
    const auto repeated=Mistwatch::AuthoredNativeExpected::BuildForestSurface(input.assets);
    Mistwatch::AuthoredNativeExpected::ValidateFields(fields,repeated);
    const auto after=NativeInputs(input,budget);const auto provenance=NativeProvenance(input,before,after);
    const std::string scope="{\"runtime_identity_only\":true,\"surface_recipe_CPU_passed\":true,\"current_geometry_qualified\":false,"
        "\"native_qualified\":false,\"native_executed\":false,\"GPU_qualified\":false,\"full49_qualified\":false,"
        "\"actual_Main_route_qualified\":false,\"art_accepted\":false,\"manual_play\":false,\"published\":false}";
    std::ostringstream expected;expected<<"{\"schema\":\"mistwatch.forest-surface.cpu-expected.1\","
        "\"world_cohort\":\"entrance-authored-surface-only-v1\",\"runtime_profile\":\"authored-forest-surface-full-v4\","
        "\"surface_contract\":"<<NativeJsonMap(fields)<<",\"provenance\":"<<provenance<<",\"scope\":"<<scope<<"}\n";
    std::ostringstream proof;proof<<"SURFACE_RUNTIME_CPU_PASS runtime_assets=418 fields="<<fields.size()
        <<" current_geometry_qualified=false native=false GPU=false full49=false\n";
    for(const auto& [key,value]:fields)proof<<key<<'='<<value<<'\n';
    const auto document=expected.str(),text=proof.str();std::ostringstream receipt;
    receipt<<"{\"schema\":\"mistwatch.forest-surface.cpu-receipt.1\",\"world_cohort\":\"entrance-authored-surface-only-v1\","
        "\"runtime_profile\":\"authored-forest-surface-full-v4\",\"status\":\"surface-runtime-CPU-passed\","
        "\"current_geometry_qualified\":false,\"native_expected_author_passed\":false,\"source_input_unchanged\":true,\"runtime_input_unchanged\":true,"
        "\"expected_file\":"<<NativeIdentityJson({"SurfaceExpected.json",NativeUpper(Sha(document)),static_cast<std::uint64_t>(document.size())})
        <<",\"proof_file\":"<<NativeIdentityJson({"surface-proof.txt",NativeUpper(Sha(text)),static_cast<std::uint64_t>(text.size())})
        <<",\"cpu_binary\":"<<after.binary<<",\"scope\":"<<scope<<"}\n";
    WriteNew(input.output/"SurfaceExpected.json",document);WriteNew(input.output/"surface-proof.txt",text);
    WriteNew(input.output/"receipt.json",receipt.str());std::cout<<text;
}
void CurrentWorldCertificate(const Input& input,const Mistwatch::AuthoredQualification::Context& context,
    const Mistwatch::AuthoredQualification::Assets& assets,const Aevrynd::HeightField3D& terrain,
    std::span<const W::Placement> scan,const NativeSnapshot& before,W::Budget& budget,std::ostringstream& log){
    namespace C=::Mistwatch::CurrentWorldExpected;
    W::Gate(input.currentWorld&&input.runtimeProfile==Mistwatch::QualificationRuntime::Profile::AuthoredCurrentForestFullV5,
        "CURRENT_WORLD_EXPLICIT_PROFILE_REQUIRED");
    const auto current=C::Build(context,terrain,assets,budget,input.assets),repeat=C::Build(context,terrain,assets,budget,input.assets);
    C::ValidateFields(current,repeat);
    log<<"CURRENT_WORLD_EXPECTATION_CPU_PASS fields="<<current.fields.size()<<" legacy_fields="<<current.legacy.size()
        <<" extras="<<current.extras.size()<<" surface="<<current.surface.size()<<" profiles="<<current.artProfiles.size()
        <<" shrubs=128 original_shrub_prefix=64 canopy=8 wood=8617 before=8853 after=8854 runtime=418 native=false GPU=false full49=false\n";
    Operations(budget,log);const auto after=NativeInputs(input,budget);const auto provenance=NativeProvenance(input,before,after);
    std::ostringstream world;world<<"{\"schema\":\"mistwatch.current-world.cpu-expected.1\",\"world_cohort\":\"entrance-current-forest-surface-v1\","
        "\"runtime_profile\":\"authored-current-forest-full-v5\",\"world\":{\"before_solids\":"<<context.worlds[0].size()<<",\"after_solids\":"<<context.worlds[1].size()
        <<",\"canopy\":"<<context.canopy.plants.size()<<",\"wood\":"<<context.wood.size()
        <<",\"before_fingerprint\":"<<NativeSolidHash(W::Boxes(context.worlds[0]))<<",\"after_fingerprint\":"<<NativeSolidHash(W::Boxes(context.worlds[1]))
        <<",\"author_fingerprint\":"<<context.author.fingerprint<<",\"source_fir_fingerprint\":"<<context.author.sourceFirFingerprint
        <<",\"source_middle_fingerprint\":"<<context.author.sourceMiddleFingerprint<<"}"
        <<",\"vegetation\":{\"firs\":"<<context.author.firs.firs.size()<<",\"middle\":"<<context.author.middle.trees.size()<<",\"saplings\":"<<context.author.understory.saplings.size()
        <<",\"props\":"<<context.author.props.size()<<",\"original_ferns\":"<<context.author.understory.ferns.size()<<",\"cover\":"<<context.retention.cover.plants.size()
        <<",\"retained\":"<<context.retention.retained<<",\"rejected\":"<<context.retention.rejected<<",\"filled\":"<<context.retention.filled
        <<",\"cover_fingerprint\":"<<NativeCoverHash(context.retention.cover.plants)<<",\"provenance_fingerprint\":"<<context.retention.provenanceFingerprint
        <<",\"fern_colonies\":"<<context.colonies.plants.size()<<",\"shrubs\":128,\"legacy_shrubs\":64,\"litter_patches\":"<<context.litter.patches.size()<<"}"
        <<",\"provenance\":"<<provenance<<",\"scope\":{\"authored_world_CPU_passed\":true,\"current_geometry_CPU_passed\":true,"
        "\"native_expected_author_passed\":false,\"native_executed\":false,\"GPU_qualified\":false,\"art_accepted\":false,\"manual_play\":false,\"published\":false}}\n";
    const auto document=world.str(),text=log.str();
    const auto native=AuthoredNativeExpectedCore(input,context,assets,terrain,scan,current.fields,provenance,&current);
    const auto writeReceipt=[&](bool nativeReceipt){std::ostringstream out;
        out<<"{\"schema\":"<<NativeJsonString(nativeReceipt?"mistwatch.current-world.native-expected-receipt.1":"mistwatch.current-world.cpu-receipt.1")
            <<",\"world_cohort\":\"entrance-current-forest-surface-v1\",\"runtime_profile\":\"authored-current-forest-full-v5\",\"status\":"
            <<NativeJsonString(nativeReceipt?"current-native-expectation-CPU-passed":"current-world-route-load-CPU-passed")
            <<",\"authored_world_CPU_passed\":true,\"current_geometry_CPU_passed\":true,\"native_expected_author_passed\":"<<(nativeReceipt?"true":"false")
            <<",\"source_input_unchanged\":true,\"runtime_input_unchanged\":true,\"expected_file\":"
            <<NativeIdentityJson({nativeReceipt?"NativeExpected.json":"WorldExpected.json",NativeUpper(Sha(nativeReceipt?native:document)),
                static_cast<std::uint64_t>(nativeReceipt?native.size():document.size())});
        if(nativeReceipt)out<<",\"world_expected_file\":"<<NativeIdentityJson({"WorldExpected.json",NativeUpper(Sha(document)),static_cast<std::uint64_t>(document.size())});
        out<<",\"proof_file\":"<<NativeIdentityJson({"world-seat.txt",NativeUpper(Sha(text)),static_cast<std::uint64_t>(text.size())})
            <<",\"cpu_binary\":"<<after.binary<<",\"scope\":{\"native_qualified\":false,\"actual_Main_route_qualified\":false,\"art_accepted\":false,\"full49_qualified\":false,\"outer_process_qualified\":false}}\n";
        return out.str();};
    WriteNew(input.output/"world-seat.txt",text);WriteNew(input.output/"WorldExpected.json",document);WriteNew(input.output/"receipt.json",writeReceipt(false));
    WriteNew(input.output/"NativeExpected.json",native);WriteNew(input.output/"NativeReceipt.json",writeReceipt(true));
    std::cout<<"CURRENT_WORLD_EXPECTATION_CPU_PASS fields="<<current.fields.size()<<" extras="<<current.extras.size()<<" native=false GPU=false full49=false\n";
}
}
int wmain(int argc,wchar_t** argv){std::ostringstream log;std::filesystem::path output;bool fresh{};std::unique_ptr<W::Budget> budget;
    try{const auto input=Arguments(argc,argv);output=input.output;W::Gate(std::filesystem::is_directory(output.parent_path())&&std::filesystem::create_directory(output),"FRESH_OUT_EXISTS_OR_PARENT_MISSING");fresh=true;budget=std::make_unique<W::Budget>();
        if(input.surfaceOnly){SurfaceOnlyCertificate(input,*budget);return 0;}
        const auto inputsBefore=NativeInputs(input,*budget);Handle source(CreateFileW(input.source.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));W::Gate(source.value!=INVALID_HANDLE_VALUE,"RAW_SOURCE_PIN_OPEN_FAILED");const auto raw=Read(source.value);W::Gate(raw.size()==input.bytes&&Sha(raw)==input.sha,"RAW_SOURCE_IDENTITY_CHANGED");auto loaded=Aevrynd::LoadAevryndMeshFile(input.source);W::Gate(static_cast<bool>(loaded)&&loaded.mesh.vertices.size()==input.vertices&&loaded.mesh.indices.size()==input.indices,"RAW_SOURCE_LOADER_COUNTS");MeshRawMatches(loaded.mesh,raw);log<<"SOURCE path="<<std::quoted(input.source.generic_string())<<" bytes="<<raw.size()<<" sha256="<<Sha(raw)<<" vertices="<<loaded.mesh.vertices.size()<<" indices="<<loaded.mesh.indices.size()<<'\n';
        W::Gate(Mistwatch::FallenLogAssets::ExactRawMesh(loaded.mesh,raw),"PRODUCTION_STRAIGHT_ASSET_RAW_IDENTITY_CHANGED");Mistwatch::ValidateFallenLogRawMesh(loaded.mesh);log<<"SOURCE_FIXED_PROFILE scanned-straight-v1 independent_stored_words_verified=true original_support_winner_not_reused=true\n";
        OverlayControls(*budget,log);const auto terrain=Mistwatch::CreateTerrain();const auto retained=Baseline(input,terrain,*budget,log);const auto audit=[&](const W::Placement& scan,const W::Footprint& footprint){FootprintProof(scan,footprint,loaded.mesh,input,*budget,log);};
        const auto scan=FixedStraightCandidate(loaded.mesh,terrain,retained.props,retained.firs.roots,retained.fullVolumes,*budget,audit);W::VerifyCurrentActiveRoles(scan);for(auto id:W::CurrentLogIds){auto missing=scan;missing.erase(std::find_if(missing.begin(),missing.end(),[&](const auto& p){return p.propId==id;}));const auto label="missing_active_log_"+std::to_string(id);Reject([&]{(void)W::Bind(retained.before[0],false,retained.props,missing);},log,label.c_str(),"ACTIVE_COHORT_IDENTITY_FAILED");}auto repeated=scan;repeated[1].propId=repeated[0].propId;Reject([&]{(void)W::Bind(retained.before[0],false,retained.props,repeated);},log,"duplicate_active_log_ID","ACTIVE_COHORT_IDENTITY_FAILED");auto extra=scan;extra.push_back(scan.front());Reject([&]{(void)W::Bind(retained.before[0],false,retained.props,extra);},log,"fake_fifth_active_log","ACTIVE_COHORT_IDENTITY_FAILED");const auto before=W::Bind(retained.before[0],false,retained.props,scan),after=W::Bind(retained.before[1],true,retained.props,scan);Reject([&]{(void)W::Bind(retained.before[0],true,retained.props,scan);},log,"wrong_bridge_state_mapping","WRONG_BRIDGE_STATE_TYPED_WORLD");auto duplicated=retained.before[0];const auto role=std::find_if(duplicated.begin(),duplicated.end(),[&](const W::Solid& value){return value.role==W::Role::ForestProp&&value.id==scan.front().propId;});W::Gate(role!=duplicated.end(),"CONTROL_PROP_ROLE_NOT_FOUND");duplicated.push_back(*role);Reject([&]{(void)W::Bind(duplicated,false,retained.props,scan);},log,"duplicate_typed_log_role","DUPLICATED_ACTIVE_LOG_ROLE");
        const auto visual=W::VisualProps(retained.props,scan);for(std::size_t id=0;id<visual.size();++id)if(!retained.props[id].FallenLog())W::Gate(visual[id].mesh==retained.props[id].mesh&&W::SamePose(visual[id].transform,retained.props[id].transform)&&W::SameBox(visual[id].collider,retained.props[id].collider),"ORIGINAL_ROCK_CHANGED");
        if(input.authoredWorld||input.currentWorld){
            // 真八樹世界另產生 CPU 證據；原四樹分支與全部既有路線／存檔控制保持。
            namespace A=::Mistwatch::AuthoredQualification;
            const auto assets=A::LoadAssets(input.assets,input.generatedRootStem);
            const auto original=A::CreateSourceContext(terrain,assets,*budget);
            const auto authored=A::CreateContext(terrain,assets,original,scan,*budget);
            A::RepeatControls(authored,terrain,assets,*budget);
            const auto& newBefore=authored.worlds[0];const auto& newAfter=authored.worlds[1];
            W::Gate(authored.author.applied&&newBefore.size()==8853&&newAfter.size()==8854&&authored.canopy.plants.size()==8&&authored.wood.size()==8617,"AUTHORED_COMPLETE_WORLD_DOMAIN");
            std::ostringstream worlds;
            for(unsigned state=0;state<2;++state)for(std::size_t id=0;id<authored.worlds[state].size();++id){
                const auto& value=authored.worlds[state][id];worlds<<"SOLID state="<<state<<" index="<<id<<" role="<<static_cast<unsigned>(value.role)<<" role_id="<<value.id<<' ';BoxText(worlds,value.box);worlds<<'\n';
            }
            WriteNew(output/"typed-worlds.txt",worlds.str());
            log<<"AUTHORED_TYPED_WORLDS bytes="<<worlds.str().size()<<" sha256="<<Sha(worlds.str())<<" before="<<newBefore.size()<<" after="<<newAfter.size()<<" source_ids_preserved=true\n";
            RouteCollisionControls(newBefore,newAfter,terrain,*budget,log);
            SaveLoadControls(retained,newBefore,newAfter,scan,terrain,*budget,log,&authored.author);
            CompleteRoute(newBefore,newAfter,terrain,*budget,log);
            if(input.currentWorld){MeshRawMatches(loaded.mesh,raw);CurrentWorldCertificate(input,authored,assets,terrain,scan,inputsBefore,*budget,log);return 0;}
            namespace N=::Mistwatch::AuthoredNativeExpected;
            const auto nativeFields=N::Build(authored,terrain,assets,*budget,input.assets);
            N::Validate(nativeFields,authored,terrain,assets,*budget,input.assets);
            log<<"AUTHORED_NATIVE_EXPECTATION_CPU_PASS fields="<<nativeFields.size()<<" from_full_authored_context=true native_executed=false GPU_qualified=false\n";
            MeshRawMatches(loaded.mesh,raw);budget->Time();Operations(*budget,log);
            log<<"WORLD_AUTHORED_FULL_CPU_PASS canopy=8 wood=8617 before=8853 after=8854 cover=3600 production_load_api=true native=false render_expected=false actual_Main_route=false art=false full49=false\n";
            const auto inputsAfter=NativeInputs(input,*budget);const auto provenance=NativeProvenance(input,inputsBefore,inputsAfter);
            std::ostringstream expected;
            expected<<"{\"schema\":\"mistwatch.authored-world.cpu-expected.1\",\"world_cohort\":\"entrance-authored-habitat-v1\",\"runtime_profile\":"
                <<NativeJsonString(Mistwatch::QualificationRuntime::ProfileName(*input.runtimeProfile))
                <<",\"world\":{\"before_solids\":"<<newBefore.size()<<",\"after_solids\":"<<newAfter.size()<<",\"canopy\":"<<authored.canopy.plants.size()<<",\"wood\":"<<authored.wood.size()
                <<",\"before_fingerprint\":"<<NativeSolidHash(W::Boxes(newBefore))<<",\"after_fingerprint\":"<<NativeSolidHash(W::Boxes(newAfter))
                <<",\"author_fingerprint\":"<<authored.author.fingerprint<<",\"source_fir_fingerprint\":"<<authored.author.sourceFirFingerprint<<",\"source_middle_fingerprint\":"<<authored.author.sourceMiddleFingerprint<<"}"
                <<",\"vegetation\":{\"firs\":"<<authored.author.firs.firs.size()<<",\"middle\":"<<authored.author.middle.trees.size()<<",\"saplings\":"<<authored.author.understory.saplings.size()
                <<",\"props\":"<<authored.author.props.size()<<",\"original_ferns\":"<<authored.author.understory.ferns.size()<<",\"cover\":"<<authored.retention.cover.plants.size()
                <<",\"retained\":"<<authored.retention.retained<<",\"rejected\":"<<authored.retention.rejected<<",\"filled\":"<<authored.retention.filled
                <<",\"cover_fingerprint\":"<<NativeCoverHash(authored.retention.cover.plants)<<",\"provenance_fingerprint\":"<<authored.retention.provenanceFingerprint
                <<",\"fern_colonies\":"<<authored.colonies.plants.size()<<",\"shrubs\":"<<authored.shrubs.plants.size()<<",\"litter_patches\":"<<authored.litter.patches.size()<<"}"
                <<",\"provenance\":"<<provenance
                <<",\"scope\":{\"authored_world_CPU_passed\":true,\"native_expected_author_passed\":false,\"native_executed\":false,\"GPU_qualified\":false,\"art_accepted\":false,\"manual_play\":false,\"published\":false}}\n";
            const auto text=log.str(),document=expected.str();
            std::ostringstream receipt;
            receipt<<"{\"schema\":\"mistwatch.authored-world.cpu-receipt.1\",\"world_cohort\":\"entrance-authored-habitat-v1\",\"runtime_profile\":"
                <<NativeJsonString(Mistwatch::QualificationRuntime::ProfileName(*input.runtimeProfile))
                <<",\"status\":\"authored-world-route-load-CPU-passed\",\"authored_world_CPU_passed\":true,\"native_expected_author_passed\":false,\"source_input_unchanged\":true,\"runtime_input_unchanged\":true"
                <<",\"expected_file\":"<<NativeIdentityJson({"WorldExpected.json",NativeUpper(Sha(document)),static_cast<std::uint64_t>(document.size())})
                <<",\"proof_file\":"<<NativeIdentityJson({"world-seat.txt",NativeUpper(Sha(text)),static_cast<std::uint64_t>(text.size())})<<",\"cpu_binary\":"<<inputsAfter.binary
                <<",\"scope\":{\"native_qualified\":false,\"actual_Main_route_qualified\":false,\"art_accepted\":false,\"full49_qualified\":false,\"outer_process_qualified\":false}}\n";
            const auto nativeDocument=AuthoredNativeExpectedCore(input,authored,assets,terrain,scan,nativeFields,provenance);
            std::ostringstream nativeReceipt;
            nativeReceipt<<"{\"schema\":\"mistwatch.authored-world.native-expected-receipt.1\",\"world_cohort\":\"entrance-authored-habitat-v1\",\"runtime_profile\":"
                <<NativeJsonString(Mistwatch::QualificationRuntime::ProfileName(*input.runtimeProfile))
                <<",\"status\":\"authored-native-expectation-CPU-passed\",\"native_expected_author_passed\":true,\"source_input_unchanged\":true,\"runtime_input_unchanged\":true"
                <<",\"expected_file\":"<<NativeIdentityJson({"NativeExpected.json",NativeUpper(Sha(nativeDocument)),static_cast<std::uint64_t>(nativeDocument.size())})
                <<",\"world_expected_file\":"<<NativeIdentityJson({"WorldExpected.json",NativeUpper(Sha(document)),static_cast<std::uint64_t>(document.size())})
                <<",\"proof_file\":"<<NativeIdentityJson({"world-seat.txt",NativeUpper(Sha(text)),static_cast<std::uint64_t>(text.size())})
                <<",\"cpu_binary\":"<<inputsAfter.binary
                <<",\"scope\":{\"native_qualified\":false,\"actual_Main_route_qualified\":false,\"art_accepted\":false,\"full49_qualified\":false,\"outer_process_qualified\":false}}\n";
            WriteNew(output/"world-seat.txt",text);WriteNew(output/"WorldExpected.json",document);WriteNew(output/"receipt.json",receipt.str());
            WriteNew(output/"NativeExpected.json",nativeDocument);WriteNew(output/"NativeReceipt.json",nativeReceipt.str());
            std::cout<<"AUTHORED_NATIVE_EXPECTATION_CPU_PASS fields="<<nativeFields.size()<<" native=false GPU=false art=false full49=false\n";
            std::cout<<"WORLD_AUTHORED_FULL_CPU_PASS native=false render_expected=false actual_Main=false art=false full49=false\n";return 0;
        }
        std::ostringstream worlds;for(unsigned state=0;state<2;++state){const auto& list=state?after:before;for(std::size_t id=0;id<list.size();++id){worlds<<"SOLID state="<<state<<" index="<<id<<" role="<<static_cast<unsigned>(list[id].role)<<" role_id="<<list[id].id<<' ';BoxText(worlds,list[id].box);worlds<<'\n';}}WriteNew(output/"typed-worlds.txt",worlds.str());log<<"TYPED_WORLDS bytes="<<worlds.str().size()<<" sha256="<<Sha(worlds.str())<<" false_count="<<before.size()<<" true_count="<<after.size()<<" all_current4_prop_roles_replaced=true\n";
        Downstream(retained,terrain,before,scan,*budget,log);RouteCollisionControls(before,after,terrain,*budget,log);SaveLoadControls(retained,before,after,scan,terrain,*budget,log);CompleteRoute(before,after,terrain,*budget,log);MeshRawMatches(loaded.mesh,raw);budget->Time();Operations(*budget,log);log<<"WORLD_SEAT_CPU_CANDIDATE_PASS logs=4 raw_render_mesh_unchanged=true fixed_pose_and_full_overlay_qualified=true source_winner_not_qualified=true native=false mesh_query=false art=false mass_stability=false actual_Main_route=false game_adopted=false full49=false\n";const auto text=log.str();W::Gate(!nativeExpectedDocument.empty(),"NATIVE_EXPECTED_NOT_PREPARED");
        const auto inputsAfter=NativeInputs(input,*budget);const auto provenance=NativeProvenance(input,inputsBefore,inputsAfter);
        const auto legacyExpected=nativeExpectedDocument+",\"provenance\":"+provenance+",\"scope\":{\"full_world_cpu_passed\":true,\"native_expected_author_passed\":true,\"native_executed\":false,\"gpu_resources_qualified\":false,\"art_accepted\":false,\"game_adopted\":false,\"full49\":false,\"submission_fields\":\"Independent CPU recipe expectations only; actual native resource/submission/cleanup gates remain mandatory.\"}}\n";
        // 明確標出此輪只驗舊四樹世界；不可讓新版部署被冒稱新版作者森林通過。
        const auto expected=input.runtimeProfile?
            "{\"schema\":\"mistwatch.runtime-versioned-legacy.native-expected.1\",\"world_cohort\":\"legacy-four-v1\",\"authored_world_qualified\":false,\"runtime_profile\":"+
                NativeJsonString(std::string(Mistwatch::QualificationRuntime::ProfileName(*input.runtimeProfile)))+",\"legacy_world_expected\":"+legacyExpected+"}\n":legacyExpected;
        WriteNew(output/"world-seat.txt",text);WriteNew(output/"Expected.json",expected);WriteNew(output/"receipt.json",FormalReceipt(input,inputsAfter,text,expected));
        std::cout<<"NATIVE_EXPECTED_AUTHOR_PASS expected_bytes="<<expected.size()<<" expected_sha256="<<Sha(expected)<<" native=false art=false game=false full49=false\n";
        std::cout<<"WORLD_SEAT_STRAIGHT_FULL_CPU_PASS logs=4 raw_input_unchanged=true native=false art=false actual_Main=false game=false full49=false\n";return 0;
    }catch(const std::exception& error){if(budget)Operations(*budget,log);log<<"FIRST_DIVERGENCE "<<error.what()<<" all_candidate_qualifications=false\n";if(fresh)try{WriteNew(output/"failure.txt",log.str());}catch(const std::exception& writeError){std::cerr<<"FAILURE_RECEIPT_WRITE_REJECTED "<<writeError.what()<<'\n';}std::cerr<<"FIRST_DIVERGENCE "<<error.what()<<" native=false art=false game=false full49=false\n";return 2;}
}
