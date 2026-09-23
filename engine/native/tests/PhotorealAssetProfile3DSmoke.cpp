#include "Aevrynd/PhotorealAssetProfile3D.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>

using namespace Aevrynd;

namespace {

int Fail(int code,const char* message){std::cerr<<message<<'\n';return code;}

PhotorealImportReceipt3D Receipt(const PhotorealCatalogAsset3D& asset){
    PhotorealImportReceipt3D value{};value.assetId=asset.id;value.source=asset.source;value.sha256=std::string(64,'A');value.bytes=4096;value.uncompressedBytes=8192;value.channels=PhotorealMaterialChannel3D::BaseColor|PhotorealMaterialChannel3D::NormalDx|PhotorealMaterialChannel3D::Roughness;value.passed=true;return value;
}

std::string MinimalCatalog(std::string_view license="CC0-1.0",std::string_view page="https://assets.example.com/a/stone",std::uint64_t budget=4096,std::uint64_t bytes=1024,std::string_view id="stone"){
    return std::string{"{\"schemaVersion\":1,\"downloadBudgetBytes\":"}+std::to_string(budget)+
        ",\"sources\":{\"official\":{\"license\":\""+std::string(license)+"\",\"licenseUrl\":\"https://example.com/license\"}},"
        "\"assets\":[{\"id\":\""+std::string(id)+"\",\"source\":\"official\",\"category\":\"rock\",\"kind\":\"mesh-pbr\",\"officialPage\":\""+std::string(page)+
        "\",\"preferred\":{\"resolution\":\"1k\",\"format\":\"gltf\",\"estimatedBytes\":"+std::to_string(bytes)+
        "},\"firstBatch\":true,\"lodAtlas\":\"LOD atlas trim\"}]}";
}

std::string RuntimeManifest(const PhotorealAssetCatalog3D& catalog){
    std::string assets;std::uint64_t runtimeBytes{};std::size_t count{};for(const auto& asset:catalog.Assets())if(asset.firstBatch){if(count++)assets+=',';const char* kind=asset.kind==PhotorealAssetKind3D::MeshPbr?"mesh-pbr":asset.kind==PhotorealAssetKind3D::Hdri?"hdri":"pbr-material";const std::uint64_t fileBytes=asset.kind==PhotorealAssetKind3D::MeshPbr?300:100;runtimeBytes+=fileBytes;assets+="{\"id\":\""+asset.id+"\",\"source\":\""+asset.source+"\",\"kind\":\""+kind+"\",\"license\":\"CC0-1.0\",\"commercialUse\":true,\"memoryGate\":\"pass\",\"materialChannels\":[\"base-color\",\"normal-dx\",\"roughness\"],\"files\":[";if(asset.kind==PhotorealAssetKind3D::MeshPbr)assets+="{\"path\":\"lod0.glb\",\"bytes\":100,\"sha256\":\""+std::string(64,'A')+"\"},{\"path\":\"lod1.glb\",\"bytes\":100,\"sha256\":\""+std::string(64,'A')+"\"},{\"path\":\"lod2.glb\",\"bytes\":100,\"sha256\":\""+std::string(64,'A')+"\"}";else assets+="{\"path\":\"asset.bin\",\"bytes\":100,\"sha256\":\""+std::string(64,'A')+"\"}";assets+="]";if(asset.kind==PhotorealAssetKind3D::MeshPbr)assets+=",\"lodLevels\":[{\"level\":0,\"triangles\":1000,\"bytes\":100,\"sha256\":\""+std::string(64,'A')+"\"},{\"level\":1,\"triangles\":500,\"bytes\":100,\"sha256\":\""+std::string(64,'A')+"\"},{\"level\":2,\"triangles\":100,\"bytes\":100,\"sha256\":\""+std::string(64,'A')+"\"}]";assets+='}';}return "{\"schemaVersion\":1,\"assetCount\":"+std::to_string(count)+",\"runtimeBytes\":"+std::to_string(runtimeBytes)+".0,\"memoryGate\":\"pass\",\"assets\":["+assets+"]}";
}

}

