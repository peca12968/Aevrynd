#pragma once

#include <Aevrynd/Renderer.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace Aevrynd::GameplayLab {

using GameplayProductionHash2563D = std::array<std::uint8_t,32>;

enum class GameplayProductionHdrSource3D : std::uint8_t {
    Unspecified,
    GameplayProductionHdrReadback3D,
    TestAccess,
    OfflinePng
};

enum class GameplayProductionHdrFormat3D : std::uint8_t { Rgba16Float };
enum class GameplayProductionArtifactKind3D : std::uint8_t { RawHdr, ObjectId, Depth };
enum class GameplayProductionHdrProvider3D : std::uint8_t { Unspecified, D3D12ProductionLinearHdrReadback3D };
enum class GameplayProductionByteOrder3D : std::uint8_t { Unspecified, LittleEndian };

struct GameplayProductionHdrRequest3D {
    std::uint64_t requestId{},frameSerial{},sequenceToken{},cameraStateHash{},gameplayStateHash{},sequenceInvariantHash{};
    std::uint32_t sequenceIndex{};
};

struct GameplayProductionHdrRaster3D {
    std::uint64_t frameSerial{},sequenceToken{},cameraStateHash{},gameplayStateHash{},viewProjectionHash{},tokenGeneration{},historyReadGeneration{};
    std::uint32_t sequenceIndex{},width{},height{},rowPitchBytes{},jitterIndex{},temporalHistoryReadIndex{};
    float jitterPixelsX{},jitterPixelsY{};
    GameplayProductionHdrFormat3D format{GameplayProductionHdrFormat3D::Rgba16Float};
};

struct GameplayProductionHdrResource3D {
    std::uint64_t resourceId{},generation{},submissionFence{},byteCount{};
    std::uint32_t width{},height{},tightRowBytes{},rowPitchBytes{};
    GameplayProductionHdrFormat3D format{GameplayProductionHdrFormat3D::Rgba16Float};
    bool copySource{},readbackComplete{};
};

struct GameplayProductionHdrFiniteStats3D {
    std::uint64_t finiteComponentCount{},nanComponentCount{},infinityComponentCount{};
    double minimum{},maximum{},mean{};
};

struct GameplayProductionObjectIdEvidence3D {
    std::uint64_t frameSerial{},sequenceToken{},pixelCount{},semanticPixelCount{},tokenCount{},acceptedDrawHashExcludingOpacity{},acceptedDrawHashIncludingOpacity{},tokenMapHash{},identityOrderHash{};
    std::uint32_t sequenceIndex{},width{},height{};
    GameplayProductionHash2563D pixelHash{};
    bool complete{};
};

struct GameplayProductionRenderCanonical3D {
    std::uint64_t worldRevision{},submittedMeshInstances{},submittedSkinnedInstances{},submittedTerrainInstances{},submittedWorldVertices{},submittedWorldIndices{};
};

struct GameplayProductionDepthEvidence3D {
    std::uint64_t frameSerial{},sequenceToken{},pixelCount{},foregroundPixelCount{};
    std::uint32_t sequenceIndex{},width{},height{};
    GameplayProductionHash2563D pixelHash{};
    bool complete{};
};

struct GameplayProductionMaterialBinding3D {
    RenderObjectIdentity3D identity{};
    std::uint64_t materialFingerprint{},bindingHash{};
    std::uint32_t order{},partCount{};
};

struct GameplayProductionArtifactHash3D {
    GameplayProductionArtifactKind3D kind{GameplayProductionArtifactKind3D::RawHdr};
    std::uint64_t canonicalNameHash{},byteCount{};
    GameplayProductionHash2563D hash{};
};

struct GameplayProductionCanonicalPayload3D {
    std::uint32_t schemaVersion{3};
    GameplayProductionHash2563D stableContentHash{},payloadHash{},rootHash{};
};

struct GameplayProductionHdrSidecar3D {
    std::uint32_t schemaVersion{3};
    GameplayProductionHdrSource3D source{GameplayProductionHdrSource3D::Unspecified};
    GameplayProductionHdrProvider3D provider{GameplayProductionHdrProvider3D::Unspecified};
    GameplayProductionByteOrder3D rawByteOrder{GameplayProductionByteOrder3D::Unspecified};
    std::uint64_t providerFingerprint{};
    bool hooksOff{};
    GameplayProductionHdrRequest3D request{};
    GameplayProductionHdrRaster3D raster{};
    GameplayProductionHdrResource3D resource{};
    std::vector<std::uint16_t> rawRgba16f;
    GameplayProductionHash2563D rawRgba16fHash{};
    GameplayProductionHdrFiniteStats3D finiteStats{};
    GameplayProductionObjectIdEvidence3D objectId{};
    GameplayProductionDepthEvidence3D depth{};
    GameplayProductionRenderCanonical3D renderCanonical{};
    ReadbackRasterProvenance3D rasterEvidence{};
    FrameBoundDeterminismEvidence3D determinismEvidence{};
    std::vector<GameplayProductionMaterialBinding3D> materialBindings;
    std::vector<GameplayProductionArtifactHash3D> artifactHashes;
    GameplayProductionCanonicalPayload3D canonical{};
};

