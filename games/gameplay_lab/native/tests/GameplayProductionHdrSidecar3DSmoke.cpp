#include "GameplayProductionHdrSidecar3D.h"

#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace Aevrynd;
using namespace Aevrynd::GameplayLab;

namespace {
GameplayProductionHash2563D HashText(std::string_view value) { return HashGameplayProductionBytes3D(std::as_bytes(std::span{value.data(),value.size()})); }
GameplayProductionHash2563D HashU32(std::span<const std::uint32_t> values) { std::vector<std::byte> bytes; for(const auto value:values)for(unsigned i=0;i<4;++i)bytes.push_back(std::byte((value>>(i*8))&0xffu)); return HashGameplayProductionBytes3D(bytes); }
GameplayProductionHash2563D HashFloat(std::span<const float> values) { std::vector<std::byte> bytes; for(const auto value:values){const auto bits=std::bit_cast<std::uint32_t>(value==0.f?0.f:value);for(unsigned i=0;i<4;++i)bytes.push_back(std::byte((bits>>(i*8))&0xffu));} return HashGameplayProductionBytes3D(bytes); }

GameplayProductionHdrSidecar3D MakeStructuralFixture() {
    GameplayProductionHdrSidecar3D value{};
    value.source=GameplayProductionHdrSource3D::GameplayProductionHdrReadback3D;value.provider=GameplayProductionHdrProvider3D::D3D12ProductionLinearHdrReadback3D;value.rawByteOrder=GameplayProductionByteOrder3D::LittleEndian;value.providerFingerprint=0x4145564844520001ull;value.hooksOff=true;
    value.request={11,101,71,17,19,23,1};value.raster={101,71,17,19,102,103,104,1,2,2,256,3,1,.25f,-.25f,GameplayProductionHdrFormat3D::Rgba16Float};value.resource={601,602,603,32,2,2,16,256,GameplayProductionHdrFormat3D::Rgba16Float,true,true};
    value.rawRgba16f={0x3c00,0x3800,0x3400,0x3c00,0x4000,0x3c00,0x3800,0x3c00,0x3a00,0x3c00,0x4000,0x3c00,0x3800,0x3a00,0x3c00,0x4000};std::vector<std::byte> raw;for(const auto component:value.rawRgba16f){raw.push_back(std::byte(component&0xffu));raw.push_back(std::byte(component>>8));}value.rawRgba16fHash=HashGameplayProductionBytes3D(raw);value.finiteStats=MeasureGameplayProductionRgba16f3D(value.rawRgba16f);
    const std::array<std::uint32_t,4> objectPixels{1,1,2,0};const std::array<float,4> depthPixels{.25f,.5f,.75f,1.f};value.objectId={101,71,4,3,2,501,502,503,504,1,2,2,HashU32(objectPixels),true};value.depth={101,71,4,3,1,2,2,HashFloat(depthPixels),true};
    value.rasterEvidence={101,102,103,104,3,1,{.25f,-.25f}};value.determinismEvidence.frameSerial=101;value.determinismEvidence.integrity={DeterminismIntegrityMode3D::SealedCapture,DeterminismIntegrityMode3D::SealedCapture,DeterminismIntegrityRejectionReason3D::None,2,2,0,true};value.determinismEvidence.canonical={1,201,202,203,204,205,206,207};value.determinismEvidence.render={101,301,302,2,0,1,12,18};value.renderCanonical={302,2,0,1,12,18};
    const RenderObjectIdentity3D first{{0x1001,0x1002},7},second{{0x2001,0x2002},8};value.materialBindings={{first,701,801,0,2},{second,702,802,1,1}};value.artifactHashes={{GameplayProductionArtifactKind3D::RawHdr,0x7261772d686472ull,32,value.rawRgba16fHash},{GameplayProductionArtifactKind3D::ObjectId,0x6f626a6563742d69ull,16,value.objectId.pixelHash},{GameplayProductionArtifactKind3D::Depth,0x64657074682d7261ull,16,value.depth.pixelHash}};
    value.canonical.stableContentHash=ComputeGameplayProductionHdrStableContentHash3D(value);value.canonical.payloadHash=ComputeGameplayProductionHdrPayloadHash3D(value);value.canonical.rootHash=ComputeGameplayProductionHdrRootHash3D(value);return value;
}
}