int main(int argc,char** argv){
    if(argc!=3)return Fail(1,"Expected realistic catalog and runtime manifest paths");
    const std::filesystem::path manifestPath=argv[1];
    PhotorealAssetCatalog3D catalog;PhotorealAssetError3D error;
    if(!catalog.LoadFile(manifestPath,&error))return Fail(2,"Real catalog load failed");
    const auto stats=catalog.Stats();
    if(stats.assetCount!=30||stats.firstBatchCount!=11||stats.firstBatchEstimatedBytes!=236639919||stats.downloadBudgetBytes!=1073741824||!stats.fingerprint)return Fail(3,"Real catalog statistics failed");
    if(catalog.Assets().size()!=30||!std::is_sorted(catalog.Assets().begin(),catalog.Assets().end(),[](const auto& a,const auto& b){return a.id<b.id;}))return Fail(4,"Catalog deterministic normalization failed");
    const auto* ground=catalog.Find("Ground108");const auto* cliff=catalog.Find("coastal_cliff_01");const auto* brick=catalog.Find("Bricks100");
    if(!ground||!cliff||!brick||catalog.Find("missing")||!cliff->atlasRecommended||!brick->trimSheetRecommended)return Fail(5,"Catalog metadata lookup failed");
    if(!catalog.EmbeddedPreflight()||catalog.EmbeddedPreflight()->assetId!="Ground108"||catalog.EmbeddedPreflight()->bytes!=9835610||!catalog.EmbeddedPreflight()->passed)return Fail(6,"Embedded import receipt failed");
    if(!PhotorealAssetProfile3D::ValidateMaterialChannels(*ground,*catalog.EmbeddedPreflight(),&error))return Fail(7,"Preflight PBR channel validation failed");
    auto incomplete=*catalog.EmbeddedPreflight();incomplete.channels=PhotorealMaterialChannel3D::BaseColor;
    if(PhotorealAssetProfile3D::ValidateMaterialChannels(*ground,incomplete,&error)||error.code!=PhotorealAssetErrorCode3D::MaterialChannelsIncomplete)return Fail(8,"Incomplete PBR channel gate failed");

    PhotorealRuntimeManifest3D realRuntime;
    if (!realRuntime.LoadFile(argv[2], &error)) {
      std::cerr << "Runtime parse error " << int(error.code) << ": "
                << error.message << '\n';
      return Fail(44, "Real runtime manifest parse failed");
    }
    if (realRuntime.Stats().assetCount != 12 ||
        !realRuntime.Stats().memoryGatePassed || !realRuntime.Passed())
      return Fail(44, "Real runtime manifest gate state failed");
    PhotorealAssetProfile3D realRuntimeProfile;
    if (!realRuntimeProfile.Initialize(catalog, realRuntime, {}, &error))
      return Fail(45, "Real runtime profile initialization failed");
    std::ifstream runtimeInput(argv[2], std::ios::binary);
    std::string unknownRuntimeJson((std::istreambuf_iterator<char>(runtimeInput)),
                                   std::istreambuf_iterator<char>());
    const std::string knownFirId = "\"id\": \"fir_tree_01_c\"";
    const auto knownFirPosition = unknownRuntimeJson.find(knownFirId);
    if (knownFirPosition == std::string::npos)
      return Fail(57, "Extended runtime fixture lost fir identity");
    unknownRuntimeJson.replace(knownFirPosition, knownFirId.size(),
                               "\"id\": \"unknown_fir_tree\"");
    PhotorealRuntimeManifest3D unknownRuntime;
    if (!unknownRuntime.LoadJson(unknownRuntimeJson, &error))
      return Fail(57, "Unknown runtime fixture did not parse");
    PhotorealAssetProfile3D unknownRuntimeProfile;
    if (unknownRuntimeProfile.Initialize(catalog, unknownRuntime, {}, &error) ||
        error.code != PhotorealAssetErrorCode3D::RuntimeManifestRejected)
      return Fail(57, "Unknown extended runtime asset was accepted");
    PhotorealAssetResidencyRequest3D realGroundRequest{};
    realGroundRequest.assetId = ground->id;
    realGroundRequest.receipt = *catalog.EmbeddedPreflight();
    realGroundRequest.selection = realRuntimeProfile.Select(
        *ground, {20.f, .02f, PhotorealImportance3D::Important});
    realGroundRequest.importance = PhotorealImportance3D::Important;
    realGroundRequest.cpuBytes = realGroundRequest.receipt.uncompressedBytes;
    realGroundRequest.gpuBytes = realGroundRequest.selection.estimatedGpuBytes;
    PhotorealAssetResidencyReceipt3D realLoaded;
    if (!realRuntimeProfile.Admit(realGroundRequest, realLoaded, &error) ||
        !realLoaded || realRuntimeProfile.Stats().residentAssets != 1)
      return Fail(45, "Real runtime admission failed");
    PhotorealAssetResidencyReceipt3D realUnloaded;
    if (!realRuntimeProfile.Unload(ground->id, realUnloaded, &error) ||
        !realUnloaded || realRuntimeProfile.Stats().residentAssets ||
        realRuntimeProfile.Stats().residentCpuBytes ||
        realRuntimeProfile.Stats().residentGpuBytes)
      return Fail(45, "Real runtime unload failed");
    PhotorealAssetProfile3D profile;if(!profile.Initialize(catalog,{},&error))return Fail(9,"Profile initialization failed");
    const auto far=profile.Select(*cliff,{200.f,.001f,PhotorealImportance3D::Background});
    const auto general=profile.Select(*cliff,{20.f,.02f,PhotorealImportance3D::Standard});
    const auto important=profile.Select(*cliff,{20.f,.02f,PhotorealImportance3D::Important});
    const auto hero=profile.Select(*cliff,{2.f,.8f,PhotorealImportance3D::Hero});
    if(far.textureDimension!=512||far.meshLod!=3||far.anisotropy!=4||far.triangleBudget!=8000||far.firstResidentMip!=3)return Fail(10,"Far quality selection failed");
    if(general.textureDimension!=1024||general.meshLod!=2||general.anisotropy!=8||important.textureDimension!=2048||important.meshLod!=1||hero.textureDimension!=4096||hero.meshLod!=0||hero.anisotropy!=16)return Fail(11,"Ultra quality selection failed");
    const auto heroRepeat=profile.Select(*cliff,{2.f,.8f,PhotorealImportance3D::Hero});if(heroRepeat.textureDimension!=hero.textureDimension||heroRepeat.meshLod!=hero.meshLod||heroRepeat.estimatedGpuBytes!=hero.estimatedGpuBytes)return Fail(12,"Selection determinism failed");
    const auto hostile=profile.Select(*cliff,{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),PhotorealImportance3D::Hero});if(hostile.textureDimension!=512||hostile.meshLod!=3)return Fail(13,"Non-finite observation normalization failed");
    PhotorealAssetProfileSettings3D capped{};capped.maximumTextureDimension=2048;capped.maximumAnisotropy=8;PhotorealAssetProfile3D cappedProfile;if(!cappedProfile.Initialize(catalog,capped,&error))return Fail(14,"Capped profile initialization failed");const auto cappedHero=cappedProfile.Select(*cliff,{0.f,1.f,PhotorealImportance3D::Hero});if(cappedHero.textureDimension!=2048||cappedHero.anisotropy!=8||cappedHero.firstResidentMip!=0)return Fail(15,"Profile cap failed");

    PhotorealAssetResidencyRequest3D groundRequest{};groundRequest.assetId=ground->id;groundRequest.receipt=Receipt(*ground);groundRequest.selection=profile.Select(*ground,{20.f,.02f,PhotorealImportance3D::Important});groundRequest.importance=PhotorealImportance3D::Hero;groundRequest.cpuBytes=groundRequest.receipt.uncompressedBytes;groundRequest.gpuBytes=groundRequest.selection.estimatedGpuBytes;PhotorealAssetResidencyReceipt3D loaded;if(profile.Admit(groundRequest,loaded,&error)||error.code!=PhotorealAssetErrorCode3D::RuntimeManifestRejected||profile.Stats().residentAssets)return Fail(46,"Missing runtime manifest admission gate failed");const std::string passedRuntimeJson=RuntimeManifest(catalog);PhotorealRuntimeManifest3D passedRuntime;if(!passedRuntime.LoadJson(passedRuntimeJson,&error)||!passedRuntime.Passed()||passedRuntime.Stats().assetCount!=11)return Fail(47,"Synthetic passed runtime manifest failed");const auto rejectedRuntime=[&](std::string json,int code,const char* message){PhotorealRuntimeManifest3D runtime;if(!runtime.LoadJson(json,&error)||runtime.Passed())return Fail(code,message);PhotorealAssetProfile3D candidate;if(candidate.Initialize(catalog,runtime,{},&error)||error.code!=PhotorealAssetErrorCode3D::RuntimeManifestRejected||candidate.Stats().residentAssets)return Fail(code,message);return 0;};
    const std::string passGate="\"memoryGate\":\"pass\"",cc0License="\"license\":\"CC0-1.0\"",channelReceipt="\"materialChannels\":[\"base-color\",\"normal-dx\",\"roughness\"]";std::string badRuntime=passedRuntimeJson;badRuntime.replace(badRuntime.find(passGate),passGate.size(),"\"memoryGate\":\"fail\"");if(const int result=rejectedRuntime(badRuntime,49,"Global runtime memory gate failed");result)return result;
    badRuntime=passedRuntimeJson;const std::size_t assetsStart=badRuntime.find("\"assets\":[");const std::size_t assetGate=badRuntime.find(passGate,assetsStart);badRuntime.replace(assetGate,passGate.size(),"\"memoryGate\":\"fail\"");if(const int result=rejectedRuntime(badRuntime,50,"Per-asset runtime memory gate failed");result)return result;
    badRuntime=passedRuntimeJson;badRuntime.replace(badRuntime.find(cc0License),cc0License.size(),"\"license\":\"MIT\"");if(const int result=rejectedRuntime(badRuntime,51,"Runtime license gate failed");result)return result;
    badRuntime=passedRuntimeJson;badRuntime.replace(badRuntime.find(std::string(64,'A')),64,"BAD");if(const int result=rejectedRuntime(badRuntime,52,"Runtime hash receipt gate failed");result)return result;
    for(const std::string_view unsafePath:{"../../escape.glb","C:/absolute.glb","texture.jpg:payload.exe","/rooted.glb","safe/../escape.glb"}){badRuntime=passedRuntimeJson;const std::string original="\"path\":\"asset.bin\"";badRuntime.replace(badRuntime.find(original),original.size(),"\"path\":\""+std::string(unsafePath)+"\"");if(const int result=rejectedRuntime(badRuntime,55,"Runtime path safety gate failed");result)return result;}
    badRuntime=passedRuntimeJson;badRuntime.replace(badRuntime.find(channelReceipt),channelReceipt.size(),"\"materialChannels\":[]");if(const int result=rejectedRuntime(badRuntime,53,"Runtime channel receipt gate failed");result)return result;
    badRuntime=passedRuntimeJson;const std::string triangleReceipt="\"triangles\":500";const std::size_t lodTriangle=badRuntime.find(triangleReceipt);badRuntime.replace(lodTriangle,triangleReceipt.size(),"\"triangles\":1000");if(const int result=rejectedRuntime(badRuntime,54,"Runtime LOD receipt gate failed");result)return result;
    if(!profile.Initialize(catalog,passedRuntime,{},&error))return Fail(48,"Bound runtime profile initialization failed");
    if(!profile.Admit(groundRequest,loaded,&error)||!loaded||!loaded.loaded||loaded.unloaded||!profile.IsResident(ground->id)||profile.Stats().residentAssets!=1)return Fail(16,"Ground admission failed");
    PhotorealAssetResidencyReceipt3D rejectedReceipt;if(profile.Admit(groundRequest,rejectedReceipt,&error)||error.code!=PhotorealAssetErrorCode3D::AlreadyResident||rejectedReceipt)return Fail(17,"Duplicate residency gate failed");
    auto mismatch=groundRequest;mismatch.assetId=cliff->id;if(profile.Admit(mismatch,rejectedReceipt,&error)||error.code!=PhotorealAssetErrorCode3D::ReceiptMismatch)return Fail(18,"Receipt identity gate failed");
    auto badChannels=groundRequest;badChannels.assetId=cliff->id;badChannels.receipt=Receipt(*cliff);badChannels.receipt.channels=PhotorealMaterialChannel3D::BaseColor;if(profile.Admit(badChannels,rejectedReceipt,&error)||error.code!=PhotorealAssetErrorCode3D::MaterialChannelsIncomplete)return Fail(19,"Admission PBR channel gate failed");

    auto cliffRequest=badChannels;cliffRequest.receipt=Receipt(*cliff);cliffRequest.importance=PhotorealImportance3D::Standard;cliffRequest.cpuBytes=2000000;cliffRequest.gpuBytes=5000000;cliffRequest.selection=general;if(!profile.Admit(cliffRequest,loaded,&error))return Fail(20,"Cliff admission failed");
    PhotorealAssetResidencyRequest3D brickRequest{};brickRequest.assetId=brick->id;brickRequest.receipt=Receipt(*brick);brickRequest.selection=profile.Select(*brick,{200.f,.001f,PhotorealImportance3D::Background});brickRequest.importance=PhotorealImportance3D::Background;brickRequest.cpuBytes=1000000;brickRequest.gpuBytes=2000000;if(!profile.Admit(brickRequest,loaded,&error))return Fail(21,"Brick admission failed");
    if(!profile.Touch(cliff->id,100)||profile.Touch("missing",100))return Fail(22,"Residency touch failed");
    const std::string_view protectedIds[]{ground->id};const auto trimmed=profile.Trim(groundRequest.cpuBytes,groundRequest.gpuBytes,protectedIds);
    if(trimmed.unloadedAssetIds.size()!=2||trimmed.unloadedAssetIds[0]!=brick->id||trimmed.unloadedAssetIds[1]!=cliff->id||trimmed.releasedCpuBytes!=3000000||trimmed.releasedGpuBytes!=7000000||!profile.IsResident(ground->id)||profile.Stats().residentAssets!=1)return Fail(23,"Deterministic trim failed");
    PhotorealAssetResidencyReceipt3D unloaded;if(!profile.Unload(ground->id,unloaded,&error)||!unloaded||!unloaded.unloaded||profile.Stats().residentAssets||profile.Stats().residentCpuBytes||profile.Stats().residentGpuBytes)return Fail(24,"Explicit unload failed");
    if(profile.Unload(ground->id,unloaded,&error)||error.code!=PhotorealAssetErrorCode3D::AssetNotFound)return Fail(25,"Missing unload gate failed");auto invalidSelection=groundRequest;invalidSelection.selection.meshLod=1;if(profile.Admit(invalidSelection,loaded,&error)||error.code!=PhotorealAssetErrorCode3D::InvalidSelection)return Fail(42,"Asset-kind selection gate failed");

    PhotorealAssetProfileSettings3D tiny{};tiny.cpuBudgetBytes=1000;tiny.gpuBudgetBytes=1000;PhotorealAssetProfile3D tinyProfile;if(!tinyProfile.Initialize(catalog,passedRuntime,tiny,&error))return Fail(26,"Tiny profile initialization failed");auto oversized=groundRequest;oversized.cpuBytes=1001;oversized.gpuBytes=1;if(tinyProfile.Admit(oversized,loaded,&error)||error.code!=PhotorealAssetErrorCode3D::CpuBudgetExceeded||tinyProfile.Stats().residentAssets)return Fail(27,"Atomic CPU budget gate failed");oversized.cpuBytes=1;oversized.gpuBytes=1001;if(tinyProfile.Admit(oversized,loaded,&error)||error.code!=PhotorealAssetErrorCode3D::GpuBudgetExceeded||tinyProfile.Stats().residentAssets)return Fail(28,"Atomic GPU budget gate failed");

    PhotorealAssetCatalog3D fixture;if(!fixture.LoadJson(MinimalCatalog(),&error)||fixture.Stats().assetCount!=1||!fixture.Find("stone")||!fixture.Find("stone")->atlasRecommended||!fixture.Find("stone")->trimSheetRecommended)return Fail(29,"Minimal catalog failed");const auto fingerprint=fixture.Stats().fingerprint;
    if(fixture.LoadJson(MinimalCatalog("MIT"),&error)||error.code!=PhotorealAssetErrorCode3D::LicenseRejected||fixture.Stats().fingerprint!=fingerprint)return Fail(30,"License rejection atomicity failed");
    if(fixture.LoadJson(MinimalCatalog("CC0-1.0","https://evil.test/a/stone"),&error)||error.code!=PhotorealAssetErrorCode3D::LicenseRejected)return Fail(31,"Official host binding failed");
    if(fixture.LoadJson(MinimalCatalog("CC0-1.0","https://assets.example.com/a/stone",100,101),&error)||error.code!=PhotorealAssetErrorCode3D::InvalidCatalog)return Fail(32,"First batch budget gate failed");
    std::string duplicate=MinimalCatalog();duplicate.insert(1,"\"schemaVersion\":1,");if(fixture.LoadJson(duplicate,&error)||error.code!=PhotorealAssetErrorCode3D::InvalidJson)return Fail(33,"Duplicate JSON key gate failed");
    std::string unsupported=MinimalCatalog();unsupported[unsupported.find("\"schemaVersion\":1")+16]='2';if(fixture.LoadJson(unsupported,&error)||error.code!=PhotorealAssetErrorCode3D::InvalidSchema)return Fail(34,"Schema gate failed");
    PhotorealCatalogLimits3D narrow{};narrow.maximumJsonBytes=16;if(fixture.LoadJson(MinimalCatalog(),&error,narrow)||error.code!=PhotorealAssetErrorCode3D::InputLimitExceeded)return Fail(35,"Catalog byte limit failed");PhotorealCatalogLimits3D excessive{};excessive.maximumJsonBytes=17u*1024u*1024u;if(fixture.LoadJson(MinimalCatalog(),&error,excessive)||error.code!=PhotorealAssetErrorCode3D::InputLimitExceeded)return Fail(43,"Catalog hard cap failed");
    std::string duplicateAsset=MinimalCatalog();const std::size_t arrayStart=duplicateAsset.find("\"assets\":[")+10;const std::size_t arrayEnd=duplicateAsset.rfind("]}");const std::string assetObject=duplicateAsset.substr(arrayStart,arrayEnd-arrayStart);duplicateAsset.insert(arrayEnd,","+assetObject);if(fixture.LoadJson(duplicateAsset,&error)||error.code!=PhotorealAssetErrorCode3D::DuplicateAsset)return Fail(36,"Duplicate asset gate failed");
    const std::string overflow="{\"schemaVersion\":18446744073709551616}";if(fixture.LoadJson(overflow,&error)||error.code!=PhotorealAssetErrorCode3D::InvalidJson)return Fail(37,"Integer overflow gate failed");
    std::string deep(70,'[');deep+=std::string(70,']');if(fixture.LoadJson(deep,&error)||error.code!=PhotorealAssetErrorCode3D::InputLimitExceeded)return Fail(38,"JSON depth gate failed");
    std::string invalidUtf8=MinimalCatalog();const std::size_t hint=invalidUtf8.find("LOD atlas trim");invalidUtf8.replace(hint,3,std::string{"\xc0\xaf",2});if(fixture.LoadJson(invalidUtf8,&error)||error.code!=PhotorealAssetErrorCode3D::InvalidJson)return Fail(56,"Invalid UTF-8 gate failed");

    const std::string receiptJson="{\"assetId\":\"Ground108\",\"source\":\"ambientcg\",\"sha256\":\""+std::string(64,'B')+"\",\"bytes\":10,\"uncompressedBytes\":20,\"maps\":[\"base-color\",\"normal-dx\",\"roughness\"],\"result\":\"pass\"}";PhotorealImportReceipt3D parsedReceipt;if(!PhotorealAssetCatalog3D::LoadReceiptJson(receiptJson,parsedReceipt,&error)||parsedReceipt.assetId!="Ground108"||!parsedReceipt.passed)return Fail(39,"Standalone receipt load failed");const auto sentinel=parsedReceipt;std::string badReceipt=receiptJson;badReceipt.replace(badReceipt.find(std::string(64,'B')),64,"BAD");if(PhotorealAssetCatalog3D::LoadReceiptJson(badReceipt,parsedReceipt,&error)||error.code!=PhotorealAssetErrorCode3D::InvalidReceipt||parsedReceipt.sha256!=sentinel.sha256)return Fail(40,"Receipt hash atomic gate failed");if(PhotorealAssetCatalog3D::LoadReceiptJson(receiptJson,parsedReceipt,&error,8)||error.code!=PhotorealAssetErrorCode3D::InputLimitExceeded)return Fail(41,"Receipt byte limit failed");

    std::cout<<"Photoreal asset profile smoke passed: assets="<<stats.assetCount<<" firstBatchBytes="<<stats.firstBatchEstimatedBytes<<" fingerprint="<<stats.fingerprint<<"\n";
    return 0;
}