enum class GameplayProductionHdrSidecarError3D : std::uint8_t {
    None,
    Schema,
    Source,
    Provider,
    ByteOrder,
    Hooks,
    Request,
    Raster,
    Resource,
    Format,
    ByteCount,
    NonFinite,
    FiniteStats,
    RawHash,
    Frame,
    Sequence,
    Camera,
    State,
    ViewProjection,
    History,
    Jitter,
    ObjectId,
    Depth,
    Determinism,
    Material,
    MaterialOrder,
    Artifact,
    CanonicalPayload,
    StableContentHash,
    RootHash
};

struct GameplayProductionHdrSidecarValidation3D {
    GameplayProductionHdrSidecarError3D error{GameplayProductionHdrSidecarError3D::None};
    const char* field{""};
    [[nodiscard]] constexpr explicit operator bool() const noexcept { return error==GameplayProductionHdrSidecarError3D::None; }
};

[[nodiscard]] GameplayProductionHash2563D HashGameplayProductionBytes3D(std::span<const std::byte> bytes);
[[nodiscard]] GameplayProductionHdrFiniteStats3D MeasureGameplayProductionRgba16f3D(std::span<const std::uint16_t> components);
[[nodiscard]] GameplayProductionHash2563D ComputeGameplayProductionHdrStableContentHash3D(const GameplayProductionHdrSidecar3D& sidecar);
[[nodiscard]] GameplayProductionHash2563D ComputeGameplayProductionHdrPayloadHash3D(const GameplayProductionHdrSidecar3D& sidecar);
[[nodiscard]] GameplayProductionHash2563D ComputeGameplayProductionHdrRootHash3D(const GameplayProductionHdrSidecar3D& sidecar);
[[nodiscard]] GameplayProductionHdrSidecarValidation3D ValidateGameplayProductionHdrSidecar3D(const GameplayProductionHdrSidecar3D& sidecar);

class GameplayProductionHdrSidecarBuildResult3D {
public:
    GameplayProductionHdrSidecarBuildResult3D(const GameplayProductionHdrSidecarBuildResult3D&)=default;
    GameplayProductionHdrSidecarBuildResult3D(GameplayProductionHdrSidecarBuildResult3D&&)noexcept=default;
    GameplayProductionHdrSidecarBuildResult3D& operator=(const GameplayProductionHdrSidecarBuildResult3D&)=default;
    GameplayProductionHdrSidecarBuildResult3D& operator=(GameplayProductionHdrSidecarBuildResult3D&&)noexcept=default;
    [[nodiscard]] const GameplayProductionHdrSidecar3D& Sidecar()const noexcept{return sidecar_;}
    [[nodiscard]] std::span<const std::byte> SerializedBytes()const noexcept{return serializedBytes_;}
private:
    GameplayProductionHdrSidecarBuildResult3D(GameplayProductionHdrSidecar3D sidecar,std::vector<std::byte> serializedBytes):sidecar_(std::move(sidecar)),serializedBytes_(std::move(serializedBytes)){}
    GameplayProductionHdrSidecar3D sidecar_;
    std::vector<std::byte> serializedBytes_;
    friend std::optional<GameplayProductionHdrSidecarBuildResult3D> BuildGameplayProductionHdrSidecar3D(const ProductionReadbackBundle3D&,GameplayProductionHdrSidecarValidation3D*);
};

struct GameplayProductionHdrSidecarPublishReceipt3D {
    std::uint64_t byteCount{};
    GameplayProductionHash2563D fileHash{},stableContentHash{},payloadHash{},rootHash{};
};

struct GameplayProductionHdrSidecarInspectReceipt3D {
    std::uint32_t schemaVersion{};
    std::uint64_t byteCount{};
    GameplayProductionHash2563D fileHash{},stableContentHash{},payloadHash{},rootHash{};
};

[[nodiscard]] std::optional<GameplayProductionHdrSidecarBuildResult3D> BuildGameplayProductionHdrSidecar3D(const ProductionReadbackBundle3D& bundle,GameplayProductionHdrSidecarValidation3D* error=nullptr);
[[nodiscard]] bool PublishGameplayProductionHdrSidecar3D(const GameplayProductionHdrSidecarBuildResult3D& sealed,const std::filesystem::path& destination,GameplayProductionHdrSidecarPublishReceipt3D* receipt=nullptr);
[[nodiscard]] std::optional<GameplayProductionHdrSidecarInspectReceipt3D> InspectGameplayProductionHdrSidecarFile3D(const std::filesystem::path& path);

}