int main() {
    constexpr std::array<std::uint8_t,32> abcSha{0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};if(HashText("abc")!=abcSha)return 1;
    static_assert(!std::is_copy_constructible_v<ProductionReadbackBundle3D>);static_assert(!std::is_constructible_v<ProductionReadbackBundle3D,LinearHdrReadback3D,ObjectIdCapture3D,DepthCapture3D,ReadbackCaptureSequenceDescriptor3D>);static_assert(!std::is_constructible_v<GameplayProductionHdrSidecarBuildResult3D,GameplayProductionHdrSidecar3D,std::vector<std::byte>>);static_assert(!std::is_invocable_v<decltype(&PublishGameplayProductionHdrSidecar3D),const GameplayProductionHdrSidecar3D&,const std::filesystem::path&,GameplayProductionHdrSidecarPublishReceipt3D*>);
    const auto fixture=MakeStructuralFixture();if(!ValidateGameplayProductionHdrSidecar3D(fixture))return 2;const auto reject=[&](auto mutation,GameplayProductionHdrSidecarError3D expected){auto hostile=fixture;mutation(hostile);const auto result=ValidateGameplayProductionHdrSidecar3D(hostile);return !result&&result.error==expected;};
    if(!reject([](auto& v){v.rawRgba16f[0]^=1;},GameplayProductionHdrSidecarError3D::FiniteStats)||!reject([](auto& v){++v.objectId.acceptedDrawHashExcludingOpacity;},GameplayProductionHdrSidecarError3D::StableContentHash)||!reject([](auto& v){++v.renderCanonical.worldRevision;},GameplayProductionHdrSidecarError3D::Determinism)||!reject([](auto& v){++v.materialBindings[0].partCount;},GameplayProductionHdrSidecarError3D::StableContentHash)||!reject([](auto& v){v.materialBindings[0].bindingHash=0;},GameplayProductionHdrSidecarError3D::Material)||!reject([](auto& v){v.materialBindings[1].order=0;},GameplayProductionHdrSidecarError3D::MaterialOrder))return 3;
    // 中文：執行身分只影響完整 payload/root，不能改變跨執行的穩定內容雜湊。
    auto volatileOnly=fixture;const auto stable=volatileOnly.canonical.stableContentHash;volatileOnly.request.frameSerial+=1000;volatileOnly.raster.frameSerial+=1000;volatileOnly.objectId.frameSerial+=1000;volatileOnly.depth.frameSerial+=1000;volatileOnly.rasterEvidence.frameSerial+=1000;volatileOnly.determinismEvidence.frameSerial+=1000;volatileOnly.determinismEvidence.render.frameSerial+=1000;volatileOnly.request.sequenceToken+=100;volatileOnly.raster.sequenceToken+=100;volatileOnly.objectId.sequenceToken+=100;volatileOnly.depth.sequenceToken+=100;volatileOnly.raster.tokenGeneration+=50;volatileOnly.rasterEvidence.tokenGeneration+=50;volatileOnly.raster.historyReadGeneration+=50;volatileOnly.rasterEvidence.temporalHistoryReadGeneration+=50;++volatileOnly.resource.resourceId;++volatileOnly.resource.generation;++volatileOnly.resource.submissionFence;if(ComputeGameplayProductionHdrStableContentHash3D(volatileOnly)!=stable)return 4;volatileOnly.canonical.payloadHash=ComputeGameplayProductionHdrPayloadHash3D(volatileOnly);volatileOnly.canonical.rootHash=ComputeGameplayProductionHdrRootHash3D(volatileOnly);if(!ValidateGameplayProductionHdrSidecar3D(volatileOnly))return 5;
    // 中文：smoke 不得合成 production bundle 或發布；只驗 malformed file 必須 fail closed。
    const auto malformed=std::filesystem::temp_directory_path()/"aevrynd-malformed-production-hdr.aevhdr";{std::ofstream output(malformed,std::ios::binary|std::ios::trunc);output<<"not-a-production-sidecar";}if(InspectGameplayProductionHdrSidecarFile3D(malformed))return 6;std::error_code ignored;std::filesystem::remove(malformed,ignored);
    std::cout<<"gameplay production HDR structural_validator=1 opaque_bundle=1 synthetic_publish=0 hostile_rejections=6\n";return 0;
}
