#include "MistwatchSession.h"
#include "MistwatchRiverbankConstruction.h"
#include "MistwatchFuseDialVisual.h"
#include "MistwatchBeaconDetail.h"
#include "MistwatchGeneratorDetail.h"
#include "MistwatchWorld.h"
#include "MistwatchWestCliffClearance.h"
#include "MistwatchFallenLogAssets.h"
#include "MistwatchBoulderDetail.h"
#include "MistwatchMiddleLayer.h"
#include "MistwatchWorldProfile.h"
#include "MistwatchFirLayout.h"
#include "MistwatchEncounter.h"
#include "MistwatchEncounterFeedback.h"
#include "MistwatchCamera.h"
#include "MistwatchFoliageView.h"
#include "MistwatchGroveFloor.h"
#include <Aevrynd/StaticColorOcclusion3D.h>
#include "MistwatchCameraInput.h"
#include "MistwatchObjectiveGuide.h"
#include "MistwatchObjectivePointer.h"
#include "MistwatchCounterWindowCue.h"
#include "MistwatchFieldInput.h"
#include "MistwatchPickupFeedback.h"
#include "MistwatchPickupIntent.h"
#include "MistwatchTuningInputRepeat.h"
#include "MistwatchFieldJournal.h"
#include "MistwatchSurveyMap.h"
#include "MistwatchActionFeedback.h"
#include "MistwatchExplorer.h"
#include "MistwatchExplorerSurface.h"
#include "MistwatchExplorerNapeHairBinding.h"
#include "MistwatchExplorerBackpack.h"
#include "MistwatchForestMaterial.h"
#include "MistwatchGroundMaterial.h"
#include "GroundForestWoodArt.h"
#include "MistwatchHabitatLeafMaterial.h"
#include "MistwatchForestSurfaceProfile.h"
#include "MistwatchForestHabitatFloor.h"
#include "MistwatchForestLeafDomain.h"
#include "MistwatchDeciduousHabitatFloor.h"
#include "MistwatchFern.h"
#include "MistwatchScannedGrass.h"
#include "MistwatchForestGrassShape.h"
#include "MistwatchNettleMix.h"
#include "MistwatchForestCover.h"
#include "MistwatchForestCoverRetention.h"
#include "MistwatchTrailLitter.h"
#include "MistwatchShrubCanopy.h"
#include "MistwatchShrubGapDensity.h"
#include "MistwatchShrubArcDensity.h"
#include "MistwatchFernColonies.h"
#include "MistwatchCanopyPopulation.h"
#include "MistwatchCanopyCollision.h"
#include "MistwatchRootStemTerrain.h"
#include "MistwatchValleyTerrain.h"
#include "MistwatchNorthWatershed.h"
#include "MistwatchValleyStartupBake.h"
#include "MistwatchValleyPlacement.h"
#include "MistwatchEntranceCanopy.h"
#include "MistwatchEntranceWorldAuthor.h"
#include "MistwatchWoodlandRecipe.h"
#include "MistwatchWoodlandBridgeWorld.h"
#include "MistwatchBearingBridge.h"
#include "MistwatchStormRiver.h"
#include "MistwatchWoodlandProfile.h"
#include "MistwatchLeafCluster.h"
#include "MistwatchEntranceComposition.h"
#include "MistwatchRoadsideCover.h"
#include "MistwatchRoadsideBroadleaf.h"
#include "MistwatchEntranceUnderstory.h"
#include "MistwatchEntranceRoadDomain.h"
#include "MistwatchEntranceRoadMaterial.h"
#include "MistwatchSettlementSurface.h"
#include "MistwatchSettlementGround.h"
#include "MistwatchGrassCaptureReference.h"
#include "MistwatchEvasion.h"
#include "MistwatchRiver.h"
#include "MistwatchBoar.h"
#include "MistwatchBoarHitReaction.h"
#include "MistwatchLandscape.h"
#include "MistwatchLandscapeTwigDraw.h"
#include "MistwatchLandscapeCrownRange.h"
#include "MistwatchLandscapeStandFill.h"
#include "MistwatchShoulderGrove.h"
#include "MistwatchShoulderGroundcover.h"
#include "MistwatchCliffCoverage.h"
#include "MistwatchCliffMaterial.h"
#include "MistwatchCliffWorldProfile.h"
#include "MistwatchRockSegment.h"
#include "MistwatchVolumetricOutcrop.h"
#include "MistwatchShoulderDraw.h"
#include "MistwatchOuterForestFloor.h"
#include "MistwatchReplayCostProbe.h"
#include "MistwatchCaptureGpuBenchmark.h"
#include "MistwatchCapturePresentation.h"
#include <optional>
#include "MistwatchAuthoredLandscapePlanting.h"
#include "MistwatchMixedLandscapeCanopy.h"
#include "MistwatchLandscapeCrownGrowth.h"
#include "MistwatchSlopeGrove.h"
#include "MistwatchForestPlayable.h"
#include "MistwatchCoreHerbClusters.h"
#include "MistwatchCoreHerbRouteLayout.h"
#include "MistwatchCoreHerbSupport.h"
#include "MistwatchCoreHerbDraw.h"
#include "MistwatchRouteGrassPlan.h"
#include "MistwatchRouteGrassCookLoad.h"
#include "MistwatchRouteGrassDraw.h"
#include "MistwatchUnderstoryRetention.h"
#include "MistwatchForestLighting.h"
#include "MistwatchGroundMicrorelief.h"
#include "MistwatchForestIbl.h"
#include <Aevrynd/PinnedAssetRead3D.h>
#include "MistwatchAudio.h"
#include "MistwatchPlayback.h"
#include "MistwatchCameraSettings.h"
#include "MistwatchCameraSettingsFile.h"
#include "MistwatchInputReplay.h"
#include "MistwatchSupplyCrate.h"
#include "MistwatchSupplyCrateBoardUv.h"
#include "MistwatchRangerHut.h"
#include "MistwatchRangerRoofBand.h"
#include "MistwatchRangerHutLining.h"
#include "MistwatchHutWoodMaterial.h"
#include "MistwatchShelterWeatheredUv.h"
#include "MistwatchRepairShelterDetail.h"
#include "MistwatchWorkshopVisual.h"
#include "MistwatchWorkshopFrame.h"
#include "MistwatchWorkshopRoofCollision.h"
#include "MistwatchWorkshopRoofCamera.h"
#include "MistwatchWorkshopWoodUv.h"
#include "MistwatchPlayableArtProfile.h"
#include "MistwatchRangerDesk.h"
#include "MistwatchBridgeWorksite.h"
#include "MistwatchRepairShelterVisual.h"
#include "MistwatchRepairShelterBoardUv.h"
#include "MistwatchRepairShelterRoofUv.h"
#include "MistwatchRepairToolsVisual.h"
#include "MistwatchRepairSignVisual.h"
#include "MistwatchSaveFile.h"
#include "MistwatchLoadPreparation.h"
#include "MistwatchPausedCheckpoint.h"
#include "MistwatchPlayerLocomotion.h"
#include "MistwatchPlayablePairFrame.h"
#include "MistwatchFrameInputLatch.h"
#include "MistwatchPairLoadPreparation.h"
#include "MistwatchBridgeRepairPreparation.h"
#include <Aevrynd/SkinnedModelAsset.h>
#include <Aevrynd/Engine.h>
#include <Aevrynd/Scene3D.h>
#include <Aevrynd/CharacterController3D.h>
#include <Aevrynd/ThirdPersonCameraMotion3D.h>
#include <Aevrynd/MaterialLibrary3D.h>
#include <Aevrynd/MeshLoader.h>
#include <Aevrynd/EnvironmentMeshes3D.h>
#include <fstream>
#include <iterator>
#include <memory>
#include <chrono>
#include <functional>
#include <numeric>
#include <iomanip>
#include <bit>

#include "MatureWorldSnapshot.h"
#include "MatureStudyRuntime.h"
using namespace Aevrynd;
class MistwatchGame final : public IGame {
#if defined(MISTWATCH_SAVE_INPUT_AUDIT)
    // 僅原生按鍵測試可存取私有狀態；正式遊戲不含此測試友元。
    friend class MistwatchSaveInputProbe;
#endif
    Mistwatch::Session session;
    CharacterController3D controller{CharacterControllerSettings3D{.jumpGraceSeconds=.1f}};
    Scene3D scene;
    MeshResource3D box;
    std::array<MeshResource3D,2> supplyMeshes;
    unsigned submittedSupplies{};
    unsigned counterCueSubmitted{};
    MeshResource3D rangerHutMesh;
    MeshResource3D plankedShelterMesh;
    std::array<MeshResource3D,2> shelterDetailMeshes;
    std::array<std::uint64_t,2> shelterDetailFingerprints{};
    bool shelterDetailEnabled{};
    unsigned shelterDetailFrameMembers{},shelterDetailCorrugations{},submittedShelterDetail{};
    bool plankedShelter{true};
    bool boardUvAligned{true};
    bool roofUvAligned{true};
    bool shelterWeatheredUv{};
    std::uint64_t shelterUvFingerprint{};
    unsigned submittedPlankedShelter{};
    MeshResource3D workshopMesh;
    bool workshopFrameEnabled{};
    std::uint64_t workshopFrameFingerprint{};
    bool manualWorkshopRoofCandidate{};
    bool workshopWoodMetricEnabled{};
    struct WorkshopRoofRuntime {
        std::vector<Mistwatch::WorkshopRoof::Part> parts;
        std::vector<Mistwatch::WorkshopRoofCollision::Member> members;
        std::vector<const ConvexAabbSweep3D*> hulls;
        std::vector<BoxCollider3D> cameraOnly,combinedCameraOnly;
        std::uint64_t visualFingerprint{},cameraFingerprint{};
    };
    // 凸包及借用指標同屬不可搬移的堆積擁有者；完成後不再修改 members。
    std::unique_ptr<WorkshopRoofRuntime> workshopRoof;
    const std::array<BoxCollider3D,1> rangerCameraRoof{Mistwatch::RangerHutCameraRoofSolid()};
    bool plankedWorkshop{true},captureWorkshopOverview{},captureWorkshopGroundWide{};
    unsigned submittedWorkshop{};
    std::uint64_t workshopUvFingerprint{};
    std::array<MeshResource3D,2> repairToolsMeshes;
    bool repairToolsDetailed{true};
    unsigned repairToolsSubmitted{};
    std::array<MeshResource3D,3> repairSignMeshes;
    std::array<Material3D,3> repairSignMaterials;
    unsigned repairSignSubmitted{},repairSignVisibleSites{};
    MeshResource3D rangerDeskMesh;
    unsigned submittedRangerDesk{};
    Material3D hutMaterial;
    std::array<MeshResource3D,2> bridgeWorksiteMeshes;
    std::array<MeshResource3D,Mistwatch::GeneratorDetailGroups> generatorDetailMeshes;
    const std::array<Material3D,Mistwatch::GeneratorDetailGroups> generatorDetailMaterials=Mistwatch::GeneratorDetailMaterials();
    unsigned submittedGeneratorLensTriangles{};
    unsigned submittedBridgeWorksite{};
    bool diagnosticLegacyRangerHut{},captureRangerHutOverview{};
    std::array<MeshResource3D,4> firMeshes;
    MeshResource3D detailedFirTwig;
    bool nearFirTwigLod{true};
    Scene3DStats firLodSubmission{};
    bool shadowRetentionEnabled{true};
    std::array<MeshResource3D,4> landscapeFirMeshes;
    bool captureLandscapeTwigLod1{};
    bool landscapeTwigDistanceLod{};
    MeshResource3D landscapeDetailedFirTwig;
    MeshBounds3D landscapeTwigUnionBounds;
    bool landscapeCrownGrowth{};
    std::array<MeshResource3D,5> grownLandscapeFir;
    std::array<Mistwatch::LandscapeCrownGrowth::Result,5> crownGrowthProof;
    MeshBounds3D grownTwigUnionBounds;
    unsigned crownGrowthAcquisitions{},crownGrowthObservedDraws{};
    bool coreHerbEnabled{};
    Mistwatch::CoreHerbRouteLayout::Profile coreHerbLayoutProfile{Mistwatch::CoreHerbRouteLayout::Profile::Original};
    Mistwatch::CoreHerbClusters::Plan coreHerbs;
    std::array<MeshResource3D,Mistwatch::CoreHerbClusters::Target> coreHerbMeshes;
    std::vector<Mistwatch::CoreHerbSupport::Proof> coreHerbProofs;
    unsigned coreHerbAcquisitions{};
    std::uint64_t coreHerbSurfaceFingerprint{};
    static constexpr std::uint64_t CoreHerbIdentityNamespace=0x4d57434845524231ull;
    struct CoreHerbFrame {
        unsigned accepted{},draws{},opaque{},shadowOnly{};
        std::uint64_t triangles{},sceneSerial{},cameraHash{};
        std::array<unsigned,Mistwatch::CoreHerbClusters::Target> visibility{};
        std::array<unsigned,Mistwatch::CoreHerbClusters::Target> cullReasons{};
    } coreHerbFrame,coreHerbFrozen;
    bool routeGrassEnabled{},routeGrassCooked{};
    std::string routeGrassBundleSha,routeGrassBuildSha;
    Mistwatch::RouteGrassPlan::Plan routeGrassPlan;
    std::vector<Mistwatch::UnderstoryRetention::Row> completeUnderstoryRows;
    std::array<MeshResource3D,128> routeGrassMeshes;
    unsigned routeGrassAcquisitions{};
    std::uint64_t routeGrassPlacementNs{};
    struct RouteGrassFrame {
        unsigned accepted{},draws{},opaque{},shadowOnly{};
        std::uint64_t triangles{},sceneSerial{},cameraHash{};
        std::array<unsigned,128> visibility{},cullReasons{};
    } routeGrassFrame,routeGrassFrozen;
    std::uint64_t landscapeDetailedTwigFingerprint{};
    float captureLandscapeTwigDistance{-1.f};
    Mistwatch::LandscapeCrownRange::Policy landscapeCrownRange{Mistwatch::LandscapeCrownRange::Original};
    std::array<std::optional<Mistwatch::LandscapeTwigDraw::Observer>,520> landscapeTwigObservers;
    std::array<std::uint64_t,4> landscapeFirFingerprints{};
    std::array<Material3D,4> firMaterials;
    MaterialLibrary3D forestMaterials;
    MaterialLibrary3D broadleafMaterials;
    std::array<MeshResource3D,3> broadleafMeshes;
    MatureStudy::Runtime matureStudy;
    std::array<Material3D,3> broadleafSurface;
    std::vector<Mistwatch::BroadleafTree> broadleafTrees;
    Mistwatch::MiddleLayerResult middleLayer;
    std::vector<BoxCollider3D> middleLayerSolids;
    bool middleLayerEnabled{true};
    Mistwatch::WorldProfile worldProfile{Mistwatch::WorldProfile::ForestWood};
    Mistwatch::FirLayoutSelector firLayoutSelector{Mistwatch::FirLayoutSelector::ForestWood};
    Mistwatch::MatureFirLayout firLayout;
    Mistwatch::EntranceWorldAuthor entranceWorld;
    bool woodlandCanopyEnabled{};
    bool manualWoodlandCandidate{};
    std::optional<Mistwatch::WoodlandBridgeWorld::ValidatedWorld> woodlandWorld;
    Mistwatch::EntranceCanopyMetadata woodlandMetadata;
    std::uint64_t woodlandInputIdentity{},woodlandOutputIdentity{};
    std::filesystem::path woodlandStartupCleanup;
    bool woodlandFailAfterCanopy{};
    bool rightForeground{};
    std::uint64_t compositionOriginalFingerprint{},compositionPoseFingerprint{};
    Mistwatch::FallenLogProfile fallenLogProfile{Mistwatch::FallenLogProfile::LegacyCylinder};
    Mistwatch::FallenLogAssets::Resources fallenLogs;
    unsigned submittedFallenLogs{};
    std::uint64_t submittedFallenTriangles{};
    unsigned submittedMiddleLayer{};
    std::size_t submittedMiddleLayerTriangles{};
    std::size_t broadleafTriangles{};
    HeightField3D terrain;
    bool valleyWorldEnabled{};
    bool manualValleyCandidate{};
    bool valleyWorkshopEnabled{};
    bool valleyTerraceEnabled{};
    std::filesystem::path valleyEvidenceDirectory;
    std::optional<Mistwatch::ValleySaveDomain> valleySaveDomain;
    MeshResource3D terrainMesh;
    bool entranceRoadEnabled{};
    std::array<MeshResource3D,3> entranceRoadMeshes;
    bool settlementSurfaceEnabled{};
    MaterialAssetDefinition3D settlementSurfaceDefinition;
    Material3D settlementSurfaceMaterial;
    std::array<std::uint64_t,3> settlementSurfaceFingerprints{};
    Mistwatch::EntranceRoadDomainReceipt entranceRoadReceipt;
    bool settlementGroundEnabled{};
    Mistwatch::EntranceRoadDomainReceipt settlementOriginalRoadReceipt;
    std::size_t settlementMovedTriangles{},settlementBoundaryVertices{},settlementChangedForest{},settlementChangedRoad{};
    MaterialLibrary3D entranceRoadMaterials;
    Material3D entranceRoadSurface;
    MaterialAssetDefinition3D entranceRoadDefinition;
    unsigned entranceRoadSubmitted{};
    Mistwatch::ShrubCanopyResult shrubCanopy;
    std::array<std::array<MeshResource3D,2>,4> shrubMeshes;
    std::array<unsigned,4> shrubRenderLeafCounts{};
    bool shrubsEnabled{true};
    unsigned shrubTarget{64};
    bool shrubGapDensity{};
    bool shrubArcDensity{};
    bool captureShrubOverview{};
    bool captureFrameCamera{};
    unsigned submittedShrubs{};
    MaterialLibrary3D canopyMaterials;
    std::array<MeshResource3D,3> canopyMeshes;
    Mistwatch::EntranceCanopyMetadata leafClusterGround;
    std::array<MeshResource3D,Mistwatch::WoodlandCanopy::TotalCount> canopyRootMeshes;
    std::array<Mesh3D,3> canopySource;
    Mistwatch::CanopyPopulation canopyPopulation;
    // 谷地只重用本次啟動已完整作者的木體；保存真輸入快照，絕不讀取跨程序快取。
    Mistwatch::CanopyPopulation valleyPreparedCanopy;
    std::vector<Mistwatch::CanopyWoodProxy> valleyPreparedWood;
    Mesh3D valleyPreparedBranches;
    bool valleyBakeEnabled{};
    std::filesystem::path valleyBakeExportPath;
    std::vector<std::byte> valleyBakeWitness;
    std::optional<Mistwatch::ValleyStartupBake::Data> valleyLoadedBake;
    Mistwatch::EntranceCanopyMetadata entranceCanopy;
    std::vector<BoxCollider3D> canopySolids;
    std::uint64_t canopyWoodSurfaceCells{},canopyWoodEnclosedGridCells{},canopyWoodSourceTriangles{};
    std::array<Material3D,3> canopySurfaces;
    Transform3D canopyPose;
    bool canopyEnabled{true},captureCanopyOverview{};
    float canopyViewYaw{},canopyViewHeight{3.1f},canopyViewDistance{8.5f},canopyViewTargetHeight{2.6f};
    unsigned canopyTriangles{},submittedCanopy{};
    Mistwatch::TrailLitterResult trailLitter;
    std::array<MeshResource3D,2> trailLitterMeshes;
    bool trailLitterEnabled{true};
    bool captureTrailOverview{};
    int captureFallenView{-1};
    unsigned submittedTrailLitter{};
    MeshResource3D landscapeMesh;
    Mistwatch::ForestHabitatFloorReceipt habitatTerrain,habitatLandscape;
    bool roadsideCoverEnabled{};
    Mistwatch::RoadsideCoverResult roadsideCover;
    unsigned submittedRoadside{};
    std::size_t submittedRoadsideTriangles{};
    bool roadsideBroadleafEnabled{};
    bool entranceUnderstoryEnabled{};
    Mistwatch::EntranceUnderstoryResult entranceUnderstory;
    unsigned submittedEntranceUnderstory{};
    Mistwatch::RoadsideCoverResult roadsideBroadleaf;
    std::array<MeshResource3D,2> roadsideBroadleafMeshes;
    std::array<Mistwatch::NettleAssetBounds,2> roadsideBroadleafBounds;
    std::array<std::uint64_t,2> roadsideBroadleafIdentities{};
    MaterialLibrary3D roadsideBroadleafMaterials;
    Material3D roadsideBroadleafSurface;
    unsigned submittedRoadsideBroadleaf{};
    std::size_t submittedRoadsideBroadleafTriangles{};
    Mistwatch::ForestHabitatFloorProfile habitatFloorProfile{Mistwatch::ForestHabitatFloorProfile::Original};
    bool deciduousHabitat{},captureDeciduousRootView{};
    Mistwatch::DeciduousHabitatPlan deciduousHabitatPlan;
    std::string_view HabitatProfileName()const{
        return deciduousHabitat?"root-cohorts-v1":
            (habitatFloorProfile==Mistwatch::ForestHabitatFloorProfile::Original?"original":"leaf-coverage-strength-v1");
    }
    std::vector<Transform3D> landscapeTrees;
    // 新作者背景候選僅用於明確擷取；原互動入口不執行昂貴的完整支撐診斷。
    bool authoredLandscapePlanting{};
    bool mixedLandscapeCanopy{};
    bool forestLightingContrast{};
    bool groundMicrorelief{};
    bool forestIblEnabled{},forestIblCreated{};
    EnvironmentLightingStore3D forestIblStore;
    EnvironmentLightingHandle3D forestIblHandle;
    EnvironmentLightingGpuReceipt3D forestIblCreate,forestIblSet;
    RendererStats forestIblBaseline;
    // 獨立可遊玩候選，離線配方不在啟動或逐幀重新搜尋。
    bool shoulderGroveEnabled{};
    bool cliffDiagnostic{},cliffCoverageEnabled{};
    bool manualCliffWorldCandidate{},cliffWorldEnabled{};
    bool manualCliffReliefCandidate{},cliffReliefEnabled{};
    bool cliffVariationEnabled{};
    std::optional<Aevrynd::HeightField3D> cliffCanonicalTerrain;
    Mistwatch::WestCliffRelief::Receipt cliffReliefReceipt;
    Mistwatch::WestCliffRelief::MeshReceipt cliffReliefMeshReceipt;
    std::array<std::uint64_t,4> cliffReliefSupportCounts{};
    Mistwatch::LoadTerrainProfile RuntimeTerrainProfile()const{
        return valleyWorldEnabled?Mistwatch::LoadTerrainProfile::ValleyContactV7:
            cliffReliefEnabled?Mistwatch::LoadTerrainProfile::WestCliffStanding45V1:Mistwatch::LoadTerrainProfile::Canonical;
    }
    bool cliffMaterialDiagnostic{},cliffMaterialEnabled{};
    bool valleyMaterialAudit{};
    bool valleyNorthWatershed{};
    bool valleyOutcropPreview{};
    bool valleyJointRockBank{};
    bool valleySlopeGrove{};
    bool manualForestGroveCandidate{},forestPlayableWorld{},groveFloorLitterEnabled{};
    Mistwatch::SlopeGrove::Resources slopeGrove;
    std::vector<BoxCollider3D> valleyRockSolids;
    Mistwatch::VolumetricOutcrop::Resources valleyOutcrop;
    std::uint64_t cliffMaterialObservedFrames{};
    std::array<std::uint64_t,4> cliffMaterialObservedDraws{};
    Material3D cliffMaterial;
    struct CliffMaterialFrame {std::uint64_t cameraHash{},serial{};std::array<unsigned,4> draws{};std::array<std::uint64_t,4> textures{};} cliffMaterialFrame,cliffMaterialFrozen;
    const Material3D& TerrainRenderMaterial()const{return cliffMaterialEnabled?cliffMaterial:groundMaterial;}
    void WriteCliffMaterialReceipt(const std::filesystem::path& base,const CliffMaterialFrame& frame)const {
        if(!cliffMaterialDiagnostic&&!valleyMaterialAudit)return;
        auto path=base;path+=L".cliff-material.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Cliff material receipt refuses overwrite");
        std::ofstream out(path);out<<"schema=cliff-material-v1\nprofile="<<(cliffMaterialEnabled?"rock-face-03-2k":"original")
            <<"\ncamera_hash="<<frame.cameraHash<<"\nscene_serial="<<frame.serial
            <<"\ncore_draws="<<frame.draws[0]<<"\nring_draws="<<frame.draws[1]
            <<"\nroad_draws="<<frame.draws[2]<<"\ngravel_draws="<<frame.draws[3]
            <<"\nterrain_handle="<<TerrainRenderMaterial().textures.semanticIdentity
            <<"\noriginal_handle="<<groundMaterial.textures.semanticIdentity
            <<"\ncore_actual_texture="<<frame.textures[0]<<"\nring_actual_texture="<<frame.textures[1]
            <<"\nroad_actual_texture="<<frame.textures[2]<<"\ngravel_actual_texture="<<frame.textures[3]
            <<"\nmaterial_verified="<<(frame.draws[0]&&frame.draws[1])
            <<"\nroad_material_verified="<<(frame.draws[2]&&frame.draws[3])<<"\nart_accepted=0\n";
        out.close();if(!out)throw std::runtime_error("Cliff material receipt write failed");
    }
    bool rockSegmentDiagnostic{},rockSegmentEnabled{};
    Mistwatch::RockSegment::Resources rockSegment;
    Mistwatch::RockSegment::Frame rockSegmentFrame,rockSegmentFrozen;
    std::array<Mistwatch::CliffCoverage::Receipt,2> cliffReceipts;
    std::optional<Mistwatch::ForestLeafDomain::Receipt> leafFloorDomainReceipt;
    struct CliffFrame { Camera3D camera; std::uint64_t cameraHash{},sceneSerial{};unsigned width{},height{};std::array<unsigned,2> draws{}; } cliffFrame,cliffFrozen;
    void WriteCliffReceipt(const std::filesystem::path& base,const CliffFrame& frame)const {
        if(!cliffDiagnostic&&!valleyMaterialAudit)return;
        auto path=base;path+=L".cliff.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Cliff receipt refuses overwrite");
        std::ofstream out(path);out<<std::setprecision(17)<<"profile="<<(valleyMaterialAudit?"valley-render-location-v1":cliffCoverageEnabled?"west-cliff-v1":"original")
            <<"\ncamera_hash="<<frame.cameraHash<<"\nscene_serial="<<frame.sceneSerial<<"\nart_accepted=0\n";
        const std::array<const MeshResource3D*,4> meshes{&entranceRoadMeshes[0],&landscapeMesh,&entranceRoadMeshes[1],&entranceRoadMeshes[2]};
        const std::array<const char*,4> names{"core","ring","road","gravel"};
        for(unsigned slot=0;slot<meshes.size();++slot){
            const auto& resource=*meshes[slot];if(!resource||!resource.cpuMesh)throw std::runtime_error("Cliff export actual mesh missing");
            const auto& mesh=*resource.cpuMesh;
            out<<names[slot]<<"_actual="<<Mistwatch::EntranceRoadDomainDetail::Fingerprint(mesh)<<'\n';
            if(slot<2&&cliffDiagnostic){const auto& r=cliffReceipts[slot];
                out<<names[slot]<<"_before="<<r.beforeFingerprint<<'\n'<<names[slot]<<"_after="<<r.afterFingerprint
                    <<'\n'<<names[slot]<<"_changed="<<r.changed<<'\n'<<names[slot]<<"_draws="<<frame.draws[slot]
                    <<'\n'<<names[slot]<<"_geometry_before="<<r.beforeGeometryFingerprint<<'\n'<<names[slot]<<"_geometry_after="<<r.afterGeometryFingerprint<<'\n';}
            auto binary=base;binary+=".cliff-"+std::string(names[slot])+".aemesh";
            if(std::filesystem::exists(binary))throw std::runtime_error("Cliff mesh export refuses overwrite");
            std::ofstream data(binary,std::ios::binary);
            const auto word=[&](std::uint32_t value){data.write(reinterpret_cast<const char*>(&value),4);};
            word(0x334d4541u);word(1);word(static_cast<std::uint32_t>(mesh.vertices.size()));word(static_cast<std::uint32_t>(mesh.indices.size()));
            for(const auto& v:mesh.vertices){for(float f:{v.x,v.y,v.z})word(std::bit_cast<std::uint32_t>(f));
                word(v.normal);word(v.color);word(std::bit_cast<std::uint32_t>(v.u));word(std::bit_cast<std::uint32_t>(v.v));}
            for(auto index:mesh.indices)word(index);
            data.close();if(!data)throw std::runtime_error("Cliff mesh export write failed");
        }
        out.close();if(!out)throw std::runtime_error("Cliff receipt write failed");
        if(leafFloorDomainReceipt){
            const auto& r=*leafFloorDomainReceipt;
            const auto actual=Mistwatch::EntranceRoadDomainDetail::Fingerprint(*entranceRoadMeshes[0].cpuMesh);
            if(actual!=cliffReceipts[0].afterFingerprint)throw std::runtime_error("Leaf final GPU forest identity differs");
            auto leafPath=base;leafPath+=L".leaf-floor.txt";
            if(std::filesystem::exists(leafPath))throw std::runtime_error("Leaf floor receipt refuses overwrite");
            std::ofstream leaf(leafPath);leaf<<"profile=leaf-coverage-strength-v1\ncanonical_habitat_profile=original\n"
                <<"source_original="<<r.sourceOriginal<<"\nsource_leaf="<<r.sourceLeaf
                <<"\nforest_original="<<r.beforeFingerprint<<"\nforest_candidate_full="<<r.candidateFingerprint
                <<"\nforest_leaf_render_source="<<r.afterFingerprint<<"\nreferenced="<<r.referenced
                <<"\nchanged="<<r.changed<<"\nseams="<<r.seams<<"\nunused_preserved="<<r.unusedPreserved
                <<"\nbridge_preserved="<<r.bridgePreserved<<"\nboundary_preserved="<<r.boundaryPreserved
                <<"\nfinal_gpu_forest="<<actual<<"\ncamera_hash="<<frame.cameraHash<<"\nscene_serial="<<frame.sceneSerial
                <<"\nforest_draws="<<frame.draws[0]<<"\nart_accepted=0\n";
            leaf.close();if(!leaf)throw std::runtime_error("Leaf floor receipt write failed");
        }
        if(cliffReliefEnabled){
            auto reliefPath=base;reliefPath+=L".cliff-relief.txt";
            if(std::filesystem::exists(reliefPath))throw std::runtime_error("Relief receipt refuses overwrite");
            const auto actual=Mistwatch::WestCliffRelief::ValidateExact(terrain);
            if(actual.derivedFingerprint!=cliffReliefReceipt.derivedFingerprint)throw std::runtime_error("Relief field changed after qualification");
            std::ofstream relief(reliefPath);relief<<std::setprecision(17)
                <<"schema=west-cliff-relief-v1\nprofile="<<Mistwatch::WestCliffRelief::Profile
                <<"\ncanonical="<<cliffReliefReceipt.canonicalFingerprint<<"\nderived="<<actual.derivedFingerprint
                <<"\nchanged_samples="<<cliffReliefReceipt.changedSamples<<"\nmaximum_cut="<<cliffReliefReceipt.maximumCut
                <<"\nmesh_positions="<<cliffReliefMeshReceipt.changedPositions<<"\nmesh_normals="<<cliffReliefMeshReceipt.changedNormals
                <<"\nsupport_instances="<<cliffReliefSupportCounts[0]<<"\nsupport_triangles="<<cliffReliefSupportCounts[1]
                <<"\nsupport_colliders="<<cliffReliefSupportCounts[2]<<"\noverhead_triangles="<<cliffReliefSupportCounts[3]
                <<"\nsupport_fixed_wind_envelope=1"
                <<"\nactive_collision_field=1\nart_accepted=0\n";
            relief.close();if(!relief)throw std::runtime_error("Relief receipt write failed");
            auto policyPath=base;policyPath+=L".locomotion-policy.txt";
            if(std::filesystem::exists(policyPath))throw std::runtime_error("Locomotion receipt refuses overwrite");
            if(controller.Settings().maximumTerrainUphillGrade!=1.f||controller.Settings().maximumTerrainStandingGrade!=1.f)
                throw std::runtime_error("Cliff uphill or standing policy missing");
            std::ofstream policy(policyPath);
            policy<<"schema=locomotion-policy-v2\nprofile=west-cliff-standing45-v1\nmaximum_terrain_uphill_grade="
                <<controller.Settings().maximumTerrainUphillGrade<<"\nmaximum_terrain_standing_grade="
                <<controller.Settings().maximumTerrainStandingGrade<<"\nterrain_fingerprint="<<actual.derivedFingerprint
                <<"\nlanding_reach_preflight=1\nend_of_step_contact_handoff=1\nvelocity_landing_lead=1\n";
            policy.close();if(!policy)throw std::runtime_error("Locomotion receipt write failed");
        }
        auto cameraPath=base;cameraPath+=L".cliff-camera.json";
        if(std::filesystem::exists(cameraPath))throw std::runtime_error("Cliff camera refuses overwrite");
        std::ofstream c(cameraPath);c<<std::setprecision(17);
        const auto vector=[&](Vec3 v){c<<'['<<v.x<<','<<v.y<<','<<v.z<<']';};
        c<<"{\"position\":";vector(frame.camera.position);c<<",\"target\":";vector(frame.camera.target);
        c<<",\"up\":";vector(frame.camera.up);c<<",\"fov\":"<<frame.camera.projection.verticalFieldOfView
            <<",\"viewport\":["<<frame.width<<','<<frame.height<<"]}\n";
        c.close();if(!c)throw std::runtime_error("Cliff camera write failed");
    }
    bool shoulderGroundcoverEnabled{};
    std::array<MeshResource3D,Mistwatch::ShoulderGroundcover::SlotCount> shoulderGroundcoverMeshes;
    std::array<bool,Mistwatch::ShoulderGroundcover::SlotCount> shoulderGroundcoverReleaseFailed{};
    unsigned shoulderGroundcoverAcquisitions{};
    struct GroundcoverFrame {
        unsigned accepted{};
        std::uint64_t sceneSerial{},cameraHash{};
        Mistwatch::ShoulderGroundcover::Result draws;
        std::array<unsigned,Mistwatch::ShoulderGroundcover::SlotCount> cullReasons{};
    } groundcoverFrame,groundcoverFrozen;
    const Material3D& GroundcoverMaterial(unsigned slot)const {
        return slot<5?grassSurface:slot<9?fernSurface:roadsideBroadleafSurface;
    }
    unsigned ReleaseShoulderGroundcover(Engine& e)noexcept {
        unsigned count{};
        for(std::size_t i=0;i<shoulderGroundcoverMeshes.size();++i){auto& mesh=shoulderGroundcoverMeshes[i];
            if(!mesh||shoulderGroundcoverReleaseFailed[i])continue;
            try{e.DestroyMesh3D(mesh.handle);++count;mesh={};}
            catch(...){shoulderGroundcoverReleaseFailed[i]=true;}
        }
        return count;
    }
    void WriteGroundcoverReceipt(const std::filesystem::path& framePath,const GroundcoverFrame& frame)const {
        auto path=framePath;path+=L".shoulder-groundcover.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Groundcover receipt refuses overwrite");
        std::ofstream out(path);out<<std::setprecision(17)
            <<"schema=mistwatch.shoulder-groundcover-native.1\nprofile="
            <<(shoulderGroundcoverEnabled?Mistwatch::ShoulderGroundcover::Profile:"original")
            <<"\ntarget=192\navailable=78\nfull_target_pass=0\npublished=0\nart_accepted=0\nretained_trees="
            <<landscapeStandPlan.combined.size()<<"\ncreated_acquisitions="<<shoulderGroundcoverAcquisitions
            <<"\naccepted_slots="<<frame.accepted<<"\npost_cull_draws="<<frame.draws.draws
            <<"\nopaque="<<frame.draws.opaque<<"\nshadow_only="<<frame.draws.shadowOnly
            <<"\nculled="<<frame.accepted-frame.draws.draws<<"\ntriangles="<<frame.draws.triangles
            <<"\nscene_serial="<<frame.sceneSerial<<"\ncamera_hash="<<frame.cameraHash<<'\n';
        if(shoulderGroundcoverEnabled)for(std::size_t i=0;i<shoulderGroundcoverMeshes.size();++i){
            const auto& p=Mistwatch::ShoulderGroundcover::Recipes[i];const auto& mesh=shoulderGroundcoverMeshes[i];
            out<<"plant_"<<i<<"_id="<<p.id<<"\nplant_"<<i<<"_slot="<<p.slot
                <<"\nplant_"<<i<<"_derived="<<p.derivedHash<<"\nplant_"<<i<<"_source="<<p.sourceHash
                <<"\nplant_"<<i<<"_handle="<<mesh.handle.value<<"\nplant_"<<i<<"_material="
                <<GroundcoverMaterial(p.slot).textures.semanticIdentity<<"\nplant_"<<i<<"_visibility="
                <<unsigned(frame.draws.visibility[i])<<"\nplant_"<<i<<"_cull_reason="<<frame.cullReasons[i]<<'\n';
        }
        out.close();if(!out)throw std::runtime_error("Groundcover receipt write failed");
    }
    int captureShoulderView{-1};
    bool captureShoulderEyeLevel{};
    Mistwatch::ShoulderGrove::Plan shoulderGrove;
    std::array<MeshResource3D,16> shoulderRootMeshes;
    std::array<bool,16> shoulderReleaseFailed{};
    unsigned shoulderRootAcquisitions{},shoulderSelectedBase{},shoulderSelectedStand{};
    double shoulderPrepareMilliseconds{},shoulderUploadMilliseconds{};
    static_assert(Mistwatch::ShoulderGrove::InstanceNamespace==Mistwatch::ShoulderDraw::InstanceNamespace);
    struct ShoulderFrame {
        unsigned accepted{};
        std::uint64_t sceneSerial{},cameraHash{};
        Mistwatch::ShoulderDraw::Result draws;
    } shoulderFrame,shoulderFrozen;
    unsigned ReleaseShoulderRoots(Engine& e)noexcept {
        unsigned count{};
        for(std::size_t i=0;i<shoulderRootMeshes.size();++i){auto& mesh=shoulderRootMeshes[i];
            if(!mesh||shoulderReleaseFailed[i])continue;
            // 退休佇列可能拋例外；保留首個失敗與未確認 handle，不在解構路徑重試或 terminate。
            try{e.DestroyMesh3D(mesh.handle);++count;mesh={};}catch(...){shoulderReleaseFailed[i]=true;}
        }
        return count;
    }
    int captureLandscapeDetail{-1};
    std::array<Mesh3D,3> landscapeBroadleafSource;
    Mistwatch::AuthoredLandscapePlantingPlan landscapePlanting;
    std::array<MeshResource3D,16> landscapeRootMeshes;
    unsigned landscapeRootAcquisitions{};
    static constexpr std::uint64_t LandscapeIdentityNamespace=0x4d574c414e445350ull;
    struct LandscapeFrame {
        unsigned acceptedFir{},acceptedBroadleaf{},opaque{},shadowOnly{};
        unsigned grownDraws{};
        unsigned nearTwigDraws{},farTwigDraws{},transitionTwigSlots{},targetNearTwig{},targetFarTwig{};
        float targetTwigDistance{},targetTwigCoverage{};
        std::uint64_t triangles{},sceneSerial{},cameraHash{};
        Camera3D camera{};
        std::array<unsigned,520*4> visibility{};
    } landscapeFrame,landscapeFrozen;
    // 肩坡樹群獨立身分與收據，保留原 520 株的索引及所有舊世界門檻。
    bool landscapeStandFill{};
    Mistwatch::LandscapeStandFill::Plan landscapeStandPlan;
    MeshBounds3D landscapeStandBounds;
    bool outerForestFloor{};
    Mistwatch::OuterForestFloor::Receipt outerFloorReceipt;
    static constexpr std::uint64_t StandIdentityNamespace=0x4d575354414e4432ull;
    struct StandFrame {
        unsigned accepted{},opaque{},shadowOnly{},draws{},nearTwig{},farTwig{},pairs{};
        std::uint64_t triangles{},sceneSerial{},cameraHash{};
        std::array<unsigned,Mistwatch::LandscapeStandFill::Target*4> visibility{};
    } standFrame,standFrozen;
    std::array<std::optional<Mistwatch::LandscapeTwigDraw::Observer>,Mistwatch::LandscapeStandFill::Target> standObservers;
    static Mesh3D LoadLandscapeBroadleaf(const std::filesystem::path& path,unsigned slot) {
        constexpr std::array<std::size_t,3> bytes{163400,3704316,57760};
        constexpr std::array<std::string_view,3> sha{
            "4182c1d2d3e726f5c77dd84adf59ea5d4e022d49b1f211b34b488dbc165b68cc",
            "c46eca29c84d6ea9db320444d01ed4f16403a2a7f50729178e70c7886b28bc5b",
            "1c765173ebc17a3be167c35ef1b8df8329fb4cf98f468f4da13fd42533b677fd"};
        if(slot>=3)throw std::runtime_error("Landscape source slot invalid");
        struct File {HANDLE handle{INVALID_HANDLE_VALUE};~File(){if(handle!=INVALID_HANDLE_VALUE)CloseHandle(handle);}};
        File file{CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr)};
        LARGE_INTEGER length{};DWORD read{};
        if(file.handle==INVALID_HANDLE_VALUE||!GetFileSizeEx(file.handle,&length)||length.QuadPart!=static_cast<LONGLONG>(bytes[slot]))
            throw std::runtime_error("Landscape raw source unavailable");
        std::vector<std::byte> raw(bytes[slot]);
        if(!ReadFile(file.handle,raw.data(),static_cast<DWORD>(raw.size()),&read,nullptr)||read!=raw.size()||
           !Mistwatch::FallenLogAssets::ExactHash(raw,sha[slot]))throw std::runtime_error("Landscape raw source integrity mismatch");
        auto loaded=LoadAevryndMeshFile(path);std::array<std::uint32_t,4> header{};std::memcpy(header.data(),raw.data(),16);
        if(!loaded||header[0]!=0x334d4541u||header[1]!=1||header[2]!=loaded.mesh.vertices.size()||header[3]!=loaded.mesh.indices.size()||
           16+loaded.mesh.vertices.size()*sizeof(Vertex3D)+loaded.mesh.indices.size()*4!=raw.size()||
           std::memcmp(raw.data()+16,loaded.mesh.vertices.data(),loaded.mesh.vertices.size()*sizeof(Vertex3D))||
           std::memcmp(raw.data()+16+loaded.mesh.vertices.size()*sizeof(Vertex3D),loaded.mesh.indices.data(),loaded.mesh.indices.size()*4))
            throw std::runtime_error("Landscape decoded raw geometry mismatch");
        return std::move(loaded.mesh);
    }
    void WriteOuterForestFloorReceipt(const std::filesystem::path& framePath)const {
        auto path=framePath;path+=L".outer-floor.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Outer floor receipt refuses overwrite");
        if(!landscapeMesh.cpuMesh)throw std::runtime_error("Outer floor uploaded mesh unavailable");
        const auto actual=Mistwatch::OuterForestFloor::MeshFingerprint(*landscapeMesh.cpuMesh);
        if(outerForestFloor&&actual!=outerFloorReceipt.afterFingerprint)
            throw std::runtime_error("Outer floor actual GPU source drift");
        std::ofstream out(path);out<<std::setprecision(17)
            <<"schema=mistwatch.outer-forest-floor-native.1\nprofile="
            <<(outerForestFloor?Mistwatch::OuterForestFloor::Profile:"original")
            <<"\ncapture_only=1\nroots="<<outerFloorReceipt.roots<<"\nvertices="<<outerFloorReceipt.vertices
            <<"\nchanged="<<outerFloorReceipt.changed<<"\nseams="<<outerFloorReceipt.seams
            <<"\nprotected_vertices="<<outerFloorReceipt.protectedVertices
            <<"\nprotected_triangles="<<outerFloorReceipt.protectedTriangles
            <<"\nempty_vertices="<<outerFloorReceipt.emptyVertices<<"\nclearing_vertices="<<outerFloorReceipt.clearingVertices
            <<"\nchanged_triangles="<<outerFloorReceipt.changedTriangles
            <<"\npose_fingerprint="<<outerFloorReceipt.poseFingerprint
            <<"\nbefore_fingerprint="<<outerFloorReceipt.beforeFingerprint<<"\nafter_fingerprint="<<outerFloorReceipt.afterFingerprint
            <<"\nbefore_habitat_fingerprint="<<outerFloorReceipt.beforeHabitatFingerprint
            <<"\nafter_habitat_fingerprint="<<outerFloorReceipt.afterHabitatFingerprint
            <<"\nactual_surface_fingerprint="<<actual
            <<"\nsoil_delta="<<outerFloorReceipt.soilDelta<<"\nrock_delta="<<outerFloorReceipt.rockDelta
            <<"\narea_weighted_soil_delta="<<outerFloorReceipt.areaWeightedSoilDelta
            <<"\nminimum_radius="<<outerFloorReceipt.minimumRadius<<"\nmaximum_radius="<<outerFloorReceipt.maximumRadius
            <<"\nscene_serial="<<landscapeFrozen.sceneSerial<<"\ncamera_hash="<<landscapeFrozen.cameraHash
            <<"\ncreated_mesh_acquisitions=0\ncreated_material_acquisitions=0\nart_accepted=0\n";
        for(unsigned b=0;b<Mistwatch::LandscapeStandFill::BandCount;++b)
            out<<"band_"<<b<<"_changed="<<outerFloorReceipt.bandChanged[b]<<'\n';
        out.close();if(!out)throw std::runtime_error("Outer floor receipt write failed");
    }
    void WriteShoulderReceipt(const std::filesystem::path& framePath,const ShoulderFrame& frame)const {
        auto path=framePath;path+=L".shoulder-grove.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Shoulder receipt refuses overwrite");
        std::ofstream out(path);out<<std::setprecision(17)
            <<"schema=mistwatch.shoulder-grove-native.1\nprofile="<<(shoulderGroveEnabled?Mistwatch::ShoulderGrove::Profile:"original")
            <<"\ncapture_mode="<<!capture.empty()<<"\nnormal_candidate_available=1\naccepted_default=0"
            <<"\nsource_population="<<(shoulderGroveEnabled?616:0)<<"\nreplacement_trees="<<shoulderGrove.plants.size()
            <<"\nretained_trees="<<shoulderGrove.retained.size()<<"\nselected_base="<<shoulderSelectedBase
            <<"\nselected_stand="<<shoulderSelectedStand<<"\nsource_pose_fingerprint="<<shoulderGrove.sourcePoseFingerprint
            <<"\nsurface_geometry_fingerprint="<<shoulderGrove.surfaceGeometryFingerprint
            <<"\nactual_surface_fingerprint="<<shoulderGrove.surfaceFingerprint
            <<"\nprepare_milliseconds="<<shoulderPrepareMilliseconds<<"\nupload_milliseconds="<<shoulderUploadMilliseconds
            <<"\ncreated_acquisitions="<<shoulderRootAcquisitions<<"\naccepted_slots="<<frame.accepted
            <<"\npost_cull_opaque="<<frame.draws.opaque<<"\npost_cull_shadow_only="<<frame.draws.shadowOnly
            <<"\npost_cull_draws="<<frame.draws.draws<<"\npost_cull_triangles="<<frame.draws.triangles
            <<"\nscene_serial="<<frame.sceneSerial<<"\ncamera_hash="<<frame.cameraHash
            <<"\ncollision_qualification=not_yet_verified\nart_accepted=0\n";
        for(unsigned slot=0;slot<3;++slot)out<<"source_"<<slot<<"="<<shoulderGrove.sourceFingerprints[slot]<<'\n';
        for(std::size_t i=0;i<shoulderGrove.plants.size();++i){const auto& p=shoulderGrove.plants[i];
            out<<"plant_"<<i<<"_ordinal="<<p.sourceOrdinal<<"\nplant_"<<i<<"_derived="<<p.derivedFingerprint<<'\n';
            for(unsigned slot=0;slot<3;++slot)out<<"plant_"<<i<<"_slot_"<<slot<<"_visibility="<<unsigned(frame.draws.visibility[i*3+slot])<<'\n';
        }
        out.close();if(!out)throw std::runtime_error("Shoulder receipt write failed");
    }
    void WriteLandscapeStandReceipt(const std::filesystem::path& framePath)const {
        auto path=framePath;path+=L".landscape-stand.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Stand receipt refuses overwrite");
        std::ofstream out(path);out<<std::setprecision(9)
            <<"schema=mistwatch.landscape-stand-native.1\nprofile="<<(landscapeStandFill?Mistwatch::LandscapeStandFill::Profile:"original")
            <<"\ncapture_only=1\nbase_population="<<landscapeTrees.size()
            <<"\nadded_population="<<landscapeStandPlan.addedBands.size()<<"\ncomplete="<<landscapeStandPlan.complete
            <<"\nproposals="<<landscapeStandPlan.generated<<"\npose_fingerprint="<<landscapeStandPlan.fingerprint
            <<"\nsource_fingerprint="<<landscapeStandPlan.sourceFingerprint
            <<"\nunion_center="<<landscapeStandBounds.center.x<<','<<landscapeStandBounds.center.y<<','<<landscapeStandBounds.center.z
            <<"\nunion_extents="<<landscapeStandBounds.extents.x<<','<<landscapeStandBounds.extents.y<<','<<landscapeStandBounds.extents.z
            <<"\nunion_radius="<<landscapeStandBounds.radius<<"\ncreated_mesh_acquisitions=0\ncreated_material_acquisitions=0"
            <<"\naccepted_slots="<<standFrozen.accepted<<"\npost_cull_opaque_slots="<<standFrozen.opaque
            <<"\npost_cull_shadow_only_slots="<<standFrozen.shadowOnly<<"\nactual_draws="<<standFrozen.draws
            <<"\npost_cull_triangles="<<standFrozen.triangles<<"\nnear_twig_draws="<<standFrozen.nearTwig
            <<"\nfar_twig_draws="<<standFrozen.farTwig<<"\ntransition_pairs="<<standFrozen.pairs
            <<"\nscene_serial="<<standFrozen.sceneSerial<<"\ncamera_hash="<<standFrozen.cameraHash
            <<"\nart_accepted=0\nplayable_outer_ring=0\n";
        for(unsigned b=0;b<Mistwatch::LandscapeStandFill::BandCount;++b){const auto& c=landscapeStandPlan.bands[b];
            out<<"band_"<<b<<"_accepted="<<c.accepted<<"\nband_"<<b<<"_proposals="<<c.proposals
                <<"\nband_"<<b<<"_slope="<<c.slope<<"\nband_"<<b<<"_core="<<c.core
                <<"\nband_"<<b<<"_edge="<<c.edge<<"\nband_"<<b<<"_spacing="<<c.spacing<<'\n';
        }
        out.close();if(!out)throw std::runtime_error("Stand receipt write failed");
    }
    #include "MistwatchCrownGrowth.inl"
    #include "MistwatchCoreHerbRender.inl"
    #include "MistwatchRouteGrassRender.inl"
    #include "MistwatchUnderstoryRetention.inl"
    void WriteLandscapePlantingReceipt(const std::filesystem::path& framePath)const {
        auto path=framePath;path+=L".landscape-planting.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Landscape receipt refuses overwrite");
        std::ofstream out(path);out<<std::setprecision(9)<<"schema="
            <<(mixedLandscapeCanopy?"mistwatch.mixed-landscape-native.1":"mistwatch.authored-landscape-native.1")<<"\nprofile="
            <<(mixedLandscapeCanopy?Mistwatch::MixedLandscapeCanopy::Profile:
                authoredLandscapePlanting?Mistwatch::AuthoredLandscapePlantingProfile:"original")
            <<"\ncapture_only="<<(!mixedLandscapeCanopy)<<"\nroot_support_profile="<<Mistwatch::RootSupportProfile
            <<"\nrational_representation=reduced-gcd-v1\nworld_profile="<<Mistwatch::WorldProfileName(worldProfile)
            <<"\nhabitat_profile="<<HabitatProfileName()
            <<"\nsource_population="<<landscapeTrees.size()<<"\nreplacement_trees="<<landscapePlanting.plants.size()
            <<"\nsource_pose_fingerprint="<<landscapePlanting.originalPoseFingerprint
            <<"\nsource_surface_fingerprint="<<landscapePlanting.surfaceFingerprint
            <<"\nactual_surface_fingerprint="<<(landscapeMesh.cpuMesh?Mistwatch::RootSupportDetail::MeshFingerprint(*landscapeMesh.cpuMesh):0)
            <<"\nrender_fingerprint="<<landscapePlanting.renderFingerprint<<"\nderived_fingerprint="<<landscapePlanting.derivedFingerprint
            <<"\ncreated_acquisitions="<<landscapeRootAcquisitions
            <<"\naccepted_fir_slots="<<landscapeFrozen.acceptedFir<<"\naccepted_broadleaf_slots="<<landscapeFrozen.acceptedBroadleaf
            <<"\npost_cull_opaque_slots="<<landscapeFrozen.opaque<<"\npost_cull_shadow_only_slots="<<landscapeFrozen.shadowOnly
            <<"\npost_cull_triangles="<<landscapeFrozen.triangles<<"\nscene_serial="<<landscapeFrozen.sceneSerial
            <<"\ncamera_hash="<<landscapeFrozen.cameraHash<<"\ncamera_position="<<landscapeFrozen.camera.position.x<<','
            <<landscapeFrozen.camera.position.y<<','<<landscapeFrozen.camera.position.z<<"\ncamera_target="<<landscapeFrozen.camera.target.x<<','
            <<landscapeFrozen.camera.target.y<<','<<landscapeFrozen.camera.target.z<<"\nart_accepted=0\nplayable_outer_ring=0\n";
        if(mixedLandscapeCanopy)out<<"full_population="<<landscapeStandPlan.combined.size()
            <<"\nretained_base_fir=504\nreplacement_broadleaf=16\nretained_stand_fir=96\nstand_pose_fingerprint="
            <<landscapeStandPlan.fingerprint<<"\nstand_source_fingerprint="<<landscapeStandPlan.sourceFingerprint<<'\n';
        // 原生對照只替換針葉網格；記錄真正上傳的資源，不把診斷旗標當成渲染證據。
        out<<"twig_profile="<<(landscapeTwigDistanceLod?"crown-distance-lod-v1":captureLandscapeTwigLod1?"crown-lod1-source-v1":"original")
            <<"\ntwig_texture_identity="<<firMaterials[1].textures.value
            <<"\ntwig_alpha_mode="<<static_cast<unsigned>(firMaterials[1].alphaMode)
            <<"\ntwig_alpha_cutoff="<<firMaterials[1].alphaCutoff
            <<"\ntwig_double_sided="<<firMaterials[1].doubleSided
            <<"\ntwig_material_class="<<static_cast<unsigned>(firMaterials[1].materialClass)<<'\n';
        out<<"twig_range_profile="<<landscapeCrownRange.label
            <<"\ntwig_switch_distance="<<(landscapeTwigDistanceLod?landscapeCrownRange.switchDistance:0.f)
            <<"\ntwig_transition_width="<<(landscapeTwigDistanceLod?landscapeCrownRange.transitionWidth:0.f)
            <<"\ntwig_lod_acquisitions="<<static_cast<unsigned>(landscapeTwigDistanceLod)
            <<"\ntwig_near_draws="<<landscapeFrozen.nearTwigDraws<<"\ntwig_far_draws="<<landscapeFrozen.farTwigDraws
            <<"\ntwig_transition_slots="<<landscapeFrozen.transitionTwigSlots
            <<"\ntwig_177_near="<<landscapeFrozen.targetNearTwig<<"\ntwig_177_far="<<landscapeFrozen.targetFarTwig
            <<"\ntwig_177_bounds_distance="<<landscapeFrozen.targetTwigDistance
            <<"\ntwig_177_far_coverage="<<landscapeFrozen.targetTwigCoverage
            <<"\ntwig_near_mesh_handle="<<landscapeDetailedFirTwig.handle.value
            <<"\ntwig_near_mesh_vertices="<<landscapeDetailedFirTwig.vertexCount
            <<"\ntwig_near_mesh_triangles="<<landscapeDetailedFirTwig.TriangleCount()
            <<"\ntwig_near_mesh_fingerprint="<<landscapeDetailedTwigFingerprint<<'\n';
        for(unsigned slot=0;slot<4;++slot)out<<"fir_mesh_"<<slot<<"_lod="<<(slot==1&&captureLandscapeTwigLod1?1:2)
            <<"\nfir_mesh_"<<slot<<"_fingerprint="<<landscapeFirFingerprints[slot]
            <<"\nfir_mesh_"<<slot<<"_handle="<<landscapeFirMeshes[slot].handle.value
            <<"\nfir_mesh_"<<slot<<"_vertices="<<landscapeFirMeshes[slot].vertexCount
            <<"\nfir_mesh_"<<slot<<"_triangles="<<landscapeFirMeshes[slot].TriangleCount()<<'\n';
        std::vector<std::uint64_t> unique;
        for(unsigned slot=0;slot<3;++slot)out<<"source_mesh_"<<slot<<"_fingerprint="<<landscapePlanting.sourceFingerprints[slot]
            <<"\nsource_mesh_"<<slot<<"_handle="<<broadleafMeshes[slot].handle.value
            <<"\nsource_mesh_"<<slot<<"_vertices="<<broadleafMeshes[slot].vertexCount
            <<"\nsource_mesh_"<<slot<<"_triangles="<<broadleafMeshes[slot].TriangleCount()<<'\n';
        for(std::size_t i=0;i<landscapePlanting.plants.size();++i){
            const auto& p=landscapePlanting.plants[i];const auto& proof=p.proof;const auto handle=landscapeRootMeshes[i].handle.value;
            if(std::find(unique.begin(),unique.end(),handle)==unique.end())unique.push_back(handle);
            out<<"plant_"<<i<<"_ordinal="<<p.sourceOrdinal<<"\nplant_"<<i<<"_handle="<<handle
                <<"\nplant_"<<i<<"_vertices="<<landscapeRootMeshes[i].vertexCount
                <<"\nplant_"<<i<<"_triangles="<<landscapeRootMeshes[i].TriangleCount()
                <<"\nplant_"<<i<<"_derived_fingerprint="<<proof.derivedMeshFingerprint
                <<"\nplant_"<<i<<"_source_foot_fingerprint="<<proof.sourceFootFingerprint
                <<"\nplant_"<<i<<"_safe="<<(proof.result==Mistwatch::RootSupportResult::Safe)
                <<"\nplant_"<<i<<"_gap="<<proof.minimumLowerGap<<','<<proof.maximumFootGap
                <<"\nplant_"<<i<<"_foot_domains="<<proof.sourceFootFaces<<','<<proof.sourceFootTriangles<<','
                <<proof.sourceFootSegments<<','<<proof.sourceFootPoints<<','<<proof.sourceRimSegments<<','<<proof.sourceNonmanifoldEdges
                <<"\nplant_"<<i<<"_lower_work="<<proof.lowerFaces<<','<<proof.evaluatedIntersections<<','<<proof.skippedLowerFaces<<'\n';
            out<<"plant_"<<i<<"_pose_words=";for(const auto word:Mistwatch::TerrainPlantingPoseWords(p.pose))out<<word<<',';out<<'\n';
            out<<"plant_"<<i<<"_bounds="<<p.bounds.center.x<<','<<p.bounds.center.y<<','<<p.bounds.center.z<<','<<p.bounds.radius<<','
                <<p.bounds.extents.x<<','<<p.bounds.extents.y<<','<<p.bounds.extents.z<<'\n';
        }
        out<<"unique_root_handles="<<unique.size()<<'\n';
        for(unsigned key=0;key<landscapeFrozen.visibility.size();++key)if(landscapeFrozen.visibility[key])
            out<<"visible_"<<key/4<<'_'<<key%4<<"="<<landscapeFrozen.visibility[key]<<'\n';
        out.close();if(!out)throw std::runtime_error("Landscape receipt write failed");
    }
    MeshResource3D riverMesh;
    float waterTime{};
    std::vector<Vec3> trees;
    std::vector<Vec3> groundCover;
    Mistwatch::ForestCoverResult forestCover;
    Mistwatch::ForestCoverRetentionResult forestCoverRetention;
    std::vector<Vec3> originalCoverTrees;
    Mistwatch::ForestUnderstory originalCoverUnderstory;
    std::vector<BoxCollider3D> originalCoverSolids,originalCoverWood;
    std::array<std::uint64_t,5> grassAssetIdentities{};
    std::uint64_t originalCoverFingerprint{};
    Mistwatch::ForestCoverSettings forestCoverSettings;
    std::array<Mistwatch::ForestCoverAsset,5> grassFootprints;
    std::array<unsigned,5> groundCoverVariants{},submittedGrassVariants{};
    std::uint64_t forestCoverFingerprint{},forestCoverColliderFingerprint{};
    double forestCoverSetupMilliseconds{};
    std::size_t forestCoverClearanceColliders{};
    MeshResource3D grassMesh;
    std::array<MeshResource3D,5> scannedGrass;
    std::array<MeshResource3D,5> curvedScannedGrass;
    std::array<Mistwatch::ForestCoverAsset,5> curvedGrassVolumes;
    std::array<std::uint64_t,5> grassRenderIdentities{};
    bool diagnosticCurvedScannedGrass{};
    unsigned curvedGrassWorldChecks{};
    bool diagnosticNettleMix{};
    std::array<MeshResource3D,6> nettleMeshes;
    std::array<Mistwatch::NettleAssetBounds,6> nettleBounds;
    std::array<std::uint64_t,6> nettleMeshIdentities{};
    MaterialLibrary3D nettleMaterials;
    Material3D nettleSurface;
    Mistwatch::NettleMixResult nettleMix;
    unsigned submittedNettles{};
    std::size_t submittedNettleTriangles{};
    MaterialLibrary3D grassMaterials;
    Material3D grassSurface;
    std::size_t submittedGrassTriangles{};
    MeshResource3D fernMesh;
    std::array<MeshResource3D,4> scannedFerns;
    std::array<Mistwatch::FernColonyAsset,4> fernFootprints;
    Mistwatch::FernColonyResult fernColonies;
    Mistwatch::FernColonySettings fernColonySettings;
    unsigned submittedColonyFerns{};
    std::array<unsigned,4> fernColonyVariants{},submittedColonyVariants{};
    std::array<unsigned,4> submittedOriginalFernVariants{};
    std::size_t submittedColonyFernTriangles{};
    std::uint64_t fernColonyFingerprint{},originalFernFingerprint{};
    double fernColonySetupMilliseconds{};
    MaterialLibrary3D fernMaterials;
    Material3D fernSurface;
    std::size_t submittedFernTriangles{};
    MeshResource3D relayDish;
    Mistwatch::ForestUnderstory understory;
    unsigned submittedFerns{},submittedSaplings{};
    // 所有岩石共用一份 GPU 網格，倒木與修復木梁共用另一份。
    std::array<MeshResource3D,2> propMeshes;
    MaterialLibrary3D boulderMaterials;
    Material3D boulderMaterial;
    std::string boulderDiagnosticProfile{"original"};
    std::string boulderColorProfile{"original"};
    std::string boulderGeometryProfile{"original"};
    std::string boulderBasePath{"materials/slot-0-base.dds"};
    std::string boulderBaseHash{"5a847a3174e09505b3a404c93716cbae576a5cb8bd32f69ec6395e8c95008d36"};
    std::vector<Mistwatch::ForestProp> forestProps;
    MaterialLibrary3D materials;
    MaterialAssetDefinition3D groundDefinition,domainGroundDefinition;
    TerrainDistanceMaterialSettings3D groundDistanceSettings;
    Material3D groundMaterial, barkMaterial, woodMaterial, rockMaterial;
    bool manualForestSurfaceCandidate{};
    bool foliageViewEnabled{};
    bool foliageViewWindowEnabled{};
    struct FoliageViewRequest {RenderObjectIdentity3D identity;float opacity;};
    std::vector<FoliageViewRequest> foliageViewRequests;
    std::uint64_t foliageViewFrames{},foliageViewFadedFrames{},foliageViewColorInstances{},foliageViewShadowCopies{};
    float foliageViewMinimumOpacity{1.f};
    struct FoliageViewPhase {std::string name;std::uint64_t frames{},colors{},shadows{};float minimum{1.f};};
    std::vector<FoliageViewPhase> foliageViewPhases;
    std::filesystem::path manualForestSurfaceReceiptPrefix;
    Mistwatch::ForestSurfaceProfile::Selection forestSurfaceSelection{};
    void WriteForestSurfaceReceipt(const std::filesystem::path& prefix,const RendererStats& stats)const{
        if(prefix.empty()||!prefix.is_absolute())throw std::runtime_error("Forest surface receipt requires absolute prefix");
        auto path=prefix;path+=L".forest-surface.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Forest surface receipt refuses overwrite");
        std::ofstream out(path);if(!out)throw std::runtime_error("Forest surface receipt unavailable");
        const auto& setting=stats.terrainParallaxAppliedSettings;
        out<<std::setprecision(17)<<"schema=mistwatch.forest-surface.1\nmode="
            <<Mistwatch::ForestSurfaceProfile::ModeName(forestSurfaceSelection.mode)
            <<"\nprofile="<<Mistwatch::ForestSurfaceProfile::Name(forestSurfaceSelection.profile)
            <<"\nmanual_cli="<<manualForestSurfaceCandidate
            <<"\nactual_setting_enabled="<<stats.terrainParallaxEnabled
            <<"\nheight_mask_last_bind="<<stats.lastTerrainParallaxHeightLayerMask
            <<"\nheight_material_binds="<<stats.lastTerrainParallaxMaterialBinds
            <<"\nlast_height_semantic="<<stats.lastTerrainParallaxMaterialSemanticIdentity
            <<"\nterrain_semantic="<<TerrainRenderMaterial().textures.semanticIdentity
            <<"\nsubmitted_frame="<<stats.submittedFrames
            <<"\nrender_observed="<<(stats.submittedFrames>0)
            <<"\ndepth_meters="<<setting.depthMeters
            <<"\nsettings_fingerprint="<<FingerprintTerrainParallaxSettings3D(setting)
            <<"\ntexture_scale="<<groundMaterial.textureScale
            <<"\nsoil_layer_scale="<<groundDistanceSettings.soilLayerScale
            <<"\nsoil_effective_scale="<<EffectiveTerrainDecorrelationSettings3D(groundDistanceSettings).soilLayerScale
            <<"\nsource_base_encoding="<<unsigned(groundDefinition.textures.surfaceEncoding)
            <<"\nsource_leaf_encoding="<<unsigned(groundDefinition.textures.additionalLayers[0].surfaceEncoding)
            <<"\ncanonical_base_encoding="<<unsigned(domainGroundDefinition.textures.surfaceEncoding)
            <<"\ncanonical_leaf_encoding="<<unsigned(domainGroundDefinition.textures.additionalLayers[0].surfaceEncoding)
            <<"\ngeometry_displaced=0\nall_draw_masks_recorded=0\nart_accepted=0\n";
        out.close();if(!out)throw std::runtime_error("Forest surface receipt write failed");
    }
    std::vector<BoxCollider3D> solids;
    Mistwatch::OrbitCamera orbit;
    Mistwatch::CameraSettings cameraOptions;
    Mistwatch::Encounter encounter;
    Mistwatch::Evasion evasion;
    Mistwatch::PlayerLocomotion playerLocomotion;
    Mistwatch::PairContact::State playablePairState;
    Mistwatch::FrameInputLatch playableInputs;
    Mistwatch::PlayableFrameScratch playableScratch;
    Mistwatch::PickupIntent pickupIntent;
    enum class PlayableRecoveryCause {None,InitialBodyOverlap,NpcNoLegalYield};
    PlayableRecoveryCause playableRecoveryCause{PlayableRecoveryCause::None};
    bool playableNeedsContactPolicy{},playableCameraOnlyCommitted{};
    std::optional<CharacterDynamicStepStatus3D> playableLastContact;
    std::optional<Mistwatch::PairContact::Status> playableLastPairStatus;
    CharacterDynamicContactState3D playableCommittedContact{};
    enum class PlayableOutcome {None,WorldCommitted,CameraOnly,Held,NeedsContactPolicy,InitialPairRecovery,InputOverflow};
    PlayableOutcome playableOutcome{PlayableOutcome::None};
    Mistwatch::Explorer explorer;
    SkinnedMeshResource3D explorerMesh;
    SkinningWorkspace3D explorerSkinning;
    const std::array<Material3D,5> explorerMaterials=Mistwatch::ExplorerArtMaterials();
    Mistwatch::ExplorerSurface::Submission explorerSurfaceSubmission;
    Mistwatch::Boar boar;
    SkinnedMeshResource3D boarMesh;
    SkinningWorkspace3D boarSkinning;
    Mistwatch::BoarHitReaction::Scratch boarHitScratch;
    bool started{}, paused{};
    bool fieldJournalOpen{};
    bool surveyMapRequested{};
    unsigned trackedRelay{Mistwatch::NoTrackedRelay},previewRelay{Mistwatch::NoTrackedRelay};
    unsigned relayNavigationHeld{};
    Mistwatch::SurveyMap::DrawReceipt surveyMapDraw;
    Mistwatch::GameAudio gameAudio;
    std::filesystem::path audioSettings;
    std::filesystem::path cameraSettings;
    std::string message="ENTER: begin expedition";
    Mistwatch::ActionFeedbackToast actionFeedback;
    Mistwatch::TuningInputRepeat tuningInputRepeat;
    std::filesystem::path save;
    std::filesystem::path capture;
    unsigned captureFrames{};
    unsigned captureSequenceFrames{},captureCompleted{},captureConsumed{};
    bool capturePending{};
    bool diagnosticNoAo{};
    bool diagnosticNoGroundNormal{},diagnosticNoMsaa{};
    unsigned diagnosticHdrStage{};
    bool diagnosticNoShadows{},diagnosticNoBloom{};
    bool diagnosticNoGroundTextures{};
    bool diagnosticNeutralGroundBase{},diagnosticNeutralGroundSurface{};
    bool diagnosticRoadUnitOcclusion{};
    bool diagnosticLegacyGround{};
    bool diagnosticSurfaceGround{};
    bool diagnosticLegacyGrass{};
    bool diagnosticDimGrass{};
    bool diagnosticProceduralGrass{};
    bool diagnosticScatterCover{};
    bool diagnosticLegacyFern{};
    bool diagnosticCrown{};
    bool captureWalk{},captureCrouch{};
    bool captureWildlife{};
    bool captureBoarOverview{};
    bool captureBoulderOverview{};
    bool captureGrassOverview{};
    bool captureSupplyOverview{};
    bool captureGrassDistance{},grassFixtureEnabled{true};
    std::size_t grassFixtureIndex{};
    float grassFixturePlayerDistance{2.f},grassFixtureCameraDistance{12.f};
    Vec3 grassFixtureSelectionFeet{};
    struct GrassFixtureState {
        Vec3 root{},feet{};
        float scale{},yaw{},playerDistance{},cameraDistance{};
        bool selected{},sceneAccepted{};
        unsigned opaque{},shadowOnly{},triangles{};
        std::uint64_t sceneSerial{},cameraHash{};
    } grassFixtureState,grassFixtureFrozen;
    bool captureFernOverview{};
    unsigned submittedGrass{};
    bool captureRelayMotion{};
    bool captureLandscapeMotion{};
    bool captureAudioProbe{};
    unsigned captureAudioTicks{};
    bool captureFocusProbe{},captureFocusComplete{};
    unsigned captureFocusTicks{},captureFocusFrozenTicks{};
    std::string captureFocusSession;
    Vec3 captureFocusFeet{};
    float captureFocusWater{},captureFocusEnemyTime{};
    // 獨立輸入來源只供明確啟用的原生流程診斷；不改玩家狀態、不搶桌面焦點。
    Mistwatch::InputReplay inputReplay;
    std::filesystem::path inputReplayDirectory;
    std::ofstream inputReplayTrace;
    std::string inputReplayPhase;
    bool inputReplayCaptureRequested{},inputReplayCapturePending{},inputReplayRecorded{};
    bool inputReplayLowRoofObserved{};
    bool inputReplayTwigReceiptWritten{};
    bool inputReplayLastDashing{};
    unsigned inputReplayLastDamage{};
    unsigned inputReplayImages{};
    unsigned inputReplayMotionTick{},inputReplayMotionSteps{};
    bool inputReplayMotionAttempted{},inputReplayMotionAccepted{},inputReplayCameraAttempted{},inputReplayCameraRollback{};
    bool inputReplayWestWorldWritten{};
    std::uint64_t inputReplayCaptureBase{};
    std::chrono::steady_clock::time_point inputReplayCaptureStart{};
    bool inputReplayCostEnabled{},captureCostEnabled{},inputReplayCostFailed{};
    std::filesystem::path captureCostDirectory;
    Mistwatch::ReplayCostProbe inputReplayCost;
    void RecordInputReplayCost(Engine& e){
        if((!inputReplayCostEnabled&&!captureCostEnabled)||inputReplayCostFailed||!e.GetFrameCostProbe3D().complete)return;
        if(!inputReplayCost.Append(e.GetFrameCostProbe3D(),e.GetRendererStats())){
            inputReplayCostFailed=true;
            throw std::runtime_error("Input replay frame cost identity/accounting rejected");
        }
    }
    #include "MatureContactMethods.inl"
    bool PrepareInputReplay(Engine& e,float dt) {
        if(inputReplayDirectory.empty())return false;
        const auto stats=e.GetRendererStats();
        if(inputReplayCaptureRequested||inputReplayCapturePending){
            if(stats.rejectedFrameCaptures)throw std::runtime_error("Input replay image rejected");
            if(inputReplayCapturePending&&stats.successfulFrameCaptures>inputReplayCaptureBase){
                if(stats.successfulFrameCaptures!=inputReplayCaptureBase+1||!stats.lastCapturedRenderSerial)
                    throw std::runtime_error("Capture uploaded-view identity mismatch");
                const auto path=inputReplayDirectory/(std::to_string(inputReplayImages)+"-"+inputReplayPhase+".png.uploaded-view.txt");
                std::ofstream view(path);view.precision(9);
                view<<"render_serial="<<stats.lastCapturedRenderSerial<<"\nuploaded_vp_bits=";
                for(float v:stats.lastCapturedViewProjection)view<<std::bit_cast<std::uint32_t>(v)<<',';
                view<<"\nuploaded_unjittered_vp_bits=";
                for(float v:stats.lastCapturedUnjitteredViewProjection)view<<std::bit_cast<std::uint32_t>(v)<<',';
                view<<"\njitter="<<stats.lastCapturedTemporalJitter.x<<','<<stats.lastCapturedTemporalJitter.y<<"\nwindow=";
                for(float v:stats.lastCapturedOcclusionWindow)view<<v<<',';
                view<<'\n';view.close();if(!view)throw std::runtime_error("Capture uploaded-view write failed");
                inputReplayCapturePending=false;++inputReplayImages;
            }else{
                if(std::chrono::steady_clock::now()-inputReplayCaptureStart>std::chrono::seconds(15))
                    throw std::runtime_error("Input replay image timeout");
                return true;
            }
        }
        // Original replay remains authoritative;registered world no longer selects the owner-only fixture.
        if(inputReplay.Finished()){
            if(!inputReplayRecorded){
                std::ofstream out(inputReplayDirectory/"result.txt");
                out<<"passed="<<inputReplay.Passed()<<"\nfailure="<<inputReplay.Failure()
                    <<"\nphase="<<inputReplay.Phase()<<"\nticks="<<inputReplay.Ticks()
                    <<"\nrepair_materials="<<session.repairMaterials<<"\nheight="<<controller.Settings().height
                    <<"\ncollected="<<session.collected<<"\nbridge_repaired="<<session.bridgeRepaired
                    <<"\nhealth="<<session.health<<"\nstage="<<static_cast<unsigned>(session.CurrentStage())
                    <<"\nroute="<<(inputReplay.GroveRoute()?"grove-walk":inputReplay.ResumeSaveRoute()?(inputReplay.ResumeAccept()?"resume-save":"reject-save"):inputReplay.SprintFlowRoute()?"sprint-flow":inputReplay.DashJumpRoute()?"dash-handoff":inputReplay.WestCliffRoute()?(inputReplay.WestCliffTargetX()==-47.f?"west-cliff-47":"west-cliff-44"):inputReplay.PivotCheckpointRoute()?"pivot-save":inputReplay.MotionCheckpointRoute()?"motion-save":inputReplay.CheckpointRoute()?"checkpoint":inputReplay.DeathRetryRoute()?"death-retry":inputReplay.CompletionRoute()?"completion":inputReplay.PowerRoute()?"power":inputReplay.CombatRoute()?"combat":inputReplay.BridgeRoute()?"bridge":"shelter")
                    <<"\nenemy_health="<<encounter.health<<"\nguardian_defeated="<<session.guardianDefeated
                    <<"\ndodges="<<inputReplay.Dodges()<<"\ncounters="<<inputReplay.Counters()
                    <<"\ndamage_events="<<inputReplay.DamageEvents()<<"\nretries="<<inputReplay.Retries()
                    <<"\ncheckpoint_saves="<<inputReplay.SavedCheckpoint()<<"\ncheckpoint_loads="<<inputReplay.LoadedCheckpoint()
                    <<"\nclue_read="<<session.fuseClueRead<<"\nfuse_dials="<<session.fuseDials
                    <<"\nfuse_collected="<<session.fuseCollected<<"\ncircuit="<<session.circuit<<"\npower_restored="<<session.powerRestored
                    <<"\nrelay_linked="<<session.relayLinked<<"\nrelay_sectors="<<session.relaySectors
                    <<"\nfield_tasks="<<session.relayFieldTasks<<"\npatch_a="<<session.relayPatchA<<"\npatch_b="<<session.relayPatchB
                    <<"\nextraction="<<session.extractionRoute<<"\nbeacon_lit="<<session.beaconLit
                    <<"\nfeet="<<session.feet.x<<','<<session.feet.y<<','<<session.feet.z
                    <<"\nimages="<<inputReplayImages<<"\ndebug_errors="<<stats.debugErrorCount
                    <<"\ndebug_warnings="<<stats.debugWarningCount
                    <<"\ndebug_layer_enabled="<<stats.debugLayerEnabled
                    <<"\ngpu_validation_requested="<<stats.gpuValidationRequested
                    <<"\ngpu_validation_executed="<<stats.gpuValidationExecuted
                    <<"\nlow_roof_stand_denied="<<inputReplayLowRoofObserved<<"\nfeedback="<<message
                    <<"\ntrace_timing=before-production-handler\nimage_timing=after-production-handler"
                    <<"\ninput_source=isolated-key-state-production-handlers\nmanual_play=0\n";
                out.close();if(!out)throw std::runtime_error("Input replay result write failed");
                inputReplayTrace.flush();if(!inputReplayTrace)throw std::runtime_error("Input replay trace write failed");
                if(inputReplay.DashJumpRoute()&&!inputReplay.SprintFlowRoute()&&inputReplay.Passed()){
                    // 計數來自正式控制器，圖片時序另列；不以按下空白鍵冒稱連跳成功。
                    std::ofstream proof(inputReplayDirectory/"dash-handoff.tsv");proof.precision(9);
                    proof<<"case\tq_tick\tspace_tick\tfull_jump_tick\tcomplete_tick\tjumps_before\tjumps_after\tlandings_before\tlandings_at_jump\tlandings_after\tremaining_at_space\tpeak_velocity_y\tstart_height\tpeak_height\ttravel\tq_observed\tfull_jump_observed\n";
                    const auto& rows=inputReplay.DashJumpEvidence();
                    for(std::size_t i=0;i<rows.size();++i){const auto& p=rows[i];
                        proof<<i<<'\t'<<p.qTick<<'\t'<<p.spaceTick<<'\t'<<p.fullJumpTick<<'\t'<<p.completeTick<<'\t'
                            <<p.jumpsBefore<<'\t'<<p.jumpsAfter<<'\t'<<p.landingsBefore<<'\t'<<p.landingsAtJump<<'\t'
                            <<p.landingsAfter<<'\t'<<p.remainingAtSpace<<'\t'<<p.peakVelocityY<<'\t'<<p.startHeight<<'\t'
                            <<p.peakHeight<<'\t'<<p.travel<<'\t'<<p.qObserved<<'\t'<<p.fullJumpObserved<<'\n';}
                    proof.close();if(!proof)throw std::runtime_error("Dash handoff evidence write failed");
                    std::ofstream world(inputReplayDirectory/"dash-world.txt");world.precision(9);
                    world<<"profile=ordinary\nterrain_standing_grade="<<controller.Settings().maximumTerrainStandingGrade
                        <<"\nterrain_uphill_grade="<<controller.Settings().maximumTerrainUphillGrade
                        <<"\njump_grace="<<controller.Settings().jumpGraceSeconds<<"\nsolids="<<solids.size()
                        <<"\nroof_hulls="<<RuntimeRoofHulls().size()<<"\ncontroller_jumps="<<controller.Stats().jumps
                        <<"\ncontroller_landings="<<controller.Stats().landings<<'\n';
                    world.close();if(!world)throw std::runtime_error("Dash world observation write failed");
                }
                if(inputReplay.WestCliffRoute()&&inputReplay.Passed()){
                    // 終態真蒙皮鞋底觀測；不是把踝高誤當接地間隙，也不改正式支撐或姿態。
                    std::ofstream contact(inputReplayDirectory/"west-sole-contact.tsv");contact.precision(9);
                    contact<<"side\tvertex\tx\ty\tz\tterrain_y\tsupport_y\tgap\thull_hits\tup_x\tup_y\tup_z\n";
                    const auto world=CreateTransformMatrix(session.feet,{0,explorer.yaw,0},{1,1,1});
                    for(unsigned side=0;side<2;++side){
                        const auto ankleWorld=world*explorer.pose.globalMatrices[11+side*3];
                        const auto up=TransformPoint(ankleWorld,{0,1,0})-TransformPoint(ankleWorld,{});
                        if(explorer.soleOffsets[side].empty())throw std::runtime_error("West sole contour missing");
                        for(std::size_t i=0;i<explorer.soleOffsets[side].size();++i){
                            const auto point=TransformPoint(ankleWorld,explorer.soleOffsets[side][i]);
                            const float ground=terrain.SampleHeight(point.x,point.z);float support=ground;unsigned hits=0;
                            for(const auto& solid:solids){const float top=solid.center.y+solid.extents.y;
                                if(std::abs(point.x-solid.center.x)<=solid.extents.x&&std::abs(point.z-solid.center.z)<=solid.extents.z&&
                                    top<=session.feet.y+.4f&&top>support)support=top;}
                            for(const auto* hull:RuntimeRoofHulls()){
                                ConvexAabbSweep3D::Result hit;
                                if(!hull||!hull->Sweep({point.x,session.feet.y+.4001f,point.z},{.0001f,.0001f,.0001f},{0,-1.5f,0},hit)||
                                    hit.status==ConvexAabbSweepStatus3D::StartedOverlapping)throw std::runtime_error("West sole hull probe invalid");
                                if(hit.status!=ConvexAabbSweepStatus3D::Hit)continue;
                                float normalY=0;
                                for(std::size_t axis=0;axis<hull->AxisCount();++axis)for(bool positive:{false,true}){
                                    if(!(positive?hit.positiveContacts:hit.negativeContacts)[axis])continue;
                                    Vec3 n;if(!hull->Normal(axis,positive,n))throw std::runtime_error("West sole hull normal invalid");
                                    normalY=std::max(normalY,n.y);}
                                if(normalY>=.65f){++hits;support=std::max(support,float(double(session.feet.y+.4001f)-hit.first*1.5)-.0001f);}
                            }
                            if(!std::isfinite(support)||!std::isfinite(point.y))throw std::runtime_error("West sole observation nonfinite");
                            contact<<side<<'\t'<<i<<'\t'<<point.x<<'\t'<<point.y<<'\t'<<point.z<<'\t'<<ground<<'\t'<<support<<'\t'
                                <<point.y-support<<'\t'<<hits<<'\t'<<up.x<<'\t'<<up.y<<'\t'<<up.z<<'\n';
                        }
                    }
                    contact.close();if(!contact)throw std::runtime_error("West sole contact write failed");
                }
                if(inputReplay.ResumeSaveRoute()){
                    std::ofstream resume(inputReplayDirectory/"resume-transaction.txt");
                    resume<<"expected_accept="<<inputReplay.ResumeAccept()<<"\nexact_restore_observed="<<inputReplay.ResumeExactObserved()
                        <<"\nrestored_simulation_advanced="<<inputReplay.ResumeAdvanceObserved()<<"\nrejection_left_state_unchanged="<<inputReplay.ResumeRejectedUnchanged()<<'\n';
                    resume.close();if(!resume)throw std::runtime_error("Resume transaction receipt failed");
                }
                WriteIndexedGameplayResult();
                inputReplayRecorded=true;
            }
            e.Quit();return true;
        }
        // 僅診斷模式讀回真正存檔；驅動只取得唯讀觀測，不能寫入快照或替代正式 F5/F9。
        Mistwatch::SaveGame replayDisk;
        const bool replayDiskExists=(inputReplay.CheckpointRoute()||inputReplay.ResumeSaveRoute())&&std::filesystem::exists(save);
        const bool replayDiskValid=replayDiskExists&&Mistwatch::ReadSaveFile(save,replayDisk);
        const auto replayMotion=CaptureSaveGame();
        const Mistwatch::WestCliffReplayWorld replayWorld{controller,terrain,solids,RuntimeRoofHulls(),
            inputReplayMotionAttempted&&inputReplayMotionAccepted&&inputReplayCameraAttempted&&!inputReplayCameraRollback&&
            inputReplayMotionTick==inputReplay.Ticks()&&inputReplayMotionSteps==2};
        inputReplay.Tick(session,Mistwatch::PlayerCameraMovementYaw(playerCameraMotion.Camera(),orbit.yaw),
            controller.Settings().height,started,paused,dt,&encounter,&evasion,controller.Grounded(),
            replayDiskValid?&replayDisk:nullptr,message,controller.Velocity(),&replayMotion,
            !explorer.footSwings[0].Active()&&!explorer.footSwings[1].Active(),
            cliffReliefEnabled?&replayWorld:nullptr,(inputReplay.DashJumpRoute()||inputReplay.GroveRoute())?&controller:nullptr,replayWorld.motionAccepted);
        if(!inputReplayWestWorldWritten&&
            ((inputReplay.WestCliffRoute()&&inputReplay.Phase().starts_with("west-cliff-"))||
             (inputReplay.GroveRoute()&&inputReplay.Phase().starts_with("grove-")))){
            // 只輸出當下正式世界供診斷，不改動或刪除碰撞體；凸屋頂仍由正式控制器處理。
            const auto prefix=inputReplay.GroveRoute()?"grove-world-":"west-world-";
            std::ofstream boxes(inputReplayDirectory/(std::string(prefix)+"boxes.tsv"));boxes.precision(9);
            boxes<<"index\tx\ty\tz\tex\tey\tez\n";
            for(std::size_t i=0;i<solids.size();++i){const auto& b=solids[i];
                boxes<<i<<'\t'<<b.center.x<<'\t'<<b.center.y<<'\t'<<b.center.z<<'\t'
                    <<b.extents.x<<'\t'<<b.extents.y<<'\t'<<b.extents.z<<'\n';}
            boxes.close();if(!boxes)throw std::runtime_error("West cliff collision snapshot write failed");
            std::ofstream heights(inputReplayDirectory/(std::string(prefix)+"heightfield.tsv"));heights.precision(9);
            heights<<"ix\tiz\tx\ty\tz\n";const auto& settings=terrain.Settings();
            for(unsigned z=0;z<settings.depth;++z)for(unsigned x=0;x<settings.width;++x){
                const auto point=terrain.SamplePosition(x,z);
                heights<<x<<'\t'<<z<<'\t'<<point.x<<'\t'<<point.y<<'\t'<<point.z<<'\n';}
            heights.close();if(!heights)throw std::runtime_error("West cliff terrain snapshot write failed");
            inputReplayWestWorldWritten=true;
        }
        const auto& controls=inputReplay.Controls();
        inputReplayTrace<<inputReplay.Ticks()<<'\t'<<inputReplay.Phase()<<'\t'
            <<session.feet.x<<'\t'<<session.feet.y<<'\t'<<session.feet.z<<'\t'
            <<controller.Settings().height<<'\t'<<session.repairMaterials<<'\t'
            <<lastCameraMotion.accepted<<'\t'<<message<<'\t'<<session.collected<<'\t'
            <<session.bridgeRepaired<<'\t'<<session.health<<'\t';
        for(unsigned key=0;key<256;++key)if(controls.Down(key))inputReplayTrace<<key<<',';
        inputReplayTrace<<'\t'<<static_cast<unsigned>(encounter.phase)<<'\t'<<encounter.phaseTime
            <<'\t'<<encounter.feet.x<<'\t'<<encounter.feet.y<<'\t'<<encounter.feet.z
            <<'\t'<<encounter.strikeTarget.x<<'\t'<<encounter.strikeTarget.z
            <<'\t'<<encounter.health<<'\t'<<encounter.countered<<'\t'<<encounter.playerCooldown
            <<'\t'<<evasion.remaining<<'\t'<<evasion.cooldown<<'\t'<<controller.Grounded()
            <<'\t'<<inputReplayLastDashing<<'\t'<<inputReplayLastDamage
            <<'\t'<<session.fuseClueRead<<'\t'<<session.fuseDials<<'\t'<<session.fuseCollected
            <<'\t'<<session.circuit<<'\t'<<session.powerRestored
            <<'\t'<<session.relayLinked<<'\t'<<session.relaySectors<<'\t'<<session.relayFieldTasks
            <<'\t'<<session.relayPatchA<<'\t'<<session.relayPatchB<<'\t'<<session.extractionRoute<<'\t'<<session.beaconLit
            <<'\t'<<session.RelaySignal(0)<<'\t'<<session.RelaySignal(1)<<'\t'<<session.RelaySignal(2)
            <<'\t'<<Mistwatch::GroundElevation(session.feet.x,session.feet.z)
            <<'\t'<<encounter.strikeTarget.y<<'\t'<<evasion.direction.x<<'\t'<<evasion.direction.y<<'\t'<<evasion.direction.z
            <<'\t'<<session.guardianDefeated<<'\t'<<dt<<'\t'<<explorer.DeathProgress()
            <<'\t'<<TransformPoint(explorer.pose.globalMatrices[0],{}).y
            <<'\t'<<TransformPoint(explorer.pose.globalMatrices[2],{}).y
            <<'\t'<<explorer.DeathSupported()<<'\t'<<explorer.DeathLimited()<<'\t'<<explorer.DeathMinimumClearance();
        for(float gap:explorer.DeathSupportGaps())inputReplayTrace<<'\t'<<gap;
        inputReplayTrace<<'\t'<<replayDiskExists<<'\t'<<replayDiskValid<<'\t';
        const auto writeSnapshot=[&](const Mistwatch::SaveGame& state){
            constexpr char hex[]="0123456789ABCDEF";
            for(unsigned char byte:state.Serialize())inputReplayTrace<<hex[byte>>4]<<hex[byte&15];
        };
        if(inputReplay.CheckpointRoute()||inputReplay.ResumeSaveRoute())writeSnapshot(CaptureSaveGame());
        inputReplayTrace<<'\t';if(replayDiskValid)writeSnapshot(replayDisk);
        inputReplayTrace<<'\t'<<unsigned(lastCameraMotion.status)<<'\t'<<lastCameraMotion.subjectVisible
            <<'\t'<<Mistwatch::LoadPreparationDetail::FinitePose(explorer.model.skeleton,explorer.pose)
            <<'\t'<<Mistwatch::LoadPreparationDetail::FinitePose(boar.model.skeleton,boar.pose)
            <<'\t'<<controller.Velocity().x<<'\t'<<controller.Velocity().y<<'\t'<<controller.Velocity().z<<'\t';
        if(inputReplay.CheckpointRoute()){
            // 不序列化含填補位元的整個物件；逐一寫出受觀測浮點欄位的原始位元。
            const auto scalar=[&](float value){inputReplayTrace<<std::bit_cast<std::uint32_t>(value)<<',';};
            const auto vector=[&](Vec3 value){scalar(value.x);scalar(value.y);scalar(value.z);};
            vector(controller.Position());vector(controller.Velocity());scalar(controller.Settings().height);scalar(waterTime);
            const auto& camera=playerCameraMotion.Camera();vector(camera.position);vector(camera.target);vector(camera.up);
            scalar(camera.projection.verticalFieldOfView);scalar(camera.projection.orthographicHeight);
            scalar(camera.projection.nearPlane);scalar(camera.projection.farPlane);
            inputReplayTrace<<unsigned(camera.projection.mode)<<',';
            for(const auto* pose:{&explorer.pose,&boar.pose}){
                inputReplayTrace<<pose->localTransforms.size()<<','<<pose->globalMatrices.size()<<',';
                for(const auto& joint:pose->localTransforms){
                    vector(joint.translation);vector(joint.rotation);vector(joint.scale);
                    scalar(joint.quaternion.x);scalar(joint.quaternion.y);scalar(joint.quaternion.z);scalar(joint.quaternion.w);
                    inputReplayTrace<<joint.usesQuaternion<<',';
                }
                for(const auto& matrix:pose->globalMatrices)for(const auto& row:matrix.values)for(float value:row)scalar(value);
            }
        }
        // 此列在正式更新前寫出；明確附上前次移動更新身分，不把鏡頭回滾後的舊成功值當成本次通過。
        inputReplayTrace<<'\t'<<terrain.SampleHeight(session.feet.x,session.feet.z)
            <<'\t'<<inputReplayMotionTick<<'\t'<<inputReplayMotionAttempted<<'\t'<<inputReplayMotionAccepted
            <<'\t'<<inputReplayCameraAttempted<<'\t'<<inputReplayCameraRollback<<'\t'<<inputReplayMotionSteps
            <<'\t'<<solids.size()<<'\t'<<RuntimeRoofHulls().size();
        for(unsigned side=0;side<2;++side){
            const auto& swing=explorer.footSwings[side];const auto landing=swing.Landing(),current=explorer.footTargets[side];
            inputReplayTrace<<'\t'<<swing.Active()<<'\t'<<swing.Elapsed()<<'\t'<<swing.Duration()<<'\t'<<swing.LiftHeight()
                <<'\t'<<landing.x<<'\t'<<landing.y<<'\t'<<landing.z<<'\t'<<current.x<<'\t'<<current.y<<'\t'<<current.z
                <<'\t'<<explorer.footFacing[side];
            const auto world=CreateTransformMatrix(session.feet,{0,explorer.yaw,0},{1,1,1});
            for(unsigned joint=9+side*3;joint<12+side*3;++joint){
                const auto point=TransformPoint(world,TransformPoint(explorer.pose.globalMatrices[joint],{}));
                inputReplayTrace<<'\t'<<point.x<<'\t'<<point.y<<'\t'<<point.z;
            }
        }
        inputReplayTrace<<'\n';
        if(inputReplayPhase!=inputReplay.Phase()){
            inputReplayPhase=inputReplay.Phase();inputReplayCaptureRequested=true;
            inputReplayCaptureStart=std::chrono::steady_clock::now();inputReplayTrace.flush();
        }
        return inputReplay.Finished();
    }
    bool captureRiverOverview{};
    bool captureShelterOverview{};
    Camera3D lastRenderedCamera{};
    ThirdPersonFramingResult3D lastPlayerFraming{};
    ThirdPersonCameraMotion3D playerCameraMotion;
    ThirdPersonCameraMotionResult3D lastCameraMotion{};
    unsigned cameraFixedTicks{},captureCameraTicks{};
    float cameraMaximumTickDisplacement{},cameraOcclusionSeconds{};
    std::span<const ConvexAabbSweep3D* const> RuntimeRoofHulls()const noexcept {
        return workshopRoof?std::span<const ConvexAabbSweep3D* const>{workshopRoof->hulls}:std::span<const ConvexAabbSweep3D* const>{};
    }
    std::span<const BoxCollider3D> RuntimeRoofCameraOnly()const noexcept {
        return workshopRoof?std::span<const BoxCollider3D>{workshopRoof->cameraOnly}:std::span<const BoxCollider3D>{};
    }
    std::span<const BoxCollider3D> RuntimeCameraOnly()const noexcept {
        if(workshopRoof)return workshopRoof->combinedCameraOnly;
        return diagnosticLegacyRangerHut?std::span<const BoxCollider3D>{}:std::span<const BoxCollider3D>{rangerCameraRoof};
    }
    static std::uint64_t CameraGeometryFingerprint(std::span<const BoxCollider3D> boxes)noexcept {
        std::uint64_t hash=14695981039346656037ull;
        for(const auto& b:boxes)for(float value:{b.center.x,b.center.y,b.center.z,b.extents.x,b.extents.y,b.extents.z}){
            hash^=std::bit_cast<std::uint32_t>(value);hash*=1099511628211ull;
        }
        return hash;
    }
    std::uint64_t RuntimeBodyGeometryIdentity(bool repaired)const {
        const auto boxes=RuntimeWorldSolids(repaired);
        auto hash=CameraGeometryFingerprint(boxes);
        const auto add=[&](std::uint64_t word){hash^=word;hash*=1099511628211ull;};
        add(boxes.size());add(repaired?1:0);
        add(workshopRoof?workshopRoof->visualFingerprint:0);
        add(workshopRoof?workshopRoof->members.size():0);
        return hash;
    }
    void WriteWorkshopRoofReceipt(std::ostream& out)const {
        out<<"workshop_roof_profile="<<(workshopRoof?Mistwatch::WorkshopRoof::Profile:"original")
            <<"\nworkshop_roof_visual_fingerprint="<<(workshopRoof?workshopRoof->visualFingerprint:0)
            <<"\nworkshop_roof_visual_fingerprint_scope=canonical_collision_source"
            <<"\nworkshop_render_fingerprint="<<workshopFrameFingerprint
            <<"\nworkshop_wood_uv_profile="<<(workshopWoodMetricEnabled?Mistwatch::WorkshopWoodMetric::Profile:"original")
            <<"\nworkshop_wood_uv_meters="<<(workshopWoodMetricEnabled?Mistwatch::WorkshopWoodMetric::MetersPerTile:0)
            <<"\nworkshop_roof_body_members="<<RuntimeRoofHulls().size()
            <<"\nworkshop_roof_camera_boxes="<<RuntimeRoofCameraOnly().size()
            <<"\nworkshop_roof_camera_fingerprint="<<(workshopRoof?workshopRoof->cameraFingerprint:0)
            <<"\nbody_geometry_identity_before="<<RuntimeBodyGeometryIdentity(false)
            <<"\nbody_geometry_identity_after="<<RuntimeBodyGeometryIdentity(true)
            <<"\nbody_geometry_identity_scope=ordered_boxes_plus_exact_roof_mesh_not_terrain"
            <<"\nworkshop_roof_opt_in="<<bool(workshopRoof)<<'\n';
    }
    Mistwatch::PlayableFrameRefs PlayableRefs()noexcept {
        return {controller,playerLocomotion,evasion,session,encounter,explorer,boar,playablePairState,orbit,
            playerCameraMotion,lastPlayerFraming,lastCameraMotion,lastRenderedCamera,tuningInputRepeat,actionFeedback,message,waterTime,
            cameraFixedTicks,captureCameraTicks,cameraMaximumTickDisplacement,cameraOcclusionSeconds,inputReplayLowRoofObserved};
    }
    void ResetPlayablePairRuntime()noexcept {
        pickupIntent.Clear();
        trackedRelay=previewRelay=Mistwatch::NoTrackedRelay;
        playablePairState={};playableInputs.Clear();playableRecoveryCause=PlayableRecoveryCause::None;playableNeedsContactPolicy=false;
        playableCameraOnlyCommitted=false;playableOutcome=PlayableOutcome::None;
        playableLastContact.reset();playableLastPairStatus.reset();
        playableCommittedContact={};
    }
    bool UpdatePlayerCamera(float dt) {return UpdatePlayerCamera(dt,solids);}
    bool UpdatePlayerCamera(float dt,std::span<const BoxCollider3D> cameraSolids) {return UpdateIndexedCamera(dt,cameraSolids);}
    bool UpdatePlayerCameraCore(float dt,std::span<const BoxCollider3D> cameraSolids) {
        const auto extra=RuntimeCameraOnly();
        lastPlayerFraming=Mistwatch::BuildPlayerCameraFraming(session.feet,controller.Settings().height,orbit,cameraSolids,terrain,extra);
        lastCameraMotion=playerCameraMotion.Update(Mistwatch::BuildPlayerCameraMotionRequest(session.feet,
            controller.Settings().height,orbit,playerCameraMotion.Initialized(),cameraSolids,terrain,extra),
            Mistwatch::PlayerCameraSamples(session.feet,controller.Settings().height),cameraSolids,
            Mistwatch::CameraCeilingSolids(),terrain,dt,extra);
        if(!lastCameraMotion.accepted)return false;
        if(lastCameraMotion.status==ThirdPersonCameraMotionStatus3D::ResetCut)
            orbit.yaw=Mistwatch::PlayerCameraMovementYaw(lastCameraMotion.camera,orbit.yaw);
        ++cameraFixedTicks;++captureCameraTicks;
        cameraMaximumTickDisplacement=(std::max)(cameraMaximumTickDisplacement,lastCameraMotion.displacement);
        cameraOcclusionSeconds=lastCameraMotion.subjectVisible?0.f:cameraOcclusionSeconds+dt;
        return true;
    }
    // 世界回滾與完成後檢視共用同一條安全鏡頭交易；失敗不提交任何鏡頭擁有者欄位。
    bool TryUpdateCameraOnly(Vec2 drag,float wheel,float keyboard,float dt,bool* attempted=nullptr,bool recenter=false) {
        const auto previousCamera=playerCameraMotion;const auto previousOrbit=orbit;
        const auto previousFraming=lastPlayerFraming;const auto previousResult=lastCameraMotion;
        const auto previousFixed=cameraFixedTicks,previousCapture=captureCameraTicks;
        const auto previousDisplacement=cameraMaximumTickDisplacement,previousOcclusion=cameraOcclusionSeconds;
        bool accepted=false;
        try{orbit.Update(drag,wheel,keyboard,dt,recenter,explorer.yaw,cameraOptions.Sensitivity(),cameraOptions.invertVertical);
            if(attempted)*attempted=true;accepted=UpdatePlayerCamera(dt);}
        catch(...){accepted=false;}
        if(!accepted){
            playerCameraMotion=previousCamera;orbit=previousOrbit;lastPlayerFraming=previousFraming;lastCameraMotion=previousResult;
            cameraFixedTicks=previousFixed;captureCameraTicks=previousCapture;
            cameraMaximumTickDisplacement=previousDisplacement;cameraOcclusionSeconds=previousOcclusion;
        }
        return accepted;
    }
    bool captureBenchmark{};
    Mistwatch::CaptureGpuBenchmark captureGpuBenchmark;
    std::chrono::steady_clock::time_point captureStart;
    std::uint64_t captureReadyNs{},benchmarkCompleteNs{},captureRequestNs{},captureObservedNs{},captureReceiptEndNs{},captureQuitNs{},captureTimeoutNs{};
    std::uint64_t CaptureElapsedNs()const {
        return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-captureStart).count());
    }
    #include "MistwatchValleyWorld.inl"
    std::vector<BoxCollider3D> RuntimeBaseSolids(bool repaired)const {
        return entranceWorld.applied?entranceWorld.WorldSolids(repaired):
            Mistwatch::WorldSolids(repaired,firLayoutSelector,fallenLogProfile);
    }
    std::vector<BoxCollider3D> RuntimeWorldSolids(bool repaired)const {
        if(woodlandWorld)return woodlandWorld->Flatten(repaired);
        auto result=RuntimeBaseSolids(repaired);
        const auto extras=RuntimeExtraSolids();
        result.insert(result.end(),extras.begin(),extras.end());
        return result;
    }
    // 舊作者基準保留：新增倒木不能重生原喬木、蕨株與完整樹冠。
    std::vector<BoxCollider3D> RetainedAuthoringWorldSolids(bool repaired)const {
        if(entranceWorld.applied)throw std::runtime_error("Retained original author world is only for original-four identity");
        auto result=Mistwatch::WorldSolids(repaired,firLayoutSelector);
        const auto extras=RuntimeExtraSolids();result.insert(result.end(),extras.begin(),extras.end());
        return result;
    }
    std::vector<BoxCollider3D> RuntimeExtraSolids()const {
        auto result=middleLayerSolids;
        result.insert(result.end(),canopySolids.begin(),canopySolids.end());
        result.insert(result.end(),valleyRockSolids.begin(),valleyRockSolids.end());
        result.insert(result.end(),slopeGrove.solids.begin(),slopeGrove.solids.end());
        return result;
    }
    void RollbackWoodlandCanopy(Engine& e)noexcept {
        if(!woodlandCanopyEnabled)return;
        try{
            const auto before=e.GetRendererStats();unsigned released{};
            for(auto& mesh:canopyRootMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++released;}mesh={};}
            for(auto& mesh:canopyMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++released;}mesh={};}
            canopyMaterials.Shutdown();if(released)e.WaitRendererIdle3D();
            const auto after=e.GetRendererStats();
            if(!woodlandStartupCleanup.empty()){
                std::ofstream out(woodlandStartupCleanup);
                out<<"schema=woodland-startup-cleanup-v1\nreleased_references="<<released
                    <<"\nreferences_before="<<before.residentMeshReferences<<"\nreferences_after="<<after.residentMeshReferences
                    <<"\nremaining_owned=0\nmaterials_remaining="<<canopyMaterials.Stats().materials
                    <<"\ncleanup="<<(after.residentMeshReferences+released==before.residentMeshReferences?"success":"failed")<<'\n';
            }
        }catch(...){/* 啟動錯誤仍由原始例外回報；缺失的cleanup證據不能被驗收為成功。 */}
    }
    void WriteWoodlandReceipt(const std::filesystem::path& frame)const {
        if(!woodlandWorld)return;
        auto path=frame;path+=".woodland.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Woodland receipt refuses overwrite");
        // Save的真base+extras另外核對，不能只靠RuntimeWorldSolids傳回同一快照自比。
        for(bool repaired:{false,true}){
            auto manual=RuntimeBaseSolids(repaired);const auto extra=RuntimeExtraSolids();manual.insert(manual.end(),extra.begin(),extra.end());
            const auto sealed=woodlandWorld->Flatten(repaired);
            if(manual.size()!=sealed.size())throw std::runtime_error("Woodland save extras count mismatch");
            for(std::size_t i=0;i<manual.size();++i)if(Mistwatch::WoodlandBridgeWorld::Detail::Words(manual[i])!=
                Mistwatch::WoodlandBridgeWorld::Detail::Words(sealed[i]))throw std::runtime_error("Woodland save extras order mismatch");
        }
        std::ofstream out(path);out<<"schema=woodland-native-candidate-v1\nprofile="<<Mistwatch::WoodlandCanopy::Profile
            <<"\ninput="<<woodlandInputIdentity<<"\noutput="<<woodlandOutputIdentity
            <<"\nauthor="<<entranceWorld.fingerprint<<"\ncanopy="<<canopyPopulation.fingerprint<<"\nfir="<<firLayout.fingerprint
            <<"\nmiddle="<<Mistwatch::MiddleLayerTransformFingerprint(middleLayer.trees)
            <<"\ngrass="<<forestCoverFingerprint<<"\nretention="<<forestCoverRetention.fingerprint<<"\nfern="<<fernColonyFingerprint
            <<"\nshrub="<<shrubCanopy.fingerprint<<"\nbroadleaf="<<roadsideBroadleaf.fingerprint
            <<"\nunderstory="<<entranceUnderstory.fingerprint<<"\nsuppression="<<entranceUnderstory.suppressionFingerprint
            <<"\nsuppressed_grass="<<entranceUnderstory.suppressedGrassIds.size()
            <<"\nsuppressed_broadleaf="<<entranceUnderstory.suppressedBroadleafIds.size()
            <<"\nhabitat_terrain="<<habitatTerrain.fingerprint<<"\nhabitat_landscape="<<habitatLandscape.fingerprint
            <<"\nroad_source="<<entranceRoadReceipt.sourceFingerprint<<"\nroad_forest="<<entranceRoadReceipt.forestFingerprint
            <<"\nroad_road="<<entranceRoadReceipt.roadFingerprint
            <<"\nbody_before="<<Mistwatch::WoodlandBridgeWorld::BodyFingerprint(woodlandWorld->Bodies(false),false)
            <<"\nbody_after="<<Mistwatch::WoodlandBridgeWorld::BodyFingerprint(woodlandWorld->Bodies(true),true)
            <<"\ngrouped_wood="<<woodlandWorld->Wood().Identity().fingerprint<<"\nwood_parts="<<canopySolids.size()
            <<"\nworld_before="<<RuntimeWorldSolids(false).size()<<"\nworld_after="<<RuntimeWorldSolids(true).size()
            <<"\nextra_count="<<RuntimeExtraSolids().size()<<"\noriginal_ferns="<<understory.ferns.size()
            <<"\ngrass_count="<<forestCover.plants.size()<<"\nfern_count="<<fernColonies.plants.size()
            <<"\nshrub_count="<<shrubCanopy.plants.size()<<"\nroot_count="<<canopyPopulation.plants.size()
            <<"\nmetadata_appended="<<woodlandMetadata.appended<<"\nmetadata_face_checks="<<woodlandMetadata.faceChecks
            <<"\nbody_and_save_order_equal=1\nbody_and_save_order_scope=ordered_boxes_only_roof_recorded_separately\nnormal_integrated=0\nart_accepted=0\n"
            <<"entry_mode="<<(!inputReplayDirectory.empty()?"isolated-production-replay":"isolated-capture")
            <<"\ncanopy_submissions="<<submittedCanopy<<"\ndefault_changed=0\n";
        const auto groups=woodlandWorld->Wood().Groups();
        WriteWorkshopRoofReceipt(out);
        for(unsigned i=0;i<canopyPopulation.plants.size();++i){
            if(!canopyRootMeshes[i]||!canopyRootMeshes[i].cpuMesh)throw std::runtime_error("Woodland actual root handle missing");
            Mistwatch::WoodlandCanopy::Detail::Hash h;h.Mesh(*canopyRootMeshes[i].cpuMesh);
            out<<"root_"<<i<<"_mesh="<<h.value<<"\nroot_"<<i<<"_handle="<<canopyRootMeshes[i].handle.value
                <<"\ngroup_"<<i<<"_parts="<<groups[i].size()<<'\n';
        }
        out<<"leaf_profile=branch-anchored-clusters-v1\nleaf_render_vertices="<<canopyMeshes[1].vertexCount
            <<"\nleaf_render_triangles="<<canopyMeshes[1].indexCount/3
            <<"\nleaf_ground_faces="<<leafClusterGround.faceChecks
            <<"\nleaf_ground_exact_overlay="<<leafClusterGround.exactOverlayFaces
            <<"\nleaf_source_triangles="<<canopySource[1].indices.size()/3<<'\n';
        out.close();if(!out)throw std::runtime_error("Woodland actual receipt write failed");
    }
    std::vector<std::byte> ValleyStartupWitness(const Mistwatch::CanopyPopulation* candidate=nullptr)const {
        namespace Bake=Mistwatch::ValleyStartupBake;
        const auto& population=candidate?*candidate:canopyPopulation;
        if(!valleyWorldEnabled||!entranceWorld.applied||population.plants.size()!=8||entranceWorld.props.size()!=33||
            !propMeshes[0].cpuMesh||!fallenLogs.mesh.cpuMesh)throw std::runtime_error("Startup bake requires complete original eight world");
        Bake::Writer w;w.U32(Bake::Version);w.Field(terrain);w.Mesh(canopySource[2]);w.U32(8);
        for(const auto& p:population.plants){w.Mesh(p.rootedTrunk);w.Pose(p.pose);w.Box(p.lower);}
        w.Mesh(*propMeshes[0].cpuMesh);w.Mesh(*fallenLogs.mesh.cpuMesh);w.U32(33);
        for(const auto& p:entranceWorld.props){w.U32(p.mesh);w.Pose(p.transform);w.Box(p.collider);}
        return std::move(w.bytes);
    }
    void BindValleyStartupBake(Engine& e,std::vector<std::byte> witness){
        namespace Bake=Mistwatch::ValleyStartupBake;
        if(valleyLoadedBake){
            if(witness!=valleyBakeWitness)throw std::runtime_error("Startup bake actual input changed after entrance binding");
            return;
        }
        if constexpr(Bake::FileBytes==0||Bake::FileSha256.size()!=64)throw std::runtime_error("Startup bake has no qualified payload seal");
        const PinnedAssetRead3D file(e.ExecutableDirectory()/"assets"/std::filesystem::path(Bake::RelativePath),Bake::FileBytes);
        if(!Mistwatch::ForestIbl::ExactHash(file.Bytes(),Bake::FileSha256))throw std::runtime_error("Startup bake payload integrity mismatch");
        auto loaded=Bake::Decode(file.Bytes(),witness);
        valleyBakeWitness=std::move(witness);valleyLoadedBake=std::move(loaded);
    }
    void InitializeCanopy(Engine& e,const std::function<void(const char*)>& stages={}) {
        const auto stage=[&](const char* name){if(stages)stages(name);};
        if(woodlandCanopyEnabled&&(!Mistwatch::WorldProfileHasWoodCollision(worldProfile)||
            firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood||
            fallenLogProfile!=Mistwatch::FallenLogProfile::ScannedStraightV1||rightForeground||
            deciduousHabitat||outerForestFloor||authoredLandscapePlanting||groundMaterial.materialClass!=MaterialClass3D::Terrain||
            diagnosticScatterCover||diagnosticProceduralGrass||diagnosticLegacyFern||
            shrubTarget!=128||!shrubArcDensity||!roadsideBroadleafEnabled||!entranceUnderstoryEnabled||
            !entranceRoadEnabled||!settlementGroundEnabled))
            throw std::runtime_error("Woodland twelve requires the qualified complete scanned composition");
        if(mixedLandscapeCanopy&&(!woodlandCanopyEnabled||!landscapeStandFill||!landscapeTwigDistanceLod||
            authoredLandscapePlanting||shoulderGroveEnabled||rightForeground||deciduousHabitat||outerForestFloor||rockSegmentEnabled))
            throw std::runtime_error("Mixed canopy requires the isolated complete woodland composition");
        if(landscapeCrownGrowth&&(!woodlandCanopyEnabled||!landscapeStandFill||!landscapeTwigDistanceLod||
            mixedLandscapeCanopy||authoredLandscapePlanting||shoulderGroveEnabled||rightForeground||deciduousHabitat||
            outerForestFloor||rockSegmentEnabled||landscapeCrownRange.label!="original"||captureLandscapeTwigDistance>=0))
            throw std::runtime_error("Crown growth requires the isolated complete original woodland stand");
        const auto canopyRoot=e.ExecutableDirectory()/"assets"/"mistwatch-canopy-preview";
        canopyMaterials.Initialize(e,canopyRoot);
        // 原網格slot為幹/葉/枝，不能套用sidecar幹/枝/葉的陣列序。
        constexpr const char* canopyNames[]{"trunk","leaves","branches"};
        constexpr const char* canopyFiles[]{
            "island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh",
            "island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh",
            "island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh"};
        constexpr const char* canopyHashes[3][3]{
            {"aa3708b589c5f80dddb98022eea49fbb71eb157b9ea7c32a02b3f0111e03c98d","638e6710501bb04d9626294b182246f7e651970b6ef2f791efb941e2fad9aaa4","24eba3fcb9e8d59b088e9cf312bdf25b32445562ce466262aa1d7834cd17bb7a"},
            {"4a1f4e5544d2b91ed3822455f457b93a4d3237019bc8ae6202ce1e0aef9f14c2","792095487b7b49038da96ca9cb9e417a52b29f98eb109d953ef4eedcb82e9013","cc43d9a2635a0f8a6a750e41cc9857600a16d39fe9edc2d7f2403d39790595ee"},
            {"75e25268369704a561432df6f3368734e8b5648fdce184a0a4c2c5863317145b","92e5ae39785dbcefc885588deaafd35a1b45d7e5e47f222320b85cb9ce810f05","1aa3118c6fba3891c31fb2e5c961edd463e2a65084f611158dea4adc3e39dbbb"}};
        for(unsigned slot=0;slot<3;++slot){
            const std::string name=canopyNames[slot];
            MaterialTextureSetDefinition3D textures{std::filesystem::path("materials")/(name+"-base.dds"),
                std::filesystem::path("materials")/(name+"-arm.dds"),1024,
                std::filesystem::path("materials")/(name+"-normal.dds")};
            textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
            for(unsigned channel=0;channel<3;++channel)textures.integrity[channel]={1398276,canopyHashes[slot][channel]};
            Material3D material{{1,1,1,1},{.85f,0,0,1},true,{},1.f,true};
            if(slot==1){textures.alphaMode=MaterialAlphaMode3D::Mask;material.alphaMode=MaterialAlphaMode3D::Mask;
                material.alphaCutoff=.5f;material.doubleSided=true;material.materialClass=MaterialClass3D::Foliage;}
            if(!canopyMaterials.Load({name,textures,material}))throw std::runtime_error("Canopy preview material unavailable");
            canopySurfaces[slot]=*canopyMaterials.Find(name);
            auto loaded=LoadAevryndMeshFile(canopyRoot/"meshes"/canopyFiles[slot]);
            if(!loaded)throw std::runtime_error("Canopy preview mesh unavailable");
            canopyTriangles+=unsigned(loaded.mesh.indices.size()/3);
            canopySource[slot]=loaded.mesh;
            canopyMeshes[slot]=e.CreateMeshResource3D(loaded.mesh);
            if(!canopyMeshes[slot])throw std::runtime_error("Canopy preview upload failed");
        }
        if(canopyTriangles!=606094)throw std::runtime_error("Canopy true primitive count changed");

        stage("canopy_meshes_materials");
        const auto geometry=Mistwatch::MeasureCanopy(canopySource);
        canopyPopulation=Mistwatch::CreateCanopyPopulation(canopySource,geometry,terrain,
            RetainedAuthoringWorldSolids(session.bridgeRepaired),understory,{}, {},fernFootprints,{});
        // V121 真實 CPU 作者證據只更新新布局；舊 forest-canopy 身分保持不變。
        const auto expectedCanopyFingerprint=firLayoutSelector==Mistwatch::FirLayoutSelector::ForestWood?
            6584434718760703877ull:13093722598475582899ull;
        if(!canopyPopulation.reachedBudget||canopyPopulation.plants.size()!=4||
            canopyPopulation.fingerprint!=expectedCanopyFingerprint)
            throw std::runtime_error("Canopy-world author identity changed");
        auto stem=LoadAevryndMeshFile(canopyRoot/"meshes"/"island-tree-01-lod2-trunk-root-stem.aemesh");
        if(!stem||stem.mesh.vertices.size()!=3608||stem.mesh.indices.size()/3!=5330)
            throw std::runtime_error("Derived canopy root stem unavailable");
        // 舊四株作者與舊三種世界原樣；只為 forest-wood 追加四個有限作者錨點。
        if(Mistwatch::WorldProfileHasWoodCollision(worldProfile)&&firLayoutSelector==Mistwatch::FirLayoutSelector::ForestWood){
            // 原四株先按完整原世界驗身分；再一次套同一作者表，最後追加四株。
            if(fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1){
                originalCoverTrees=trees;originalCoverUnderstory=understory;
                originalCoverSolids=Mistwatch::WorldSolids(session.bridgeRepaired,firLayoutSelector,fallenLogProfile);
                originalCoverSolids.insert(originalCoverSolids.end(),middleLayerSolids.begin(),middleLayerSolids.end());
                if(originalCoverTrees.size()!=100||originalCoverUnderstory.saplings.size()!=42||
                    originalCoverSolids.size()!=247u+unsigned(session.bridgeRepaired))
                    throw std::runtime_error("Original scanned forest-cover cohort incomplete");
            }
            const auto retainedProps=Mistwatch::ForestProps(terrain,trees);
            entranceWorld=Mistwatch::CreateEntranceWorldAuthor(terrain,firLayout,retainedProps,understory,
                broadleafTrees,middleLayer,fernFootprints,fallenLogProfile);
            firLayout=entranceWorld.firs;trees=firLayout.roots;forestProps=entranceWorld.props;
            understory=entranceWorld.understory;broadleafTrees=entranceWorld.broadleaf;
            middleLayer=entranceWorld.middle;middleLayerSolids.clear();
            for(const auto& tree:middleLayer.trees)middleLayerSolids.push_back(tree.trunk);
            Mistwatch::EntranceCanopyQualifiedWood qualifiedWood;
            if(valleyBakeEnabled){
                qualifiedWood=[&](const Mistwatch::CanopyPopulation& candidate){
                    auto actual=candidate;
                    for(auto& plant:actual.plants){
                        plant.rootedTrunk=Mistwatch::PlantDerivedRootStem(stem.mesh,geometry,plant.pose,terrain);
                        plant.lower=Mistwatch::ExtendRootCollision(plant.lower,plant.rootedTrunk,plant.pose);
                    }
                    BindValleyStartupBake(e,ValleyStartupWitness(&actual));
                    return std::vector<Mistwatch::CanopyWoodProxy>(valleyLoadedBake->wood.begin()+4,valleyLoadedBake->wood.end());
                };
            }
            canopyPopulation=Mistwatch::AppendEntranceCanopy(canopyPopulation,canopySource,geometry,stem.mesh,terrain,
                RuntimeWorldSolids(session.bridgeRepaired),understory,fernFootprints,entranceCanopy,Mistwatch::ActiveEntryLayout,qualifiedWood);
        }
        stage("canopy_population_and_entrance");
        canopyTriangles=606207;
        std::vector<Mistwatch::CanopyWoodProxy> proxies;
        proxies.reserve(canopyPopulation.plants.size());
        // 先完成原八株CPU短幹與完整包絡；固定配方必須驗同一個作者，不能先上傳替代網格。
        for(std::size_t i=0;i<canopyPopulation.plants.size();++i){
            auto& plant=canopyPopulation.plants[i];
            // 同一 pose、真正短幹與完整木格同時供渲染／角色／鏡頭／後續地被避讓。
            plant.rootedTrunk=Mistwatch::PlantDerivedRootStem(stem.mesh,geometry,plant.pose,terrain);
            plant.lower=Mistwatch::ExtendRootCollision(plant.lower,plant.rootedTrunk,plant.pose);
        }
        if(valleyBakeEnabled||!valleyBakeExportPath.empty()){
            auto actual=ValleyStartupWitness();
            if(valleyBakeEnabled)BindValleyStartupBake(e,std::move(actual));
            else valleyBakeWitness=std::move(actual);
        }
        for(std::size_t i=0;i<canopyPopulation.plants.size();++i){auto& plant=canopyPopulation.plants[i];
            if(Mistwatch::WorldProfileHasWoodCollision(worldProfile)){
                auto proxy=valleyLoadedBake?valleyLoadedBake->wood.at(i):Mistwatch::CreateCanopyWoodProxy(plant.rootedTrunk,canopySource[2],plant.pose);
                if(canopySolids.size()+proxy.parts.size()>9000)throw std::runtime_error("Canopy wood collision author budget exceeded");
                canopySolids.insert(canopySolids.end(),proxy.parts.begin(),proxy.parts.end());
                proxies.push_back(std::move(proxy));
            }else canopySolids.push_back(plant.lower);
        }
        stage("canopy_wood_proxies");
        if(entranceWorld.applied)Mistwatch::ValidateEntranceFernWood(entranceWorld,fernFootprints,canopySolids);
        if(woodlandCanopyEnabled){
            auto authored=Mistwatch::WoodlandCanopy::ApplyRecipe(terrain,entranceWorld,canopyPopulation,
                canopySource,stem.mesh,fernFootprints,Mistwatch::WoodlandCanopy::FixedRecipe());
            if(!authored.complete||authored.outputIdentity!=Mistwatch::WoodlandCanopy::FixedOutputIdentity)
                throw std::runtime_error("Woodland fixed recipe incomplete");
            woodlandInputIdentity=authored.inputIdentity;woodlandOutputIdentity=authored.outputIdentity;
            woodlandMetadata=authored.metadata;
            entranceWorld=std::move(authored.author);canopyPopulation=std::move(authored.canopy);
            firLayout=entranceWorld.firs;trees=firLayout.roots;forestProps=entranceWorld.props;
            understory=entranceWorld.understory;broadleafTrees=entranceWorld.broadleaf;middleLayer=entranceWorld.middle;
            middleLayerSolids.clear();for(const auto& tree:middleLayer.trees)middleLayerSolids.push_back(tree.trunk);
            proxies.assign(authored.proxies.begin(),authored.proxies.end());canopySolids=std::move(authored.wood);
            std::vector<Mistwatch::GroupedCanopyVolume::Group> groups;
            for(const auto& proxy:proxies)groups.emplace_back(proxy.parts);
            const Mistwatch::GroupedCanopyVolume::ValidatedWood wood(groups,Mistwatch::WoodlandBridgeWorld::WoodIdentity);
            woodlandWorld.emplace(entranceWorld.VegetationSolids(false),entranceWorld.VegetationSolids(true),wood);
        }
        if(canopyPopulation.plants.size()>canopyRootMeshes.size())throw std::runtime_error("Canopy root resource storage exceeded");
        for(std::size_t i=0;i<proxies.size();++i){const auto& proxy=proxies[i];
            if(i<4&&!originalCoverTrees.empty())originalCoverWood.insert(originalCoverWood.end(),proxy.parts.begin(),proxy.parts.end());
            canopyWoodSurfaceCells+=proxy.surfaceCells;canopyWoodEnclosedGridCells+=proxy.enclosedGridCells;
            canopyWoodSourceTriangles+=proxy.sourceTriangles;
        }
        try{
            for(std::size_t i=0;i<canopyPopulation.plants.size();++i){
                canopyRootMeshes[i]=e.CreateMeshResource3D(canopyPopulation.plants[i].rootedTrunk);
                if(!canopyRootMeshes[i]||!canopyRootMeshes[i].cpuMesh||
                    canopyRootMeshes[i].vertexCount!=canopyPopulation.plants[i].rootedTrunk.vertices.size()||
                    canopyRootMeshes[i].indexCount!=canopyPopulation.plants[i].rootedTrunk.indices.size()||
                    canopyRootMeshes[i].cpuMesh->indices!=canopyPopulation.plants[i].rootedTrunk.indices||
                    std::memcmp(canopyRootMeshes[i].cpuMesh->vertices.data(),canopyPopulation.plants[i].rootedTrunk.vertices.data(),
                        canopyPopulation.plants[i].rootedTrunk.vertices.size()*sizeof(Vertex3D))!=0)
                    throw std::runtime_error("Rooted canopy actual upload failed");
            }
        }catch(...){for(auto& mesh:canopyRootMeshes){if(mesh)e.DestroyMesh3D(mesh.handle);mesh={};}throw;}
        // 可玩十二株候選使用已實拍的葉簇；碰撞作者仍取完整原始枝幹／包絡。
        // 原八株入口不變。先完成全三角形接地及上傳驗證，才取代唯一共享葉資源。
        if(woodlandCanopyEnabled){
            auto leaves=LoadAevryndMeshFile(e.ExecutableDirectory()/"assets"/"mistwatch-leaf-clusters"/"leaves-v1.aemesh");
            if(!leaves)throw std::runtime_error("Woodland leaf cluster asset unavailable");
            Mistwatch::ValidateLeafCluster(leaves.mesh,canopySource[1]);
            for(std::size_t i=0;i<canopyPopulation.plants.size();++i)
                Mistwatch::EntranceCanopyDetail::CheckGround(leaves.mesh,canopyPopulation.plants[i].pose,terrain,.04f,i,leafClusterGround);
            auto replacement=e.CreateMeshResource3D(leaves.mesh);
            if(!replacement)throw std::runtime_error("Woodland leaf cluster upload failed");
            if(!replacement.cpuMesh||replacement.cpuMesh->indices!=leaves.mesh.indices||
                replacement.cpuMesh->vertices.size()!=leaves.mesh.vertices.size()||
                std::memcmp(replacement.cpuMesh->vertices.data(),leaves.mesh.vertices.data(),leaves.mesh.vertices.size()*sizeof(Vertex3D))!=0){
                e.DestroyMesh3D(replacement.handle);throw std::runtime_error("Woodland leaf cluster upload mismatch");
            }
            e.DestroyMesh3D(canopyMeshes[1].handle);canopyMeshes[1]=std::move(replacement);
        }
        if(rightForeground){
            if(!entranceWorld.applied)throw std::runtime_error("Entrance composition requires canonical complete author");
            std::array<Mesh3D,3> sources;
            for(unsigned i=0;i<3;++i){
                if(!broadleafMeshes[i].cpuMesh)throw std::runtime_error("Entrance composition actual mesh missing");
                sources[i]=*broadleafMeshes[i].cpuMesh;
            }
            auto composed=Mistwatch::CreateRightForegroundComposition(entranceWorld,terrain,sources,fernFootprints,canopySolids);
            compositionOriginalFingerprint=composed.originalFingerprint;compositionPoseFingerprint=composed.finalPoseFingerprint;
            entranceWorld=std::move(composed.author);
            understory=entranceWorld.understory;broadleafTrees=entranceWorld.broadleaf;
        }
        canopyPose=canopyPopulation.plants.front().pose;
        if(valleyWorldEnabled){
            if(canopyPopulation.plants.size()!=8||proxies.size()!=8||woodlandCanopyEnabled)
                throw std::runtime_error("Valley prepared canopy scope");
            valleyPreparedCanopy=canopyPopulation;valleyPreparedWood=proxies;valleyPreparedBranches=canopySource[2];
        }
        stage("canopy_validation_and_upload");
    }
    Vec3 FocusProbeStartFeet() const {
        // 焦點測試不是重疊恢復測試；使用真玩家與敵人體積另留四分之一公尺。
        auto feet=encounter.feet;
        feet.z-=controller.Settings().radius+Mistwatch::EncounterBodyRadius+.25f;
        feet.y=terrain.SampleHeight(feet.x,feet.z);
        return feet;
    }
    void ResetController() {
        controller.Reset(session.feet);
        playerLocomotion.Reset();
        // 讀檔／重試時先採低姿，再嘗試站起，避免載入棚內存檔後頭部穿過屋頂。
        controller.SetHeight(1.4f);
        controller.TrySetHeight(1.8f,solids,RuntimeRoofHulls());
        playerCameraMotion.Reset();
        const bool crouched=controller.Settings().height<1.5f;
        explorer.ResetDeath();
        explorer.ResetLocomotion();
        explorer.Update({},0,crouched);
        if(!explorer.PlaceFeet(session.feet,controller.Grounded(),crouched,terrain,solids,RuntimeRoofHulls()))
            throw std::runtime_error("Reset explorer foot IK failed");
        gameAudio.director.Reset();
    }
    void TickAudio(Engine& e,float dt) {
        Mistwatch::AudioState state;
        state.session=session;state.enemyPhase=encounter.phase;state.enemyHealth=encounter.health;state.enemyFeet=encounter.feet;
        state.active=started&&!paused&&session.Alive();state.grounded=controller.Grounded();state.crouched=controller.Settings().height<1.5f;
        const auto listener=lastRenderedCamera.position;
        const auto right=Mistwatch::AudioListenerRight(lastRenderedCamera);
        gameAudio.Tick(e.Audio(),state,listener,right,dt);
    }
    Mistwatch::SaveGame CaptureSaveGame() const {
        Mistwatch::SaveGame snapshot{session,encounter,evasion};
        // 保存同一固定步完成後的控制器與動作擁有者，不重建或推估飛行速度。
        snapshot.motion=Mistwatch::SavedPlayerMotion{controller.CaptureMotionState(),
            playerLocomotion.CaptureMotionState(),explorer.yaw,explorer.requestedYaw,explorer.phase,
            explorer.locomotionDistance,explorer.locomotionSeconds};
        if(matureIndexedDomain)snapshot.worldIdentity=matureIndexedDomain->Identity();
        else if(valleySaveDomain)snapshot.worldIdentity=valleySaveDomain->Identity();
        return snapshot;
    }
    Mistwatch::LoadPreparationResult PrepareRuntimeCheckpoint(const Mistwatch::SaveGame& snapshot)const {
        if(matureIndexedDomain)return Mistwatch::PrepareIndexedSavedGame(snapshot,{controller,explorer,boar,orbit},terrain,
            RuntimeRoofCameraOnly(),RuntimeRoofHulls(),firLayoutSelector,fallenLogProfile,*matureIndexedDomain);
        return Mistwatch::PrepareSavedGame(snapshot,{controller,explorer,boar,orbit},terrain,RuntimeRoofCameraOnly(),RuntimeExtraSolids(),firLayoutSelector,fallenLogProfile,entranceWorld.applied?&entranceWorld:nullptr,RuntimeRoofHulls(),RuntimeTerrainProfile(),valleySaveDomain?&*valleySaveDomain:nullptr);
    }
    void SaveCheckpoint() {
        if(matureIndexedDomain)++matureSaveHandlerCalls;
        const auto snapshot=CaptureSaveGame();
        // 先確認目前世界可安全還原，避免走出地形後寫下格式合法、實際讀不回的檔案。
        // 預檢只操作候選 CPU 狀態；失敗不寫檔，也不取代先前可用的檢查點。
        const auto prepared=PrepareRuntimeCheckpoint(snapshot);
        if(!prepared||!Mistwatch::ValidatePreparedPairLoad(*prepared.candidate)){
            message="Save position unavailable";
            actionFeedback.Publish(Mistwatch::ActionFeedbackEvent::SavePositionUnavailable);
            return;
        }
        const auto result=Mistwatch::WriteSaveFile(save,snapshot);
        message=result==Mistwatch::SaveFileResult::Saved?"Saved":"Save failed";
        actionFeedback.Publish(result==Mistwatch::SaveFileResult::Saved?
            Mistwatch::ActionFeedbackEvent::Saved:Mistwatch::ActionFeedbackEvent::SaveFailed);
    }
    void SaveAudioOptions() {
        if(audioSettings.empty())return;
        std::error_code error;std::filesystem::create_directories(audioSettings.parent_path(),error);
        auto temporary=audioSettings;temporary+=".tmp";
        std::ofstream out(temporary);out<<gameAudio.options.Serialize();out.close();
        if(!out||!MoveFileExW(temporary.c_str(),audioSettings.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))message="Audio settings save failed";
    }
    void SaveCameraOptions() {
        if(cameraSettings.empty())return;
        if(!Mistwatch::WriteCameraSettingsFile(cameraSettings,cameraOptions))
            message="鏡頭設定寫入失敗；本次設定仍有效。";
    }
    void StopGameAudio(Engine& e) {
        gameAudio.Shutdown(e.Audio());
        const auto stats=e.Audio().GetStats();
        if(stats.residentSounds||stats.residentBytes||stats.activeVoices)throw std::runtime_error("Game audio resources not released");
        if(!capture.empty()) {
            auto receipt=capture;receipt+=".audio-cleanup.txt";
            std::ofstream out(receipt);out<<"sounds=0\nbytes=0\nvoices=0\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Audio cleanup receipt failed");
        }
    }
    void ResetBoarPose() {
        boar.initialized=false;
        if(!boar.Update({},0,encounter.phase,(std::max)(0.f,encounter.phaseTime),{0,0,-1})||
            !boar.PlaceFeet(encounter.feet,terrain))throw std::runtime_error("Boar reset pose failed");
    }
    std::filesystem::path CapturePath()const {
        if(!captureSequenceFrames)return capture;
        const auto ordinal=std::to_wstring(captureCompleted);
        return capture.parent_path()/(capture.stem().wstring()+L"-"+std::wstring(3-ordinal.size(),L'0')+ordinal+L".png");
    }
#include "MistwatchWestCliffSupport.inl"
public:
    void WriteFallenLogReceipt(std::filesystem::path path)const {
        path+=".fallen-log.txt";
        if(std::filesystem::exists(path))throw std::runtime_error("Fallen-log receipt refuses overwrite");
        std::ofstream out(path);
        const bool scanned=fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1;
        out<<"profile="<<(scanned?"scanned-straight-v1":"legacy-cylinder")
            <<"\nsource_sha256="<<Mistwatch::FallenLogAssets::MeshSha
            <<"\nmesh_handle="<<fallenLogs.mesh.handle.value<<"\nmaterial_handle="<<fallenLogs.material.textures.value
            <<"\nvertices="<<fallenLogs.mesh.vertexCount<<"\nindices="<<fallenLogs.mesh.indexCount
            <<"\ntriangles="<<fallenLogs.mesh.TriangleCount()<<"\nmaterials="<<fallenLogs.materials.Stats().materials
            <<"\nuse_uv="<<fallenLogs.material.useUv<<"\ntexture_scale="<<fallenLogs.material.textureScale
            <<"\nsubmitted_instances="<<submittedFallenLogs<<"\nsubmitted_triangles="<<submittedFallenTriangles
            <<"\nretained_beam_handle="<<propMeshes[1].handle.value<<"\ndetail_view="<<captureFallenView<<'\n';
        for(std::size_t index=0;index<forestProps.size();++index)if(forestProps[index].FallenLog()){
            const auto sourceId=entranceWorld.applied?entranceWorld.propSourceIds[index]:unsigned(index);
            out<<"prop_"<<sourceId<<"=";const auto& p=forestProps[index];
            for(float v:{p.transform.position.x,p.transform.position.y,p.transform.position.z,
                p.transform.rotation.x,p.transform.rotation.y,p.transform.rotation.z,
                p.transform.scale.x,p.transform.scale.y,p.transform.scale.z,
                p.collider.center.x,p.collider.center.y,p.collider.center.z,
                p.collider.extents.x,p.collider.extents.y,p.collider.extents.z})
                out<<std::bit_cast<std::uint32_t>(v)<<',';
            out<<'\n';
        }
        out.close();if(!out)throw std::runtime_error("Fallen-log receipt write failed");
    }
    unsigned ReleaseRoadsideBroadleaf(Engine& e) noexcept {
        roadsideBroadleafMaterials.Shutdown();unsigned destroyed{};
        for(auto& mesh:roadsideBroadleafMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++destroyed;}mesh={};}
        return destroyed;
    }
    void WriteSettlementSurface(std::ostream& out) const {
        out<<"settlement_surface_profile="<<(settlementSurfaceEnabled?Mistwatch::SettlementSurface::Profile:"original")
            <<"\nentrance_road_receipt_scope="<<(settlementSurfaceEnabled?"combined-source-before-material-split":"submitted-domain")
            <<"\nsettlement_surface_submitted="<<(settlementSurfaceEnabled?entranceRoadSubmitted:0)
            <<"\nsettlement_surface_material_identity="<<settlementSurfaceMaterial.textures.semanticIdentity
            <<"\nsettlement_surface_material_occlusion="<<settlementSurfaceMaterial.surface.occlusion
            <<"\nsettlement_surface_base_path="<<settlementSurfaceDefinition.textures.baseColorPath.generic_string()
            <<"\nsettlement_surface_orm_path="<<settlementSurfaceDefinition.textures.roughnessPath.generic_string()
            <<"\nsettlement_surface_normal_path="<<settlementSurfaceDefinition.textures.normalPath.generic_string()<<'\n';
        for(unsigned i=0;i<3;++i){
            out<<"settlement_surface_mesh_"<<i<<"_fingerprint="<<settlementSurfaceFingerprints[i]
                <<"\nsettlement_surface_mesh_"<<i<<"_triangles="<<entranceRoadMeshes[i].TriangleCount()
                <<"\nsettlement_surface_channel_"<<i<<"_bytes="<<settlementSurfaceDefinition.textures.integrity[i].expectedBytes
                <<"\nsettlement_surface_channel_"<<i<<"_sha256="<<settlementSurfaceDefinition.textures.integrity[i].expectedSha256<<'\n';
        }
    }
    unsigned ReleaseEntranceRoad(Engine& e) noexcept {
        entranceRoadMaterials.Shutdown();unsigned destroyed{};
        for(auto& mesh:entranceRoadMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++destroyed;}mesh={};}
        return destroyed;
    }
    unsigned ReleaseShelterDetail(Engine& e) {
        unsigned destroyed{};
        for(auto& mesh:shelterDetailMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++destroyed;}mesh={};}
        return destroyed;
    }
    void RollbackShelterDetail(Engine& e) noexcept {
        // GPU 退休或配置也可能失敗；逐件嘗試，不以第二個例外覆蓋原始失敗。
        for(auto& mesh:shelterDetailMeshes)if(mesh){
            try {e.DestroyMesh3D(mesh.handle);mesh={};}
            catch(...) {OutputDebugStringW(L"Mistwatch shelter rollback incomplete; resource handle retained\n");}
        }
    }
public:
    explicit MistwatchGame(bool candidate=false,bool roofCandidate=false,bool shoulderCandidate=false,bool cliffCandidate=false,bool reliefCandidate=false,bool surfaceCandidate=false,bool valleyCandidate=false,bool terraceCandidate=false,bool groveCandidate=false):
        manualValleyCandidate(valleyCandidate||terraceCandidate||groveCandidate),valleyTerraceEnabled(terraceCandidate||groveCandidate),
        manualWorkshopRoofCandidate(roofCandidate),manualWoodlandCandidate(candidate),
        shoulderGroveEnabled(shoulderCandidate),manualCliffWorldCandidate(cliffCandidate),manualCliffReliefCandidate(reliefCandidate),
        manualForestSurfaceCandidate(surfaceCandidate),manualForestGroveCandidate(groveCandidate){}
private:
    void RollbackForestIbl(Engine& e)noexcept{
        if(!forestIblHandle)return;
        try{if(e.ActiveEnvironmentLighting3D()==forestIblHandle)(void)e.ClearEnvironmentLighting3D();}catch(...){}
        try{if(forestIblCreated)(void)e.DestroyEnvironmentLighting3D(forestIblStore,forestIblHandle);}catch(...){}
        try{forestIblStore.Destroy(forestIblHandle);forestIblStore.Trim();}catch(...){}
        forestIblHandle={};forestIblCreated=false;
    }
    void InitializeForestIbl(Engine& e){
        forestIblBaseline=e.GetRendererStats();
        if(!forestIblEnabled)return;
        if(e.ActiveEnvironmentLighting3D()||forestIblBaseline.residentEnvironmentLightingSets)
            throw std::runtime_error("Forest IBL refuses to replace another environment owner");
        const auto root=e.ExecutableDirectory()/"assets";
        std::array<std::unique_ptr<PinnedAssetRead3D>,3> files;
        std::array<std::span<const std::byte>,3> payloads;
        for(unsigned i=0;i<3;++i){const auto& spec=Mistwatch::ForestIbl::Specs[i];
            files[i]=std::make_unique<PinnedAssetRead3D>(root/spec.relativePath,spec.bytes);payloads[i]=files[i]->Bytes();}
        if(!Mistwatch::ForestIbl::ValidatePayloads(payloads))throw std::runtime_error("Forest IBL content identity mismatch");
        forestIblHandle=forestIblStore.Create(Mistwatch::ForestIbl::Definition(root));
        if(!forestIblHandle)throw std::runtime_error("Forest IBL CPU parsing failed");
        if(!e.CreateEnvironmentLighting3D(forestIblStore,forestIblHandle,&forestIblCreate))
            throw std::runtime_error("Forest IBL GPU create failed");
        forestIblCreated=true;
        if(!e.SetEnvironmentLighting3D(forestIblStore,forestIblHandle,&forestIblSet)||
           e.ActiveEnvironmentLighting3D()!=forestIblHandle||!forestIblCreate.rendererAccepted||!forestIblSet.rendererAccepted)
            throw std::runtime_error("Forest IBL GPU binding failed");
        e.WaitRendererIdle3D();const auto stats=e.GetRendererStats();
        if(!stats.environmentLightingActive||stats.residentEnvironmentLightingSets!=forestIblBaseline.residentEnvironmentLightingSets+1)
            throw std::runtime_error("Forest IBL renderer ownership mismatch");
    }
    void ReleaseForestIbl(Engine& e){
        if(!forestIblEnabled)return;
        const auto owned=forestIblHandle.value;EnvironmentLightingGpuReceipt3D clear,destroy,trim;
        if(forestIblHandle){
            if(e.ActiveEnvironmentLighting3D()!=forestIblHandle||!forestIblCreated||
               !e.ClearEnvironmentLighting3D(&clear)||!e.DestroyEnvironmentLighting3D(forestIblStore,forestIblHandle,&destroy))
                throw std::runtime_error("Forest IBL reverse release failed");
            forestIblCreated=false;forestIblStore.Destroy(forestIblHandle);forestIblHandle={};forestIblStore.Trim();
        }
        e.WaitRendererIdle3D();e.TrimEnvironmentLighting3D(&trim);const auto stats=e.GetRendererStats();
        const bool restored=!e.ActiveEnvironmentLighting3D()&&!stats.environmentLightingActive&&
            stats.residentEnvironmentLightingSets==forestIblBaseline.residentEnvironmentLightingSets&&
            stats.residentEnvironmentLightingBytes==forestIblBaseline.residentEnvironmentLightingBytes&&
            stats.residentEnvironmentLightingDescriptors==forestIblBaseline.residentEnvironmentLightingDescriptors&&
            !stats.pendingEnvironmentLightingReleaseBytes&&forestIblStore.MemoryStats().residentReferences==0;
        const auto prefix=!CapturePath().empty()?CapturePath():(!inputReplayDirectory.empty()?inputReplayDirectory/"first-frame":std::filesystem::path{});
        if(!prefix.empty()){
            auto path=prefix;path+=L".forest-ibl-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Forest IBL cleanup refuses overwrite");
            std::ofstream out(path);out<<"schema=mistwatch.forest-ibl-cleanup.1\nenabled="<<forestIblEnabled
                <<"\nowned_handle="<<owned<<"\ncreate_accepted="<<forestIblCreate.rendererAccepted
                <<"\nset_accepted="<<forestIblSet.rendererAccepted<<"\nclear_accepted="<<clear.rendererAccepted
                <<"\ndestroy_accepted="<<destroy.rendererAccepted<<"\nresident_before="<<forestIblBaseline.residentEnvironmentLightingSets
                <<"\nresident_after="<<stats.residentEnvironmentLightingSets<<"\nbytes_before="<<forestIblBaseline.residentEnvironmentLightingBytes
                <<"\nbytes_after="<<stats.residentEnvironmentLightingBytes<<"\ndescriptors_before="<<forestIblBaseline.residentEnvironmentLightingDescriptors
                <<"\ndescriptors_after="<<stats.residentEnvironmentLightingDescriptors<<"\npending_after="<<stats.pendingEnvironmentLightingReleaseBytes
                <<"\nactive_after="<<stats.environmentLightingActive<<"\nremaining_owned="<<forestIblStore.MemoryStats().residentReferences
                <<"\ncleanup="<<(restored?"success":"failed")<<'\n';out.close();if(!out)throw std::runtime_error("Forest IBL cleanup receipt failed");
        }
        if(!restored)throw std::runtime_error("Forest IBL did not restore baseline");
    }
    void OnStart(Engine& e) override {
        std::ofstream startupStages;
        wchar_t startupStagePath[32768]{};
        const auto startupStageLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_STARTUP_STAGES",startupStagePath,32768);
        if(startupStageLength){
            const std::filesystem::path path(startupStagePath);
            if(startupStageLength>=32768||!path.is_absolute()||std::filesystem::exists(path)||
                (!manualValleyCandidate&&!GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_INPUT_REPLAY",nullptr,0)))
                throw std::runtime_error("Startup stage recording requires fresh isolated diagnostic path");
            startupStages.open(path);startupStages<<"stage\tinterval_seconds\ttotal_seconds\n";
            if(!startupStages)throw std::runtime_error("Startup stage recording unavailable");
        }
        const auto startupStageBegin=std::chrono::steady_clock::now();auto startupStagePrevious=startupStageBegin;
        const auto startupStage=[&](const char* stage){
            if(!startupStages.is_open())return;
            const auto now=std::chrono::steady_clock::now();
            startupStages<<stage<<'\t'<<std::chrono::duration<double>(now-startupStagePrevious).count()<<'\t'
                <<std::chrono::duration<double>(now-startupStageBegin).count()<<'\n';startupStages.flush();
            if(!startupStages)throw std::runtime_error("Startup stage recording failed");startupStagePrevious=now;
        };
        const auto rollbackIbl=[&e](MistwatchGame* game)noexcept {game->RollbackForestIbl(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackIbl)> iblStartupGuard(this,rollbackIbl);
        const auto rollbackRock=[&e](MistwatchGame* game)noexcept {game->rockSegment.Release(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackRock)> rockStartupGuard(this,rollbackRock);
        // 新十二株一直持有至OnStart成功；後續草/灌木/讀檔失敗也不能漏回收。
        const auto rollbackWoodland=[&e](MistwatchGame* game)noexcept {game->RollbackWoodlandCanopy(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackWoodland)> woodlandStartupGuard(this,rollbackWoodland);
        // 後續任一初始化失敗也須釋放棚屋資源，不依賴未必執行的 OnStop。
        const auto rollbackShelter=[&e](MistwatchGame* game)noexcept {game->RollbackShelterDetail(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackShelter)> shelterStartupGuard(this,rollbackShelter);
        const auto rollbackRoad=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseEntranceRoad(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackRoad)> roadStartupGuard(this,rollbackRoad);
        // 啟動任一後段失敗都要回收新增群落；Engine 此時不會呼叫 OnStop。
        const auto rollbackRoadside=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseRoadsideBroadleaf(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackRoadside)> roadsideStartupGuard(this,rollbackRoadside);
        // Engine 的啟動失敗不呼叫 OnStop；整個 OnStart 成功前保留本候選的釋放責任。
        const auto rollbackLandscape=[&e](MistwatchGame* game)noexcept {
            game->RollbackRouteGrass(e);
            game->RollbackCoreHerbs(e);
            game->RollbackCrownGrowth(e);
            for(auto& mesh:game->landscapeRootMeshes){if(mesh)e.DestroyMesh3D(mesh.handle);mesh={};}
            if(game->landscapeDetailedFirTwig)e.DestroyMesh3D(game->landscapeDetailedFirTwig.handle);
            game->landscapeDetailedFirTwig={};
            game->landscapeRootAcquisitions=0;
        };
        std::unique_ptr<MistwatchGame,decltype(rollbackLandscape)> landscapeStartupGuard(this,rollbackLandscape);
        const auto rollbackShoulder=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseShoulderRoots(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackShoulder)> shoulderStartupGuard(this,rollbackShoulder);
        const auto rollbackGroundcover=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseShoulderGroundcover(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackGroundcover)> groundcoverStartupGuard(this,rollbackGroundcover);
        const auto rollbackWorkshopRoof=[&e](MistwatchGame* game)noexcept {
            if(!game->workshopRoof)return;
            if(game->workshopMesh)e.DestroyMesh3D(game->workshopMesh.handle);
            game->workshopMesh={};game->workshopRoof.reset();
        };
        std::unique_ptr<MistwatchGame,decltype(rollbackWorkshopRoof)> workshopRoofStartupGuard(this,rollbackWorkshopRoof);
        const auto rollbackValley=[&e](MistwatchGame* game)noexcept {
            try{game->valleyOutcrop.Release(e);}catch(...){}
            for(auto& mesh:game->slopeGrove.roots)if(mesh){try{e.DestroyMesh3D(mesh.handle);mesh={};++game->slopeGrove.released;}catch(...){}}
        };
        std::unique_ptr<MistwatchGame,decltype(rollbackValley)> valleyStartupGuard(this,rollbackValley);
        // 資產載入前決定完整世界版本；截斷或相對截圖路徑不能啟用舊世界。
        wchar_t profileCapture[32768]{};
        const auto profileCaptureLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",profileCapture,32768);
        if(profileCaptureLength>=32768)throw std::runtime_error("Capture path truncated");
        const bool validProfileCapture=profileCaptureLength>0;
        if(validProfileCapture) {
            const std::filesystem::path path(profileCapture);
            if(!path.is_absolute()||!path.has_filename()||path.filename()==L"."||path.filename()==L"..")
                throw std::runtime_error("Capture path must be complete and absolute");
        }
        const auto profileFlag=[](const wchar_t* name) {
            wchar_t text[64]{};
            const auto length=GetEnvironmentVariableW(name,text,64);
            if(length>=64)throw std::runtime_error("World profile flag truncated");
            return std::wstring(text,length);
        };
        // 回放仍是正常操作路線，不能借用capture旗標改變正常美術配置。
        wchar_t profileReplay[32768]{};
        const auto profileReplayLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_INPUT_REPLAY",profileReplay,32768);
        if(profileReplayLength>=32768)throw std::runtime_error("Input replay path truncated");
        const bool validProfileReplay=profileReplayLength>0;
        if(validProfileReplay){const std::filesystem::path path(profileReplay);
            if(!path.is_absolute()||std::filesystem::exists(path))
                throw std::runtime_error("Input replay requires a fresh absolute output directory");}
        const std::array<std::wstring,6> forestDependencies{
            profileFlag(L"AEVRYND_MISTWATCH_VALLEY_WORLD"),profileFlag(L"AEVRYND_MISTWATCH_VALLEY_ROCK_MATERIAL"),
            profileFlag(L"AEVRYND_MISTWATCH_NORTH_WATERSHED"),profileFlag(L"AEVRYND_MISTWATCH_VOLUMETRIC_OUTCROP"),
            profileFlag(L"AEVRYND_MISTWATCH_JOINT_ROCK_BANK"),profileFlag(L"AEVRYND_MISTWATCH_SLOPE_GROVE")};
        forestPlayableWorld=Mistwatch::ForestPlayable::Resolve(manualForestGroveCandidate,validProfileCapture,validProfileReplay,
            profileFlag(L"AEVRYND_MISTWATCH_PLAYABLE_WORLD"),profileFlag(L"AEVRYND_MISTWATCH_INPUT_REPLAY_ROUTE"),forestDependencies);
        groveFloorLitterEnabled=Mistwatch::GroveFloor::Resolve(forestPlayableWorld,validProfileCapture,validProfileReplay,
            profileFlag(L"AEVRYND_MISTWATCH_GROVE_FLOOR"));
        forestSurfaceSelection=Mistwatch::ForestSurfaceProfile::Resolve(manualForestSurfaceCandidate,
            validProfileCapture,validProfileReplay,profileFlag(L"AEVRYND_MISTWATCH_FOREST_SURFACE_PROFILE"));
        foliageViewEnabled=Mistwatch::FoliageView::Resolve(validProfileCapture||validProfileReplay,forestPlayableWorld,
            profileFlag(L"AEVRYND_MISTWATCH_FOLIAGE_VIEW"));
        foliageViewWindowEnabled=Mistwatch::FoliageView::ResolveWindow(validProfileCapture||validProfileReplay,forestPlayableWorld,
            profileFlag(L"AEVRYND_MISTWATCH_FOLIAGE_VIEW"));
        const auto suppliedValleyProfile=profileFlag(L"AEVRYND_MISTWATCH_VALLEY_WORLD");
        const auto valleyProfile=forestPlayableWorld?std::wstring(L"south-valley-authored-terrace-v9"):suppliedValleyProfile;
        if(!valleyProfile.empty()&&valleyProfile!=L"original"&&valleyProfile!=L"south-valley-contact-supports-v7"&&valleyProfile!=L"south-valley-contact-workshop-v8"&&valleyProfile!=L"south-valley-authored-terrace-v9")
            throw std::runtime_error("Unknown valley world profile");
        if(manualValleyCandidate&&(validProfileReplay||validProfileCapture||!suppliedValleyProfile.empty()))
            throw std::runtime_error("Manual valley requires separate normal-input entry without replay/capture overrides");
        valleyTerraceEnabled=valleyTerraceEnabled||valleyProfile==L"south-valley-authored-terrace-v9";
        valleyWorkshopEnabled=manualValleyCandidate||valleyProfile==L"south-valley-contact-workshop-v8"||valleyTerraceEnabled;
        valleyWorldEnabled=valleyWorkshopEnabled||valleyProfile==L"south-valley-contact-supports-v7";
        if(valleyWorldEnabled&&(!validProfileReplay&&!manualValleyCandidate||validProfileCapture))
            throw std::runtime_error("Valley study requires isolated replay or explicit manual candidate");
        const auto valleyReplayRoute=profileFlag(L"AEVRYND_MISTWATCH_INPUT_REPLAY_ROUTE");
        if(valleyWorldEnabled&&!manualValleyCandidate&&valleyReplayRoute!=L"shelter"&&valleyReplayRoute!=L"completion"&&valleyReplayRoute!=L"checkpoint"&&valleyReplayRoute!=L"resume-save"&&valleyReplayRoute!=L"reject-save"&&!(forestPlayableWorld&&valleyReplayRoute==L"grove-walk"))
            throw std::runtime_error("Valley study requires isolated shelter, completion or checkpoint replay");
        const auto bakeProfile=profileFlag(L"AEVRYND_MISTWATCH_VALLEY_STARTUP_BAKE");
        if(!bakeProfile.empty()&&bakeProfile!=L"canopy-support-v1")throw std::runtime_error("Unknown valley startup bake profile");
        valleyBakeEnabled=bakeProfile==L"canopy-support-v1"||(manualForestGroveCandidate&&bakeProfile.empty());
        wchar_t bakeExportBuffer[32768]{};
        const auto bakeExportLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_VALLEY_BAKE_EXPORT",bakeExportBuffer,32768);
        if(bakeExportLength>=32768)throw std::runtime_error("Startup bake export path truncated");
        const std::wstring_view bakeExport(bakeExportBuffer,bakeExportLength);
        if((valleyBakeEnabled||!bakeExport.empty())&&(!valleyTerraceEnabled||validProfileCapture))
            throw std::runtime_error("Startup bake requires explicit v9 valley");
        if(!bakeExport.empty()){
            valleyBakeExportPath=std::filesystem::path(bakeExport);
            if(valleyBakeEnabled||!validProfileReplay||valleyReplayRoute!=L"shelter"||!valleyBakeExportPath.is_absolute()||std::filesystem::exists(valleyBakeExportPath))
                throw std::runtime_error("Startup bake export requires fresh isolated author replay");
        }
        const std::array<std::wstring,11> cliffOverrides{
            profileFlag(L"AEVRYND_MISTWATCH_WOODLAND_PROFILE"),profileFlag(L"AEVRYND_MISTWATCH_WORKSHOP_ROOF"),
            profileFlag(L"AEVRYND_MISTWATCH_SHOULDER_GROVE_PROFILE"),profileFlag(L"AEVRYND_MISTWATCH_SHOULDER_GROUNDCOVER"),
            profileFlag(L"AEVRYND_MISTWATCH_CLIFF_COVERAGE"),profileFlag(L"AEVRYND_MISTWATCH_CLIFF_MATERIAL"),
            profileFlag(L"AEVRYND_MISTWATCH_ROCK_SEGMENT"),profileFlag(L"AEVRYND_MISTWATCH_WORKSHOP_WOOD_UV"),
            profileFlag(L"AEVRYND_MISTWATCH_ENTRANCE_COMPOSITION"),profileFlag(L"AEVRYND_MISTWATCH_TREE_LOD"),
            profileFlag(L"AEVRYND_MISTWATCH_NO_AO")};
        std::array<std::wstring_view,11> cliffOverrideViews;
        for(unsigned i=0;i<cliffOverrides.size();++i)cliffOverrideViews[i]=cliffOverrides[i];
        cliffWorldEnabled=Mistwatch::CliffWorld::Resolve(validProfileCapture,validProfileReplay,manualCliffWorldCandidate,
            profileFlag(L"AEVRYND_MISTWATCH_CLIFF_WORLD"),cliffOverrideViews);
        woodlandCanopyEnabled=Mistwatch::ResolveWoodlandCandidate(validProfileCapture,validProfileReplay,
            manualWoodlandCandidate,cliffWorldEnabled&&!manualCliffWorldCandidate?L"woodland-canopy-author-v2":cliffOverrides[0]);
        const auto shoulderProfile=profileFlag(L"AEVRYND_MISTWATCH_SHOULDER_GROVE_PROFILE");
        if(!shoulderProfile.empty()){
            if((shoulderProfile!=L"original"&&shoulderProfile!=L"shoulder-deformed-root-v1")||
                (!validProfileCapture&&!validProfileReplay))throw std::runtime_error("Invalid isolated shoulder grove profile");
            shoulderGroveEnabled=shoulderProfile==L"shoulder-deformed-root-v1";
        }
        if(shoulderGroveEnabled&&!woodlandCanopyEnabled)throw std::runtime_error("Shoulder grove requires woodland candidate world");
        const auto groundcoverProfile=profileFlag(L"AEVRYND_MISTWATCH_SHOULDER_GROUNDCOVER");
        if(!groundcoverProfile.empty()&&groundcoverProfile!=L"original"&&groundcoverProfile!=L"partial-m3014-78")
            throw std::runtime_error("Invalid shoulder groundcover profile");
        shoulderGroundcoverEnabled=cliffWorldEnabled||groundcoverProfile==L"partial-m3014-78";
        const auto cliffProfile=profileFlag(L"AEVRYND_MISTWATCH_CLIFF_COVERAGE");
        if(!cliffProfile.empty()&&cliffProfile!=L"original"&&cliffProfile!=L"west-cliff-v1")
            throw std::runtime_error("Invalid cliff coverage profile");
        cliffDiagnostic=cliffWorldEnabled||!cliffProfile.empty();cliffCoverageEnabled=cliffWorldEnabled||cliffProfile==L"west-cliff-v1";
        if(cliffDiagnostic&&(!woodlandCanopyEnabled||shoulderGroveEnabled||(!validProfileCapture&&!validProfileReplay&&!cliffWorldEnabled)))
            throw std::runtime_error("Cliff comparison requires isolated original woodland");
        const auto rockProfile=profileFlag(L"AEVRYND_MISTWATCH_ROCK_SEGMENT");
        if(!rockProfile.empty()&&rockProfile!=L"original"&&rockProfile!=L"edge-shear-v1")
            throw std::runtime_error("Invalid rock segment profile");
        rockSegmentDiagnostic=!rockProfile.empty();rockSegmentEnabled=rockProfile==L"edge-shear-v1";
        // 先檢視真網格與材質；尚未有可玩碰撞驗收，不能以環境變數偷偷啟用正常遊戲。
        if(rockSegmentDiagnostic&&(!validProfileCapture||!cliffDiagnostic||cliffCoverageEnabled))
            throw std::runtime_error("Rock segment requires isolated capture and original terrain");
        const auto cliffMaterialProfile=profileFlag(L"AEVRYND_MISTWATCH_CLIFF_MATERIAL");
        if(!cliffMaterialProfile.empty()&&cliffMaterialProfile!=L"original"&&cliffMaterialProfile!=L"rock-face-03-2k")
            throw std::runtime_error("Invalid cliff material profile");
        cliffMaterialDiagnostic=cliffWorldEnabled||!cliffMaterialProfile.empty();cliffMaterialEnabled=cliffWorldEnabled||cliffMaterialProfile==L"rock-face-03-2k";
        // 覆蓋配方可選原版或既有西坡配方；外部對照必須兩邊使用相同覆蓋，僅換三張岩圖。
        if(cliffMaterialDiagnostic&&((!validProfileCapture&&!cliffWorldEnabled)||!cliffDiagnostic||rockSegmentDiagnostic))
            throw std::runtime_error("Cliff material requires isolated terrain capture without rock segment");
        // 谷地換岩層三圖並接既有西坡選材；道路／幾何／碰撞／光照與固定世界資料保持原樣。
        const auto valleyMaterialProfile=forestPlayableWorld?std::wstring(L"rock-face-03-2k"):profileFlag(L"AEVRYND_MISTWATCH_VALLEY_ROCK_MATERIAL");
        if(!valleyMaterialProfile.empty()){
            if((valleyMaterialProfile!=L"original"&&valleyMaterialProfile!=L"rock-face-03-2k")||
                !valleyTerraceEnabled||(!validProfileReplay&&!manualValleyCandidate)||validProfileCapture||
                cliffMaterialDiagnostic||cliffWorldEnabled||rockSegmentDiagnostic)
                throw std::runtime_error("Valley rock material requires explicit isolated v9 world");
            valleyMaterialAudit=true;cliffMaterialEnabled=valleyMaterialProfile==L"rock-face-03-2k";
        }
        const auto northProfile=forestPlayableWorld?std::wstring(L"authored-north-v2"):profileFlag(L"AEVRYND_MISTWATCH_NORTH_WATERSHED");
        if(!northProfile.empty()){
            if(northProfile!=L"authored-north-v2"||!valleyMaterialAudit||!valleyTerraceEnabled||
                (!validProfileReplay&&!manualValleyCandidate))throw std::runtime_error("North watershed requires isolated v9 authoring world");
            valleyNorthWatershed=true;
        }
        const auto outcropProfile=profileFlag(L"AEVRYND_MISTWATCH_VOLUMETRIC_OUTCROP");
        if(!outcropProfile.empty()){
            if(outcropProfile!=L"three-rock-visual-v1"||!validProfileReplay||manualValleyCandidate||
                valleyReplayRoute!=L"shelter"||!valleyMaterialAudit||!cliffMaterialEnabled||!valleyNorthWatershed)
                throw std::runtime_error("Outcrop preview requires isolated shelter route, no manual or save route");
            valleyOutcropPreview=true;
        }
        const auto bankProfile=profileFlag(L"AEVRYND_MISTWATCH_JOINT_ROCK_BANK");
        if(!bankProfile.empty()){
            if(bankProfile!=L"upstream-contact-soil-v1"||!valleyOutcropPreview)
                throw std::runtime_error("Joint rock bank requires isolated outcrop authoring");
            valleyJointRockBank=true;
        }
        const auto reliefProfile=profileFlag(L"AEVRYND_MISTWATCH_CLIFF_RELIEF");
        const auto groveProfile=profileFlag(L"AEVRYND_MISTWATCH_SLOPE_GROVE");
        if(!groveProfile.empty()){
            if(groveProfile!=L"young-canopy-slope-bands-v1"||!valleyJointRockBank)
                throw std::runtime_error("Slope grove requires isolated joint bank world");
            valleySlopeGrove=true;
        }
        if(forestPlayableWorld){valleyOutcropPreview=true;valleyJointRockBank=true;valleySlopeGrove=true;}
        if(!reliefProfile.empty()&&reliefProfile!=L"west-cliff-ledges-v1")throw std::runtime_error("Unknown cliff relief profile");
        cliffReliefEnabled=manualCliffReliefCandidate||!reliefProfile.empty();
        if(cliffReliefEnabled&&((manualCliffWorldCandidate&&!reliefProfile.empty())||
            (!cliffWorldEnabled&&!validProfileCapture)||!cliffMaterialEnabled||!cliffCoverageEnabled||
            rockSegmentDiagnostic||shoulderGroveEnabled))throw std::runtime_error("Conflicting cliff relief context");
        const auto cliffVariationProfile=profileFlag(L"AEVRYND_MISTWATCH_CLIFF_VARIATION");
        if(!cliffVariationProfile.empty()&&cliffVariationProfile!=L"original"&&cliffVariationProfile!=L"rock-dual-domain-v1")
            throw std::runtime_error("Unknown cliff variation profile");
        cliffVariationEnabled=cliffVariationProfile==L"rock-dual-domain-v1";
        if(cliffVariationEnabled&&(!cliffCoverageEnabled||!cliffMaterialEnabled))
            throw std::runtime_error("Cliff variation requires authored cliff material and coverage");
        if(cliffReliefEnabled){
            // 僅獨立西坡候選採用新坡度政策，舊遊玩入口仍保持既有控制器設定。
            auto settings=controller.Settings();settings.maximumTerrainUphillGrade=1.f;settings.maximumTerrainStandingGrade=1.f;
            controller=CharacterController3D(settings);
        }
        if(shoulderGroundcoverEnabled&&(!woodlandCanopyEnabled||shoulderGroveEnabled))
            throw std::runtime_error("Groundcover requires original woodland trees, not shoulder replacements");
        const auto woodlandFault=profileFlag(L"AEVRYND_MISTWATCH_WOODLAND_FAIL_AFTER_CANOPY");
        if(!woodlandFault.empty()){
            if(!validProfileCapture||!woodlandCanopyEnabled||woodlandFault!=L"1")throw std::runtime_error("Invalid isolated woodland startup fault");
            woodlandFailAfterCanopy=true;
        }
        if(woodlandCanopyEnabled&&validProfileCapture){woodlandStartupCleanup=std::filesystem::path(profileCapture);woodlandStartupCleanup+=".woodland-startup-cleanup.txt";}
        // 正常遊玩直接採用已驗收組合；舊環境開關僅在明確的截圖診斷生效。
        const auto playableArt=[&](Mistwatch::PlayableArt::Feature feature,const wchar_t* name) {
            return Mistwatch::PlayableArt::Resolve(validProfileCapture,feature,
                validProfileCapture?profileFlag(name):std::wstring{});
        };
        worldProfile=Mistwatch::ResolveWorldProfile(validProfileCapture,
            validProfileCapture?profileFlag(L"AEVRYND_MISTWATCH_WORLD_PROFILE"):std::wstring{},
            validProfileCapture?profileFlag(L"AEVRYND_MISTWATCH_MIDDLE_LAYER"):std::wstring{});
        firLayoutSelector=Mistwatch::WorldProfileHasWoodCollision(worldProfile)?
            Mistwatch::FirLayoutSelector::ForestWood:Mistwatch::FirLayoutSelector::Legacy;
        // 在任何可能略過InitializeCanopy的舊世界分支之前拒絕不相容組合。
        if(woodlandCanopyEnabled&&firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood)
            throw std::runtime_error("WOODLAND_REQUIRES_FOREST_WOOD");
        const auto compositionProfile=profileFlag(L"AEVRYND_MISTWATCH_ENTRANCE_COMPOSITION");
        if(!compositionProfile.empty()){
            if(compositionProfile!=L"original"&&compositionProfile!=L"entrance-right-foreground-v1")
                throw std::runtime_error("Invalid entrance composition profile");
            rightForeground=compositionProfile==L"entrance-right-foreground-v1";
            if(rightForeground&&firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood)
                throw std::runtime_error("Entrance composition requires forest-wood world");
        }
        const auto plantingProfile=profileFlag(L"AEVRYND_MISTWATCH_LANDSCAPE_PLANTING_PROFILE");
        if(!plantingProfile.empty()){
            if(!validProfileCapture||(plantingProfile!=L"original"&&plantingProfile!=L"authored-source-foot-v1"))
                throw std::runtime_error("Invalid capture-only landscape planting profile");
            authoredLandscapePlanting=plantingProfile==L"authored-source-foot-v1";
            if(authoredLandscapePlanting&&firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood)
                throw std::runtime_error("Authored landscape requires forest-wood world");
        }
        const auto landscapeDetail=profileFlag(L"AEVRYND_MISTWATCH_LANDSCAPE_DETAIL");
        if(!landscapeDetail.empty()){
            if(!validProfileCapture||(landscapeDetail!=L"177-crown"&&landscapeDetail!=L"177-root"))
                throw std::runtime_error("Invalid capture-only landscape detail view");
            captureLandscapeDetail=landscapeDetail==L"177-crown"?0:1;
        }
        const auto twigProfile=playableArt(Mistwatch::PlayableArt::Feature::LandscapeTwig,
            L"AEVRYND_MISTWATCH_LANDSCAPE_TWIG_PROFILE");
        captureLandscapeTwigLod1=twigProfile==L"crown-lod1-source-v1";
        landscapeTwigDistanceLod=twigProfile==L"crown-distance-lod-v1";
        landscapeCrownRange=Mistwatch::LandscapeCrownRange::Resolve(validProfileCapture,
            validProfileCapture?profileFlag(L"AEVRYND_MISTWATCH_LANDSCAPE_CROWN_RANGE"):std::wstring{});
        if(landscapeCrownRange.label!="original"&&!landscapeTwigDistanceLod)
            throw std::runtime_error("Extended crown range requires distance LOD source");
        landscapeStandFill=playableArt(Mistwatch::PlayableArt::Feature::LandscapeStand,
            L"AEVRYND_MISTWATCH_LANDSCAPE_STAND_PROFILE")==L"landscape-stand-fill-v2";
        if(landscapeStandFill&&(!landscapeTwigDistanceLod||authoredLandscapePlanting||
            firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood))
            throw std::runtime_error("Landscape stand requires original forest-wood distance crown");
        mixedLandscapeCanopy=Mistwatch::MixedLandscapeCanopy::Resolve(validProfileCapture,validProfileReplay,
            profileFlag(L"AEVRYND_MISTWATCH_LANDSCAPE_CANOPY_PROFILE"));
        landscapeCrownGrowth=Mistwatch::LandscapeCrownGrowth::Resolve(validProfileCapture,validProfileReplay,
            profileFlag(L"AEVRYND_MISTWATCH_LANDSCAPE_CROWN_GROWTH"));
        coreHerbEnabled=Mistwatch::CoreHerbClusters::Resolve(validProfileCapture,validProfileReplay,
            profileFlag(L"AEVRYND_MISTWATCH_CORE_HERBS"));
        coreHerbLayoutProfile=Mistwatch::CoreHerbRouteLayout::Resolve(validProfileCapture,validProfileReplay,coreHerbEnabled,
            profileFlag(L"AEVRYND_MISTWATCH_CORE_HERB_LAYOUT"));
        routeGrassEnabled=Mistwatch::RouteGrassPlan::Resolve(validProfileCapture,validProfileReplay,coreHerbEnabled,
            profileFlag(L"AEVRYND_MISTWATCH_ROUTE_GRASS"));
        routeGrassCooked=routeGrassEnabled&&profileFlag(L"AEVRYND_MISTWATCH_ROUTE_GRASS")==L"complete-route-cooked-source-foot-v1";
        forestIblEnabled=Mistwatch::ForestIbl::Resolve(validProfileCapture,validProfileReplay,
            profileFlag(L"AEVRYND_MISTWATCH_FOREST_IBL"));
        // 正常遊玩與未覆寫的回放採用已對照的補光；獨立IBL診斷仍保留原始光照。
        forestLightingContrast=Mistwatch::ForestLighting::ResolvePlayable(validProfileCapture,validProfileReplay,
            forestIblEnabled,profileFlag(L"AEVRYND_MISTWATCH_FOREST_LIGHTING"));
        if(mixedLandscapeCanopy&&(!landscapeStandFill||!landscapeTwigDistanceLod||!woodlandCanopyEnabled||
            firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood||authoredLandscapePlanting||shoulderGroveEnabled||
            rightForeground||deciduousHabitat||rockSegmentEnabled))
            throw std::runtime_error("Mixed canopy requires an isolated complete forest-wood stand");
        if(shoulderGroveEnabled&&(!landscapeStandFill||!landscapeTwigDistanceLod||authoredLandscapePlanting||
            firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood))
            throw std::runtime_error("Shoulder grove requires complete original 616 forest-wood distance crown");
        const auto outerFloorProfile=profileFlag(L"AEVRYND_MISTWATCH_OUTER_FLOOR_PROFILE");
        if(!outerFloorProfile.empty()){
            if(!validProfileCapture||(outerFloorProfile!=L"original"&&outerFloorProfile!=L"outer-tree-habitat-v1"))
                throw std::runtime_error("Invalid capture-only outer forest floor profile");
            outerForestFloor=outerFloorProfile==L"outer-tree-habitat-v1";
            if(outerForestFloor&&!landscapeStandFill)
                throw std::runtime_error("Outer forest floor requires actual complete landscape stand");
        }
        if(mixedLandscapeCanopy&&outerForestFloor)
            throw std::runtime_error("Mixed canopy does not support the outer floor diagnostic");
        if(validProfileCapture){
            const auto distanceView=profileFlag(L"AEVRYND_MISTWATCH_LANDSCAPE_TWIG_DISTANCE");
            if(!distanceView.empty()){
                if(landscapeCrownGrowth)throw std::runtime_error("Crown growth does not use the original twig distance fixture");
                if(mixedLandscapeCanopy)throw std::runtime_error("Mixed canopy does not use the replaced fir twig distance fixture");
                if(captureLandscapeDetail!=0)throw std::runtime_error("Twig distance requires ordinal177 crown capture");
                if(distanceView==L"near")captureLandscapeTwigDistance=44.f;
                else if(distanceView==L"transition-start")captureLandscapeTwigDistance=54.f;
                else if(distanceView==L"transition-mid")captureLandscapeTwigDistance=60.f;
                else if(distanceView==L"transition-end")captureLandscapeTwigDistance=66.f;
                else if(distanceView==L"far")captureLandscapeTwigDistance=76.f;
                else throw std::runtime_error("Invalid capture-only twig distance view");
            }
        }
        // 隔離正常入口候選：完整 ForestWood 使用共享掃描配置；正式工作樹不變。
        if(firLayoutSelector==Mistwatch::FirLayoutSelector::ForestWood)
            fallenLogProfile=Mistwatch::FallenLogProfile::ScannedStraightV1;
        if(validProfileCapture){
            const auto flag=profileFlag(L"AEVRYND_MISTWATCH_FALLEN_LOGS");
            if(!flag.empty()&&flag!=L"0"&&flag!=L"1")throw std::runtime_error("Invalid fallen-log capture profile");
            if(flag==L"0")fallenLogProfile=Mistwatch::FallenLogProfile::LegacyCylinder;
            if(flag==L"1"){
                if(firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood)
                    throw std::runtime_error("Scanned fallen logs require forest-wood profile");
                fallenLogProfile=Mistwatch::FallenLogProfile::ScannedStraightV1;
            }
        }
        if(validProfileCapture){
            const auto view=profileFlag(L"AEVRYND_MISTWATCH_FALLEN_VIEW");
            if(!view.empty()){
                if(firLayoutSelector!=Mistwatch::FirLayoutSelector::ForestWood)
                    throw std::runtime_error("Fallen-log detail requires forest-wood");
                if(view==L"0")captureFallenView=0;else if(view==L"8")captureFallenView=8;
                else if(view==L"34")captureFallenView=34;else if(view==L"35")captureFallenView=35;
                else throw std::runtime_error("Invalid fallen-log detail id");
            }
        }
        middleLayerEnabled=Mistwatch::WorldProfileHasMiddle(worldProfile);
        canopyEnabled=Mistwatch::WorldProfileHasCanopy(worldProfile);
        if(!canopyEnabled)canopyPopulation.fingerprint=0;
        const auto wildlife=LoadSkinnedModelAssetFile(e.ExecutableDirectory()/"assets"/"mistwatch-wildlife"/"boar.aeskin");
        if(!wildlife||wildlife.model.skeleton.joints.size()!=16||
            wildlife.model.bindMesh.indices!=boar.model.bindMesh.indices)throw std::runtime_error("Boar asset unavailable or incompatible");
        boar.model=wildlife.model;
        boarMesh=e.CreateSkinnedMeshResource3D(boar.model);
        if(!boarMesh)throw std::runtime_error("Boar GPU upload failed");
        Mistwatch::ExplorerNapeHairBinding::Apply(explorer);
        Mistwatch::BackpackFit1250::Apply(explorer, {+[](void*) {}, nullptr});
        if(!explorer.model.Valid())throw std::runtime_error("Explorer model invalid");
        explorerMesh=e.CreateSkinnedMeshResource3D(explorer.model);
        if(!explorerMesh)throw std::runtime_error("Explorer GPU upload failed");
        box=e.CreateMeshResource3D(Mesh3D::CreateUnitBox());
        wchar_t toolsCapture[2]{},toolsMode[3]{};
        const auto toolsLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_REPAIR_TOOLS",toolsMode,3);
        if(toolsLength&&(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",toolsCapture,2)==0||
            toolsLength!=1||(toolsMode[0]!=L'0'&&toolsMode[0]!=L'1')))throw std::runtime_error("Invalid capture-only repair tools flag");
        if(toolsLength)repairToolsDetailed=toolsMode[0]==L'1';
        if(repairToolsDetailed){
            const auto tools=Mistwatch::CreateRepairToolsVisual();
            repairToolsMeshes[0]=e.CreateMeshResource3D(tools.metal);
            repairToolsMeshes[1]=e.CreateMeshResource3D(tools.handle);
            if(!repairToolsMeshes[0]||!repairToolsMeshes[1]){
                for(auto& mesh:repairToolsMeshes){if(mesh)e.DestroyMesh3D(mesh.handle);mesh={};}
                throw std::runtime_error("Repair tools actual upload failed");}
        }
        scene.SetBoxMesh(box);
        const auto supplyGeometry=Mistwatch::CreateSupplyCrateBoardUv();
        supplyMeshes[0]=e.CreateMeshResource3D(supplyGeometry.wood);
        supplyMeshes[1]=e.CreateMeshResource3D(supplyGeometry.metal);
        if(!supplyMeshes[0]||!supplyMeshes[1])throw std::runtime_error("Supply crate GPU upload failed");
        const auto bridgeWorksiteGeometry=Mistwatch::CreateBridgeWorksite();
        bridgeWorksiteMeshes[0]=e.CreateMeshResource3D(bridgeWorksiteGeometry.wood);
        bridgeWorksiteMeshes[1]=e.CreateMeshResource3D(bridgeWorksiteGeometry.metal);
        if(!bridgeWorksiteMeshes[0]||!bridgeWorksiteMeshes[1])
            throw std::runtime_error("Bridge worksite GPU upload failed");
        const auto generatorDetail=Mistwatch::CreateGeneratorDetail();
        try {
            for(std::size_t group=0;group<generatorDetailMeshes.size();++group) {
                generatorDetailMeshes[group]=e.CreateMeshResource3D(generatorDetail.meshes[group]);
                if(!generatorDetailMeshes[group])throw std::runtime_error("Generator detail GPU upload failed");
            }
        } catch(...) {
            for(auto& mesh:generatorDetailMeshes){if(mesh)e.DestroyMesh3D(mesh.handle);mesh={};}
            throw;
        }
        wchar_t hutCapture[2]{},legacyHut[3]{};
        diagnosticLegacyRangerHut=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",hutCapture,2)>0&&
            GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_LEGACY_RANGER_HUT",legacyHut,3)==1&&legacyHut[0]==L'1';
        rangerHutMesh=e.CreateMeshResource3D(diagnosticLegacyRangerHut?Mistwatch::CreateLegacyRangerHut():Mistwatch::RangerHutLining::CreateRangerHutLining());
        if(!rangerHutMesh)throw std::runtime_error("Ranger hut GPU upload failed");
        wchar_t shelterCapture[2]{},shelterMode[2]{};
        if(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",shelterCapture,2)>0&&
           GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_PLANKED_SHELTER",shelterMode,2)==1&&shelterMode[0]==L'0')plankedShelter=false;
        // 互動版使用已實拍驗證的直壁UV，僅capture允許原UV對照。
        wchar_t boardUvMode[3]{};
        const auto boardUvLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_SHELTER_BOARD_UV",boardUvMode,3);
        if(boardUvLength&&(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",shelterCapture,2)==0||
            boardUvLength!=1||(boardUvMode[0]!=L'0'&&boardUvMode[0]!=L'1')))
            throw std::runtime_error("Invalid capture-only board UV flag");
        if(boardUvLength)boardUvAligned=boardUvMode[0]==L'1';
        // 舊屋頂僅供同來源實拍比較；正式遊戲預設使用獨立屋頂紋理投影。
        wchar_t roofUvMode[3]{};
        const auto roofUvLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_SHELTER_ROOF_UV",roofUvMode,3);
        if(roofUvLength&&(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",shelterCapture,2)==0||
            roofUvLength!=1||(roofUvMode[0]!=L'0'&&roofUvMode[0]!=L'1')||!plankedShelter||!boardUvAligned))
            throw std::runtime_error("Invalid capture-only shelter roof UV flag");
        if(roofUvLength)roofUvAligned=roofUvMode[0]==L'1';
        // 已通過同鏡頭美術比較的四帶木紋成為試玩版預設；舊診斷幾何仍保持原路徑。
        shelterWeatheredUv=plankedShelter&&boardUvAligned&&roofUvAligned;
        wchar_t shelterWoodMode[32]{};
        const auto shelterWoodLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_SHELTER_WOOD",shelterWoodMode,32);
        if(shelterWoodLength){
            if(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",shelterCapture,2)==0||shelterWoodLength>=32||
                (std::wstring_view(shelterWoodMode)!=L"original"&&std::wstring_view(shelterWoodMode)!=L"weathered-bands-v1"))
                throw std::runtime_error("Invalid capture-only shelter wood profile");
            shelterWeatheredUv=std::wstring_view(shelterWoodMode)==L"weathered-bands-v1";
            if(shelterWeatheredUv&&(!plankedShelter||!boardUvAligned||!roofUvAligned))
                throw std::runtime_error("Weathered shelter requires original complete planked UV source");
        }
        const auto shelterDetailProfile=playableArt(Mistwatch::PlayableArt::Feature::Shelter,L"AEVRYND_MISTWATCH_SHELTER_DETAIL");
        if(!shelterDetailProfile.empty()&&shelterDetailProfile!=L"original"&&shelterDetailProfile!=L"framed-corrugated-v1")
            throw std::runtime_error("Invalid shelter detail profile");
        shelterDetailEnabled=shelterDetailProfile==L"framed-corrugated-v1";
        if(shelterDetailEnabled&&(!plankedShelter||!shelterWeatheredUv||!boardUvAligned||!roofUvAligned))
            throw std::runtime_error("Shelter detail requires complete weathered source");
        if(plankedShelter){
            const auto shelter=shelterWeatheredUv?Mistwatch::ShelterWeatheredUv::Create():
                (boardUvAligned?(roofUvAligned?Mistwatch::ShelterRoofUv::CreatePlankedRepairShelterRoofUv():
                    Mistwatch::ShelterBoardUv::CreatePlankedRepairShelterBoardUv()):Mistwatch::CreatePlankedRepairShelter());
            shelterUvFingerprint=Mistwatch::ShelterWeatheredUv::Fingerprint(shelter);
            plankedShelterMesh=e.CreateMeshResource3D(shelter);
            if(!plankedShelterMesh||plankedShelterMesh.vertexCount!=Mistwatch::ShelterVisualVertices||
               plankedShelterMesh.indexCount!=Mistwatch::ShelterVisualIndices)throw std::runtime_error("Planked shelter actual upload mismatch");}
        if(shelterDetailEnabled){
            const auto detail=Mistwatch::ShelterDetail::Create();
            shelterDetailFingerprints={detail.woodFingerprint,detail.roofFingerprint};
            shelterDetailFrameMembers=detail.frameMembers;shelterDetailCorrugations=detail.corrugations;
            try {
                shelterDetailMeshes[0]=e.CreateMeshResource3D(detail.wood);
                shelterDetailMeshes[1]=e.CreateMeshResource3D(detail.roof);
                for(unsigned i=0;i<2;++i)if(!shelterDetailMeshes[i]||!shelterDetailMeshes[i].cpuMesh||
                    Mistwatch::ShelterDetail::Fingerprint(*shelterDetailMeshes[i].cpuMesh)!=shelterDetailFingerprints[i])
                    throw std::runtime_error("Shelter detail actual upload mismatch");
            } catch(...) {
                RollbackShelterDetail(e);
                throw;
            }
        }
        // 已通過同鏡頭局部美術比較；舊箱體僅供明確的capture對照。
        wchar_t workshopMode[3]{};
        const auto workshopLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_PLANKED_WORKSHOP",workshopMode,3);
        if(workshopLength){
            if(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",shelterCapture,2)==0||
                workshopLength!=1||(workshopMode[0]!=L'0'&&workshopMode[0]!=L'1'))
                throw std::runtime_error("Invalid capture-only workshop flag");
            plankedWorkshop=workshopMode[0]==L'1';
        }
        const auto workshopFrameProfile=playableArt(Mistwatch::PlayableArt::Feature::Workshop,L"AEVRYND_MISTWATCH_WORKSHOP_FRAME");
        if(!workshopFrameProfile.empty()&&workshopFrameProfile!=L"original"&&workshopFrameProfile!=L"framed-workshop-v1")
            throw std::runtime_error("Invalid workshop frame profile");
        workshopFrameEnabled=workshopFrameProfile==L"framed-workshop-v1";
        if(workshopFrameEnabled&&!plankedWorkshop)throw std::runtime_error("Workshop frame requires original planked source");
        const auto roofProfile=profileFlag(L"AEVRYND_MISTWATCH_WORKSHOP_ROOF");
        if(!roofProfile.empty()&&roofProfile!=L"original"&&roofProfile!=L"workshop-gabled-roof-candidate-v1")
            throw std::runtime_error("Invalid workshop roof profile");
        if(manualWorkshopRoofCandidate&&roofProfile==L"original")throw std::runtime_error("Conflicting workshop roof candidate selection");
        if(valleyWorkshopEnabled&&roofProfile==L"original")throw std::runtime_error("Valley workshop entry cannot disable its roof");
        const bool roofRequested=valleyWorkshopEnabled||cliffWorldEnabled||manualWorkshopRoofCandidate||roofProfile==L"workshop-gabled-roof-candidate-v1";
        const auto woodUvProfile=profileFlag(L"AEVRYND_MISTWATCH_WORKSHOP_WOOD_UV");
        if(!woodUvProfile.empty()&&woodUvProfile!=L"original"&&woodUvProfile!=L"workshop-face-metric-wood-candidate-v2")
            throw std::runtime_error("Invalid workshop wood UV profile");
        workshopWoodMetricEnabled=woodUvProfile==L"workshop-face-metric-wood-candidate-v2"||
            (woodUvProfile.empty()&&roofRequested&&!validProfileCapture);
        if(workshopWoodMetricEnabled&&!roofRequested)throw std::runtime_error("Workshop metric wood requires qualified roof");
        if(roofRequested&&!workshopFrameEnabled)throw std::runtime_error("Workshop roof requires framed workshop");
        // 存檔預檢固定使用新版巡守屋相機頂棚；不能混入不同碰撞世界的舊屋診斷。
        if(roofRequested&&diagnosticLegacyRangerHut)throw std::runtime_error("Workshop roof does not support legacy ranger-hut diagnostic");
        if(plankedWorkshop){
            Mesh3D workshop;
            if(roofRequested){
                auto candidate=std::make_unique<WorkshopRoofRuntime>();
                workshop=Mistwatch::WorkshopRoof::Create(&candidate->parts);
                if(!Mistwatch::WorkshopRoofCollision::Build(workshop,candidate->parts,candidate->members)||
                   !Mistwatch::BuildWorkshopRoofCamera(workshop,candidate->parts,candidate->cameraOnly))
                    throw std::runtime_error("Workshop roof body/camera preparation failed");
                candidate->visualFingerprint=Mistwatch::WorkshopFrame::Fingerprint(workshop);
                candidate->cameraFingerprint=CameraGeometryFingerprint(candidate->cameraOnly);
                if(candidate->members.size()!=75||candidate->cameraOnly.size()!=954||
                   candidate->visualFingerprint!=358593194902208623ull||candidate->cameraFingerprint!=15601988987549546475ull)
                    throw std::runtime_error("Workshop roof qualified geometry differs");
                candidate->hulls.reserve(candidate->members.size());
                for(const auto& member:candidate->members)candidate->hulls.push_back(&member.hull);
                if(!diagnosticLegacyRangerHut)candidate->combinedCameraOnly.push_back(Mistwatch::RangerHutCameraRoofSolid());
                candidate->combinedCameraOnly.insert(candidate->combinedCameraOnly.end(),candidate->cameraOnly.begin(),candidate->cameraOnly.end());
                // 碰撞與存檔身分仍使用已驗證的作者原網格；僅替換實際繪製的 UV。
                if(workshopWoodMetricEnabled){
                    auto mapped=Mistwatch::WorkshopWoodMetric::Create(workshop,candidate->parts);
                    if(!Mistwatch::WorkshopWoodMetric::Validate(workshop,candidate->parts,mapped)||
                       !Mistwatch::WorkshopWoodMetric::SameGeometry(workshop,mapped.mesh))
                        throw std::runtime_error("Workshop metric wood changed qualified geometry");
                    workshop=std::move(mapped.mesh);
                }
                workshopRoof=std::move(candidate);
            }else workshop=workshopFrameEnabled?Mistwatch::WorkshopFrame::Create():Mistwatch::WorkshopVisual::Create();
            workshopUvFingerprint=Mistwatch::WorkshopVisual::Fingerprint(workshop);
            workshopFrameFingerprint=workshopFrameEnabled?Mistwatch::WorkshopFrame::Fingerprint(workshop):0;
            workshopMesh=e.CreateMeshResource3D(workshop);
            if(!workshopMesh||!workshopMesh.cpuMesh||workshopMesh.vertexCount!=workshop.vertices.size()||workshopMesh.indexCount!=workshop.indices.size()||
                (workshopFrameEnabled&&Mistwatch::WorkshopFrame::Fingerprint(*workshopMesh.cpuMesh)!=workshopFrameFingerprint))
                throw std::runtime_error("Workshop actual upload mismatch");
        }
        rangerDeskMesh=e.CreateMeshResource3D(Mistwatch::CreateRangerDesk());
        if(!rangerDeskMesh||rangerDeskMesh.vertexCount!=216||rangerDeskMesh.indexCount!=324||rangerDeskMesh.TriangleCount()!=108)
            throw std::runtime_error("Ranger desk actual GPU resource differs from qualified mesh");
        startupStage("profiles_and_structures");
        materials.Initialize(e,e.ExecutableDirectory()/"assets"/"mistwatch-materials");
        wchar_t groundCapture[2]{},legacyGround[3]{};
        const auto roadProfile=playableArt(Mistwatch::PlayableArt::Feature::Road,L"AEVRYND_MISTWATCH_ENTRANCE_ROAD");
        if(!roadProfile.empty()&&roadProfile!=L"original"&&roadProfile!=L"entrance-road-material-domain-v1")
            throw std::runtime_error("Invalid entrance road material profile");
        entranceRoadEnabled=roadProfile==L"entrance-road-material-domain-v1";
        const auto settlementRequested=validProfileCapture?profileFlag(L"AEVRYND_MISTWATCH_SETTLEMENT_GROUND"):std::wstring{};
        const auto settlementProfile=Mistwatch::SettlementGround::ResolveProfile(validProfileCapture,settlementRequested);
        settlementGroundEnabled=settlementProfile==L"workshop-ground-domain-v1";
        const auto surfaceRequested=validProfileCapture?profileFlag(L"AEVRYND_MISTWATCH_SETTLEMENT_SURFACE"):std::wstring{};
        if(!surfaceRequested.empty()&&surfaceRequested!=L"original"&&surfaceRequested!=L"workshop-gravel-surface-candidate-v1")
            throw std::runtime_error("Invalid settlement surface profile");
        settlementSurfaceEnabled=!validProfileCapture||surfaceRequested==L"workshop-gravel-surface-candidate-v1";
        if(settlementSurfaceEnabled&&!settlementGroundEnabled)
            throw std::runtime_error("Settlement surface requires existing settlement domain");
        if(settlementGroundEnabled&&!entranceRoadEnabled)
            throw std::runtime_error("Settlement ground requires existing two-domain road material");
        if(entranceRoadEnabled&&validProfileCapture){
            for(const auto* name:{L"AEVRYND_MISTWATCH_NO_GROUND_TEXTURES",L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_BASE",
                L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_SURFACE"})
                if(profileFlag(name)==L"1")throw std::runtime_error("Entrance road does not support ground channel isolation");
        }
        const auto roadsideProfile=playableArt(Mistwatch::PlayableArt::Feature::Roadside,L"AEVRYND_MISTWATCH_ROADSIDE_COVER");
        roadsideCoverEnabled=roadsideProfile==L"roadside-ground-cover-v1";
        roadsideBroadleafEnabled=roadsideProfile==L"roadside-broadleaf-v1";
        const auto understoryProfile=playableArt(Mistwatch::PlayableArt::Feature::Understory,L"AEVRYND_MISTWATCH_ENTRANCE_UNDERSTORY");
        if(!understoryProfile.empty()&&understoryProfile!=L"original"&&understoryProfile!=L"entrance-understory-v2")
            throw std::runtime_error("Invalid entrance understory profile");
        entranceUnderstoryEnabled=understoryProfile==L"entrance-understory-v2";
        if(entranceUnderstoryEnabled&&(!roadsideBroadleafEnabled||!entranceRoadEnabled))
            throw std::runtime_error("Entrance understory requires broadleaf and road domains");
        if(validProfileCapture&&entranceUnderstoryEnabled&&profileFlag(L"AEVRYND_MISTWATCH_SHRUBS")==L"0")
            throw std::runtime_error("Layered understory requires visible replacement shrubs");
        // 正常遊玩採用已通過實拍與任務路線的新群落；舊配置只供明確的capture對照。
        const auto shrubProfile=playableArt(Mistwatch::PlayableArt::Feature::Shrubs,L"AEVRYND_MISTWATCH_SHRUB_PROFILE");
        shrubGapDensity=shrubProfile==L"woodland-gap-density-v2";
        shrubArcDensity=shrubProfile==L"woodland-feasible-arc-v3";
        shrubTarget=shrubProfile==L"original"?64u:128u;
        wchar_t habitatProfile[40]{};
        const auto habitatLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_HABITAT_PROFILE",habitatProfile,40);
        if(habitatLength){
            const bool leafProfile=habitatLength<40&&std::wstring_view(habitatProfile)==L"leaf-coverage-strength-v1";
            if(leafProfile)(void)Mistwatch::ForestLeafDomain::Resolve(validProfileCapture,validProfileReplay,habitatProfile);
            if((!leafProfile&&GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",groundCapture,2)==0)||habitatLength>=40||
                (std::wstring_view(habitatProfile)!=L"original"&&std::wstring_view(habitatProfile)!=L"leaf-coverage-strength-v1"&&
                 std::wstring_view(habitatProfile)!=L"root-cohorts-v1"))
                throw std::runtime_error("Invalid capture-only habitat profile");
            deciduousHabitat=std::wstring_view(habitatProfile)==L"root-cohorts-v1";
            habitatFloorProfile=std::wstring_view(habitatProfile)==L"leaf-coverage-strength-v1"?
                Mistwatch::ForestHabitatFloorProfile::LeafCoverageStrengthV1:Mistwatch::ForestHabitatFloorProfile::Original;
        }
        wchar_t habitatView[24]{};
        const auto habitatViewLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_HABITAT_VIEW",habitatView,24);
        if(habitatViewLength){
            if(!validProfileCapture||habitatViewLength>=24||std::wstring_view(habitatView)!=L"broadleaf-5")
                throw std::runtime_error("Invalid capture-only habitat view");
            captureDeciduousRootView=true;
        }
        if(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",groundCapture,2)>0) {
            const auto length=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GROUND_LEGACY",legacyGround,3);
            if(length&&(length!=1||(legacyGround[0]!=L'0'&&legacyGround[0]!=L'1')))
                throw std::runtime_error("Invalid ground comparison flag");
            diagnosticLegacyGround=legacyGround[0]==L'1';
            wchar_t surfaceGround[3]{};
            const auto surfaceLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GROUND_SINGLE_SURFACE",surfaceGround,3);
            if(surfaceLength&&(surfaceLength!=1||(surfaceGround[0]!=L'0'&&surfaceGround[0]!=L'1')))
                throw std::runtime_error("Invalid single-surface ground comparison flag");
            diagnosticSurfaceGround=surfaceGround[0]==L'1';
            wchar_t legacyGrass[3]{};
            const auto grassLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_LEGACY",legacyGrass,3);
            if(grassLength&&(grassLength!=1||(legacyGrass[0]!=L'0'&&legacyGrass[0]!=L'1')))
                throw std::runtime_error("Invalid grass comparison flag");
            diagnosticLegacyGrass=legacyGrass[0]==L'1';
            wchar_t dimGrass[3]{};
            const auto dimLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_DIM",dimGrass,3);
            if(dimLength&&(dimLength!=1||(dimGrass[0]!=L'0'&&dimGrass[0]!=L'1')))
                throw std::runtime_error("Invalid grass brightness comparison flag");
            diagnosticDimGrass=dimGrass[0]==L'1';
            wchar_t proceduralGrass[3]{};
            const auto proceduralLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_PROCEDURAL",proceduralGrass,3);
            if(proceduralLength&&(proceduralLength!=1||(proceduralGrass[0]!=L'0'&&proceduralGrass[0]!=L'1')))
                throw std::runtime_error("Invalid procedural grass comparison flag");
            diagnosticProceduralGrass=proceduralGrass[0]==L'1'||diagnosticLegacyGrass||diagnosticDimGrass;
            wchar_t grassShape[32]{};
            const auto shapeLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_SHAPE",grassShape,32);
            if(shapeLength){
                if(shapeLength>=32||(std::wstring_view(grassShape)!=L"source-geometry"&&
                    std::wstring_view(grassShape)!=L"source-uv-curved-v1"))
                    throw std::runtime_error("Invalid scanned grass shape profile");
                diagnosticCurvedScannedGrass=std::wstring_view(grassShape)==L"source-uv-curved-v1";
                if(diagnosticCurvedScannedGrass&&diagnosticProceduralGrass)
                    throw std::runtime_error("Scanned shape cannot mix with procedural grass");
            }
            wchar_t nettleProfile[32]{};
            const auto nettleLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_UNDERSTORY",nettleProfile,32);
            if(nettleLength){
                if(nettleLength>=32||(std::wstring_view(nettleProfile)!=L"original"&&
                    std::wstring_view(nettleProfile)!=L"nettle-mixed-v1"))throw std::runtime_error("Invalid nettle mix profile");
                diagnosticNettleMix=std::wstring_view(nettleProfile)==L"nettle-mixed-v1";
                if(diagnosticNettleMix&&(diagnosticCurvedScannedGrass||diagnosticProceduralGrass))
                    throw std::runtime_error("Nettle mix requires original scanned grass");
            }
            wchar_t scatter[3]{};
            const auto scatterLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_SCATTER_BASELINE",scatter,3);
            if(scatterLength&&(scatterLength!=1||(scatter[0]!=L'0'&&scatter[0]!=L'1')))
                throw std::runtime_error("Invalid ground-cover scatter comparison flag");
            diagnosticScatterCover=scatter[0]==L'1';
            wchar_t legacyFern[3]{};
            const auto fernLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_FERN_LEGACY",legacyFern,3);
            if(fernLength&&(fernLength!=1||(legacyFern[0]!=L'0'&&legacyFern[0]!=L'1')))
                throw std::runtime_error("Invalid fern comparison flag");
            diagnosticLegacyFern=legacyFern[0]==L'1';
        }
        groundDefinition=Mistwatch::HabitatGroundDefinition(Mistwatch::WorldProfileHasWoodCollision(worldProfile),
            firLayoutSelector==Mistwatch::FirLayoutSelector::ForestWood&&fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1,
            diagnosticLegacyGround,diagnosticSurfaceGround);
        // 道路與聚落共用原作者來源；新高度配置只屬森林，不放寬分域驗證。
        domainGroundDefinition=groundDefinition;
        wchar_t microreliefProfile[64]{};
        const auto microreliefLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GROUND_MICRORELIEF",microreliefProfile,64);
        if(microreliefLength>=64)throw std::runtime_error("Ground microrelief profile too long");
        groundMicrorelief=Mistwatch::GroundMicrorelief::Resolve(validProfileCapture,validProfileReplay,
            habitatFloorProfile==Mistwatch::ForestHabitatFloorProfile::LeafCoverageStrengthV1,microreliefProfile);
        if(forestIblEnabled&&(groundMicrorelief||forestLightingContrast))
            throw std::runtime_error("Forest IBL requires independent original light and normal settings");
        groundDistanceSettings.normalDetailStrength=Mistwatch::GroundMicrorelief::Strength(groundMicrorelief);
        groundDistanceSettings.mesoPartitionEnabled=false;
        groundDistanceSettings.rockVariationEnabled=cliffVariationEnabled;
        Mistwatch::ForestSurfaceProfile::Apply(groundDefinition,groundDistanceSettings,forestSurfaceSelection);
        const std::array<MaterialAssetDefinition3D,5> definitions{{
            groundDefinition,
            {"bark",{"poly-haven/bark_brown_02/1k/base-color.jpg","poly-haven/bark_brown_02/1k/roughness.jpg",1024,"poly-haven/bark_brown_02/1k/normal-dx.jpg"},{{1,1,1,1},{.85f,0,0,.8f},true,{},1.f}},
            {"wood",{"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Color.jpg","ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_Roughness.jpg",1024,"ambientcg/woodsiding006/1k/WoodSiding006_1K-JPG_NormalDX.jpg"},{{1,1,1,1},{.8f,0,0,.8f},true,{},.5f}},
            {"rock",{"poly-haven/rock_surface/1k/base-color.jpg","poly-haven/rock_surface/1k/roughness.jpg",1024,"poly-haven/rock_surface/1k/normal-dx.jpg"},{{1,1,1,1},{.9f,0,0,.85f},true,{},1.f}}
            , Mistwatch::HutWeatheredWoodDefinition()
        }};
        if(!materials.LoadAll(definitions)) throw std::runtime_error("Mistwatch CC0 materials unavailable");
        groundMaterial=*materials.Find("ground");
        if(groundMaterial.materialClass==MaterialClass3D::Terrain&&groundMaterial.textures){
            groundMaterial.useUv=true;
            groundMaterial.normalScale=TerrainLayerCoverageNormalScaleMarker3D;
        }
        barkMaterial=*materials.Find("bark");
        woodMaterial=*materials.Find("wood");
        // 僅巡林屋採作者UV；桌子、補給箱與修橋木材保留共用材質。
        const auto* loadedHut=materials.Find("hut-weathered-wood");
        if(!loadedHut)throw std::runtime_error("Weathered hut material unavailable");
        hutMaterial=diagnosticLegacyRangerHut?woodMaterial:Mistwatch::HutWeatheredWoodMaterial(woodMaterial,*loadedHut,{});
        rockMaterial=*materials.Find("rock");
        startupStage("ground_materials");
        const auto forestRoot=e.ExecutableDirectory()/"assets"/"mistwatch-forest";
        forestMaterials.Initialize(e,forestRoot);
        constexpr const char* slots[]{"bark","twig","dead-branches","trunk-c"};
        wchar_t lodOverride[8]{};
        GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_TREE_LOD",lodOverride,8);
        const std::string treeLod=lodOverride[0]==L'0'?"0":"1";
        wchar_t twigMode[16]{},captureFlag[2]{};
        const bool treeDiagnostic=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",captureFlag,2)>0;
        if(treeDiagnostic)GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_TWIG_MODE",twigMode,16);
        if(treeDiagnostic&&std::wstring(twigMode)==L"baseline")nearFirTwigLod=false;
        wchar_t shadowMode[16]{};
        if(treeDiagnostic)GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_SHADOW_RETAIN_MODE",shadowMode,16);
        if(treeDiagnostic&&std::wstring(shadowMode)==L"baseline")shadowRetentionEnabled=false;
        constexpr const char* hashes[3][3]{
            {"49fd9058e6a5059e6bced88360e44d0bcf303a164a3c0f4c08a151ddb236015a","a1d895baa855d31dd705e10be6b3c8ed56e2e663665b8a7001c0cf39e128a3e8","21291b807a30761309f810086d6732b752546c951e2a2a091101dd4f7cdb29f3"},
            {"b1a458c5584b5fa40ce04144f341e013b1a999881bf17fff2880032363d58d5e","37af8ba7a3a7f94edb8a58f3bb716c978ca9631bd04027625ea5818be0223e10","3f656dfa0f462f4812c17ff0487261c770a637d8d7c97536c9d98a36f429d3bb"},
            {"c5f308b9a1ebc6fbf4a1eb44750309e1cc191fd829bfdc50098d69b817f8a168","6b894f1c1da10361f2f07c1211cb1902342fbdee10afef66c03150c995836dee","22726faaab8c104237ffe7c8f17ccfa50637561222b4148b61eda0b0fcc03344"}};
        for(unsigned slot=0;slot<4;++slot) {
            const std::string name=slots[slot];
            MaterialTextureSetDefinition3D textures{
                std::filesystem::path("materials")/(name+"-base.dds"),
                std::filesystem::path("materials")/(name+"-arm.dds"),1024,
                std::filesystem::path("materials")/(name+"-normal.dds")};
            textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
            for(unsigned channel=0;channel<3;++channel)
                textures.integrity[channel]={1398276,hashes[slot==3?2:slot==1?1:0][channel]};
            Material3D material{{1,1,1,1},{.85f,0,0,.8f},true,{},1.f,true};
            if(slot==1) {
                textures.alphaMode=MaterialAlphaMode3D::Mask;
                material=Mistwatch::FirTwigMaterial();
                if(std::wstring(twigMode)==L"opaque")material.alphaMode=MaterialAlphaMode3D::Opaque;
                else if(std::wstring(twigMode)==L"low")material.alphaCutoff=.15f;
                else if(std::wstring(twigMode)==L"original")material.alphaCutoff=.5f;
            }
            if(!forestMaterials.Load({name,textures,material})) throw std::runtime_error("Fir material unavailable");
            firMaterials[slot]=*forestMaterials.Find(name);
            const std::string meshName="fir-tree-01-c-lod"+treeLod+"-node0-mesh0-primitive"+std::to_string(slot)+"-material"+std::to_string(slot)+"-fir-tree-01-"+name+".aemesh";
            const auto loaded=LoadAevryndMeshFile(forestRoot/"meshes"/meshName);
            if(!loaded) throw std::runtime_error("Fir mesh unavailable");
            firMeshes[slot]=e.CreateMeshResource3D(loaded.mesh);
            if(!firMeshes[slot]) throw std::runtime_error("Fir GPU upload failed");
            if(slot==1) {
                const auto detail=LoadAevryndMeshFile(forestRoot/"meshes"/"fir-tree-01-c-lod0-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh");
                if(!detail)throw std::runtime_error("Detailed fir twig unavailable");
                detailedFirTwig=e.CreateMeshResource3D(detail.mesh);
                if(!detailedFirTwig)throw std::runtime_error("Detailed fir twig GPU upload failed");
            }
            const std::string distantLod=slot==1&&captureLandscapeTwigLod1?"1":"2";
            const std::string distantName="fir-tree-01-c-lod"+distantLod+"-node0-mesh0-primitive"+std::to_string(slot)+"-material"+std::to_string(slot)+"-fir-tree-01-"+name+".aemesh";
            const auto distant=LoadAevryndMeshFile(forestRoot/"meshes"/distantName);
            if(!distant)throw std::runtime_error("Landscape fir mesh unavailable");
            landscapeFirMeshes[slot]=e.CreateMeshResource3D(distant.mesh);
            if(!landscapeFirMeshes[slot])throw std::runtime_error("Landscape fir GPU upload failed");
            if(!landscapeFirMeshes[slot].cpuMesh)throw std::runtime_error("Landscape fir CPU identity unavailable");
            landscapeFirFingerprints[slot]=Mistwatch::ForestGrassShapeFingerprint(*landscapeFirMeshes[slot].cpuMesh);
            if(slot==1&&landscapeTwigDistanceLod){
                const auto nearSource=LoadAevryndMeshFile(forestRoot/"meshes"/"fir-tree-01-c-lod1-node0-mesh0-primitive1-material1-fir-tree-01-twig.aemesh");
                if(!nearSource)throw std::runtime_error("Landscape detailed twig unavailable");
                landscapeDetailedFirTwig=e.CreateMeshResource3D(nearSource.mesh);
                if(!landscapeDetailedFirTwig||!landscapeDetailedFirTwig.cpuMesh)
                    throw std::runtime_error("Landscape detailed twig upload failed");
                landscapeDetailedTwigFingerprint=Mistwatch::ForestGrassShapeFingerprint(*landscapeDetailedFirTwig.cpuMesh);
                landscapeTwigUnionBounds=Mistwatch::LandscapeTwigDraw::UnionBounds(landscapeDetailedFirTwig,landscapeFirMeshes[1]);
            }
        }
        startupStage("fir_assets");
        const auto broadleafRoot=e.ExecutableDirectory()/"assets"/"mistwatch-broadleaf";
        broadleafMaterials.Initialize(e,broadleafRoot);
        constexpr const char* broadleafSlots[]{"branches","leaves","trunk"};
        constexpr const char* broadleafHashes[3][3]{
            {"cf4f6fcc9329e66f05b7b2e86d20a38b2d0e8e054013cf3e0431a4cbc1e17d0a","1c54a88ec7a17c55528a99319378283d61b4800002ab4e63ca0fe30ebb1249ea","ea8a046e64ce7eacce435e64f31fc45501624fd131c7e6c8492bfd9de9fb1beb"},
            {"88fce8dc2f1815c421a0c7c4d9cc7d9cacb4024f6c51e8fc928a5e938b7996a9","c8a6a8747023a64ed506ad62b8e5c0ae16bf14e0353d5e67f8d2511967aaf2ed","320d6df57bf075dbd701acab52740a623f0c86d298bd3048f3986d1988bb0cd9"},
            {"513d07ece694d03e3c095c4148d7da4649ffdf6db54826cf1bf1754a368b1453","716c128da96feb1f3ad4ea041ecd9d5c167115a7aa0671f517ebaefbc9963b7d","099e981864b1e90b2c0500b0bcb0a92fcdc38ee196f00f7161905dc9bf762712"}};
        for(unsigned slot=0;slot<3;++slot) {
            const std::string name=broadleafSlots[slot];
            MaterialTextureSetDefinition3D textures{std::filesystem::path("materials")/(name+"-base.dds"),
                std::filesystem::path("materials")/(name+"-arm.dds"),1024,
                std::filesystem::path("materials")/(name+"-normal.dds")};
            textures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
            for(unsigned channel=0;channel<3;++channel)textures.integrity[channel]={1398276,broadleafHashes[slot][channel]};
            Material3D material{{1,1,1,1},{.85f,0,0,1},true,{},1.f,true};
            if(slot==1){textures.alphaMode=MaterialAlphaMode3D::Mask;material.alphaMode=MaterialAlphaMode3D::Mask;
                material.alphaCutoff=.5f;material.doubleSided=true;material.materialClass=MaterialClass3D::Foliage;}
            if(!broadleafMaterials.Load({name,textures,material}))throw std::runtime_error("Broadleaf material unavailable");
            broadleafSurface[slot]=*broadleafMaterials.Find(name);
            auto loaded=(authoredLandscapePlanting||mixedLandscapeCanopy||rightForeground||shoulderGroveEnabled)?
                MeshLoadResult3D{LoadLandscapeBroadleaf(broadleafRoot/"meshes"/Mistwatch::BroadleafMeshName(slot),slot),{}}:
                LoadAevryndMeshFile(broadleafRoot/"meshes"/Mistwatch::BroadleafMeshName(slot));
            if(!loaded)throw std::runtime_error("Broadleaf mesh unavailable");
            Mistwatch::RootBroadleafMesh(loaded.mesh);
            if(authoredLandscapePlanting||mixedLandscapeCanopy||shoulderGroveEnabled)landscapeBroadleafSource[slot]=loaded.mesh;
            broadleafTriangles+=loaded.mesh.indices.size()/3;
            broadleafMeshes[slot]=e.CreateMeshResource3D(loaded.mesh);
            if(!broadleafMeshes[slot])throw std::runtime_error("Broadleaf upload failed");
        }
        matureStudy.Initialize(e);
        startupStage("broadleaf_assets");
        const auto boulderRoot=e.ExecutableDirectory()/"assets"/"mistwatch-boulders";
        boulderMaterials.Initialize(e,boulderRoot);
        MaterialTextureSetDefinition3D boulderTextures{"materials/slot-0-base.dds","materials/slot-0-arm.dds",1024,
            "materials/slot-0-normal.dds"};
        boulderTextures.surfaceEncoding=MaterialSurfaceMapEncoding3D::PackedOrm;
        constexpr const char* boulderHashes[]{
            "5a847a3174e09505b3a404c93716cbae576a5cb8bd32f69ec6395e8c95008d36",
            "564be8685c266fe7fa03acda76c6c06f706911c4344777ceab6563d743284a1e",
            "68fbbe04ffdb1c6d327f4262b9230438623f0d3b4b17a0a5155eb13b9ed21e83"};
        for(unsigned channel=0;channel<3;++channel)boulderTextures.integrity[channel]={1398276,boulderHashes[channel]};
        // 原始照片以 sRGB 輸入／輸出重烘焙；保留歷史貼圖供同鏡頭對照，不改光照或曝光。
        const auto boulderColor=playableArt(Mistwatch::PlayableArt::Feature::BoulderColor,L"AEVRYND_MISTWATCH_BOULDER_COLOR");
        if(!boulderColor.empty()&&boulderColor!=L"original"){
            if(boulderColor!=L"source-srgb-v1")throw std::runtime_error("Invalid boulder color profile");
            boulderColorProfile="source-srgb-v1";
            boulderBasePath="materials/slot-0-base-source-srgb-v1.dds";
            boulderBaseHash="d874d5ac2e2c01ca7bd87970ba23415ef285e051b0a501c74e383e6219758b79";
            boulderTextures.baseColorPath=boulderBasePath;
            boulderTextures.integrity[0]={1398276,boulderBaseHash};
        }
        if(!boulderMaterials.Load({"boulder",boulderTextures,{{1,1,1,1},{.9f,0,0,1},true,{},1.f,true}}))
            throw std::runtime_error("Boulder material unavailable");
        boulderMaterial=*boulderMaterials.Find("boulder");
        // 固定岩石網格、光照與曝光，只在截圖診斷隔離法線或完整材質。
        const auto boulderDiagnostic=profileFlag(L"AEVRYND_MISTWATCH_BOULDER_DIAGNOSTIC");
        if(!boulderDiagnostic.empty()){
            if(!validProfileCapture||(boulderDiagnostic!=L"original"&&boulderDiagnostic!=L"no-normal"&&boulderDiagnostic!=L"untextured"))
                throw std::runtime_error("Invalid capture-only boulder diagnostic profile");
            if(boulderDiagnostic==L"no-normal"||boulderDiagnostic==L"untextured"){
                boulderMaterial.normalScale=0;
                boulderDiagnosticProfile=boulderDiagnostic==L"no-normal"?"no-normal":"untextured";
            }
            if(boulderDiagnostic==L"untextured"){
                boulderMaterial.textures={};boulderMaterial.tint={.35f,.35f,.35f,1};
            }
        }
        const auto boulderGeometry=playableArt(Mistwatch::PlayableArt::Feature::BoulderGeometry,L"AEVRYND_MISTWATCH_BOULDER_GEOMETRY");
        if(!boulderGeometry.empty()&&boulderGeometry!=L"original"){
            if(boulderGeometry!=L"lod0-source-v1")throw std::runtime_error("Invalid boulder geometry profile");
            boulderGeometryProfile="lod0-source-v1";
        }
        const bool detailedBoulder=boulderGeometryProfile=="lod0-source-v1";
        // 高細節原始網格沿用原座標系；視覺比較不變更碰撞體或偷偷縮放外凸部分。
        auto boulder=detailedBoulder?
            MeshLoadResult3D{Mistwatch::BoulderDetail::LoadBoulderDetailMesh(boulderRoot/"meshes"/Mistwatch::BoulderDetail::MeshName),{}}:
            LoadAevryndMeshFile(boulderRoot/"meshes"/Mistwatch::BoulderMeshName);
        if(!detailedBoulder){
            if(!boulder||boulder.mesh.vertices.size()!=56938||boulder.mesh.indices.size()!=79833)
                throw std::runtime_error("Boulder geometry unavailable");
            Mistwatch::RootBoulderMesh(boulder.mesh);
            const auto boulderBounds=boulder.mesh.Bounds(),boulderProfile=Mistwatch::BoulderBounds();
            if(Length(boulderBounds.center-boulderProfile.center)>.00001f||Length(boulderBounds.extents-boulderProfile.extents)>.00001f)
                throw std::runtime_error("Boulder collision profile mismatch");
        }
        propMeshes[0]=e.CreateMeshResource3D(boulder.mesh);
        propMeshes[1]=e.CreateMeshResource3D(Mistwatch::ForestPropLogMesh());
        if(!propMeshes[0]||!propMeshes[1])throw std::runtime_error("Forest prop GPU upload failed");
        if(fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1&&
           !fallenLogs.Initialize(e,e.ExecutableDirectory()/"assets"/"mistwatch-fallen-logs"))
            throw std::runtime_error("Scanned fallen-log resource initialization failed");
        terrain=Mistwatch::CreateTerrain();
        const bool layeredGround=groundMaterial.materialClass==MaterialClass3D::Terrain;
        // 成熟森林選用新地表遮罩；歷史配置維持原網格，兩側共用相同接縫權重。
        const auto floorMask=Mistwatch::WorldProfileHasWoodCollision(worldProfile)?
            Mistwatch::ForestFloorMaskSelector::ForestWood:Mistwatch::ForestFloorMaskSelector::Legacy;
        const auto bankMask=Mistwatch::WorldProfileHasWoodCollision(worldProfile)?
            Mistwatch::BankCoverageSelector::Patchy:Mistwatch::BankCoverageSelector::Legacy;
        auto terrainCpuMesh=Mistwatch::CreateTerrainMesh(terrain,layeredGround,floorMask,bankMask);
        auto landscapeCpuMesh=Mistwatch::CreateLandscapeMesh(terrain,layeredGround,floorMask,bankMask);
        landscapeTrees=Mistwatch::LandscapeTrees(terrain);
        if(landscapeStandFill){
            Vec3 lo{10000,10000,10000},hi{-10000,-10000,-10000};
            const auto include=[&](const MeshResource3D& resource){
                if(!resource||!resource.cpuMesh)throw std::runtime_error("Stand source resource unavailable");
                for(const auto& v:resource.cpuMesh->vertices){
                    lo.x=(std::min)(lo.x,v.x);lo.y=(std::min)(lo.y,v.y);lo.z=(std::min)(lo.z,v.z);
                    hi.x=(std::max)(hi.x,v.x);hi.y=(std::max)(hi.y,v.y);hi.z=(std::max)(hi.z,v.z);
                }
            };
            for(const auto& resource:landscapeFirMeshes)include(resource);
            include(landscapeDetailedFirTwig);
            const auto extents=(hi-lo)*.5f;landscapeStandBounds={(hi+lo)*.5f,Length(extents),extents};
            landscapeStandPlan=Mistwatch::LandscapeStandFill::Create(terrain,landscapeTrees,landscapeStandBounds);
            if(!landscapeStandPlan.complete)throw std::runtime_error("Landscape stand population incomplete");
        }
        InitializeCrownGrowth(e);
        riverMesh=e.CreateMeshResource3D(Mistwatch::CreateRiverMesh(terrain));
        if(!riverMesh)throw std::runtime_error("River GPU upload failed");
        // 同一 typed 作者保留根、完整 render pose 與縮放後樹幹，不能只重排畫面。
        firLayout=Mistwatch::CreateMatureFirLayout(terrain,firLayoutSelector);
        if(!firLayout.reachedBudget||firLayout.firs.size()!=100||firLayout.roots.size()!=100)
            throw std::runtime_error("Mature fir authored population incomplete");
        trees=firLayout.roots;
        const auto retainedProps=Mistwatch::ForestProps(terrain,trees);
        forestProps=Mistwatch::ApplyFallenLogProfile(retainedProps,firLayoutSelector,fallenLogProfile);
        understory=Mistwatch::CreateUnderstory(terrain,trees,retainedProps);
        broadleafTrees=Mistwatch::CreateBroadleafTrees(terrain,trees,retainedProps,understory);
        if(broadleafTrees.size()!=12||broadleafTriangles!=82854)throw std::runtime_error("Broadleaf population/geometry invalid");
        fernMesh=e.CreateMeshResource3D(CreateFernMesh3D(EnvironmentMeshDetail3D::Near,17));
        if(!fernMesh)throw std::runtime_error("Fern GPU upload failed");
        startupStage("terrain_and_props");
        const auto fernRoot=e.ExecutableDirectory()/"assets"/"mistwatch-ferns";
        fernMaterials.Initialize(e,fernRoot);
        if(!fernMaterials.Load(Mistwatch::FernMaterialDefinition()))throw std::runtime_error("Fern material unavailable");
        fernSurface=*fernMaterials.Find("fern");
        for(unsigned variant=0;variant<4;++variant) {
            auto loaded=LoadAevryndMeshFile(fernRoot/"meshes"/Mistwatch::FernMeshName(variant));
            if(!loaded||loaded.mesh.vertices.size()!=Mistwatch::FernVertexCounts[variant]||
                loaded.mesh.indices.size()!=3u*Mistwatch::FernTriangleCounts[variant])throw std::runtime_error("Fern variant unavailable");
            Mistwatch::NormalizeFernAsset(loaded.mesh);
            fernFootprints[variant]=Mistwatch::FernColonyAssetFromMesh(loaded.mesh);
            scannedFerns[variant]=e.CreateMeshResource3D(loaded.mesh);
            if(!scannedFerns[variant])throw std::runtime_error("Scanned fern upload failed");
        }
        groundCover=Mistwatch::GroundCoverPositions(terrain);
        if(middleLayerEnabled) {
            const std::array<float,4> radii{fernFootprints[0].radius,fernFootprints[1].radius,fernFootprints[2].radius,fernFootprints[3].radius};
            middleLayer=Mistwatch::CreateMiddleLayer(terrain,trees,retainedProps,understory,broadleafTrees,Mistwatch::WorldSolids(false,firLayoutSelector),24,radii);
            if(!middleLayer.reachedBudget||middleLayer.trees.size()!=24)
                throw std::runtime_error("Middle-layer authored budget incomplete");
            for(const auto& tree:middleLayer.trees)middleLayerSolids.push_back(tree.trunk);
        }
        grassMesh=e.CreateMeshResource3D(CreateGrassClumpMesh3D(9,diagnosticLegacyGrass?3:5,diagnosticLegacyGrass?GrassBladeStyle3D::Upright:GrassBladeStyle3D::Curved));
        const auto grassRoot=e.ExecutableDirectory()/"assets"/"mistwatch-grass";
        grassMaterials.Initialize(e,grassRoot);
        if(!grassMaterials.Load(Mistwatch::GrassMaterialDefinition()))throw std::runtime_error("Grass material unavailable");
        grassSurface=*grassMaterials.Find("grass");
        for(unsigned variant=0;variant<scannedGrass.size();++variant){
            auto loaded=LoadAevryndMeshFile(grassRoot/"meshes"/Mistwatch::GrassMeshName(variant));
            if(!loaded||loaded.mesh.vertices.size()!=Mistwatch::GrassVertexCounts[variant]||
                loaded.mesh.indices.size()!=3u*Mistwatch::GrassTriangleCounts[variant])throw std::runtime_error("Scanned grass topology changed");
            Mistwatch::NormalizeGrassAsset(loaded.mesh);
            grassFootprints[variant]=Mistwatch::ForestCoverAssetFromMesh(loaded.mesh);
            // 完整正規化網格身分包含頂點、UV、法線／顏色與每個索引，不只記錄尺寸。
            std::uint64_t assetIdentity=14695981039346656037ull;
            const auto mixWord=[&](std::uint32_t word){for(unsigned byte=0;byte<4;++byte){
                assetIdentity^=(word>>(byte*8))&255u;assetIdentity*=1099511628211ull;}};
            mixWord(unsigned(loaded.mesh.vertices.size()));mixWord(unsigned(loaded.mesh.indices.size()));
            for(const auto& vertex:loaded.mesh.vertices){
                for(float value:{vertex.x,vertex.y,vertex.z,vertex.u,vertex.v})mixWord(std::bit_cast<std::uint32_t>(value));
                mixWord(vertex.color);mixWord(vertex.normal);
            }
            for(auto index:loaded.mesh.indices)mixWord(index);
            grassAssetIdentities[variant]=assetIdentity;
            grassRenderIdentities[variant]=assetIdentity;
            scannedGrass[variant]=e.CreateMeshResource3D(loaded.mesh);
            if(!scannedGrass[variant])throw std::runtime_error("Scanned grass upload failed");
            if(diagnosticCurvedScannedGrass){
                const auto candidate=Mistwatch::CreateForestGrassShape(loaded.mesh,variant);
                curvedGrassVolumes[variant]=Mistwatch::ForestCoverAssetFromMesh(candidate);
                grassRenderIdentities[variant]=Mistwatch::ForestGrassShapeFingerprint(candidate);
                if(grassRenderIdentities[variant]==Mistwatch::ForestGrassShapeFingerprint(loaded.mesh))
                    throw std::runtime_error("Scanned grass candidate geometry unchanged");
                curvedScannedGrass[variant]=e.CreateMeshResource3D(candidate);
                if(!curvedScannedGrass[variant])throw std::runtime_error("Curved scanned grass upload failed");
            }
        }
        if(diagnosticNettleMix){
            const auto root=e.ExecutableDirectory()/"assets/mistwatch-nettle";
            nettleMaterials.Initialize(e,root);
            if(!nettleMaterials.Load(Mistwatch::NettleMaterialDefinition()))throw std::runtime_error("Nettle material unavailable");
            nettleSurface=*nettleMaterials.Find("nettle");
            for(unsigned variant=0;variant<6;++variant){
                auto loaded=LoadAevryndMeshFile(root/"meshes"/Mistwatch::NettleMeshName(variant));
                if(!loaded)throw std::runtime_error("Nettle actual mesh load failed");
                Mistwatch::NormalizeNettleAsset(loaded.mesh,variant);
                nettleBounds[variant]=Mistwatch::MeasureNettleAssetBounds(loaded.mesh);
                nettleMeshIdentities[variant]=Mistwatch::ForestGrassShapeFingerprint(loaded.mesh);
                nettleMeshes[variant]=e.CreateMeshResource3D(loaded.mesh);
                if(!nettleMeshes[variant])throw std::runtime_error("Nettle upload failed");
            }
        }
        startupStage("fern_and_grass_assets");
        relayDish=e.CreateMeshResource3D(Mistwatch::CreateRelayDish());
        if(!relayDish)throw std::runtime_error("Relay dish GPU upload failed");
        // 作者世界與完整冠層先完成，所有後續地被皆讀同一最終世界。
        if(Mistwatch::WorldProfileHasWoodCollision(worldProfile)&&firLayoutSelector==Mistwatch::FirLayoutSelector::ForestWood)
            InitializeCanopy(e,startupStage);
        if(woodlandFailAfterCanopy)throw std::runtime_error("WOODLAND_ISOLATED_AFTER_CANOPY_FAILURE");
        solids=RuntimeWorldSolids(session.bridgeRepaired);
        trailLitter=Mistwatch::CreateTrailLitter(terrain,solids);
        if(!trailLitter.reachedBudget)throw std::runtime_error("Trail litter authored budget incomplete");
        trailLitterMeshes[0]=e.CreateMeshResource3D(trailLitter.branches);
        trailLitterMeshes[1]=e.CreateMeshResource3D(trailLitter.stones);
        if(!trailLitterMeshes[0]||!trailLitterMeshes[1])throw std::runtime_error("Trail litter GPU upload failed");
        // 原生成器與舊模式保持；作者版地被使用完成的 render/body/camera 同世界。
        if(Mistwatch::WorldProfileHasCanopy(worldProfile)&&
            !(Mistwatch::WorldProfileHasWoodCollision(worldProfile)&&firLayoutSelector==Mistwatch::FirLayoutSelector::ForestWood))
            InitializeCanopy(e,startupStage);
        startupStage("canopy_and_litter");
        // 真實作者樹根完成後才編碼地表，避免材質斑塊仍跟隨已退讓的舊株。
        if((deciduousHabitat||habitatFloorProfile!=Mistwatch::ForestHabitatFloorProfile::Original)&&(!entranceWorld.applied||!layeredGround))
            throw std::runtime_error("Habitat candidate requires authored layered world");
        std::optional<Mesh3D> leafTerrainSource;
        if(entranceWorld.applied&&layeredGround){
            if(trees.size()!=95||canopyPopulation.plants.size()!=(woodlandWorld?Mistwatch::WoodlandCanopy::TotalCount:Mistwatch::EntranceCanopyTotalCount))
                throw std::runtime_error("Habitat actual final roots incomplete");
            std::vector<Vec3> canopyRoots;canopyRoots.reserve(canopyPopulation.plants.size());
            for(const auto& plant:canopyPopulation.plants)canopyRoots.push_back(plant.pose.position);
            const Mistwatch::ForestHabitatRoots roots{trees,canopyRoots};
            // 完整道路世界的canonical與外圈保持Original；葉層另走正式分域，不能先污染道路收據。
            const bool splitLeaf=entranceRoadEnabled&&habitatFloorProfile==Mistwatch::ForestHabitatFloorProfile::LeafCoverageStrengthV1;
            const auto canonicalProfile=splitLeaf?Mistwatch::ForestHabitatFloorProfile::Original:habitatFloorProfile;
            habitatTerrain=Mistwatch::ApplyForestHabitatFloor(terrainCpuMesh,terrain,roots,false,canonicalProfile);
            habitatLandscape=Mistwatch::ApplyForestHabitatFloor(landscapeCpuMesh,terrain,roots,true,canonicalProfile);
            if(splitLeaf){leafTerrainSource=terrainCpuMesh;
                (void)Mistwatch::ApplyForestHabitatFloor(*leafTerrainSource,terrain,roots,false,habitatFloorProfile);}
            if(deciduousHabitat){
                deciduousHabitatPlan=Mistwatch::CreateDeciduousHabitatPlan(broadleafTrees,middleLayer.trees,entranceWorld.middleSourceIds);
                habitatTerrain=Mistwatch::ApplyDeciduousHabitatFloor(terrainCpuMesh,terrain,deciduousHabitatPlan);
                habitatLandscape=Mistwatch::ApplyDeciduousHabitatFloor(landscapeCpuMesh,terrain,deciduousHabitatPlan,true);
            }
        }
        if(outerForestFloor){
            if(!entranceWorld.applied||!layeredGround||deciduousHabitat||
                (habitatFloorProfile!=Mistwatch::ForestHabitatFloorProfile::Original&&!leafTerrainSource))
                throw std::runtime_error("Outer forest floor requires original authored layered habitat");
            outerFloorReceipt=Mistwatch::OuterForestFloor::Apply(landscapeCpuMesh,terrain,landscapeStandPlan,landscapeStandBounds);
            if(outerFloorReceipt.beforeHabitatFingerprint!=habitatLandscape.fingerprint)
                throw std::runtime_error("Outer forest floor preimage differs from actual habitat");
        }
        landscapePlanting.originalPoseFingerprint=Mistwatch::LandscapeSourcePoseFingerprint(landscapeTrees);
        landscapePlanting.surfaceFingerprint=Mistwatch::RootSupportDetail::MeshFingerprint(landscapeCpuMesh);
        const auto uploadLandscapeRoots=[&]{
            if(!landscapePlanting.complete||landscapePlanting.plants.size()!=16)
                throw std::runtime_error("Authored landscape sixteen root proofs incomplete");
            for(unsigned slot=0;slot<3;++slot)if(!broadleafMeshes[slot].cpuMesh||
                Mistwatch::RootSupportDetail::MeshFingerprint(*broadleafMeshes[slot].cpuMesh)!=landscapePlanting.sourceFingerprints[slot])
                throw std::runtime_error("Uploaded shared broadleaf differs from proven source");
            try{
                for(std::size_t i=0;i<landscapePlanting.plants.size();++i){
                    landscapeRootMeshes[i]=e.CreateMeshResource3D(landscapePlanting.plants[i].derivedTrunk);
                    if(!landscapeRootMeshes[i])throw std::runtime_error("Authored landscape root upload failed");
                    ++landscapeRootAcquisitions;
                    if(!landscapeRootMeshes[i].cpuMesh||Mistwatch::RootSupportDetail::MeshFingerprint(*landscapeRootMeshes[i].cpuMesh)!=
                        landscapePlanting.plants[i].proof.derivedMeshFingerprint)
                        throw std::runtime_error("Uploaded derived trunk differs from proven mesh");
                }
            }catch(...){
                for(auto& mesh:landscapeRootMeshes){if(mesh)e.DestroyMesh3D(mesh.handle);mesh={};}
                landscapeRootAcquisitions=0;throw;
            }
        };
        if(authoredLandscapePlanting){
            if(!entranceWorld.applied||!layeredGround)throw std::runtime_error("Authored landscape actual material surface missing");
            landscapePlanting=Mistwatch::CreateAuthoredLandscapePlantingPlan(landscapeTrees,landscapeCpuMesh,landscapeBroadleafSource);
            uploadLandscapeRoots();
        }
        if(shoulderGroveEnabled){
            if(!entranceWorld.applied||!layeredGround)throw std::runtime_error("Shoulder grove actual surface missing");
            const auto prepareStart=std::chrono::steady_clock::now();
            shoulderGrove=Mistwatch::ShoulderGrove::Create(landscapeStandPlan.combined,landscapeCpuMesh,landscapeBroadleafSource);
            shoulderPrepareMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-prepareStart).count();
            if(!shoulderGrove.complete||shoulderGrove.plants.size()!=16||shoulderGrove.retained.size()!=600)
                throw std::runtime_error("Shoulder grove sealed sixteen recipe incomplete");
            for(unsigned slot=0;slot<3;++slot)if(!broadleafMeshes[slot].cpuMesh||
                Mistwatch::RootSupportDetail::MeshFingerprint(*broadleafMeshes[slot].cpuMesh)!=shoulderGrove.sourceFingerprints[slot])
                throw std::runtime_error("Shoulder shared source upload identity mismatch");
            for(const auto& plant:shoulderGrove.plants){
                if(plant.sourceOrdinal<Mistwatch::LandscapeStandFill::BaseCount)++shoulderSelectedBase;
                else ++shoulderSelectedStand;
            }
            const auto uploadStart=std::chrono::steady_clock::now();
            for(std::size_t i=0;i<shoulderGrove.plants.size();++i){
                shoulderRootMeshes[i]=e.CreateMeshResource3D(shoulderGrove.plants[i].derivedTrunk);
                if(!shoulderRootMeshes[i])throw std::runtime_error("Shoulder root upload failed");
                ++shoulderRootAcquisitions;
                if(!shoulderRootMeshes[i].cpuMesh||Mistwatch::RootSupportDetail::MeshFingerprint(*shoulderRootMeshes[i].cpuMesh)!=
                    shoulderGrove.plants[i].derivedFingerprint)throw std::runtime_error("Shoulder uploaded root identity mismatch");
            }
            shoulderUploadMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-uploadStart).count();
        }
        if(cliffDiagnostic){
            if(!layeredGround||!entranceRoadEnabled||!settlementSurfaceEnabled||outerForestFloor||authoredLandscapePlanting||deciduousHabitat)
                throw std::runtime_error("Cliff comparison requires unmodified layered habitat and split roads");
            auto candidate=Mistwatch::CliffCoverage::Create(landscapeCpuMesh,terrain,Mistwatch::CliffCoverage::Domain::Ring,
                cliffCoverageEnabled?Mistwatch::CliffCoverage::Profile::WestCliffV1:Mistwatch::CliffCoverage::Profile::Original);
            cliffReceipts[1]=candidate.receipt;landscapeCpuMesh=std::move(candidate.mesh);
        }
        // 混合群落的根部證明必須在岩層 UV 覆蓋完成後建立，對應真正上傳的地表。
        if(mixedLandscapeCanopy){
            if(!entranceWorld.applied||!layeredGround||outerForestFloor||authoredLandscapePlanting||
                shoulderGroveEnabled||rightForeground||deciduousHabitat||rockSegmentEnabled)
                throw std::runtime_error("Mixed canopy actual final surface is unsupported");
            landscapePlanting=Mistwatch::MixedLandscapeCanopy::Create(terrain,landscapeTrees,landscapeStandBounds,
                landscapeStandPlan,landscapeCpuMesh,landscapeBroadleafSource);
            uploadLandscapeRoots();
        }
        terrainMesh=e.CreateMeshResource3D(terrainCpuMesh);
        landscapeMesh=e.CreateMeshResource3D(landscapeCpuMesh);
        if(!terrainMesh||!landscapeMesh)throw std::runtime_error("Actual terrain/landscape GPU upload failed");
        if(cliffDiagnostic&&(!landscapeMesh.cpuMesh||Mistwatch::EntranceRoadDomainDetail::Fingerprint(*landscapeMesh.cpuMesh)!=cliffReceipts[1].afterFingerprint))
            throw std::runtime_error("Cliff ring actual upload mismatch");
        if(outerForestFloor&&(!landscapeMesh.cpuMesh||
            Mistwatch::OuterForestFloor::MeshFingerprint(*landscapeMesh.cpuMesh)!=outerFloorReceipt.afterFingerprint))
            throw std::runtime_error("Uploaded outer forest floor differs from qualified mesh");
        if((authoredLandscapePlanting||mixedLandscapeCanopy)&&(!landscapeMesh.cpuMesh||
           Mistwatch::RootSupportDetail::MeshFingerprint(*landscapeMesh.cpuMesh)!=landscapePlanting.surfaceFingerprint))
            throw std::runtime_error("Uploaded landscape differs from proven material surface");
        if(shoulderGroveEnabled&&(!landscapeMesh.cpuMesh||
            Mistwatch::RootSupportDetail::MeshFingerprint(*landscapeMesh.cpuMesh)!=shoulderGrove.surfaceFingerprint))
            throw std::runtime_error("Shoulder uploaded terrain identity mismatch");
        solids=RuntimeWorldSolids(session.bridgeRepaired);
        startupStage("habitat_and_terrain_upload");
        forestCoverClearanceColliders=solids.size();
        // 舊世界固體保留2D路線排除；新增木件只篩完整3D植物體積，避免高枝投影抹掉地被。
        std::vector<BoxCollider3D> vegetationSolids=solids;
        const bool woodWorld=Mistwatch::WorldProfileHasWoodCollision(worldProfile);
        if(woodWorld){vegetationSolids=RuntimeBaseSolids(session.bridgeRepaired);
            vegetationSolids.insert(vegetationSolids.end(),middleLayerSolids.begin(),middleLayerSolids.end());}
        const auto* groupedWood=woodlandWorld?&woodlandWorld->Wood():nullptr;
        const std::span<const BoxCollider3D> vegetationWood=woodWorld&&!groupedWood?std::span<const BoxCollider3D>(canopySolids):std::span<const BoxCollider3D>{};
        const auto coverStart=std::chrono::steady_clock::now();
        if(!diagnosticScatterCover&&!diagnosticProceduralGrass){
            if(entranceWorld.applied&&fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1){
                if(originalCoverWood.size()!=4335||(!groupedWood&&vegetationWood.size()!=8617)||
                    (groupedWood&&groupedWood->Identity()!=Mistwatch::WoodlandBridgeWorld::WoodIdentity))
                    throw std::runtime_error("Original/final forest-cover complete wood identity missing");
                // 同一原始地被只生成一次；不可變快照供保留配置逐位核對，不重複生成。
                const auto originalToken=Mistwatch::ValidatedForestCoverOriginal::Create(terrain,originalCoverTrees,originalCoverUnderstory,
                    originalCoverSolids,grassFootprints,grassAssetIdentities,forestCoverSettings,originalCoverWood);
                const auto& original=originalToken.Original();
                originalCoverFingerprint=14695981039346656037ull;
                const auto word=[&](std::uint32_t value){for(unsigned byte=0;byte<4;++byte){
                    originalCoverFingerprint^=(value>>(byte*8))&255u;originalCoverFingerprint*=1099511628211ull;}};
                for(const auto& plant:original.plants){const auto p=plant.transform.position;
                    for(float value:{p.x,p.y,p.z,Mistwatch::ForestCoverScale(p),p.x+p.z})word(std::bit_cast<std::uint32_t>(value));
                    word(plant.variant);
                }
                if(!original.reachedBudget||original.plants.size()!=3600||
                    (!session.bridgeRepaired&&originalCoverFingerprint!=6588246535391004864ull))
                    throw std::runtime_error("Original scanned forest-cover certified identity changed");
                Mistwatch::ForestCoverWorldView source{terrain,originalCoverTrees,originalCoverUnderstory,
                    originalCoverSolids,originalCoverWood,4335,0};
                Mistwatch::ForestCoverWorldView finalWorld{terrain,trees,understory,vegetationSolids,vegetationWood,
                    groupedWood?Mistwatch::WoodlandCanopy::FixedWoodParts:8617u,0,groupedWood};
                source.expectedInputFingerprint=Mistwatch::ForestCoverRetentionWorldFingerprint(source);
                finalWorld.expectedInputFingerprint=Mistwatch::ForestCoverRetentionWorldFingerprint(finalWorld);
                forestCoverRetention=Mistwatch::CreateForestCoverRetainedFromValidatedOriginal(originalToken,original,source,finalWorld,
                    grassFootprints,grassAssetIdentities,forestCoverSettings);
                if(woodlandWorld&&(forestCoverRetention.originalInputFingerprint!=15091677462053052260ull||
                    forestCoverRetention.finalInputFingerprint!=5744543264249080638ull||
                    forestCoverRetention.fingerprint!=12149160048223516636ull||forestCoverRetention.retained!=3571||
                    forestCoverRetention.filled!=29||forestCoverRetention.rejected!=29))
                    throw std::runtime_error("Woodland retained grass differs from complete CPU evidence");
                forestCover=forestCoverRetention.cover;
            }else forestCover=Mistwatch::CreateForestCover(terrain,trees,understory,vegetationSolids,grassFootprints,forestCoverSettings,vegetationWood,fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1);
            if(!forestCover.reachedBudget||forestCover.plants.size()!=forestCoverSettings.targetPlants)
                throw std::runtime_error("Forest cover authored population incomplete");
            groundCover.clear();groundCover.reserve(forestCover.plants.size());
            for(const auto& plant:forestCover.plants)groundCover.push_back(plant.transform.position);
        }
        forestCoverSetupMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-coverStart).count();
        if(diagnosticNettleMix){
            if(!entranceWorld.applied)throw std::runtime_error("Nettle mix requires authored complete forest");
            const auto before=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(false),
                after=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(true);
            nettleMix=Mistwatch::CreateNettleMix(forestCover,terrain,nettleBounds,vegetationSolids,before,after,woodlandWorld?&*woodlandWorld:nullptr);
            if(!nettleMix.accepted)throw std::runtime_error("Nettle mix accepted no real world plants");
        }
        // 候選只切繪製網格；原人口與原保守配置資產保持，並再驗修橋前後完整世界體積。
        if(diagnosticCurvedScannedGrass){
            if(forestCover.plants.empty())throw std::runtime_error("Curved scanned grass needs certified population");
            for(const bool repaired:{false,true}){
                const auto completeWorld=RuntimeWorldSolids(repaired);
                for(const auto& plant:forestCover.plants){
                    const auto& asset=curvedGrassVolumes[plant.variant];
                    const Mistwatch::CanopyVolume::PlantVolume volume{plant.transform.position,
                        Mistwatch::CanopyVolume::ConservativeLocalVolume(asset.radius,asset.minimumY,asset.maximumY),
                        plant.transform.scale.x,plant.transform.rotation};
                    if(woodlandWorld?!woodlandWorld->Clears(volume,repaired):!Mistwatch::CanopyVolume::ClearsCanopyWood(volume,completeWorld))
                        throw std::runtime_error("Curved scanned grass intersects actual complete world");
                    ++curvedGrassWorldChecks;
                }
            }
        }
        const auto colonyStart=std::chrono::steady_clock::now();
        if(!forestCover.plants.empty()&&!diagnosticLegacyFern){
            fernColonies=Mistwatch::CreateFernColonies(terrain,forestCover,understory.ferns,vegetationSolids,fernFootprints,fernColonySettings,vegetationWood,fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1,groupedWood);
            if(!fernColonies.reachedBudget||fernColonies.plants.size()!=fernColonySettings.targetPlants)
                throw std::runtime_error("Fern colony authored population incomplete");
        }
        fernColonySetupMilliseconds=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-colonyStart).count();
        // 固定世界配置只在初始化生成，不隨鏡頭或角色移動重生；收據保留根、變體與原碰撞。
        const auto mix=[](std::uint64_t& hash,std::uint32_t bits){
            for(unsigned byte=0;byte<4;++byte){hash^=(bits>>(byte*8))&255u;hash*=1099511628211ull;}
        };
        forestCoverFingerprint=forestCoverColliderFingerprint=14695981039346656037ull;
        fernColonyFingerprint=originalFernFingerprint=14695981039346656037ull;
        const auto fernTransformHash=[&](std::uint64_t& fingerprint,const Transform3D& plant,unsigned variant){
            for(float value:{plant.position.x,plant.position.y,plant.position.z,plant.rotation.x,plant.rotation.y,
                plant.rotation.z,plant.scale.x,plant.scale.y,plant.scale.z})mix(fingerprint,std::bit_cast<std::uint32_t>(value));
            mix(fingerprint,variant);
        };
        for(const auto& plant:fernColonies.plants){
            fernTransformHash(fernColonyFingerprint,plant.transform,plant.variant);
            ++fernColonyVariants[plant.variant];
        }
        for(std::size_t index=0;index<understory.ferns.size();++index)
            fernTransformHash(originalFernFingerprint,understory.ferns[index],unsigned(index%4));
        for(std::size_t index=0;index<groundCover.size();++index){
            const auto root=groundCover[index];
            const unsigned variant=forestCover.plants.empty()?unsigned(index%5):forestCover.plants[index].variant;
            ++groundCoverVariants[variant];
            for(float value:{root.x,root.y,root.z,Mistwatch::ForestCoverScale(root),root.x+root.z})
                mix(forestCoverFingerprint,std::bit_cast<std::uint32_t>(value));
            mix(forestCoverFingerprint,variant);
        }
        for(const auto& solid:solids)
            for(float value:{solid.center.x,solid.center.y,solid.center.z,solid.extents.x,solid.extents.y,solid.extents.z})
                mix(forestCoverColliderFingerprint,std::bit_cast<std::uint32_t>(value));
        controller.Reset({});
        auto shrubLeaves=LoadAevryndMeshFile(broadleafRoot/"meshes"/Mistwatch::BroadleafMeshName(1));
        if(!shrubLeaves)throw std::runtime_error("Shrub leaf source missing");
        if(shrubTarget!=64u&&(!entranceWorld.applied||fallenLogProfile!=Mistwatch::FallenLogProfile::ScannedStraightV1))
            throw std::runtime_error("Shrub density candidate requires complete authored world");
        startupStage("grass_and_fern_authoring");
        Mistwatch::ShrubCompositionContract shrubComposition;
        if(shrubArcDensity){
            // 明確選用新版；以同一世界的原64株驗證完整前綴，不能略過下游組合契約。
            const auto prefix=Mistwatch::ValidatedShrubArcPrefix::CreateOriginal64(shrubLeaves.mesh,terrain,vegetationSolids,understory,forestCover,fernColonies,fernFootprints,vegetationWood,fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1,groupedWood);
            const auto& original=prefix.Original();
            shrubCanopy=Mistwatch::CreateShrubArcDensityFromValidatedPrefix(prefix,original,shrubLeaves.mesh,terrain,vegetationSolids,understory,forestCover,fernColonies,fernFootprints,shrubTarget,vegetationWood,fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1,nullptr,groupedWood);
            shrubComposition=Mistwatch::ShrubCompositionContract::PreservedPrefix128(original,shrubCanopy);
        }else if(shrubGapDensity)
            shrubCanopy=Mistwatch::CreateShrubGapDensity(shrubLeaves.mesh,terrain,vegetationSolids,understory,forestCover,fernColonies,fernFootprints,shrubTarget,vegetationWood,fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1);
        else
            shrubCanopy=Mistwatch::CreateShrubCanopy(shrubLeaves.mesh,terrain,vegetationSolids,understory,forestCover,fernColonies,fernFootprints,shrubTarget,vegetationWood,fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1,nullptr,groupedWood);
        if(!shrubCanopy.reachedBudget)throw std::runtime_error("Shrub authored population incomplete");
        for(unsigned i=0;i<4;++i){
            shrubMeshes[i][0]=e.CreateMeshResource3D(shrubCanopy.variants[i].stems);
            const auto openCrown=Mistwatch::CreateShrubOpenCrown(shrubCanopy.variants[i],i);
            shrubMeshes[i][1]=e.CreateMeshResource3D(openCrown.leaves);
            shrubRenderLeafCounts[i]=openCrown.renderLeafCount;
            if(!shrubMeshes[i][0]||!shrubMeshes[i][1])throw std::runtime_error("Shrub GPU upload failed");
            if(shrubMeshes[i][0].indexCount!=shrubCanopy.variants[i].stems.indices.size()||
                shrubMeshes[i][1].indexCount!=openCrown.leaves.indices.size())
                throw std::runtime_error("Actual original shrub upload index count changed");
        }
        if(roadsideCoverEnabled||roadsideBroadleafEnabled){
            if(!entranceWorld.applied||!woodWorld||rightForeground||shrubTarget!=shrubComposition.ExpectedCount()||diagnosticProceduralGrass||
                diagnosticScatterCover||diagnosticCurvedScannedGrass||diagnosticNettleMix||diagnosticLegacyFern)
                throw std::runtime_error("Roadside cover requires original complete scanned authored world");
            const auto before=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(false),
                after=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(true);
            if(roadsideCoverEnabled)roadsideCover=Mistwatch::CreateRoadsideCover(terrain,forestCover,understory,fernColonies,shrubCanopy,
                grassFootprints,fernFootprints,before,after,shrubComposition,woodlandWorld?&*woodlandWorld:nullptr);
        }
        // 原網格上傳順序不動；新木牌在所有原資源之後只初始化一次。
        try {
            const auto geometry=Mistwatch::RepairSign::Create();
            repairSignMaterials=Mistwatch::RepairSign::Materials(woodMaterial);
            for(unsigned group=0;group<repairSignMeshes.size();++group) {
                auto& mesh=repairSignMeshes[group];
                mesh=e.CreateMeshResource3D(geometry.meshes[group]);
                if(!mesh||mesh.vertexCount!=Mistwatch::RepairSign::VertexCounts[group]||
                    mesh.indexCount!=Mistwatch::RepairSign::IndexCounts[group])
                    throw std::runtime_error("Repair sign actual upload mismatch");
            }
        } catch(...) {
            for(auto& mesh:repairSignMeshes){if(mesh)e.DestroyMesh3D(mesh.handle);mesh={};}
            repairSignMaterials={};
            throw;
        }
        // 原資源順序與草群不變；新增寬葉只使用原 CC0 掃描材質及兩種實際小型網格。
        if(roadsideBroadleafEnabled){
            const auto root=e.ExecutableDirectory()/"assets/mistwatch-nettle";
            roadsideBroadleafMaterials.Initialize(e,root);
            if(!roadsideBroadleafMaterials.Load(Mistwatch::NettleMaterialDefinition()))
                throw std::runtime_error("Roadside broadleaf material unavailable");
            roadsideBroadleafSurface=*roadsideBroadleafMaterials.Find("nettle");
            for(unsigned i=0;i<2;++i){
                auto loaded=LoadAevryndMeshFile(root/"meshes"/Mistwatch::NettleMeshName(Mistwatch::RoadsideBroadleafVariants[i]));
                if(!loaded)throw std::runtime_error("Roadside broadleaf actual mesh load failed");
                Mistwatch::NormalizeNettleAsset(loaded.mesh,Mistwatch::RoadsideBroadleafVariants[i]);
                roadsideBroadleafBounds[i]=Mistwatch::MeasureNettleAssetBounds(loaded.mesh);
                roadsideBroadleafIdentities[i]=Mistwatch::ForestGrassShapeFingerprint(loaded.mesh);
                roadsideBroadleafMeshes[i]=e.CreateMeshResource3D(loaded.mesh);
                if(!roadsideBroadleafMeshes[i])throw std::runtime_error("Roadside broadleaf actual upload failed");
            }
            const auto before=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(false),
                after=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(true);
            roadsideBroadleaf=Mistwatch::CreateRoadsideBroadleaf(terrain,forestCover,understory,fernColonies,shrubCanopy,
                grassFootprints,fernFootprints,roadsideBroadleafBounds,before,after,shrubComposition,woodlandWorld?&*woodlandWorld:nullptr);
        }
        startupStage("shrubs_and_roadside");
        std::optional<Mesh3D> cliffReliefForestSource;
        std::optional<Mesh3D> leafForestSource;
        if(entranceRoadEnabled){
            if(!entranceWorld.applied||!layeredGround||deciduousHabitat||
                (habitatFloorProfile!=Mistwatch::ForestHabitatFloorProfile::Original&&!leafTerrainSource))
                throw std::runtime_error("Entrance road requires original authored habitat");
            auto partition=Mistwatch::CreateEntranceRoadDomain(terrainCpuMesh);
            std::optional<Mistwatch::SettlementSurface::Result> splitSurface;
            settlementOriginalRoadReceipt=partition.receipt;
            if(settlementGroundEnabled){
                auto settlement=Mistwatch::SettlementGround::Create(terrainCpuMesh,partition);
                settlementMovedTriangles=settlement.movedSourceTriangles.size();
                settlementBoundaryVertices=settlement.newBoundaryVertices.size();
                settlementChangedForest=settlement.changedForestVertices;
                settlementChangedRoad=settlement.changedRoadVertices;
                if(settlementSurfaceEnabled)splitSurface=Mistwatch::SettlementSurface::Create(terrainCpuMesh,partition,settlement);
                partition=std::move(settlement.partition);
            }
            if(leafTerrainSource){
                if(!splitSurface||!cliffReliefEnabled||!woodlandWorld)
                    throw std::runtime_error("Leaf floor requires the complete split woodland relief world");
                const auto leafEntrance=Mistwatch::CreateEntranceRoadDomain(*leafTerrainSource);
                const auto leafSettlement=Mistwatch::SettlementGround::Create(*leafTerrainSource,leafEntrance);
                const auto leafSplit=Mistwatch::SettlementSurface::Create(*leafTerrainSource,leafEntrance,leafSettlement);
                auto leaf=Mistwatch::ForestLeafDomain::Create(terrainCpuMesh,*leafTerrainSource,*splitSurface,leafSplit);
                leafFloorDomainReceipt=leaf.receipt;
                leafForestSource=std::move(leaf.mesh);
            }
            entranceRoadReceipt=partition.receipt;
            entranceRoadMaterials.Initialize(e,e.ExecutableDirectory()/"assets/mistwatch-materials");
            entranceRoadDefinition=Mistwatch::EntranceRoadMaterialDefinition(domainGroundDefinition);
            if(!entranceRoadMaterials.Load(entranceRoadDefinition))
                throw std::runtime_error("Entrance road material load failed");
            entranceRoadSurface=*entranceRoadMaterials.Find("entrance-road");
            entranceRoadSurface.useUv=true;
            entranceRoadSurface.normalScale=TerrainLayerCoverageNormalScaleMarker3D;
            if(splitSurface){
                settlementSurfaceDefinition=Mistwatch::SettlementSurface::MaterialDefinition(domainGroundDefinition);
                if(!entranceRoadMaterials.Load(settlementSurfaceDefinition))throw std::runtime_error("Settlement gravel material load failed");
                settlementSurfaceMaterial=*entranceRoadMaterials.Find("settlement-gravel");
                settlementSurfaceMaterial.useUv=true;
                settlementSurfaceMaterial.normalScale=TerrainLayerCoverageNormalScaleMarker3D;
                settlementSurfaceFingerprints=splitSurface->fingerprints;
                entranceRoadMeshes[2]=e.CreateMeshResource3D(splitSurface->gravel);
                if(!entranceRoadMeshes[2]||!entranceRoadMeshes[2].cpuMesh||
                    Mistwatch::EntranceRoadDomainDetail::Fingerprint(*entranceRoadMeshes[2].cpuMesh)!=settlementSurfaceFingerprints[2]||
                    !settlementSurfaceMaterial.textures||settlementSurfaceMaterial.textures.semanticIdentity==entranceRoadSurface.textures.semanticIdentity||
                    settlementSurfaceMaterial.textures.semanticIdentity==groundMaterial.textures.semanticIdentity)
                    throw std::runtime_error("Settlement gravel upload or material identity differs");
            }
            const auto& originalForest=splitSurface?splitSurface->forest:partition.forest;
            if(cliffReliefEnabled)cliffReliefForestSource=originalForest;
            const auto canonicalForest=splitSurface?settlementSurfaceFingerprints[0]:entranceRoadReceipt.forestFingerprint;
            if(Mistwatch::EntranceRoadDomainDetail::Fingerprint(originalForest)!=canonicalForest)
                throw std::runtime_error("Cliff original forest canonical identity mismatch");
            auto actualForestFingerprint=canonicalForest;
            if(cliffDiagnostic){
                auto candidate=Mistwatch::CliffCoverage::Create(originalForest,terrain,Mistwatch::CliffCoverage::Domain::Core,
                    cliffCoverageEnabled?Mistwatch::CliffCoverage::Profile::WestCliffV1:Mistwatch::CliffCoverage::Profile::Original);
                cliffReceipts[0]=candidate.receipt;actualForestFingerprint=candidate.receipt.afterFingerprint;
                entranceRoadMeshes[0]=e.CreateMeshResource3D(candidate.mesh);
            }else entranceRoadMeshes[0]=e.CreateMeshResource3D(originalForest);
            entranceRoadMeshes[1]=e.CreateMeshResource3D(splitSurface?splitSurface->entrance:partition.road);
            if(!entranceRoadMeshes[0]||!entranceRoadMeshes[1])
                throw std::runtime_error("Entrance road material domain upload failed");
            if(!entranceRoadMeshes[0].cpuMesh||!entranceRoadMeshes[1].cpuMesh||
                Mistwatch::EntranceRoadDomainDetail::Fingerprint(*entranceRoadMeshes[0].cpuMesh)!=actualForestFingerprint||
                Mistwatch::EntranceRoadDomainDetail::Fingerprint(*entranceRoadMeshes[1].cpuMesh)!=(splitSurface?settlementSurfaceFingerprints[1]:entranceRoadReceipt.roadFingerprint)||
                !entranceRoadSurface.textures||entranceRoadSurface.textures.semanticIdentity==groundMaterial.textures.semanticIdentity)
                throw std::runtime_error("Entrance road uploaded material or mesh identity differs");
            if(splitSurface&&entranceRoadMeshes[0].TriangleCount()+entranceRoadMeshes[1].TriangleCount()+entranceRoadMeshes[2].TriangleCount()!=entranceRoadReceipt.sourceTriangles)
                throw std::runtime_error("Settlement material split lost or duplicated terrain triangles");
        }
        if(entranceUnderstoryEnabled){
            // 共用既有灌木網格與材質；原64株保持，新群落只接替明確衝突的軟地被顯示實例。
            const auto before=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(false),
                after=woodlandWorld?std::vector<BoxCollider3D>{}:RuntimeWorldSolids(true);
            entranceUnderstory=Mistwatch::CreateEntranceUnderstory(terrain,forestCover,understory,fernColonies,
                shrubCanopy,grassFootprints,fernFootprints,roadsideBroadleaf,before,after,nullptr,shrubComposition,woodlandWorld?&*woodlandWorld:nullptr);
        }
        startupStage("road_domains_materials_understory");
        e.SetClearColor({.16f,.23f,.25f,1});
        wchar_t local[32768]{};
        const auto count=GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768);
        // 美術預設升格不搬移、刪除或覆寫玩家舊存檔；隔離診斷仍使用原分支名稱。
        if(woodlandWorld&&(!count||count>=32768||!std::filesystem::path(local).is_absolute()))
            throw std::runtime_error("Woodland candidate requires valid LOCALAPPDATA");
        if(count && count<32768) save=std::filesystem::path(local)/"Aevrynd"/"Mistwatch"/
            (woodlandWorld?"save-woodland-canopy-v2.txt":!validProfileCapture?"save.txt":shelterDetailEnabled?"save-framed-corrugated-shelter-v1.txt":entranceUnderstoryEnabled?"save-entrance-understory-v2.txt":entranceRoadEnabled?"save-entrance-road-material-domain-v1.txt":rightForeground?"save-entrance-right-foreground-v1.txt":roadsideCoverEnabled?"save-roadside-ground-cover-v1.txt":
                roadsideBroadleafEnabled?"save-roadside-broadleaf-v1.txt":"save.txt");
        if(valleyWorldEnabled){
            if(!count||count>=32768||!std::filesystem::path(local).is_absolute())throw std::runtime_error("Valley requires valid LOCALAPPDATA");
            const auto root=std::filesystem::path(local)/"Aevrynd"/"Mistwatch";
            save=root/"save-valley-contact-v7.txt";
            if(manualValleyCandidate){const auto sessions=root/"valley-sessions";std::filesystem::create_directories(sessions);
                const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
                valleyEvidenceDirectory=sessions/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(stamp));
                if(!std::filesystem::create_directory(valleyEvidenceDirectory))throw std::runtime_error("Manual valley receipt refuses reuse");
            }
        }
        if(manualForestSurfaceCandidate){
            if(!count||count>=32768||!std::filesystem::path(local).is_absolute())
                throw std::runtime_error("Forest surface candidate requires valid LOCALAPPDATA");
            // 手動遊玩不是截圖模式；使用獨立存檔及每次唯一的實測收據目錄。
            const auto root=std::filesystem::path(local)/"Aevrynd"/"Mistwatch";
            save=root/"save-forest-surface-v1.txt";
            const auto sessions=root/"surface-sessions";
            std::filesystem::create_directories(sessions);
            const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
            const auto surfaceSessionPath=sessions/(std::to_string(GetCurrentProcessId())+"-"+std::to_string(stamp));
            if(!std::filesystem::create_directory(surfaceSessionPath))throw std::runtime_error("Forest surface session refuses reuse");
            manualForestSurfaceReceiptPrefix=surfaceSessionPath/"last-render";
        }
        // 新屋頂使用獨立存檔；舊世界的存檔不能被無聲搬移或覆寫。
        if(workshopRoof){
            if(save.empty()||!save.is_absolute())throw std::runtime_error("Workshop roof candidate requires isolated absolute save path");
            save=save.parent_path()/(save.stem().string()+"-roof-v1-"+std::to_string(workshopRoof->visualFingerprint)+save.extension().string());
        }
        // 新世界候選保留獨立玩家存檔，不覆寫既有屋頂或森林候選。
        if(cliffWorldEnabled){
            if(save.empty()||!save.is_absolute())throw std::runtime_error("Cliff world requires isolated absolute save path");
            save=save.parent_path()/(save.stem().string()+"-cliff-rock-face-03-v1"+save.extension().string());
        }
        if(cliffReliefEnabled){
            if(save.empty()||!save.is_absolute())throw std::runtime_error("Relief requires isolated save path");
            save=save.parent_path()/(save.stem().string()+"-west-cliff-standing45-v1"+save.extension().string());
        }
        wchar_t output[32768]{};
        const auto outputCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",output,32768);
        wchar_t replayOutput[32768]{};
        const auto replayCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_INPUT_REPLAY",replayOutput,32768);
        wchar_t costProbe[8]{};
        const auto costCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_REPLAY_COST_PROBE",costProbe,8);
        if(costCount&&(costCount!=1||costProbe[0]!=L'1'||!replayCount||outputCount))
            throw std::runtime_error("Frame cost probe requires isolated input replay");
        inputReplayCostEnabled=costCount==1;
        wchar_t captureCost[8]{};
        const auto captureCostCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_COST_PROBE",captureCost,8);
        if(captureCostCount&&(captureCostCount!=1||captureCost[0]!=L'1'||!outputCount||replayCount||costCount))
            throw std::runtime_error("Capture cost probe requires separate absolute capture output");
        captureCostEnabled=captureCostCount==1;
        if(captureCostEnabled){
            const std::filesystem::path outputPath(output);
            if(outputCount>=32768||!outputPath.is_absolute()||!std::filesystem::is_directory(outputPath.parent_path()))
                throw std::runtime_error("Capture cost probe output parent must exist");
            captureCostDirectory=outputPath.parent_path();
            if(std::filesystem::exists(captureCostDirectory/"frame-cost.tsv")||std::filesystem::exists(captureCostDirectory/"frame-cost.txt"))
                throw std::runtime_error("Capture cost probe cannot overwrite existing evidence");
        }
        // 短截圖保持原8192筆on-stop-only；長回放稍後在全新證據目錄啟用有界分段記錄。
        if(captureCostEnabled)inputReplayCost.Begin();
        wchar_t replayRoute[16]{};
        const auto replayRouteCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_INPUT_REPLAY_ROUTE",replayRoute,16);
        if(replayRouteCount&&(!replayCount||replayRouteCount>=16||
            (std::wstring_view(replayRoute)!=L"shelter"&&std::wstring_view(replayRoute)!=L"bridge"&&std::wstring_view(replayRoute)!=L"combat"&&std::wstring_view(replayRoute)!=L"power"&&std::wstring_view(replayRoute)!=L"completion"&&std::wstring_view(replayRoute)!=L"death-retry"&&std::wstring_view(replayRoute)!=L"checkpoint"&&std::wstring_view(replayRoute)!=L"motion-save"&&std::wstring_view(replayRoute)!=L"pivot-save"&&std::wstring_view(replayRoute)!=L"west-cliff-47"&&std::wstring_view(replayRoute)!=L"west-cliff-44"&&std::wstring_view(replayRoute)!=L"dash-handoff"&&std::wstring_view(replayRoute)!=L"sprint-flow"&&std::wstring_view(replayRoute)!=L"resume-save"&&std::wstring_view(replayRoute)!=L"reject-save"&&std::wstring_view(replayRoute)!=L"grove-walk")))
            throw std::runtime_error("Invalid input replay route");
        if((std::wstring_view(replayRoute)==L"west-cliff-47"||std::wstring_view(replayRoute)==L"west-cliff-44")&&!cliffReliefEnabled)
            throw std::runtime_error("West cliff replay requires the slope world candidate");
        if(replayCount){
            if(replayCount>=32768||outputCount)throw std::runtime_error("Input replay requires a separate output without capture fixtures");
            inputReplayDirectory=replayOutput;
            if(!inputReplayDirectory.is_absolute()||std::filesystem::exists(inputReplayDirectory))
                throw std::runtime_error("Input replay requires a fresh absolute output directory");
            std::filesystem::create_directories(inputReplayDirectory);
            if(inputReplayCostEnabled&&!inputReplayCost.BeginStreaming(inputReplayDirectory))
                throw std::runtime_error("Replay cost streaming evidence initialization rejected");
            if(e.WindowMode3D()==EngineWindowMode3D::OffscreenNoActivateDiagnostic){
                RECT rect{},client{};const auto desktop=DiagnosticWindow3D::VirtualDesktop();
                if(!GetWindowRect(e.Window(),&rect)||!GetClientRect(e.Window(),&client))throw std::runtime_error("Replay window receipt unavailable");
                DiagnosticWindow3D::RequireValid(e.Window(),client.right-client.left,client.bottom-client.top);
                std::ofstream windowReceipt(inputReplayDirectory/"replay-window.txt");
                windowReceipt<<"mode=offscreen-noactivate\nvirtual_rect="<<desktop.left<<','<<desktop.top<<','<<desktop.right<<','<<desktop.bottom
                    <<"\nwindow_rect="<<rect.left<<','<<rect.top<<','<<rect.right<<','<<rect.bottom
                    <<"\nclient_size="<<client.right-client.left<<','<<client.bottom-client.top
                    <<"\nextended_style="<<GetWindowLongPtrW(e.Window(),GWL_EXSTYLE)
                    <<"\nvisible="<<IsWindowVisible(e.Window())<<"\niconic="<<IsIconic(e.Window())
                    <<"\nforeground="<<(GetForegroundWindow()==e.Window())<<"\nstable_display_topology_required=1\n";
                windowReceipt.close();if(!windowReceipt)throw std::runtime_error("Replay window receipt write failed");
            }
            // 記錄真正完成初始化的資源與正常存檔位置，再切換測試隔離存檔。
            std::ofstream artReceipt(inputReplayDirectory/"playable-art.txt");
            artReceipt<<"capture_mode="<<validProfileCapture<<"\nplayer_save_name="<<save.filename().string()
                <<"\nwoodland_candidate="<<woodlandCanopyEnabled<<"\nnormal_default_changed=0\nnormal_default_changed_scope=woodland"
                <<"\nsettlement_surface_normal_default_changed="<<settlementSurfaceEnabled
                <<"\nshelter_detail="<<shelterDetailEnabled<<"\nshelter_frame_members="<<shelterDetailFrameMembers
                <<"\nworkshop_frame="<<workshopFrameEnabled<<"\nworkshop_fingerprint="<<workshopFrameFingerprint
                <<"\nworkshop_vertices="<<workshopMesh.vertexCount<<"\nworkshop_indices="<<workshopMesh.indexCount
                <<"\nentrance_road="<<entranceRoadEnabled<<"\nroad_meshes="<<unsigned(bool(entranceRoadMeshes[0]))+unsigned(bool(entranceRoadMeshes[1]))+unsigned(bool(entranceRoadMeshes[2]))
                <<"\nsettlement_ground_profile="<<(settlementGroundEnabled?Mistwatch::SettlementGround::Profile:"original")
                <<"\nsettlement_ground_moved_triangles="<<settlementMovedTriangles
                <<"\nsettlement_ground_boundary_vertices="<<settlementBoundaryVertices
                <<"\nsettlement_ground_changed_forest="<<settlementChangedForest
                <<"\nsettlement_ground_changed_road="<<settlementChangedRoad
                <<"\nsettlement_ground_original_forest_fingerprint="<<settlementOriginalRoadReceipt.forestFingerprint
                <<"\nsettlement_ground_original_road_fingerprint="<<settlementOriginalRoadReceipt.roadFingerprint
                <<"\nentrance_road_source_fingerprint="<<entranceRoadReceipt.sourceFingerprint
                <<"\nentrance_road_forest_fingerprint="<<entranceRoadReceipt.forestFingerprint
                <<"\nentrance_road_road_fingerprint="<<entranceRoadReceipt.roadFingerprint
                <<"\nentrance_road_source_triangles="<<entranceRoadReceipt.sourceTriangles
                <<"\nentrance_road_forest_triangles="<<entranceRoadReceipt.forestTriangles
                <<"\nentrance_road_road_triangles="<<entranceRoadReceipt.roadTriangles
                <<"\nentrance_road_shared_vertices="<<entranceRoadReceipt.sharedVertices
                <<"\nentrance_road_texture_identity="<<entranceRoadSurface.textures.semanticIdentity
                <<"\nentrance_road_materials="<<entranceRoadMaterials.Stats().materials
                <<"\nroadside_broadleaf="<<roadsideBroadleafEnabled<<"\nentrance_understory="<<entranceUnderstoryEnabled
                <<"\nshrubs_profile="<<(shrubArcDensity?"woodland-feasible-arc-v3":shrubGapDensity?"woodland-gap-density-v2":shrubTarget==64u?"original":"woodland-density-v1")
                <<"\nshrubs_target="<<shrubTarget<<"\nshrubs_plants="<<shrubCanopy.plants.size()
                <<"\nshrubs_fingerprint="<<shrubCanopy.fingerprint
                <<"\nstand_profile="<<(landscapeStandFill?Mistwatch::LandscapeStandFill::Profile:"original")
                <<"\nstand_base_population="<<landscapeTrees.size()
                <<"\nstand_added_population="<<landscapeStandPlan.addedBands.size()
                <<"\nstand_complete="<<landscapeStandPlan.complete
                <<"\nstand_pose_fingerprint="<<landscapeStandPlan.fingerprint
                <<"\nstand_source_fingerprint="<<landscapeStandPlan.sourceFingerprint
                <<"\ntwig_profile="<<(landscapeTwigDistanceLod?"crown-distance-lod-v1":captureLandscapeTwigLod1?"crown-lod1-source-v1":"original")
                <<"\ntwig_range_profile="<<landscapeCrownRange.label
                <<"\ntwig_switch_distance="<<(landscapeTwigDistanceLod?landscapeCrownRange.switchDistance:0.f)
                <<"\ntwig_transition_width="<<(landscapeTwigDistanceLod?landscapeCrownRange.transitionWidth:0.f)
                <<"\ntwig_near_mesh_handle="<<landscapeDetailedFirTwig.handle.value
                <<"\ntwig_near_mesh_triangles="<<landscapeDetailedFirTwig.TriangleCount()
                <<"\ntwig_near_mesh_fingerprint="<<landscapeDetailedTwigFingerprint
                <<"\ntwig_far_mesh_handle="<<landscapeFirMeshes[1].handle.value
                <<"\ntwig_far_mesh_triangles="<<landscapeFirMeshes[1].TriangleCount()
                <<"\ntwig_far_mesh_fingerprint="<<landscapeFirFingerprints[1]
                <<"\nboulder_color="<<boulderColorProfile<<"\nboulder_geometry="<<boulderGeometryProfile
                <<"\nboulder_base_hash="<<boulderBaseHash<<'\n';
            WriteWorkshopRoofReceipt(artReceipt);
            WriteSettlementSurface(artReceipt);
            artReceipt.flush();
            if(!artReceipt)throw std::runtime_error("Playable art initialization receipt unavailable");
            save=inputReplayDirectory/"test-save.txt";
            if(replayRouteCount&&(std::wstring_view(replayRoute)==L"resume-save"||std::wstring_view(replayRoute)==L"reject-save")&&
                !inputReplay.EnableResumeSaveRoute(std::wstring_view(replayRoute)==L"resume-save"))throw std::runtime_error("Resume replay configuration rejected");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"dash-handoff"&&!inputReplay.EnableDashJumpRoute())
                throw std::runtime_error("Input dash handoff replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"sprint-flow"&&!inputReplay.EnableSprintFlowRoute())
                throw std::runtime_error("Input sprint flow replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"bridge"&&!inputReplay.EnableBridgeRoute())
                throw std::runtime_error("Bridge input replay configuration rejected");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"combat"&&!inputReplay.EnableCombatRoute())
                throw std::runtime_error("Input combat replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"power"&&!inputReplay.EnablePowerRoute())
                throw std::runtime_error("Input power replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"completion"&&!inputReplay.EnableCompletionRoute())
                throw std::runtime_error("Input completion replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"death-retry"&&!inputReplay.EnableDeathRetryRoute())
                throw std::runtime_error("Input death/retry replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"checkpoint"&&!inputReplay.EnableCheckpointRoute())
                throw std::runtime_error("Input checkpoint replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"grove-walk"&&
                (!forestPlayableWorld||!valleySlopeGrove||!inputReplay.EnableGroveRoute()))
                throw std::runtime_error("Grove replay requires the complete forest grove world");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"motion-save"&&!inputReplay.EnableMotionCheckpointRoute())
                throw std::runtime_error("Input motion checkpoint replay route unavailable");
            if(replayRouteCount&&std::wstring_view(replayRoute)==L"pivot-save"&&!inputReplay.EnablePivotCheckpointRoute())
                throw std::runtime_error("Input pivot checkpoint replay route unavailable");
            if(replayRouteCount&&(std::wstring_view(replayRoute)==L"west-cliff-47"||std::wstring_view(replayRoute)==L"west-cliff-44")&&
                !inputReplay.EnableWestCliffRoute(std::wstring_view(replayRoute)==L"west-cliff-47"?-47.f:-44.f,cliffReliefEnabled))
                throw std::runtime_error("Input west cliff replay route unavailable");
            inputReplayTrace.open(inputReplayDirectory/"input-trace.tsv");
            inputReplayTrace<<"tick\tphase\tx\ty\tz\theight\tmaterials\tcamera_accepted\tfeedback\tcollected\tbridge\thealth\tkeys\tenemy_phase\tenemy_time\tenemy_x\tenemy_y\tenemy_z\tstrike_x\tstrike_z\tenemy_health\tcountered\tcounter_cooldown\tdash_remaining\tdash_cooldown\tgrounded\tprevious_dashing\tprevious_damage\tclue\tdials\tfuse\tcircuit\tpower\tlinks\tsectors\tfield_tasks\tpatch_a\tpatch_b\textraction\tbeacon\tsignal_0\tsignal_1\tsignal_2\tground_y\tstrike_y\tdash_x\tdash_y\tdash_z\tguardian\tdt\tdeath_progress\tpelvis_y\thead_y"
                <<"\tdeath_supported\tdeath_limited\tdeath_clearance\tleft_knee_gap\tright_knee_gap\tleft_hand_gap\tright_hand_gap"
                <<"\tdisk_exists\tdisk_valid\tstate_hex\tdisk_hex\tcamera_status\tcamera_visible\texplorer_finite\tboar_finite\tvelocity_x\tvelocity_y\tvelocity_z\ttransient_bits"
                <<"\truntime_terrain_y\tmotion_tick\tmotion_attempted\tmotion_accepted\tmotion_camera_attempted\tmotion_camera_rollback\tmotion_committed_steps\tworld_solids\tworld_roof_hulls"
                <<"\tleft_active\tleft_elapsed\tleft_duration\tleft_lift\tleft_landing_x\tleft_landing_y\tleft_landing_z\tleft_current_x\tleft_current_y\tleft_current_z\tleft_facing"
                <<"\tleft_hip_x\tleft_hip_y\tleft_hip_z\tleft_knee_x\tleft_knee_y\tleft_knee_z\tleft_ankle_x\tleft_ankle_y\tleft_ankle_z"
                <<"\tright_active\tright_elapsed\tright_duration\tright_lift\tright_landing_x\tright_landing_y\tright_landing_z\tright_current_x\tright_current_y\tright_current_z\tright_facing"
                <<"\tright_hip_x\tright_hip_y\tright_hip_z\tright_knee_x\tright_knee_y\tright_knee_z\tright_ankle_x\tright_ankle_y\tright_ankle_z\n";
            if(!inputReplayTrace)throw std::runtime_error("Input replay trace unavailable");
        }
        if(outputCount && outputCount<32768) {
            wchar_t flag[3]{};const auto flagCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CANOPY_ENABLED",flag,3);
            if(flagCount){if(flagCount!=1||(flag[0]!=L'0'&&flag[0]!=L'1'))throw std::runtime_error("Invalid canopy preview flag");if(flag[0]==L'1'&&!Mistwatch::WorldProfileHasCanopy(worldProfile))throw std::runtime_error("Canopy draw requires canopy world");canopyEnabled=flag[0]!=L'0';}
            wchar_t angle[16]{};const auto angleCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CANOPY_VIEW",angle,16);
            if(angleCount){
                const std::wstring text=angle;unsigned degrees{};
                if(text==L"0")degrees=0;else if(text==L"45")degrees=45;else if(text==L"60")degrees=60;else if(text==L"120")degrees=120;
                else if(text==L"180")degrees=180;else if(text==L"240")degrees=240;else if(text==L"300")degrees=300;
                else if(text==L"detail"){degrees=45;canopyViewHeight=3.8f;canopyViewDistance=5.657f;canopyViewTargetHeight=3.f;}
                else if(text==L"under"){degrees=0;canopyViewHeight=1.7f;canopyViewDistance=4.8f;canopyViewTargetHeight=3.2f;}
                else if(text==L"root"){degrees=45;canopyViewHeight=.35f;canopyViewDistance=2.f;canopyViewTargetHeight=.3f;}
                else if(text==L"high"){degrees=45;canopyViewHeight=8.f;canopyViewDistance=7.071f;canopyViewTargetHeight=2.2f;}
                else throw std::runtime_error("Invalid canopy preview angle");
                canopyViewYaw=float(degrees)*3.1415926535f/180.f;
            }
            wchar_t cameraFlag[3]{};const auto cameraCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_FRAME_CAMERA",cameraFlag,3);
            if(cameraCount){if(cameraCount!=1||cameraFlag[0]!=L'1')throw std::runtime_error("Invalid fixed-frame camera flag");captureFrameCamera=true;}
            wchar_t shrubFlag[3]{};const auto shrubCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_SHRUBS",shrubFlag,3);
            if(shrubCount){if(shrubCount!=1||(shrubFlag[0]!=L'0'&&shrubFlag[0]!=L'1'))throw std::runtime_error("Invalid shrub diagnostic flag");shrubsEnabled=shrubFlag[0]!=L'0';}
            // 同執行檔單變量比較；正式遊玩不允許此環境開關關閉裝飾層。
            wchar_t litterFlag[3]{};
            const auto litterCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_TRAIL_LITTER",litterFlag,3);
            if(litterCount){
                if(litterCount!=1||(litterFlag[0]!=L'0'&&litterFlag[0]!=L'1'))throw std::runtime_error("Invalid trail litter comparison flag");
                trailLitterEnabled=litterFlag[0]!=L'0';
            }
            capture=output;
            captureStart=std::chrono::steady_clock::now();
            started=true;
            waterTime=2.5f;
            wchar_t sequence[4]{};
            if(GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_SERIES",sequence,4)>0 && sequence[0]==L'1')captureSequenceFrames=36;
            wchar_t benchmark[4]{};
            captureBenchmark=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_BENCHMARK",benchmark,4)>0 && benchmark[0]==L'1';
            if(captureBenchmark && captureSequenceFrames)throw std::runtime_error("Benchmark and capture series must run separately");
            message="Capture mode - initial playable scene";
            // 階段截圖只使用記憶體測試資料，不讀寫玩家存檔。
            wchar_t scenario[32]{};
            const auto scenarioCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_SCENE",scenario,32);
            if(scenarioCount) {
                if(scenarioCount>=32) throw std::runtime_error("Capture scenario too long");
                const std::wstring name=scenario;
                std::string fixture;
                if(name==L"panel") fixture="MISTWATCH 3 7 80 1 1 0 5.4 0 65 1 0 3";
                else if(name==L"powered") fixture="MISTWATCH 3 7 80 1 1 0 5.4 0 65 1 1 7";
                else if(name==L"complete") fixture="MISTWATCH 3 7 80 1 1 1 0 0 69 1 1 7";
                else if(name==L"relay-west"||name==L"relay-aligned"||name==L"relay-linked"||name==L"relay-turn"||name==L"ridge-pan") {
                    fixture="MISTWATCH 5 7 80 1 1 0 0 0 65 1 1 7 7 1 50";
                    captureRelayMotion=name==L"relay-turn";
                    captureLandscapeMotion=name==L"ridge-pan";
                }
                else if(name==L"strike"||name==L"boar-recovery"||name==L"boar-motion"||name==L"boar-profile") {
                    fixture=name==L"boar-motion"?"MISTWATCH 3 7 100 1 0 0 0 0 43 0 0 0":"MISTWATCH 3 7 100 1 0 0 0 0 46.5 0 0 0";
                    captureWildlife=name==L"boar-motion";
                    captureBoarOverview=name==L"boar-profile";
                }
                else if(name==L"river" || name==L"river-overlook") {
                    fixture="MISTWATCH 3 7 100 0 0 0 0 0 30 0 0 0";
                    captureRiverOverview=name==L"river-overlook";
                }
                else if(name==L"repair-shelter" || name==L"repair-player-camera") fixture="MISTWATCH 4 7 100 0 0 0 -7.25 0 18 0 0 0 0";
                else if(name==L"repair-ready") fixture="MISTWATCH 4 7 100 0 0 0 0 0 30 0 0 0 7";
                else if(name==L"settlement-clue") fixture="MISTWATCH 5 7 100 1 1 0 7.15 0 79 0 0 0 7 0 0";
                else if(name==L"settlement-clue-read") fixture="MISTWATCH 5 7 100 1 1 0 7.15 0 79 0 0 0 7 1 0";
                else if(name==L"ranger-hut-profile") {
                    captureRangerHutOverview=true;
                    fixture="MISTWATCH 5 7 100 1 1 0 2 0 79 0 0 0 7 0 0";
                }
                else if(name==L"shoulder-west"||name==L"shoulder-east"||
                        name==L"shoulder-west-eye"||name==L"shoulder-east-eye"){
                    captureShoulderView=(name==L"shoulder-west"||name==L"shoulder-west-eye")?0:1;
                    captureShoulderEyeLevel=name==L"shoulder-west-eye"||name==L"shoulder-east-eye";
                    fixture="MISTWATCH 5 7 100 1 1 0 -6.9 0 59 0 0 0 7 1 0";
                }
                else if(name==L"workshop-profile"||name==L"workshop-ground-wide") {
                    captureWorkshopOverview=true;
                    captureWorkshopGroundWide=name==L"workshop-ground-wide";
                    fixture="MISTWATCH 5 7 100 1 1 0 -6.9 0 59 0 0 0 7 1 0";
                }
                else if(name==L"fuse-lock-wrong") fixture="MISTWATCH 5 7 100 1 1 0 -6.9 0 59 0 0 0 7 1 0";
                else if(name==L"fuse-lock-ready") fixture="MISTWATCH 5 7 100 1 1 0 -6.9 0 59 0 0 0 7 1 50";
                else if(name==L"focus-pause") {captureFocusProbe=true;fixture="MISTWATCH 3 7 100 1 0 0 0 0 46.5 0 0 0";}
                else if(name==L"walk" || name==L"crouch"||name==L"audio-check") {captureWalk=true;captureCrouch=name==L"crouch";captureAudioProbe=name==L"audio-check";}
                else if(name==L"boulder-profile")captureBoulderOverview=true;
                else if(name==L"trail-profile")captureTrailOverview=true;
                else if(name==L"shrub-profile")captureShrubOverview=true;
                else if(name==L"canopy-profile")captureCanopyOverview=true;
                else if(name==L"grass-profile")captureGrassOverview=true;
                else if(name==L"supply-profile"||name==L"supply-collected") {
                    captureSupplyOverview=true;
                    session.feet=Mistwatch::Session::supplies[0]+Vec3{0,0,-2};
                    if(name==L"supply-collected"&&!session.Collect(0))throw std::runtime_error("Supply collection fixture failed");
                }
                else if(name==L"grass-distance") {
                    if(diagnosticNettleMix)throw std::runtime_error("Nettle mix cannot impersonate original grass distance fixture");
                    captureGrassDistance=true;
                    wchar_t mode[32]{},enabled[3]{};
                    const auto modeLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_FIXTURE",mode,32);
                    const auto enabledLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_GRASS_FIXTURE_ENABLED",enabled,3);
                    if(!modeLength||modeLength>=32||enabledLength!=1||(enabled[0]!=L'0'&&enabled[0]!=L'1')||
                        captureSequenceFrames||captureBenchmark||diagnosticProceduralGrass||diagnosticLegacyFern)
                        throw std::runtime_error("Invalid grass distance fixture configuration");
                    grassFixtureEnabled=enabled[0]==L'1';
                    const std::wstring_view distanceMode=mode;
                    if(distanceMode==L"player-near")grassFixturePlayerDistance=2.f;
                    else if(distanceMode==L"player-inside")grassFixturePlayerDistance=34.99f;
                    else if(distanceMode==L"player-edge")grassFixturePlayerDistance=35.f;
                    else if(distanceMode==L"player-outside")grassFixturePlayerDistance=35.01f;
                    else if(distanceMode==L"image-near")grassFixtureCameraDistance=2.f;
                    else if(distanceMode==L"image-middle")grassFixtureCameraDistance=12.f;
                    else if(distanceMode==L"image-far")grassFixtureCameraDistance=35.f;
                    else throw std::runtime_error("Unknown grass distance fixture mode");
                    // 每個程序從相同初始原配置選取同一株，不因腳或鏡頭移動重新尋找最近植株。
                    const auto plant=std::find_if(groundCover.begin(),groundCover.end(),[](Vec3 p){return Length(p)<12.f;});
                    if(plant==groundCover.end())throw std::runtime_error("Grass distance root missing");
                    grassFixtureIndex=std::size_t(plant-groundCover.begin());
                    // 同高選擇座標不是合法出生點；診斷不能把真正玩家／鏡頭放到遠坡地底。
                    grassFixtureSelectionFeet=*plant+Vec3{grassFixturePlayerDistance,0,0};
                }
                else if(name==L"fern-profile")captureFernOverview=true;
                else throw std::runtime_error("Unknown capture scenario");
                if(!fixture.empty() && !session.Restore(fixture)) throw std::runtime_error("Invalid capture fixture");
                if(name==L"relay-west"||name==L"relay-aligned"||name==L"relay-linked"||name==L"relay-turn"||name==L"ridge-pan") {
                    session.feet=Mistwatch::Session::relaySites[0]+Vec3{1.15f,0,0};
                    session.feet.y=terrain.SampleHeight(session.feet.x,session.feet.z);
                    if(name==L"relay-linked"||name==L"relay-turn") {
                        // 只有指定成功展示補真局部前置；aligned保留100訊號但尚未確認的狀態。
                        if(!session.CycleRelayPatch(0)||!session.ConfirmRelayCircuit())
                            throw std::runtime_error("Relay fixture local circuit confirmation failed");
                    }
                    if(name==L"relay-aligned"||name==L"relay-linked") {
                        for(unsigned turn=0;turn<Mistwatch::Session::RelayTargetSector(0);++turn)
                            if(!session.TuneRelay(0,1))throw std::runtime_error("Relay fixture tuning failed");
                        if(name==L"relay-linked"&&!session.ConnectRelay(0))throw std::runtime_error("Relay fixture link failed");
                    }
                }
                if(captureFocusProbe)session.feet=FocusProbeStartFeet();
                controller.Reset(session.feet);
                solids=RuntimeWorldSolids(session.bridgeRepaired);
                ResetController();
                if(captureFocusProbe){
                    const auto body=Mistwatch::PairContact::LiveBody(session,encounter);
                    BoxCylinderContact3D contact;
                    if(!body||!QueryBoxCylinderContact3D(controller.Center(),controller.Extents(),{},*body,contact)||
                       contact.kind!=BoxCylinderContactKind3D::Clear)
                        throw std::runtime_error("Focus probe fixture starts in dynamic contact");
                }
                if(name==L"repair-shelter" || name==L"repair-player-camera") {
                    captureShelterOverview=name==L"repair-shelter";
                    explorer.Update({},1.f/60.f,true);
                    if(!explorer.PlaceFeet(session.feet,true,true,terrain,solids,RuntimeRoofHulls()))throw std::runtime_error("Shelter capture IK failed");
                }
                orbit.yaw=captureWalk?-.6f:name==L"complete"?0.f:-.65f;
                message="TEST FIXTURE - not a playthrough or player save";
                if(name==L"river") {orbit.yaw=.5f;orbit.pitch=1.f;orbit.distance=10.f;}
                if(name==L"settlement-clue" || name==L"settlement-clue-read")orbit.yaw=-1.25f;
                if(name==L"fuse-lock-wrong" || name==L"fuse-lock-ready")orbit.yaw=1.25f;
                if(name==L"relay-west"||name==L"relay-aligned"||name==L"relay-linked"||name==L"relay-turn"||name==L"ridge-pan")orbit.yaw=1.25f;
                if(name==L"strike"||name==L"focus-pause") {
                    encounter.Tick(session.feet,1.f/60.f,false);
                    for(unsigned tick=0;tick<4;++tick)encounter.Tick(session.feet,.1f,false);
                }
                if(name==L"boar-recovery") {
                    encounter.Tick(session.feet,1.f/60.f,false);
                    for(unsigned tick=0;tick<9;++tick)encounter.Tick(session.feet,.1f,false);
                }
            }
        }
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
        wchar_t selectedFrame[4]{};
        const auto frameLength=!capture.empty()?GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_FRAME",selectedFrame,4):0;
        if(frameLength) {
            const std::wstring_view frame=selectedFrame;
            if(frameLength>=4||!captureLandscapeMotion||captureSequenceFrames||captureBenchmark||
                (frame!=L"18"&&frame!=L"27"))throw std::runtime_error("Invalid selected ridge capture frame");
            captureCompleted=frame==L"18"?18u:27u;captureSequenceFrames=captureCompleted+1;
            orbit.yaw=1.25f+float(captureCompleted)*6.2831853f/36.f;
            waterTime=2.5f+float(captureCompleted)*.1f;
        }
#endif
        wchar_t noAo[8]{};
        diagnosticNoAo=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_NO_AO",noAo,8)>0 && noAo[0]==L'1';
        // 僅影像診斷允許單變量隔離；不改玩家預設品質或正式玩法。
        if(!capture.empty()) {
            const auto enabled=[](const wchar_t* name) {
                wchar_t value[3]{};const auto length=GetEnvironmentVariableW(name,value,3);
                if(!length)return false;
                if(length!=1||(value[0]!=L'0'&&value[0]!=L'1'))throw std::runtime_error("Invalid lighting diagnostic flag");
                return value[0]==L'1';
            };
            diagnosticNoGroundNormal=enabled(L"AEVRYND_MISTWATCH_NO_GROUND_NORMAL");
            diagnosticNoMsaa=enabled(L"AEVRYND_MISTWATCH_NO_MSAA");
            diagnosticNoShadows=enabled(L"AEVRYND_MISTWATCH_NO_SHADOWS");
            diagnosticNoBloom=enabled(L"AEVRYND_MISTWATCH_NO_BLOOM");
            diagnosticRoadUnitOcclusion=enabled(L"AEVRYND_MISTWATCH_ROAD_UNIT_OCCLUSION");
            if(diagnosticRoadUnitOcclusion) {
                if(!entranceRoadEnabled)throw std::runtime_error("Road occlusion diagnostic requires road material domain");
                // 同時涵蓋由舊道路域拆出的工棚，維持新舊分域單變量診斷；貼圖與所有光源不變。
                entranceRoadSurface.surface.occlusion=1.f;
                if(settlementSurfaceEnabled)settlementSurfaceMaterial.surface.occlusion=1.f;
            }
            diagnosticNoGroundTextures=enabled(L"AEVRYND_MISTWATCH_NO_GROUND_TEXTURES");
            diagnosticNeutralGroundBase=enabled(L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_BASE");
            diagnosticNeutralGroundSurface=enabled(L"AEVRYND_MISTWATCH_GROUND_NEUTRAL_SURFACE");
            if(forestSurfaceSelection.profile==Mistwatch::ForestSurfaceProfile::Profile::ForestMossLeafMetricV1&&
               (diagnosticNoGroundNormal||diagnosticNoGroundTextures||diagnosticNeutralGroundBase||diagnosticNeutralGroundSurface))
                throw std::runtime_error("FOREST_SURFACE_CHANNEL_DIAGNOSTIC_CONFLICT");
            if(diagnosticNoGroundTextures)groundMaterial.textures={};
            if(diagnosticNoGroundTextures&&(diagnosticNeutralGroundBase||diagnosticNeutralGroundSurface))
                throw std::runtime_error("Conflicting ground channel diagnostics");
            if(diagnosticNeutralGroundBase||diagnosticNeutralGroundSurface) {
                // 中性測試圖由診斷收據固定；其他貼圖與品質不變。
                auto definition=Mistwatch::HabitatGroundDiagnosticDefinition(Mistwatch::WorldProfileHasWoodCollision(worldProfile),
                    entranceWorld.applied&&fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1,
                    diagnosticLegacyGround,diagnosticNeutralGroundBase,diagnosticNeutralGroundSurface,diagnosticSurfaceGround);
                if(!materials.Load(definition))throw std::runtime_error("Ground channel diagnostic material unavailable");
                groundMaterial.textures=materials.Textures(definition.name);
                groundDefinition=std::move(definition);
            }
            wchar_t stage[8]{};const auto stageLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_HDR_STAGE",stage,8);
            if(stageLength) {
                const std::wstring_view name=stage;
                if(stageLength>=8||!captureSequenceFrames)throw std::runtime_error("Invalid HDR diagnostic stage or capture mode");
                if(name==L"pre")diagnosticHdrStage=1;
                else if(name==L"post")diagnosticHdrStage=2;
                else if(name==L"final")diagnosticHdrStage=3;
                else throw std::runtime_error("Unknown HDR diagnostic stage");
#if AEVRYND_ATMOSPHERE_OUTPUT_DIAGNOSTICS
                if(diagnosticHdrStage==1&&!EngineAtmosphereDiagnosticAccess3D::SetPreAtmosphereHdrCapture(e,true))throw std::runtime_error("Pre-atmosphere diagnostic setup failed");
                if(diagnosticHdrStage==2&&!EngineAtmosphereDiagnosticAccess3D::SetPostAtmosphereHdrCapture(e,true))throw std::runtime_error("Post-atmosphere diagnostic setup failed");
#else
                if(diagnosticHdrStage!=3)throw std::runtime_error("HDR stages require atmosphere diagnostic build");
#endif
            }
            if(diagnosticNoGroundNormal) {
                // Terrain 的 normalScale 同時是覆蓋路由標記，停用法線不得移除材質覆蓋。
                if(groundMaterial.materialClass!=MaterialClass3D::Terrain)groundMaterial.normalScale=0;
                groundDistanceSettings.normalDetailStrength=0;
            }
            if(diagnosticNoMsaa) {
#if AEVRYND_INTERNAL_RENDERER_TEST_HOOKS
                if(!EngineRendererTestAccess3D::SetMultisampling(e,false))throw std::runtime_error("MSAA diagnostic setup failed");
#else
                throw std::runtime_error("MSAA diagnostic requires internal renderer test hooks");
#endif
            }
        }
        wchar_t crown[8]{};
        diagnosticCrown=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CROWN",crown,8)>0 && crown[0]==L'1';
        if(!boar.Update({},0,encounter.phase,encounter.phaseTime,session.feet-encounter.feet)||
            !boar.PlaceFeet(encounter.feet,terrain))throw std::runtime_error("Boar initial pose failed");
        if(!boarHitScratch.Initialize(boar.model))
            throw std::runtime_error("Boar hit render skeleton incompatible");
        if(capture.empty()&&!save.empty()) {
            audioSettings=save.parent_path()/"audio-settings.txt";
            std::ifstream file(audioSettings);std::string line;
            if(std::getline(file,line))gameAudio.options.Restore(line);
        }
        // 自動擷取與輸入重播保持固定預設值，不讀寫玩家的鏡頭偏好。
        if(capture.empty()&&inputReplayDirectory.empty()&&!save.empty()){
            cameraSettings=save.parent_path()/"camera-settings.txt";
            Mistwatch::ReadCameraSettingsFile(cameraSettings,cameraOptions);
        }
        // 自動影像驗證靜音，不寫玩家音量設定；播放收據仍記錄真實裝置與 voice 結果。
        if(!capture.empty()||!inputReplayDirectory.empty())gameAudio.options.muted=true;
        gameAudio.Initialize(e.Audio());
        // 初始切鏡是明確事件；之後只在固定更新推進，繪圖不重複消耗時間。
        playerCameraMotion.Reset();
        if(!UpdatePlayerCamera(1.f/60.f))throw std::runtime_error("Initial player camera unavailable");
        lastRenderedCamera=lastCameraMotion.camera;
        if(shoulderGroundcoverEnabled){
            if(mixedLandscapeCanopy&&(!landscapePlanting.complete||landscapePlanting.plants.size()!=16||landscapeRootAcquisitions!=16))
                throw std::runtime_error("Mixed canopy groundcover requires complete actual root resources");
            if(!landscapeStandFill||authoredLandscapePlanting||!roadsideBroadleafEnabled||
               landscapeStandPlan.combined.size()!=616||!landscapeMesh.cpuMesh||
               Mistwatch::ShoulderGrove::Detail::PoseFingerprint(landscapeStandPlan.combined)!=Mistwatch::ShoulderGrove::Detail::PopulationSeal||
               Mistwatch::ShoulderGrove::Detail::MeshFingerprint(*landscapeMesh.cpuMesh,true)!=Mistwatch::ShoulderGrove::Detail::TerrainGeometrySeal)
                throw std::runtime_error("Groundcover sealed world identity mismatch");
            const auto directory=e.ExecutableDirectory()/Mistwatch::ShoulderGroundcover::RelativeDirectory;
            for(std::size_t i=0;i<shoulderGroundcoverMeshes.size();++i){
                const auto& p=Mistwatch::ShoulderGroundcover::Recipes[i];
                const auto& source=p.slot<5?scannedGrass[p.slot]:p.slot<9?scannedFerns[p.slot-5]:roadsideBroadleafMeshes[p.slot-9];
                if(!source.cpuMesh||Mistwatch::ShoulderGrove::Detail::MeshFingerprint(*source.cpuMesh)!=p.sourceHash)
                    throw std::runtime_error("Groundcover material source identity mismatch");
                auto mesh=Mistwatch::ShoulderGroundcover::Load(directory,p);
                shoulderGroundcoverMeshes[i]=e.CreateMeshResource3D(mesh);
                if(!shoulderGroundcoverMeshes[i])throw std::runtime_error("Groundcover upload failed");
                ++shoulderGroundcoverAcquisitions;
                if(!shoulderGroundcoverMeshes[i].cpuMesh||
                   Mistwatch::ShoulderGrove::Detail::MeshFingerprint(*shoulderGroundcoverMeshes[i].cpuMesh)!=p.derivedHash)
                    throw std::runtime_error("Groundcover uploaded identity mismatch");
            }
        }
        if(rockSegmentEnabled)rockSegment.Initialize(e);
        if(cliffMaterialDiagnostic&&(diagnosticLegacyGround||diagnosticSurfaceGround||diagnosticNoGroundTextures||
           diagnosticNeutralGroundBase||diagnosticNeutralGroundSurface||diagnosticNoGroundNormal||diagnosticNoMsaa||
           diagnosticRoadUnitOcclusion||diagnosticHdrStage||(!cliffWorldEnabled&&!captureFrameCamera)||
           (cliffWorldEnabled&&captureFrameCamera)))
            throw std::runtime_error("Cliff material comparison rejects conflicting ground or camera diagnostics");
        if(cliffMaterialEnabled){
            if(!entranceRoadEnabled||!settlementSurfaceEnabled||!materials.Load(Mistwatch::CliffMaterial::Definition(groundDefinition)))
                throw std::runtime_error("Cliff material candidate load failed");
            const auto* loaded=materials.Find("cliff-rock-face-03-2k");
            if(!loaded||!loaded->textures||loaded->textures.semanticIdentity==groundMaterial.textures.semanticIdentity)
                throw std::runtime_error("Cliff candidate texture identity missing");
            // 保留目前地形所有 scalar 與 marker，只使用另一份具完整雜湊的 texture set。
            cliffMaterial=groundMaterial;cliffMaterial.textures=loaded->textures;
        }
        if(cliffReliefEnabled){
            if(!cliffReliefForestSource||!terrainMesh||!entranceRoadMeshes[0]||!cliffCoverageEnabled)
                throw std::runtime_error("Relief missing final canonical forest resources");
            // 所有 canonical 植被作者化與真資產載入完成，才驗證支撐並衍生實際碰撞場。
            auto derived=Mistwatch::WestCliffRelief::Build(terrain);
            cliffReliefSupportCounts=RequireWestCliffSupportClearance(derived.terrain);
            auto fallback=terrainCpuMesh;
            (void)Mistwatch::WestCliffRelief::UpdateMesh(fallback,terrain,derived.terrain);
            auto forest=*cliffReliefForestSource;
            const auto meshReceipt=Mistwatch::WestCliffRelief::UpdateMesh(forest,terrain,derived.terrain);
            if(leafForestSource){
                auto leafForest=*leafForestSource;
                const auto leafMeshReceipt=Mistwatch::WestCliffRelief::UpdateMesh(leafForest,terrain,derived.terrain);
                if(leafMeshReceipt.changedPositions!=meshReceipt.changedPositions||leafMeshReceipt.changedNormals!=meshReceipt.changedNormals||
                    leafMeshReceipt.changedColors!=meshReceipt.changedColors||leafMeshReceipt.changedVertices!=meshReceipt.changedVertices||
                    Mistwatch::CliffCoverage::Detail::GeometryFingerprint(leafForest)!=Mistwatch::CliffCoverage::Detail::GeometryFingerprint(forest))
                    throw std::runtime_error("Leaf relief geometry differs from original canonical forest");
                forest=std::move(leafForest);
            }
            auto covered=Mistwatch::CliffCoverage::Create(forest,derived.terrain,Mistwatch::CliffCoverage::Domain::Core);
            MeshResource3D newFallback{},newForest{};
            try {
                newFallback=e.CreateMeshResource3D(fallback);newForest=e.CreateMeshResource3D(covered.mesh);
                if(!newFallback||!newForest||!newFallback.cpuMesh||!newForest.cpuMesh||
                    Mistwatch::EntranceRoadDomainDetail::Fingerprint(*newFallback.cpuMesh)!=Mistwatch::EntranceRoadDomainDetail::Fingerprint(fallback)||
                    Mistwatch::EntranceRoadDomainDetail::Fingerprint(*newForest.cpuMesh)!=covered.receipt.afterFingerprint)
                    throw std::runtime_error("Relief actual GPU resource mismatch");
            }catch(...){if(newFallback)e.DestroyMesh3D(newFallback.handle);if(newForest)e.DestroyMesh3D(newForest.handle);throw;}
            cliffCanonicalTerrain=terrain;
            e.DestroyMesh3D(terrainMesh.handle);e.DestroyMesh3D(entranceRoadMeshes[0].handle);
            terrainMesh=newFallback;entranceRoadMeshes[0]=newForest;
            terrain=std::move(derived.terrain);cliffReliefReceipt=derived.receipt;cliffReliefMeshReceipt=meshReceipt;
            cliffReceipts[0]=covered.receipt;
        }
        if(valleyWorldEnabled&&!inputReplayDirectory.empty())valleyEvidenceDirectory=inputReplayDirectory;
        startupStage("actors_save_and_scene_setup");
        InitializeValleyWorld(e,startupStage);
        if(inputReplay.GroveRoute()&&(slopeGrove.trees.size()!=8||slopeGrove.roots.size()!=8||slopeGrove.solids.size()!=512||slopeGrove.acquired!=8))
            throw std::runtime_error("Grove replay actual initialized resources incomplete");
        startupStage("derived_valley_world");
        wchar_t seedBuffer[32768]{};
        const auto seedCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_REPLAY_SAVE_SEED",seedBuffer,32768);
        if(seedCount>=32768)throw std::runtime_error("Replay save seed path truncated");
        const std::wstring_view seed(seedBuffer,seedCount);
        if(!seed.empty()){
            const std::filesystem::path source(seed);
            if(!inputReplay.ResumeSaveRoute()||!source.is_absolute()||!std::filesystem::is_regular_file(source)||
                std::filesystem::is_symlink(source)||std::filesystem::file_size(source)>Mistwatch::SaveGame::MaximumBytes+1||
                save!=inputReplayDirectory/"test-save.txt"||std::filesystem::exists(save))throw std::runtime_error("Replay seed requires bounded fresh isolated resume output");
            std::filesystem::copy_file(source,save,std::filesystem::copy_options::none);
        }
        if(inputReplay.ResumeSaveRoute()&&seed.empty())throw std::runtime_error("Resume replay seed missing");
        InitializeCoreHerbs(e);
        WriteCompleteUnderstoryRetention();
        InitializeRouteGrass(e);
        InitializeForestIbl(e);
        startupStage("herbs_route_ibl");
        if(captureCostEnabled&&e.WindowMode3D()==EngineWindowMode3D::OffscreenNoActivateDiagnostic){
            // 場景拒絕或旁錄失敗仍由已建立的startup guards負責回收，不能先解除其責任。
            if(!Mistwatch::CaptureBenchmarkRemainsStatic(!capture.empty(),captureBenchmark,captureFrameCamera,captureWalk,
                captureSequenceFrames!=0,captureAudioProbe,captureWildlife,captureFocusProbe))
                throw std::runtime_error("Offscreen capture benchmark must remain static");
            RECT rect{},client{};const auto desktop=DiagnosticWindow3D::VirtualDesktop();
            if(!GetWindowRect(e.Window(),&rect)||!GetClientRect(e.Window(),&client))throw std::runtime_error("Capture window receipt unavailable");
            DiagnosticWindow3D::RequireValid(e.Window(),client.right-client.left,client.bottom-client.top);
            auto path=capture;path+=L".window.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Capture window receipt refuses overwrite");
            std::ofstream window(path);
            window<<"mode=offscreen-noactivate\nvirtual_rect="<<desktop.left<<','<<desktop.top<<','<<desktop.right<<','<<desktop.bottom
                <<"\nwindow_rect="<<rect.left<<','<<rect.top<<','<<rect.right<<','<<rect.bottom
                <<"\nclient_size="<<client.right-client.left<<','<<client.bottom-client.top
                <<"\nextended_style="<<GetWindowLongPtrW(e.Window(),GWL_EXSTYLE)
                <<"\nvisible="<<IsWindowVisible(e.Window())<<"\niconic="<<IsIconic(e.Window())
                <<"\nforeground="<<(GetForegroundWindow()==e.Window())<<"\nstable_display_topology_required=1\n";
            window.close();if(!window)throw std::runtime_error("Capture window receipt write failed");
        }
        (void)rockStartupGuard.release();
        (void)landscapeStartupGuard.release();
        (void)shoulderStartupGuard.release();
        (void)groundcoverStartupGuard.release();
        (void)woodlandStartupGuard.release();
        (void)roadsideStartupGuard.release();
        (void)roadStartupGuard.release();
        (void)shelterStartupGuard.release();
        (void)workshopRoofStartupGuard.release();
        (void)iblStartupGuard.release();
        (void)valleyStartupGuard.release();
        if(captureCostEnabled)captureReadyNs=CaptureElapsedNs();
        InitializeIndexedGameplayWorld(e);
        startupStage("complete");
    }
    void OnUpdate(Engine& e,float dt) override {
        if(PrepareManualEntrySmoke(e,dt))return;
        if(PrepareInputReplay(e,dt))return;
        // 上一步觀測已寫出；所有早退分支也必須清掉傷害／閃避事件，不能延用舊攻擊。
        inputReplayLastDamage=0;inputReplayLastDashing=false;
        inputReplayMotionTick=inputReplay.Ticks();inputReplayMotionSteps=0;
        inputReplayMotionAttempted=false;inputReplayMotionAccepted=false;inputReplayCameraAttempted=false;inputReplayCameraRollback=false;
        if(captureFocusProbe&&!captureFocusComplete) {
            // 測試只送訊息給自己的隱藏視窗；不切換使用者前景視窗、不讀寫玩家存檔。
            const auto tick=++captureFocusTicks;
            if(tick==1){SendMessageW(e.Window(),WM_SETFOCUS,0,0);e.GetInput().ConsumePresses();SendMessageW(e.Window(),WM_KEYDOWN,'W',0);}
            if(tick==2){
                captureFocusSession=session.Serialize();captureFocusFeet=session.feet;
                captureFocusWater=waterTime;captureFocusEnemyTime=encounter.phaseTime;
                SendMessageW(e.Window(),WM_KILLFOCUS,0,0);
            }
            if(tick>=3&&tick<=19){
                if(!paused||session.Serialize()!=captureFocusSession||waterTime!=captureFocusWater||encounter.phaseTime!=captureFocusEnemyTime)
                    throw std::runtime_error("Focus pause failed: world advanced while paused");
                ++captureFocusFrozenTicks;
            }
            if(tick==18){SendMessageW(e.Window(),WM_SETFOCUS,0,0);SendMessageW(e.Window(),WM_KEYDOWN,'W',0);}
            if(tick==20){SendMessageW(e.Window(),WM_KEYDOWN,VK_ESCAPE,0);SendMessageW(e.Window(),WM_KEYUP,VK_ESCAPE,0);}
            if(tick==21){
                // 上一步只恢復暫停；同批輸入不能推進世界，本步才讓持續 W 進入正式更新。
                if(paused||session.Serialize()!=captureFocusSession||waterTime!=captureFocusWater||encounter.phaseTime!=captureFocusEnemyTime)
                    throw std::runtime_error("Focus resume boundary failed: world advanced before the next update");
            }
            if(tick==22){
                if(paused||Length(session.feet-captureFocusFeet)<.001f||waterTime==captureFocusWater||encounter.phaseTime==captureFocusEnemyTime)
                    throw std::runtime_error("Focus resume failed: explicit ESC did not resume simulation");
                SendMessageW(e.Window(),WM_KEYUP,'W',0);
                captureFocusComplete=true;paused=true;
                message="FOCUS PAUSE VERIFIED - ESC resumes expedition";
            }
        }
        if(!capture.empty()&&(!captureFocusProbe||captureFocusComplete)) {
            if(captureAudioProbe&&!capturePending){++captureAudioTicks;paused=captureAudioTicks>=60&&captureAudioTicks<80;}
            if(captureWildlife&&!capturePending) {
                const auto previous=encounter.feet;
                const auto enemyMotion=Mistwatch::TickEncounterWorld(encounter,session.feet,1.f/60.f,false,solids,terrain);
                if(enemyMotion.status==Mistwatch::EncounterMotionStatus::Invalid)throw std::runtime_error("Boar capture motion rejected");
                session.Damage(enemyMotion.damage);
                const auto look=(encounter.phase==Mistwatch::EnemyPhase::Windup?encounter.strikeTarget:session.feet)-encounter.feet;
                if(!boar.Update(encounter.feet-previous,1.f/60.f,encounter.phase,(std::max)(0.f,encounter.phaseTime),look)||
                    !boar.PlaceFeet(encounter.feet,terrain))throw std::runtime_error("Boar capture pose failed");
            }
            if(captureWalk && !capturePending&&!paused&&(!captureAudioProbe||captureAudioTicks<=180)){
                const auto previous=controller.Position();
                controller.TrySetHeight(captureCrouch?1.4f:1.8f,solids,RuntimeRoofHulls());
                if(!controller.Step({{captureAudioProbe?.55f:0.f,0,1},false,captureCrouch?.45f:1.f},solids,RuntimeRoofHulls(),terrain,1.f/60.f))
                    throw std::runtime_error("Capture character convex movement rejected");
                session.feet=controller.Position();
                explorer.Update(session.feet-previous,1.f/60.f,captureCrouch);
                if(!explorer.PlaceFeet(session.feet,controller.Grounded(),captureCrouch,terrain,solids,RuntimeRoofHulls()))
                    throw std::runtime_error("Capture locomotion IK failed");
                if(captureAudioProbe)for(unsigned index=0;index<3;++index)session.Collect(index);
            }
            // Readback 尚未回收時保留送出影格的鏡頭，收據不得描述後續更新的位置。
            if(!captureFrameCamera&&!capturePending&&!UpdatePlayerCamera(1.f/60.f))throw std::runtime_error("Capture camera transition rejected");
            if(captureAudioProbe)TickAudio(e,1.f/60.f);
            const auto stats=e.GetRendererStats();
            if(capturePending && stats.rejectedFrameCaptures)throw std::runtime_error("Frame capture rejected");
            if(capturePending && stats.successfulFrameCaptures>captureConsumed) {
                if(captureCostEnabled&&!captureObservedNs)captureObservedNs=CaptureElapsedNs();
                if(diagnosticHdrStage&&(captureCompleted==18||captureCompleted==27)) {
                    e.WaitRendererIdle3D();
                    const auto hdr=e.TakeLinearHdrReadback3D();
                    if(!hdr||!hdr->Complete())throw std::runtime_error("HDR diagnostic readback incomplete");
                    auto path=CapturePath();path+=L".hdr-half";
                    std::ofstream raw(path,std::ios::binary);
                    raw.write(reinterpret_cast<const char*>(hdr->rawHalfBytes.data()),std::streamsize(hdr->rawHalfBytes.size()));
                    raw.close();if(!raw)throw std::runtime_error("HDR diagnostic write failed");
                    path+=L".receipt.txt";std::ofstream proof(path);
                    proof<<"diagnostic_only=1\nstage="<<diagnosticHdrStage<<"\nframe_serial="<<hdr->receipt.rasterProvenance.frameSerial
                        <<"\nwidth="<<hdr->receipt.width<<"\nheight="<<hdr->receipt.height<<"\nbytes="<<hdr->rawHalfBytes.size()
                        <<"\nfinite_channels="<<hdr->receipt.statistics.finiteChannelCount<<"\nnan_channels="<<hdr->receipt.statistics.nanChannelCount
                        <<"\ncamera_hash="<<hdr->receipt.requestMetadata.cameraHash<<"\nstate_hash="<<hdr->receipt.requestMetadata.stateHash<<'\n';
                    proof.close();if(!proof)throw std::runtime_error("HDR diagnostic receipt write failed");
                }
                auto receipt=CapturePath();receipt+=".receipt.txt";
                WriteLandscapePlantingReceipt(CapturePath());
                WriteCrownGrowthReceipt(CapturePath(),landscapeFrozen);
                WriteCoreHerbReceipt(CapturePath(),coreHerbFrozen);
                WriteRouteGrassReceipt(CapturePath(),routeGrassFrozen);
                WriteLandscapeStandReceipt(CapturePath());
                WriteShoulderReceipt(CapturePath(),shoulderFrozen);
                WriteGroundcoverReceipt(CapturePath(),groundcoverFrozen);
                WriteCliffReceipt(CapturePath(),cliffFrozen);
                WriteCliffMaterialReceipt(CapturePath(),cliffMaterialFrozen);
                if(rockSegmentDiagnostic)rockSegment.Write(CapturePath(),rockSegmentFrozen,rockSegmentEnabled);
                WriteOuterForestFloorReceipt(CapturePath());
                WriteForestSurfaceReceipt(CapturePath(),stats);
                std::ofstream out(receipt);
                out<<"capture=success\nfir_lod_enabled="<<nearFirTwigLod
                    <<"\nfir_lod_candidates="<<firLodSubmission.lodMeshInstances
                    <<"\nfir_lod_near="<<firLodSubmission.nearLodInstances
                    <<"\nfir_lod_far="<<firLodSubmission.farLodInstances
                    <<"\nfir_lod_transitions="<<firLodSubmission.lodTransitionInstances
                    <<"\nfir_lod_dual="<<firLodSubmission.lodDualSubmittedInstances
                    <<"\nfir_lod_budget_fallbacks="<<firLodSubmission.lodTransitionBudgetFallbacks
                    <<"\nshadow_retention_enabled="<<(shadowRetentionEnabled&&!diagnosticNoShadows)
                    <<"\nscene_shadow_only_instances="<<firLodSubmission.shadowOnlyInstances
                    <<"\nscene_shadow_budget_dropped="<<firLodSubmission.shadowOnlyBudgetDropped
                    <<"\nscene_cpu_fallback_unavailable="<<firLodSubmission.cpuFallbackUnavailable
                    <<"\nscene_projected_overlay_instances="<<firLodSubmission.shadowInstances
                    <<"\nscene_visible_instances="<<firLodSubmission.visibleMeshInstances
                    <<"\nwater_phase_seconds="<<waterTime
                    <<"\nwater_surface_fingerprint="<<WaterSurfaceFingerprint3D(Mistwatch::RiverSurface())
                    <<"\nwater_single_composite="<<stats.waterSingleCompositeEnabled
                    <<"\nwater_composite_draws="<<stats.waterSingleCompositeDraws
                    <<"\nrefraction="<<stats.screenSpaceWaterRefractionEnabled
                    <<"\nrefraction_passes="<<stats.screenSpaceWaterRefractionPasses
                    <<"\nscreen_space_reflections="<<stats.screenSpaceReflectionsEnabled
                    <<"\nreflection_passes="<<stats.screenSpaceReflectionPasses<<'\n';
                out<<"output_width="<<stats.outputWidth<<"\noutput_height="<<stats.outputHeight
                    <<"\nadapter_luid_low="<<stats.adapterLuidLow<<"\nadapter_luid_high="<<stats.adapterLuidHigh
                    <<"\ndebug_errors="<<stats.debugErrorCount<<"\ndebug_warnings="<<stats.debugWarningCount<<'\n';
                out<<"debug_layer_enabled="<<stats.debugLayerEnabled<<"\ngpu_validation_requested="<<stats.gpuValidationRequested
                    <<"\ngpu_validation_available="<<stats.gpuValidationAvailable<<"\ngpu_validation_executed="<<stats.gpuValidationExecuted
                    <<"\ngpu_validation_passed="<<stats.gpuValidationPassed<<'\n';
                out<<"mission_stage="<<unsigned(session.CurrentStage())<<"\nrepair_materials="<<session.repairMaterials
                    <<"\ncharacter_height="<<controller.Settings().height<<'\n';
                out<<"fuse_clue_read="<<session.fuseClueRead<<"\nfuse_dials="<<session.fuseDials
                    <<"\nfuse_collected="<<session.fuseCollected<<'\n';
                out<<"understory_ferns="<<understory.ferns.size()<<"\nunderstory_saplings="<<understory.saplings.size()
                    <<"\nsubmitted_ferns="<<submittedFerns<<"\nsubmitted_saplings="<<submittedSaplings<<'\n';
                out<<"fern_colony_population="<<fernColonies.plants.size()
                    <<"\nfern_colony_authored_population="<<(fernColonies.plants.empty()?0u:fernColonySettings.targetPlants)
                    <<"\nfern_colonies="<<fernColonies.colonies.size()
                    <<"\nfern_colony_seed="<<(fernColonies.plants.empty()?0u:fernColonySettings.seed)
                    <<"\nfern_colony_reached_budget="<<(fernColonies.plants.empty()||fernColonies.reachedBudget)
                    <<"\nfern_colony_fingerprint="<<fernColonyFingerprint
                    <<"\nfern_colony_original_fingerprint="<<originalFernFingerprint
                    <<"\nfern_colony_setup_ms="<<fernColonySetupMilliseconds
                    <<"\nsubmitted_colony_ferns="<<submittedColonyFerns
                    <<"\nsubmitted_colony_fern_triangles="<<submittedColonyFernTriangles
                    <<"\nfern_colony_variants="<<fernColonyVariants[0]<<','<<fernColonyVariants[1]<<','<<fernColonyVariants[2]<<','<<fernColonyVariants[3]
                    <<"\nsubmitted_colony_variants="<<submittedColonyVariants[0]<<','<<submittedColonyVariants[1]<<','<<submittedColonyVariants[2]<<','<<submittedColonyVariants[3]
                    <<"\nsubmitted_original_ferns="<<submittedFerns-submittedColonyFerns
                    <<"\nsubmitted_original_fern_triangles="<<submittedFernTriangles-submittedColonyFernTriangles
                    <<"\nsubmitted_original_fern_variants="<<submittedOriginalFernVariants[0]<<','<<submittedOriginalFernVariants[1]<<','<<submittedOriginalFernVariants[2]<<','<<submittedOriginalFernVariants[3]<<'\n';
                out<<"fern_source="<<(diagnosticLegacyFern?"procedural":"poly-haven-fern-02")
                    <<"\nfern_variants=4\nfern_asset_scale="<<Mistwatch::FernAssetScale
                    <<"\nfern_materials="<<fernMaterials.Stats().materials<<"\nsubmitted_fern_triangles="<<submittedFernTriangles
                    <<"\nfern_alpha_cutoff="<<fernSurface.alphaCutoff<<"\nfern_use_uv="<<fernSurface.useUv<<'\n';
                out<<"grass_tint_scale="<<(diagnosticDimGrass?.7f:1.f)<<'\n';
                out<<"ranger_hut_visual="<<(diagnosticLegacyRangerHut?"legacy_flat":"planked_pitched_roof")
                    <<"\nranger_hut_triangles="<<rangerHutMesh.TriangleCount()<<"\nranger_hut_mesh_resources=1"
                    <<"\nranger_hut_wood_textured="<<bool(hutMaterial.textures)
                    <<"\nranger_hut_authored_uv="<<!diagnosticLegacyRangerHut
                    <<"\nranger_hut_visible_trim="<<!diagnosticLegacyRangerHut
                    <<"\nranger_hut_use_uv="<<hutMaterial.useUv
                    <<"\nranger_hut_texture_scale="<<hutMaterial.textureScale
                    <<"\nranger_hut_normal_scale="<<hutMaterial.normalScale
                    <<"\nranger_hut_indices="<<rangerHutMesh.indexCount
                    <<"\nranger_hut_camera_roof="<<!diagnosticLegacyRangerHut<<'\n';
                out<<"planked_shelter_uv_aligned="<<boardUvAligned<<'\n';
                out<<"shelter_detail_profile="<<(shelterDetailEnabled?Mistwatch::ShelterDetail::Profile:"original")
                    <<"\nshelter_detail_wood_vertices="<<shelterDetailMeshes[0].vertexCount
                    <<"\nshelter_detail_wood_indices="<<shelterDetailMeshes[0].indexCount
                    <<"\nshelter_detail_wood_fingerprint="<<shelterDetailFingerprints[0]
                    <<"\nshelter_detail_roof_vertices="<<shelterDetailMeshes[1].vertexCount
                    <<"\nshelter_detail_roof_indices="<<shelterDetailMeshes[1].indexCount
                    <<"\nshelter_detail_roof_fingerprint="<<shelterDetailFingerprints[1]
                    <<"\nshelter_detail_frame_members="<<shelterDetailFrameMembers
                    <<"\nshelter_detail_corrugations="<<shelterDetailCorrugations
                    <<"\nshelter_detail_submitted="<<submittedShelterDetail
                    <<"\nshelter_detail_roof_material="<<(shelterDetailEnabled?"procedural-weathered-metal":"none")<<'\n';
                out<<"planked_workshop_enabled="<<plankedWorkshop<<"\nplanked_workshop_handle="<<workshopMesh.handle.value
                    <<"\nplanked_workshop_profile="<<(workshopRoof?Mistwatch::WorkshopRoof::Profile:workshopFrameEnabled?Mistwatch::WorkshopFrame::Profile:plankedWorkshop?Mistwatch::WorkshopVisual::Profile:"original-cuboids")
                    <<"\nplanked_workshop_vertices="<<workshopMesh.vertexCount<<"\nplanked_workshop_indices="<<workshopMesh.indexCount
                    <<"\nplanked_workshop_submitted="<<submittedWorkshop<<"\nplanked_workshop_uv_fingerprint="<<workshopUvFingerprint
                    <<"\nplanked_workshop_texture_handle="<<(plankedWorkshop?hutMaterial.textures.value:woodMaterial.textures.value)<<'\n';
                out<<"workshop_frame_profile="<<(workshopFrameEnabled?Mistwatch::WorkshopFrame::Profile:"original")
                    <<"\nworkshop_frame_members="<<(workshopFrameEnabled?Mistwatch::WorkshopFrame::FrameMembers:0)
                    <<"\nworkshop_frame_fingerprint="<<workshopFrameFingerprint<<'\n';
                WriteWorkshopRoofReceipt(out);
                out<<"planked_shelter_wood_profile="<<(shelterWeatheredUv?Mistwatch::ShelterWeatheredUv::Profile:"original")
                    <<"\nplanked_shelter_uv_fingerprint="<<shelterUvFingerprint<<'\n';
                out<<"planked_shelter_roof_uv_aligned="<<roofUvAligned<<'\n';
                out<<"planked_shelter_enabled="<<plankedShelter<<"\nplanked_shelter_handle="<<plankedShelterMesh.handle.value
                    <<"\nplanked_shelter_vertices="<<plankedShelterMesh.vertexCount<<"\nplanked_shelter_indices="<<plankedShelterMesh.indexCount
                    <<"\nplanked_shelter_submitted="<<submittedPlankedShelter<<"\nplanked_shelter_texture_handle="<<(plankedShelter?hutMaterial.textures.value:0)<<'\n';
                out<<"repair_tools_detailed="<<repairToolsDetailed<<"\nrepair_tools_resources="<<(bool(repairToolsMeshes[0])+bool(repairToolsMeshes[1]))
                    <<"\nrepair_tools_metal_handle="<<repairToolsMeshes[0].handle.value<<"\nrepair_tools_handle_handle="<<repairToolsMeshes[1].handle.value
                    <<"\nrepair_tools_vertices="<<(repairToolsMeshes[0].vertexCount+repairToolsMeshes[1].vertexCount)
                    <<"\nrepair_tools_indices="<<(repairToolsMeshes[0].indexCount+repairToolsMeshes[1].indexCount)
                    <<"\nrepair_tools_triangles="<<(repairToolsMeshes[0].TriangleCount()+repairToolsMeshes[1].TriangleCount())
                    <<"\nrepair_tools_submitted="<<repairToolsSubmitted<<"\nrepair_tools_materials_mask="<<session.repairMaterials<<'\n';
                out<<"repair_sign_visual=jointed_two_plank\nrepair_sign_resources="
                    <<(bool(repairSignMeshes[0])+bool(repairSignMeshes[1])+bool(repairSignMeshes[2]))
                    <<"\nrepair_sign_wood_handle="<<repairSignMeshes[0].handle.value
                    <<"\nrepair_sign_paint_handle="<<repairSignMeshes[1].handle.value
                    <<"\nrepair_sign_steel_handle="<<repairSignMeshes[2].handle.value
                    <<"\nrepair_sign_vertices="<<(repairSignMeshes[0].vertexCount+repairSignMeshes[1].vertexCount+repairSignMeshes[2].vertexCount)
                    <<"\nrepair_sign_indices="<<(repairSignMeshes[0].indexCount+repairSignMeshes[1].indexCount+repairSignMeshes[2].indexCount)
                    <<"\nrepair_sign_triangles="<<(repairSignMeshes[0].TriangleCount()+repairSignMeshes[1].TriangleCount()+repairSignMeshes[2].TriangleCount())
                    <<"\nrepair_sign_visible_sites="<<repairSignVisibleSites<<"\nrepair_sign_submitted="<<repairSignSubmitted
                    <<"\nrepair_sign_wood_texture_handle="<<repairSignMaterials[0].textures.value
                    <<"\nrepair_sign_wood_texture_semantic="<<repairSignMaterials[0].textures.semanticIdentity
                    <<"\nrepair_sign_wood_use_uv="<<repairSignMaterials[0].useUv
                    <<"\nrepair_sign_wood_texture_scale="<<repairSignMaterials[0].textureScale
                    <<"\nrepair_sign_materials_mask="<<session.repairMaterials<<'\n';
                out<<"ranger_desk_resource_handle="<<rangerDeskMesh.handle.value<<"\nranger_desk_mesh_resources="<<bool(rangerDeskMesh)
                    <<"\nranger_desk_vertices="<<rangerDeskMesh.vertexCount<<"\nranger_desk_triangles="<<rangerDeskMesh.TriangleCount()
                    <<"\nranger_desk_indices="<<rangerDeskMesh.indexCount<<"\nranger_desk_scene_submissions="<<submittedRangerDesk
                    <<"\nranger_desk_wood_textured="<<bool(woodMaterial.textures)<<'\n';
                out<<"supply_visual=planked_wood_crate\nsupply_mesh_resources=2\nsupply_wood_triangles="<<supplyMeshes[0].TriangleCount()
                    <<"\nsupply_metal_triangles="<<supplyMeshes[1].TriangleCount()<<"\nsubmitted_supply_crates="<<submittedSupplies
                    <<"\nsupply_collected_mask="<<session.collected<<"\nsupply_wood_textured="<<bool(woodMaterial.textures)<<'\n';
                out<<"bridge_worksite_visual=stacked_timber_tool_station\nbridge_worksite_mesh_resources=2"
                    <<"\nbridge_worksite_wood_triangles="<<bridgeWorksiteMeshes[0].TriangleCount()
                    <<"\nbridge_worksite_metal_triangles="<<bridgeWorksiteMeshes[1].TriangleCount()
                    <<"\nbridge_worksite_fingerprint="<<Mistwatch::BridgeWorksiteFingerprintV1
                    <<"\nbridge_worksite_unrepaired="<<!session.bridgeRepaired
                    <<"\nsubmitted_bridge_worksite="<<submittedBridgeWorksite
                    <<"\nbridge_worksite_wood_textured="<<bool(woodMaterial.textures)
                    <<"\nbridge_worksite_wood_use_uv=1\n";
                out<<"entrance_road_profile="<<(entranceRoadEnabled?"entrance-road-material-domain-v1":"original")
                    <<"\nentrance_road_source_fingerprint="<<entranceRoadReceipt.sourceFingerprint
                    <<"\nentrance_road_forest_fingerprint="<<entranceRoadReceipt.forestFingerprint
                    <<"\nentrance_road_road_fingerprint="<<entranceRoadReceipt.roadFingerprint
                    <<"\nentrance_road_source_triangles="<<entranceRoadReceipt.sourceTriangles
                    <<"\nentrance_road_forest_triangles="<<entranceRoadReceipt.forestTriangles
                    <<"\nentrance_road_road_triangles="<<entranceRoadReceipt.roadTriangles
                    <<"\nentrance_road_shared_vertices="<<entranceRoadReceipt.sharedVertices
                    <<"\nentrance_road_submitted="<<entranceRoadSubmitted
                    <<"\nentrance_road_materials="<<entranceRoadMaterials.Stats().materials
                    <<"\nentrance_road_texture_identity="<<entranceRoadSurface.textures.semanticIdentity
                    <<"\nentrance_road_base_path="<<entranceRoadDefinition.textures.baseColorPath.generic_string()
                    <<"\nentrance_road_surface_path="<<entranceRoadDefinition.textures.roughnessPath.generic_string()
                    <<"\nentrance_road_normal_path="<<entranceRoadDefinition.textures.normalPath.generic_string()
                    <<"\nentrance_road_surface_encoding="<<unsigned(entranceRoadDefinition.textures.surfaceEncoding)
                    <<"\nentrance_road_authored_occlusion="<<entranceRoadDefinition.material.surface.occlusion
                    <<"\nentrance_road_submitted_occlusion="<<entranceRoadSurface.surface.occlusion
                    <<"\nentrance_road_submitted_roughness="<<entranceRoadSurface.surface.roughness
                    <<"\nentrance_road_submitted_metallic="<<entranceRoadSurface.surface.metallic
                    <<"\nentrance_road_submitted_emissive="<<entranceRoadSurface.surface.emissive
                    <<"\nentrance_road_unit_occlusion_diagnostic="<<diagnosticRoadUnitOcclusion
                    <<"\nentrance_road_original_terrain_drawn="<<!entranceRoadEnabled<<'\n';
                WriteSettlementSurface(out);
                out<<"settlement_ground_profile="<<(settlementGroundEnabled?Mistwatch::SettlementGround::Profile:"original")
                    <<"\nsettlement_ground_moved_triangles="<<settlementMovedTriangles
                    <<"\nsettlement_ground_boundary_vertices="<<settlementBoundaryVertices
                    <<"\nsettlement_ground_changed_forest="<<settlementChangedForest
                    <<"\nsettlement_ground_changed_road="<<settlementChangedRoad
                    <<"\nsettlement_ground_original_forest_fingerprint="<<settlementOriginalRoadReceipt.forestFingerprint
                    <<"\nsettlement_ground_original_road_fingerprint="<<settlementOriginalRoadReceipt.roadFingerprint<<'\n';
                out<<"roadside_profile="<<(roadsideCoverEnabled?Mistwatch::RoadsideCoverProfile:"original")
                    <<"\nroadside_population="<<roadsideCover.plants.size()<<"\nroadside_fingerprint="<<(roadsideCoverEnabled?roadsideCover.fingerprint:0)
                    <<"\nroadside_left="<<roadsideCover.left<<"\nroadside_right="<<roadsideCover.right
                    <<"\nroadside_attempted="<<roadsideCover.attempted
                    <<"\nroadside_submitted="<<submittedRoadside<<"\nroadside_submitted_triangles="<<submittedRoadsideTriangles<<'\n';
                out<<"entrance_understory_profile="<<(entranceUnderstoryEnabled?Mistwatch::EntranceUnderstoryProfile:"original")
                    <<"\nentrance_understory_population="<<entranceUnderstory.plants.size()
                    <<"\nentrance_understory_fingerprint="<<(entranceUnderstoryEnabled?entranceUnderstory.fingerprint:0)
                    <<"\nentrance_understory_attempted="<<entranceUnderstory.attempted
                    <<"\nentrance_understory_left="<<entranceUnderstory.left
                    <<"\nentrance_understory_right="<<entranceUnderstory.right
                    <<"\nentrance_understory_clusters="<<(entranceUnderstoryEnabled?entranceUnderstory.clusterCounts.size():0)
                    <<"\nentrance_understory_suppressed_grass="<<entranceUnderstory.suppressedGrassIds.size()
                    <<"\nentrance_understory_suppressed_broadleaf="<<entranceUnderstory.suppressedBroadleafIds.size()
                    <<"\nentrance_understory_suppression_fingerprint="<<(entranceUnderstoryEnabled?entranceUnderstory.suppressionFingerprint:0)
                    <<"\nentrance_understory_submitted="<<submittedEntranceUnderstory
                    <<"\nentrance_understory_available_shared_meshes="<<(entranceUnderstoryEnabled?8:0)<<'\n';
                out<<"roadside_broadleaf_profile="<<(roadsideBroadleafEnabled?Mistwatch::RoadsideBroadleafProfile:"original")
                    <<"\nroadside_broadleaf_population="<<roadsideBroadleaf.plants.size()
                    <<"\nroadside_broadleaf_fingerprint="<<(roadsideBroadleafEnabled?roadsideBroadleaf.fingerprint:0)
                    <<"\nroadside_broadleaf_left="<<roadsideBroadleaf.left<<"\nroadside_broadleaf_right="<<roadsideBroadleaf.right
                    <<"\nroadside_broadleaf_attempted="<<roadsideBroadleaf.attempted
                    <<"\nroadside_broadleaf_submitted="<<submittedRoadsideBroadleaf
                    <<"\nroadside_broadleaf_submitted_triangles="<<submittedRoadsideBroadleafTriangles
                    <<"\nroadside_broadleaf_materials="<<roadsideBroadleafMaterials.Stats().materials<<'\n';
                for(unsigned i=0;i<2;++i)out<<"roadside_broadleaf_mesh_"<<i<<"_identity="<<roadsideBroadleafIdentities[i]
                    <<"\nroadside_broadleaf_mesh_"<<i<<"_triangles="<<roadsideBroadleafMeshes[i].TriangleCount()<<'\n';
                out<<"grass_source="<<(diagnosticProceduralGrass?"procedural":"poly-haven-grass-medium-02")
                    <<"\ngrass_variants="<<(diagnosticProceduralGrass?1:5)<<"\ngrass_minimum_asset_height="<<Mistwatch::GrassAssetHeight
                    <<"\ngrass_materials="<<grassMaterials.Stats().materials<<"\nsubmitted_grass_triangles="<<submittedGrassTriangles
                    <<"\ngrass_alpha_cutoff="<<grassSurface.alphaCutoff<<"\ngrass_use_uv="<<grassSurface.useUv<<'\n';
                out<<"grass_shape_profile="<<(diagnosticCurvedScannedGrass?Mistwatch::ForestGrassShapeProfile:"source-geometry")
                    <<"\ngrass_shape_candidate_resources="<<(diagnosticCurvedScannedGrass?5:0)
                    <<"\ngrass_shape_complete_world_checks="<<curvedGrassWorldChecks<<'\n';
                for(unsigned variant=0;variant<5;++variant){
                    const auto& mesh=diagnosticCurvedScannedGrass?curvedScannedGrass[variant]:scannedGrass[variant];
                    out<<"grass_render_mesh_"<<variant<<"_identity="<<grassRenderIdentities[variant]
                        <<"\ngrass_render_mesh_"<<variant<<"_handle="<<mesh.handle.value
                        <<"\ngrass_render_mesh_"<<variant<<"_vertices="<<mesh.vertexCount
                        <<"\ngrass_render_mesh_"<<variant<<"_triangles="<<mesh.TriangleCount()<<'\n';
                }
                out<<"understory_mix_profile="<<(diagnosticNettleMix?Mistwatch::NettleMixProfile:"original")
                    <<"\nnettle_mix_accepted="<<nettleMix.accepted<<"\nnettle_mix_rejected="<<nettleMix.rejected
                    <<"\nnettle_mix_world_checks="<<nettleMix.worldChecks<<"\nnettle_mix_fingerprint="<<nettleMix.fingerprint
                    <<"\nnettle_materials="<<nettleMaterials.Stats().materials<<"\nsubmitted_nettles="<<submittedNettles
                    <<"\nsubmitted_nettle_triangles="<<submittedNettleTriangles<<'\n';
                for(unsigned variant=0;variant<6;++variant)out<<"nettle_mesh_"<<variant<<"_identity="<<nettleMeshIdentities[variant]
                    <<"\nnettle_mesh_"<<variant<<"_vertices="<<nettleMeshes[variant].vertexCount
                    <<"\nnettle_mesh_"<<variant<<"_triangles="<<nettleMeshes[variant].TriangleCount()<<'\n';
                out<<"enemy_visual=skinned_boar\nenemy_joints="<<boar.model.skeleton.joints.size()
                    <<"\nenemy_phase="<<unsigned(encounter.phase)<<"\nenemy_brace="<<boar.brace<<'\n';
                out<<"relay_linked="<<session.relayLinked<<"\nrelay_sectors="<<session.relaySectors
                    <<"\nrelay_target_west="<<Mistwatch::Session::RelayTargetSector(0)<<"\nrelay_signal_west="<<session.RelaySignal(0)<<'\n';
                out<<"session_serialization_version="<<(session.Serialize().starts_with("MISTWATCH 7 ")?7u:0u)<<"\nrelay_patch_a="<<session.relayPatchA
                    <<"\nrelay_patch_b="<<session.relayPatchB<<"\nrelay_field_tasks="<<session.relayFieldTasks
                    <<"\nextraction_route="<<session.extractionRoute
                    <<"\nrelay_field_ready_west="<<session.RelayFieldTaskReady(0)
                    <<"\nrelay_field_ready_northwest="<<session.RelayFieldTaskReady(2)
                    <<"\nrelay_local_task_fixture_scope=synthetic-api-not-native-key-playthrough\n";
                out<<"landscape_vertices="<<landscapeMesh.vertexCount<<"\nlandscape_triangles="<<landscapeMesh.TriangleCount()
                    <<"\nlandscape_trees="<<landscapeTrees.size()<<"\ncamera_orbit_yaw="<<orbit.yaw<<'\n';
                out<<"fir_layout="<<(firLayoutSelector==Mistwatch::FirLayoutSelector::ForestWood?"forest-wood-mature":"legacy")
                    <<"\nfir_layout_population="<<firLayout.firs.size()<<"\nfir_layout_reached_budget="<<firLayout.reachedBudget
                    <<"\nfir_layout_fingerprint="<<firLayout.fingerprint<<"\nfir_layout_patch_centres="<<firLayout.patchCentres.size()<<'\n';
                out<<"landscape_river_release_m=24\n";
                out<<"grass_style="<<(diagnosticProceduralGrass?(diagnosticLegacyGrass?"upright":"curved"):"scanned")
                    <<"\ngrass_blades="<<(diagnosticProceduralGrass?"9":diagnosticCurvedScannedGrass?"source-uv-curved-v1":"source-geometry")<<"\ngrass_segments="<<(diagnosticProceduralGrass?(diagnosticLegacyGrass?"3":"5"):diagnosticCurvedScannedGrass?"source-uv-curved-v1":"source-geometry")
                    <<"\ngrass_vertices="<<(diagnosticProceduralGrass?grassMesh.vertexCount:7031)
                    <<"\ngrass_triangles="<<(diagnosticProceduralGrass?grassMesh.TriangleCount():7842)<<"\nground_cover_instances="<<groundCover.size()
                    <<"\nforest_cover_mode="<<(forestCover.plants.empty()?"original_scatter":"fixed_cluster_patches")
                    <<"\nforest_cover_seed="<<(forestCover.plants.empty()?0u:forestCoverSettings.seed)
                    <<"\nforest_cover_authored_population="<<(forestCover.plants.empty()?groundCover.size():forestCoverSettings.targetPlants)
                    <<"\nforest_cover_maximum_patches="<<(forestCover.plants.empty()?0u:forestCoverSettings.maximumPatches)
                    <<"\nforest_cover_patch_minimum_plants="<<(forestCover.plants.empty()?0u:forestCoverSettings.minimumPlantsPerPatch)
                    <<"\nforest_cover_patch_maximum_plants="<<(forestCover.plants.empty()?0u:forestCoverSettings.maximumPlantsPerPatch)
                    <<"\nforest_cover_independent_anchors="<<(forestCover.plants.empty()?0u:forestCoverSettings.independentAnchors)
                    <<"\nforest_cover_patches="<<forestCover.patches.size()
                    <<"\nforest_cover_reached_budget="<<(forestCover.plants.empty()||forestCover.reachedBudget)
                    <<"\nforest_cover_fingerprint="<<forestCoverFingerprint
                    <<"\nforest_cover_clearance_colliders="<<forestCoverClearanceColliders
                    <<"\nforest_cover_clearance_collider_fingerprint="<<forestCoverColliderFingerprint
                    <<"\nforest_cover_setup_ms="<<forestCoverSetupMilliseconds
                    <<"\nforest_cover_retention_revision="<<(forestCoverRetention.retained?"original-safe-pose-v1":"none")
                    <<"\nforest_cover_original_fingerprint="<<originalCoverFingerprint
                    <<"\nforest_cover_retained="<<forestCoverRetention.retained
                    <<"\nforest_cover_rejected_original="<<forestCoverRetention.rejected
                    <<"\nforest_cover_filled="<<forestCoverRetention.filled
                    <<"\nforest_cover_provenance_fingerprint="<<forestCoverRetention.fingerprint
                    <<"\nforest_cover_source_world_fingerprint="<<forestCoverRetention.originalInputFingerprint
                    <<"\nforest_cover_final_world_fingerprint="<<forestCoverRetention.finalInputFingerprint
                    <<"\nforest_cover_variants="<<groundCoverVariants[0]<<','<<groundCoverVariants[1]<<','<<groundCoverVariants[2]<<','<<groundCoverVariants[3]<<','<<groundCoverVariants[4]
                    <<"\nsubmitted_grass_variants="<<submittedGrassVariants[0]<<','<<submittedGrassVariants[1]<<','<<submittedGrassVariants[2]<<','<<submittedGrassVariants[3]<<','<<submittedGrassVariants[4]
                    <<"\nsubmitted_grass="<<submittedGrass<<'\n';
                out<<"broadleaf_trees="<<broadleafTrees.size()<<"\nbroadleaf_mesh_triangles="<<broadleafTriangles
                    <<"\nbroadleaf_materials="<<broadleafMaterials.Stats().materials<<'\n';
                out<<"middle_layer_comparison="<<middleLayerEnabled
                    <<"\nmiddle_layer_population="<<middleLayer.trees.size()
                    <<"\nmiddle_layer_target="<<(middleLayerEnabled?(entranceWorld.applied?23:24):0)
                    <<"\nmiddle_layer_reached_budget="<<(!middleLayerEnabled||middleLayer.reachedBudget)
                    <<"\nsubmitted_middle_layer="<<submittedMiddleLayer
                    <<"\nsubmitted_middle_layer_triangles="<<submittedMiddleLayerTriangles
                    <<"\nmiddle_layer_attempted="<<middleLayer.attempted
                    <<"\nmiddle_layer_transform_fingerprint="<<Mistwatch::MiddleLayerTransformFingerprint(middleLayer.trees)<<'\n';
                out<<"boulder_instances="<<std::count_if(forestProps.begin(),forestProps.end(),[](const auto& p){return !p.FallenLog();})
                    <<"\nboulder_mesh_triangles="<<propMeshes[0].TriangleCount()<<"\nboulder_materials="<<boulderMaterials.Stats().materials
                    <<"\nboulder_use_uv="<<boulderMaterial.useUv<<"\nboulder_texture_scale="<<boulderMaterial.textureScale
                    <<"\nboulder_diagnostic_profile="<<boulderDiagnosticProfile<<"\nboulder_normal_scale="<<boulderMaterial.normalScale
                    <<"\nboulder_has_textures="<<bool(boulderMaterial.textures)
                    <<"\nboulder_color_profile="<<boulderColorProfile<<"\nboulder_base_path="<<boulderBasePath
                    <<"\nboulder_base_sha256="<<boulderBaseHash
                    <<"\nboulder_geometry_profile="<<boulderGeometryProfile
                    <<"\nboulder_mesh_vertices="<<propMeshes[0].vertexCount
                    <<"\nboulder_gpu_resources="<<unsigned(bool(propMeshes[0]))<<'\n';
                if(captureFocusProbe)out<<"focus_pause_verified="<<captureFocusComplete<<"\nfocus_frozen_ticks="<<captureFocusFrozenTicks
                    <<"\nfocus_explicit_resume_verified="<<captureFocusComplete<<"\npaused="<<paused<<'\n';
                out<<"survey_map_requested="<<surveyMapRequested<<"\nsurvey_map_drawn="<<surveyMapDraw.drawn<<"\nsurvey_map_tiles="<<surveyMapDraw.tiles
                    <<"\nsurvey_map_markers="<<surveyMapDraw.markers<<"\nsurvey_map_text_lines="<<surveyMapDraw.textLines<<'\n';
                out<<"ground_habitat_profile="<<HabitatProfileName()<<'\n';
                out<<"ground_habitat_revision="<<(outerForestFloor?"outer-tree-habitat-v1":(habitatTerrain.vertices?
                    (deciduousHabitat?"actual-deciduous-root-cohorts-v1":
                     (habitatFloorProfile==Mistwatch::ForestHabitatFloorProfile::Original?"root-soil-and-mission-spurs-v1":"root-soil-and-mission-spurs-leaf-strength-v1")):"none"))
                    <<"\nground_habitat_fir_roots="<<(habitatTerrain.vertices?trees.size():0)
                    <<"\nground_habitat_canopy_roots="<<(habitatTerrain.vertices?canopyPopulation.plants.size():0)
                    <<"\nground_habitat_terrain_fingerprint="<<habitatTerrain.fingerprint
                    <<"\nground_habitat_landscape_fingerprint="<<(outerForestFloor?outerFloorReceipt.afterHabitatFingerprint:habitatLandscape.fingerprint)
                    <<"\nground_habitat_terrain_changed="<<habitatTerrain.changed
                    <<"\nground_habitat_terrain_soil_dominant="<<habitatTerrain.soilDominant
                    <<"\nground_habitat_terrain_moss_dominant="<<habitatTerrain.mossDominant
                    <<"\nground_habitat_landscape_changed="<<(outerForestFloor?outerFloorReceipt.changed:habitatLandscape.changed)<<'\n';
                out<<"ground_habitat_deciduous_roots="<<deciduousHabitatPlan.roots.size()
                    <<"\nground_habitat_deciduous_fingerprint="<<deciduousHabitatPlan.fingerprint
                    <<"\nground_habitat_view="<<(captureDeciduousRootView?"broadleaf-5":"player")<<'\n';
                out<<"ground_normal_scale="<<groundMaterial.normalScale<<"\ndiagnostic_no_msaa="<<diagnosticNoMsaa
                    <<"\nground_material_class="<<unsigned(groundMaterial.materialClass)
                    <<"\nground_single_surface="<<diagnosticSurfaceGround
                    <<"\nground_texture_layers="<<(groundDefinition.textures.additionalLayers[0].baseColorPath.empty()?1:3)
                    <<"\nground_height_alpha="<<(groundMaterial.materialClass==MaterialClass3D::Terrain?"shared_core_normalized":"opaque_surface_baseline")
                    <<"\nground_distance_normal_strength="<<groundDistanceSettings.normalDetailStrength
                    <<"\nground_meso_partition="<<groundDistanceSettings.mesoPartitionEnabled
                    <<"\nground_rock_variation="<<groundDistanceSettings.rockVariationEnabled
                    <<"\nsubmitted_terrain_instances="<<stats.lastSubmittedTerrainInstances
                    <<"\nground_textured="<<bool(groundMaterial.textures)
                    <<"\nground_source="<<Mistwatch::GroundForestWoodArtSource(groundDefinition)
                    <<"\nground_habitat_layer_source="<<Mistwatch::HabitatGroundLayerSource(groundDefinition)
                    <<"\nground_habitat_layer_base_path="<<groundDefinition.textures.additionalLayers[0].baseColorPath.generic_string()
                    <<"\nground_habitat_layer_surface_path="<<groundDefinition.textures.additionalLayers[0].roughnessPath.generic_string()
                    <<"\nground_habitat_layer_normal_path="<<groundDefinition.textures.additionalLayers[0].normalPath.generic_string()
                    <<"\nground_habitat_layer_surface_encoding="<<unsigned(groundDefinition.textures.additionalLayers[0].surfaceEncoding)
                    <<"\nground_loaded_texture_identity="<<groundMaterial.textures.semanticIdentity
                    <<"\nground_art_base_color_path="<<groundDefinition.textures.baseColorPath.generic_string()
                    <<"\nground_art_surface_path="<<groundDefinition.textures.roughnessPath.generic_string()
                    <<"\nground_art_normal_path="<<groundDefinition.textures.normalPath.generic_string()
                    <<"\nground_art_surface_encoding="<<unsigned(groundDefinition.textures.surfaceEncoding)
                    <<"\nground_maximum_resolution="<<groundDefinition.textures.maximumResolution
                    <<"\nground_packed_orm="<<(groundDefinition.textures.surfaceEncoding==MaterialSurfaceMapEncoding3D::PackedOrm)
                    <<"\nground_texture_scale="<<groundMaterial.textureScale<<"\nground_use_uv="<<groundMaterial.useUv
                    <<"\nlast_draw_calls="<<stats.lastDrawCalls<<"\nresident_texture_bytes="<<stats.residentTextureBytes
                    <<"\nterrain_vertices="<<terrainMesh.vertexCount<<"\nterrain_triangles="<<terrainMesh.TriangleCount()
                    <<"\nground_neutral_base="<<diagnosticNeutralGroundBase<<"\nground_neutral_surface="<<diagnosticNeutralGroundSurface
                    <<"\ndiagnostic_no_shadows="<<diagnosticNoShadows<<"\ndiagnostic_no_bloom="<<diagnosticNoBloom
                    <<"\ndiagnostic_hdr_stage="<<diagnosticHdrStage
                    <<"\nmultisample_count="<<stats.multisampleCount<<"\nquality_shadows="<<stats.effectiveQuality.directionalShadows
                    <<"\nquality_ao="<<stats.effectiveQuality.ambientOcclusion<<"\nquality_post_aa="<<stats.effectiveQuality.postProcessAntiAliasing<<'\n';
                // 固定鏡頭光照 A/B 記錄上一個實際提交 draw 的參數，而非測試器猜值。
                const auto& skyPhase=e.Draw().PhaseRadianceProfile();
                const auto& submittedSun=e.Draw().DirectionalLight();
                const auto& skyBlend=e.Draw().EnvironmentLightingBlend();
                // 全域品質開關不等於實際繪製設定；接觸陰影與螢幕遮蔽共用通道，分別保留證據。
                const auto& submittedAo=e.Draw().AmbientOcclusion();
                const auto& submittedContact=e.Draw().ContactShadows();
                out<<"draw_ao_enabled="<<submittedAo.enabled
                    <<"\ndraw_ao_radius="<<submittedAo.radius<<"\ndraw_ao_bias="<<submittedAo.bias
                    <<"\ndraw_ao_intensity="<<submittedAo.intensity
                    <<"\ndraw_contact_enabled="<<submittedContact.enabled
                    <<"\ndraw_contact_steps="<<submittedContact.maximumSteps
                    <<"\ndraw_contact_distance="<<submittedContact.maximumDistance
                    <<"\ndraw_contact_thickness="<<submittedContact.thickness
                    <<"\ndraw_contact_strength="<<submittedContact.strength
                    <<"\nrenderer_ao_contact_pass_enabled="<<stats.ambientOcclusionEnabled
                    <<"\nrenderer_contact_enabled="<<stats.contactShadowsEnabled<<'\n';
                out<<"sky_diffuse_model=cosine-legendre8-v1\n"
                    <<"sky_environment_handle="<<e.ActiveEnvironmentLighting3D().value
                    <<"\nsky_phase_fingerprint="<<skyPhase.phaseFingerprint
                    <<"\nsky_zenith="<<skyPhase.skyZenith.x<<','<<skyPhase.skyZenith.y<<','<<skyPhase.skyZenith.z
                    <<"\nsky_horizon="<<skyPhase.skyHorizon.x<<','<<skyPhase.skyHorizon.y<<','<<skyPhase.skyHorizon.z
                    <<"\nsky_ground="<<skyPhase.groundBounce.x<<','<<skyPhase.groundBounce.y<<','<<skyPhase.groundBounce.z
                    <<"\nsky_intensity="<<skyPhase.skyIntensity<<"\nsky_exponent="<<skyPhase.horizonExponent
                    <<"\nsky_diffuse_scale="<<skyPhase.analyticDiffuse.x<<','<<skyPhase.analyticDiffuse.y<<','<<skyPhase.analyticDiffuse.z<<','<<skyPhase.analyticDiffuseIntensity
                    <<"\nsky_environment_scale="<<skyBlend.diffuseRgbScale.x<<','<<skyBlend.diffuseRgbScale.y<<','<<skyBlend.diffuseRgbScale.z<<','<<skyBlend.diffuseIntensity
                    <<"\nsky_sun_direction="<<submittedSun.direction.x<<','<<submittedSun.direction.y<<','<<submittedSun.direction.z
                    <<"\nsky_sun_intensity="<<submittedSun.intensity<<"\nsky_ambient="<<submittedSun.ambient
                    <<"\nforest_light_profile="<<(forestLightingContrast?Mistwatch::ForestLighting::CandidateName:"original")
                    <<"\nground_microrelief_profile="<<(groundMicrorelief?Mistwatch::GroundMicrorelief::Candidate:"original")
                    <<"\nforest_ibl_profile="<<(forestIblEnabled?"xanderklinge-forest-ibl-v1":"original")
                    <<"\nforest_ibl_owned_handle="<<forestIblHandle.value
                    <<"\nforest_ibl_create_accepted="<<forestIblCreate.rendererAccepted
                    <<"\nforest_ibl_set_accepted="<<forestIblSet.rendererAccepted
                    <<"\nforest_ibl_source_hdr_sha256=e688f624cd66973515154f200d6998b8731fd57dd8f6c2d71eb315454e9c3ef8"
                    <<"\nsky_exposure_ev="<<e.Draw().ExposureCompensation()<<'\n';
                const auto& exposureReceipt=stats.timeOfDayExposureReceipt;
                const auto& environmentReceipt=stats.environmentLightingBlendReceipt;
                out<<"exposure_actual_total_ev="<<exposureReceipt.actualTotalEv
                    <<"\nexposure_phase_ev="<<exposureReceipt.phaseExposureEv
                    <<"\nexposure_phase="<<static_cast<unsigned>(exposureReceipt.phase)
                    <<"\nexposure_fingerprint="<<exposureReceipt.fingerprint
                    <<"\nexposure_applied="<<exposureReceipt.applied<<"\nexposure_rolled_back="<<exposureReceipt.rolledBack
                    <<"\nexposure_shadow_lift="<<exposureReceipt.shadowLift
                    <<"\nexposure_toe_strength="<<exposureReceipt.toeStrength
                    <<"\nexposure_shoulder_strength="<<exposureReceipt.shoulderStrength
                    <<"\nenvironment_applied_fingerprint="<<environmentReceipt.appliedFingerprint
                    <<"\nenvironment_active="<<environmentReceipt.environmentActive
                    <<"\nenvironment_post_exposure_scale="<<environmentReceipt.postProcessExposureScale
                    <<"\nenvironment_linear_hdr_unexposed="<<environmentReceipt.linearHdrUnexposed<<'\n';
                if(!Mistwatch::ExplorerSurface::WriteReceipt(out,stats,explorerSurfaceSubmission))
                    throw std::runtime_error("Explorer actual material route counters diverge");
                std::size_t generatorTriangles{};
                for(const auto& mesh:generatorDetailMeshes)generatorTriangles+=mesh.TriangleCount();
                out<<"art_contract=mistwatch-art-v297\nexplorer_vertices="<<explorerMesh.vertexCount
                    <<"\nexplorer_triangles="<<explorerMesh.indexCount/3
                    <<"\ngenerator_static_triangles="<<generatorTriangles
                    <<"\ngenerator_indicator_triangles="<<submittedGeneratorLensTriangles
                    <<"\ngenerator_circuit="<<session.circuit<<'\n';
                out<<"shadow_cascades="<<stats.lastDirectionalShadowCascades<<"\nbloom_enabled="<<stats.bloomEnabled
                    <<"\nvolumetric_passes="<<stats.volumetricAtmosphereCompositePasses<<"\nvolumetric_scale="<<stats.volumetricAtmosphereResolutionScale
                    <<"\nvolumetric_width="<<stats.volumetricAtmosphereWidth<<"\nvolumetric_height="<<stats.volumetricAtmosphereHeight<<'\n';
                const auto audio=e.Audio().GetStats();
                std::uint64_t steps{};for(unsigned i=0;i<6;++i)steps+=gameAudio.requests[i];
                out<<"audio_device="<<audio.deviceAvailable<<"\naudio_sounds="<<audio.residentSounds
                    <<"\naudio_bytes="<<audio.residentBytes<<"\naudio_plays="<<audio.successfulPlays<<"\naudio_dropped="<<audio.droppedPlays
                    <<"\naudio_peak_voices="<<audio.peakActiveVoices<<"\naudio_master="<<audio.masterVolume
                    <<"\naudio_step_requests="<<steps<<"\naudio_interact_requests="<<gameAudio.requests[unsigned(Mistwatch::Sound::Interact)]
                    <<"\naudio_wind_requests="<<gameAudio.requests[unsigned(Mistwatch::Sound::Wind)]<<"\naudio_probe_ticks="<<captureAudioTicks<<'\n';
                out<<"trail_litter_enabled="<<trailLitterEnabled
                    <<"\nworld_profile="<<Mistwatch::WorldProfileName(worldProfile)
                    <<"\ncanopy_collision_mode="<<(Mistwatch::WorldProfileHasWoodCollision(worldProfile)?"wood-grid-conservative":"full-render-envelope")
                    <<"\ncanopy_collision_parts="<<canopySolids.size()
                    <<"\ncanopy_collision_spacing_m="<<(Mistwatch::WorldProfileHasWoodCollision(worldProfile)?.15:0.)
                    <<"\ncanopy_wood_source_triangles="<<canopyWoodSourceTriangles
                    <<"\ncanopy_wood_surface_cells="<<canopyWoodSurfaceCells
                    <<"\ncanopy_wood_enclosed_grid_cells="<<canopyWoodEnclosedGridCells
                    <<"\ncanopy_preview_fixture=0\ncanopy_world_profile="<<Mistwatch::WorldProfileHasCanopy(worldProfile)<<"\ncanopy_enabled="<<canopyEnabled
                    <<"\ncanopy_plants="<<canopyPopulation.plants.size()<<"\ncanopy_population_fingerprint="<<canopyPopulation.fingerprint
                    <<"\ncanopy_ground_checks="<<canopyPopulation.groundChecks<<"\ncanopy_rooted_meshes="<<canopyPopulation.plants.size()
                    <<"\ncanopy_primitive_triangles="<<canopyTriangles<<"\ncanopy_materials="<<canopyMaterials.Stats().materials
                    <<"\ncanopy_submitted_slots="<<submittedCanopy<<"\ncanopy_view_yaw="<<canopyViewYaw
                    <<"\nentrance_canopy_layout="<<(woodlandWorld?Mistwatch::WoodlandCanopy::Profile:entranceCanopy.appended?"entrance-authored-world-v1":"original-four")
                    <<"\nentrance_author_applied="<<entranceWorld.applied
                    <<"\nentrance_author_fingerprint="<<entranceWorld.fingerprint
                    <<"\nentrance_author_source_fir_fingerprint="<<entranceWorld.sourceFirFingerprint
                    <<"\nentrance_author_source_middle_fingerprint="<<entranceWorld.sourceMiddleFingerprint
                    <<"\nentrance_author_withdrawals="<<(entranceWorld.applied?(woodlandWorld?18:11):0)
                    <<"\nentrance_author_fern_relocations="<<(entranceWorld.applied?(rightForeground?7:5):0)
                    <<"\nentrance_composition_profile="<<(rightForeground?Mistwatch::RightForegroundProfile:"original")
                    <<"\nentrance_composition_original_fingerprint="<<compositionOriginalFingerprint
                    <<"\nentrance_composition_pose_fingerprint="<<compositionPoseFingerprint
                    <<"\nentrance_composition_tree_moves="<<(rightForeground?1:0)
                    <<"\nentrance_composition_fern_moves="<<(rightForeground?2:0)
                    <<"\nentrance_author_fir_target="<<(entranceWorld.applied?95:100)
                    <<"\nentrance_author_middle_target="<<(entranceWorld.applied?(woodlandWorld?20:23):24)
                    <<"\nentrance_canopy_original_fingerprint="<<entranceCanopy.originalFingerprint
                    <<"\nentrance_canopy_appended="<<entranceCanopy.appended
                    <<"\nentrance_canopy_full_face_checks="<<entranceCanopy.faceChecks
                    <<"\nentrance_canopy_exact_overlay_faces="<<entranceCanopy.exactOverlayFaces
                    <<"\nentrance_canopy_new_wood_parts="<<entranceCanopy.woodParts
                    <<"\nshrubs_profile="<<(shrubArcDensity?"woodland-feasible-arc-v3":shrubGapDensity?"woodland-gap-density-v2":(shrubTarget==64u?"original":"woodland-density-v1"))
                    <<"\nshrubs_target="<<shrubTarget
                    <<"\nshrubs_enabled="<<shrubsEnabled<<"\nshrubs_plants="<<shrubCanopy.plants.size()
                    <<"\nshrubs_fingerprint="<<shrubCanopy.fingerprint<<"\nshrubs_submitted="<<submittedShrubs
                    <<"\ntrail_litter_patches="<<trailLitter.patches.size()
                    <<"\ntrail_litter_branches="<<trailLitter.branchCount
                    <<"\ntrail_litter_stones="<<trailLitter.stoneCount
                    <<"\ntrail_litter_fingerprint="<<trailLitter.fingerprint
                    <<"\nsubmitted_trail_litter="<<submittedTrailLitter
                    <<"\ntrail_litter_triangles="<<(trailLitter.branches.indices.size()+trailLitter.stones.indices.size())/3<<'\n';
                std::uint64_t shrubAuthorTriangles{},shrubRenderTriangles{};
                unsigned shrubUploaded{};
                for(const auto& p:shrubCanopy.plants) {
                    shrubAuthorTriangles+=(shrubCanopy.variants[p.variant].stems.indices.size()+shrubCanopy.variants[p.variant].leaves.indices.size())/3;
                    shrubRenderTriangles+=shrubMeshes[p.variant][0].TriangleCount()+shrubMeshes[p.variant][1].TriangleCount();
                }
                for(const auto& pair:shrubMeshes)for(const auto& mesh:pair)shrubUploaded+=bool(mesh);
                out<<"shrubs_author_leaf_count="<<shrubCanopy.variants[0].leafCount
                    <<"\nshrubs_render_leaf_counts=";
                for(unsigned i=0;i<4;++i){if(i)out<<',';out<<shrubRenderLeafCounts[i];}
                out<<"\nshrubs_author_instance_triangles="<<shrubAuthorTriangles
                    <<"\nshrubs_render_instance_triangles="<<shrubRenderTriangles
                    <<"\nshrubs_uploaded_meshes="<<shrubUploaded<<"\nshrubs_uploaded_stem_indices=";
                for(unsigned i=0;i<4;++i){if(i)out<<',';out<<shrubMeshes[i][0].indexCount;}
                out<<"\nshrubs_uploaded_leaf_indices=";
                for(unsigned i=0;i<4;++i){if(i)out<<',';out<<shrubMeshes[i][1].indexCount;}
                out<<'\n';
                out<<"camera_source="<<((captureFallenView>=0||captureLandscapeDetail>=0||captureCanopyOverview||captureShrubOverview||captureTrailOverview||diagnosticCrown||captureRiverOverview||captureShelterOverview||captureBoarOverview||captureBoulderOverview||captureGrassOverview||captureGrassDistance||captureFernOverview||captureSupplyOverview||captureRangerHutOverview||captureWorkshopOverview||captureDeciduousRootView)?"diagnostic":"player")
                    <<"\ncamera_distance="<<Length(lastRenderedCamera.position-lastRenderedCamera.target)
                    <<"\ncamera_height="<<lastRenderedCamera.position.y
                    <<"\ncamera_anchor_above_feet="<<lastRenderedCamera.target.y-session.feet.y<<'\n';
                out<<"camera_position="<<lastRenderedCamera.position.x<<','<<lastRenderedCamera.position.y<<','<<lastRenderedCamera.position.z
                    <<"\ncamera_target="<<lastRenderedCamera.target.x<<','<<lastRenderedCamera.target.y<<','<<lastRenderedCamera.target.z
                    <<"\ncamera_up="<<lastRenderedCamera.up.x<<','<<lastRenderedCamera.up.y<<','<<lastRenderedCamera.up.z
                    <<"\ncamera_fov="<<lastRenderedCamera.projection.verticalFieldOfView
                    <<"\ncamera_framing_source=player_candidate"
                    <<"\ncamera_subject_visible="<<lastPlayerFraming.subjectVisible
                    <<"\ncamera_yaw_adjustment="<<lastPlayerFraming.yawAdjustment
                    <<"\ncamera_candidates_tested="<<lastPlayerFraming.candidatesTested<<'\n';
                out<<"camera_motion_source="<<(captureFrameCamera?"diagnostic_render_step":"fixed_update")<<"\ncamera_motion_status="<<int(lastCameraMotion.status)
                    <<"\ncamera_motion_accepted="<<lastCameraMotion.accepted
                    <<"\ncamera_motion_subject_visible="<<lastCameraMotion.subjectVisible
                    <<"\ncamera_fixed_ticks="<<cameraFixedTicks
                    <<"\ncamera_max_tick_displacement="<<cameraMaximumTickDisplacement
                    <<"\ncamera_occlusion_seconds="<<cameraOcclusionSeconds<<'\n';
                if(captureGrassDistance) {
                    const auto& proof=grassFixtureFrozen;
                    out<<std::setprecision(std::numeric_limits<float>::max_digits10)
                        <<"grass_fixture_schema=1\ngrass_fixture_index="<<grassFixtureIndex
                        <<"\ngrass_fixture_variant="<<(forestCover.plants.empty()?unsigned(grassFixtureIndex%5):forestCover.plants[grassFixtureIndex].variant)
                        <<"\ngrass_fixture_identity_high=5572150615523553875\ngrass_fixture_identity_low="<<grassFixtureIndex+1
                        <<"\ngrass_fixture_generation=1\ngrass_fixture_enabled="<<grassFixtureEnabled
                        <<"\ngrass_fixture_feet_mode=synthetic_same_height\ngrass_fixture_root="<<proof.root.x<<','<<proof.root.y<<','<<proof.root.z
                        <<"\ngrass_fixture_feet="<<proof.feet.x<<','<<proof.feet.y<<','<<proof.feet.z
                        <<"\ngrass_fixture_selection_source=isolated_diagnostic_reference"
                        <<"\ngrass_fixture_actor_feet="<<session.feet.x<<','<<session.feet.y<<','<<session.feet.z
                        <<"\ngrass_fixture_actor_isolated="<<(Length(session.feet-Mistwatch::Session{}.feet)==0)
                        <<"\ngrass_fixture_scale="<<proof.scale<<"\ngrass_fixture_yaw="<<proof.yaw
                        <<"\ngrass_fixture_player_distance="<<proof.playerDistance
                        <<"\ngrass_fixture_camera_distance="<<proof.cameraDistance
                        <<"\ngrass_fixture_gate_selected="<<proof.selected
                        <<"\ngrass_fixture_scene_accepted="<<proof.sceneAccepted
                        <<"\ngrass_fixture_opaque_draw_instances="<<proof.opaque
                        <<"\ngrass_fixture_shadow_only_instances="<<proof.shadowOnly
                        <<"\ngrass_fixture_draw_input_triangles="<<proof.triangles
                        <<"\ngrass_fixture_scene_serial="<<proof.sceneSerial
                        <<"\ngrass_fixture_camera_hash="<<proof.cameraHash
                        <<"\ngrass_fixture_selection_limit=35\ngrass_fixture_image_kind=production_final_image_not_direct_pixel_count\n"
                        <<std::setprecision(6);
                }
                if(captureBenchmark) {
                    auto rawPath=CapturePath();rawPath+=L".gpu-samples.tsv";
                    std::ofstream raw(rawPath);raw<<"serial\tbegin_tick\tend_tick\tfrequency\tnanoseconds\tbloom_resource_identity\n";
                    for(const auto& sample:captureGpuBenchmark.Receipts())raw<<sample.serial<<'\t'<<sample.beginTick<<'\t'<<sample.endTick<<'\t'
                        <<sample.frequency<<'\t'<<sample.nanoseconds<<'\t'<<sample.bloomResourceIdentity<<'\n';
                    raw.close();if(!raw)throw std::runtime_error("GPU benchmark raw samples write failed");
                    const auto samples=captureGpuBenchmark.Milliseconds();
                    std::vector<double> ordered(samples.begin(),samples.end());std::sort(ordered.begin(),ordered.end());
                    out<<"benchmark_mode=fixed_scene_gpu_only\ngpu_samples="<<ordered.size()
                        <<"\ngpu_first_sample_serial="<<captureGpuBenchmark.Serials().front()
                        <<"\ngpu_last_sample_serial="<<captureGpuBenchmark.Serials().back()
                        <<"\ngpu_mean_ms="<<std::accumulate(ordered.begin(),ordered.end(),0.0)/double(ordered.size())
                        <<"\ngpu_p50_ms="<<ordered[59]<<"\ngpu_p95_ms="<<ordered[113]
                        <<"\ngpu_max_ms="<<ordered.back()<<'\n';
                }
                out.close();if(!out)throw std::runtime_error("Capture receipt write failed");
                WriteWoodlandReceipt(CapturePath());
                WriteFallenLogReceipt(CapturePath());
                if(captureCostEnabled)captureReceiptEndNs=CaptureElapsedNs();
                ++captureCompleted;++captureConsumed;
                if(!captureSequenceFrames || captureCompleted>=captureSequenceFrames){
                    e.Quit();if(captureCostEnabled)captureQuitNs=CaptureElapsedNs();
                }
                else {
                    capturePending=false;waterTime=2.5f+float(captureCompleted)*.1f;
                    // 固定按鍵輸入展示完整一圈，再回到訊號方向；不冒充人工遊玩。
                    if(captureRelayMotion&&captureCompleted<20&&!session.TuneRelay(0,1))throw std::runtime_error("Relay motion input failed");
                    if(captureRelayMotion&&captureCompleted==20&&!session.ConnectRelay(0))throw std::runtime_error("Relay motion link failed");
                    if(captureLandscapeMotion)orbit.yaw=1.25f+float(captureCompleted)*6.2831853f/36.f;
                    captureCameraTicks=0;cameraMaximumTickDisplacement=0;
                }
            }
            // GPU 驗證會插入額外 shader 檢查；診斷等待不作為正式效能數據。
            const auto captureTimeout=std::chrono::seconds(stats.gpuValidationRequested||captureLandscapeMotion?120:30);
            if(std::chrono::steady_clock::now()-captureStart>captureTimeout){
                if(captureCostEnabled)captureTimeoutNs=CaptureElapsedNs();
                throw std::runtime_error("Frame capture timeout; rendered="+std::to_string(captureFrames)+
                    "; submitted="+std::to_string(stats.submittedFrames)+"; gpu-validation="+std::to_string(stats.gpuValidationRequested));
            }
            return;
        }
        auto& in=matureKeyActive?matureKeyControls:inputReplayDirectory.empty()?e.GetInput():inputReplay.Controls();
        // 取物緩衝屬短暫輸入，不寫入存檔，也不能跨越選單、焦點或存讀檔邊界。
        if(!started||paused||!in.Focused()||in.FocusLost()||!session.Alive()||session.beaconLit||
           in.Pressed(VK_ESCAPE)||in.Pressed('J')||in.Pressed(VK_F5)||in.Pressed(VK_F9)||in.Pressed('R'))
            pickupIntent.Clear();
        if(!Mistwatch::CanTrackRelay(session,trackedRelay))trackedRelay=Mistwatch::NoTrackedRelay;
        if(!Mistwatch::CanTrackRelay(session,previewRelay))previewRelay=Mistwatch::NoTrackedRelay;
        if(!in.Down(VK_LEFT))relayNavigationHeld&=~1u;
        if(!in.Down(VK_RIGHT))relayNavigationHeld&=~2u;
        const auto cameraTurn=[&]{return float(in.Down(VK_RIGHT)&&!(relayNavigationHeld&2u))-
            float(in.Down(VK_LEFT)&&!(relayNavigationHeld&1u));};
        // 焦點安全仍優先；可遊玩狀態的讀檔先於暫停及音量鍵，避免同一步遺失或修改設定。
        if(!paused&&in.Focused()&&!in.FocusLost()&&in.Pressed(VK_F9)&&!save.empty()) {
            Mistwatch::SaveGame saved;
            if(matureIndexedDomain)++matureLoadHandlerCalls;
            auto prepared=Mistwatch::ReadSaveFile(save,saved)?PrepareRuntimeCheckpoint(saved):Mistwatch::LoadPreparationResult{};
            if(prepared&&Mistwatch::ValidatePreparedPairLoad(*prepared.candidate)){
                Mistwatch::SwapPreparedSavedGame(*prepared.candidate,{session,encounter,evasion,controller,explorer,boar,orbit,
                    solids,playerCameraMotion,lastPlayerFraming,lastCameraMotion,lastRenderedCamera,&playerLocomotion});
                tuningInputRepeat.Reset();ResetPlayablePairRuntime();
                started=true;paused=false;
                cameraOcclusionSeconds=0;
                gameAudio.director.Reset();
                message="Loaded";
                actionFeedback.Publish(Mistwatch::ActionFeedbackEvent::Loaded);
                TickAudio(e,dt);
            }else{
                message="Save unavailable";
                actionFeedback.Publish(Mistwatch::ActionFeedbackEvent::LoadUnavailable);
            }
            // 成功或失敗都不在載入當步推進世界與動作，也不執行同時按下的其他遊戲按鍵。
            // 失敗也不更新遊戲音效事件觀測器；底層已播放的聲音仍由引擎正常服務。
            return;
        }
        if(matureIndexedDomain&&!RefreshRuntimeBody(dt)){message="World collision query unavailable - controls retained";return;}
        const bool wasStarted=started,wasPaused=paused;
        Mistwatch::UpdatePlaybackControl(started,paused,in.Focused()&&!in.FocusLost(),in.Pressed(VK_RETURN),in.Pressed(VK_ESCAPE));
        const bool resumedFromPause=wasStarted&&wasPaused&&!paused;
        const bool cameraSettingsEditable=Mistwatch::CanEditCameraSettings(in.Focused()&&!in.FocusLost(),
            wasStarted,wasPaused,started,paused,in.Pressed(VK_RETURN),in.Pressed(VK_ESCAPE));
        if(cameraOptions.Adjust(cameraSettingsEditable,in.Pressed(VK_OEM_4),in.Pressed(VK_OEM_6),
            in.Pressed('I'),in.Pressed(VK_BACK)))SaveCameraOptions();
        // 只在真正開始時更新回饋；恢復暫停或無焦點按鍵不能覆蓋既有操作結果。
        if(!wasStarted&&started)message="New expedition";
        bool changedAudio=false;
        if(in.Pressed('M')){gameAudio.options.muted=!gameAudio.options.muted;changedAudio=true;}
        if(in.Pressed(VK_PRIOR)){gameAudio.options.volume=std::clamp(gameAudio.options.volume+.1f,0.f,1.f);changedAudio=true;}
        if(in.Pressed(VK_NEXT)){gameAudio.options.volume=std::clamp(gameAudio.options.volume-.1f,0.f,1.f);changedAudio=true;}
        if(changedAudio){e.Audio().SetMasterVolume(gameAudio.options.Gain());SaveAudioOptions();}
        std::optional<Mistwatch::PlayableFrameGuard> playableFrame;
        playableCameraOnlyCommitted=false;playableOutcome=PlayableOutcome::None;
        if(started&&!paused&&!resumedFromPause&&session.Alive()&&!session.beaconLit){
            if(!(playableRecoveryCause!=PlayableRecoveryCause::None&&in.Pressed('R'))){
                const float inputYaw=Mistwatch::PlayerCameraMovementYaw(playerCameraMotion.Camera(),orbit.yaw);
                const float ix=float(in.Down('D'))-float(in.Down('A')),iz=float(in.Down('W'))-float(in.Down('S'));
                Vec3 inputDirection{ix*std::cos(inputYaw)-iz*std::sin(inputYaw),0,ix*std::sin(inputYaw)+iz*std::cos(inputYaw)};
                if(Length(inputDirection)<.001f)inputDirection={std::sin(inputYaw),0,-std::cos(inputYaw)};
                if(!playableInputs.Observe(in,inputDirection)){
                    playableOutcome=PlayableOutcome::InputOverflow;message="Input queue full - ESC cancels pending controls";return;
                }
            }
            if(!playableScratch.Capture(PlayableRefs())){message="Frame preparation unavailable - input retained";return;}
            playableFrame.emplace(PlayableRefs(),playableScratch);
        }
        actionFeedback.Advance(dt,(!started||paused||resumedFromPause)?Mistwatch::ActionFeedbackClock::Suspended:
            Mistwatch::ActionFeedbackClock::Active);
        if(in.Focused()&&!in.FocusLost()&&in.Pressed('J'))fieldJournalOpen=!fieldJournalOpen;
        // 地圖操作只發生在原已暫停且仍暫停的視窗；不改世界，不把選站鍵交給下一步。
        const bool relaySelection=wasPaused&&paused&&started&&session.Alive()&&
            session.CurrentStage()==Mistwatch::Stage::Relays&&!fieldJournalOpen&&
            in.Focused()&&!in.FocusLost()&&!in.Pressed('J')&&!in.Pressed(VK_ESCAPE);
        if(relaySelection){
            if(in.Pressed(VK_LEFT)&&in.Down(VK_LEFT))relayNavigationHeld|=1u;
            if(in.Pressed(VK_RIGHT)&&in.Down(VK_RIGHT))relayNavigationHeld|=2u;
        }
        if(!relaySelection)previewRelay=Mistwatch::NoTrackedRelay;
        else{
            const bool left=in.Pressed(VK_LEFT),right=in.Pressed(VK_RIGHT);
            if(left!=right){
                const auto nearest=Mistwatch::BuildObjectiveGuide(session,encounter,evasion,lastRenderedCamera,
                    controller.Settings().height<1.5f,controller.Grounded(),RuntimeGuideSource(solids),terrain);
                const unsigned from=Mistwatch::CanTrackRelay(session,previewRelay)?previewRelay:
                    Mistwatch::CanTrackRelay(session,trackedRelay)?trackedRelay:nearest.index;
                if(nearest.valid)previewRelay=Mistwatch::CycleTrackedRelay(session,from,right?1:-1);
            }else if(!left&&!right&&in.Pressed(VK_RETURN)&&Mistwatch::CanTrackRelay(session,previewRelay)){
                trackedRelay=previewRelay;previewRelay=Mistwatch::NoTrackedRelay;
            }
        }
        // 恢復當步只跨越暫停邊界；同批暫停輸入不交給世界，持續按鍵下一步仍可直接使用。
        if(Mistwatch::CanSavePausedCheckpoint(started,paused,resumedFromPause,in.Focused(),in.FocusLost(),in.Pressed(VK_F5),!save.empty()))
            SaveCheckpoint();
        if(!started || paused || resumedFromPause){playableInputs.Clear();tuningInputRepeat.Reset();playerLocomotion.Suspend();TickAudio(e,dt);return;}
        if(session.Alive()&&session.beaconLit&&!in.Pressed('R')){
            // 完成後只檢視鏡頭；R仍走原重開分支，F5仍先保存，不推進世界／步態時間。
            if(in.Pressed(VK_F5)&&!save.empty())SaveCheckpoint();
            if(in.Pressed('C')||playableInputs.RecenterRequested()){
                if(playableInputs.ObserveCamera(in)){
                    if(playableInputs.RecenterRequested()){
                        if(TryUpdateCameraOnly(playableInputs.Drag(),playableInputs.Wheel(),cameraTurn(),dt,nullptr,true))
                            playableInputs.CommitCamera();
                    }else{
                        // 已取消的快捷意圖不殘留；手動檢視保持原完成分支的單步語意。
                        TryUpdateCameraOnly(in.LeftMouseDragDelta(),in.MouseWheel(),cameraTurn(),dt);
                        playableInputs.CommitCamera();
                    }
                }
            }else TryUpdateCameraOnly(in.LeftMouseDragDelta(),in.MouseWheel(),cameraTurn(),dt);
            TickAudio(e,dt);return;
        }
        bool resetTick=false;
        if(std::isfinite(dt) && dt>0)waterTime=std::fmod(waterTime+(std::min)(dt,.1f),3600.f);
        if(playableRecoveryCause!=PlayableRecoveryCause::None&&in.Pressed('R')&&playableFrame){
            const auto previousAudioDirector=gameAudio.director;bool recovered=false;
            try{
                // 僅初始非法重疊或明確無可行讓位允許安全R；普通BlockOnly接觸不走此分支。
                session.Retry();if(matureIndexedDomain)solids=RuntimeGameplayBody(session.bridgeRepaired,session.feet);ResetController();if(!session.guardianDefeated)encounter={};ResetBoarPose();evasion={};
                bool clear=session.Valid()&&encounter.Valid()&&Mistwatch::EncounterSupported(encounter.feet,terrain)&&
                    !Mistwatch::EncounterBodyOverlaps(encounter.feet,RuntimeNpcSource(solids))&&
                    !Mistwatch::LoadPreparationDetail::BodyOverlaps(controller.Center(),controller.Extents(),solids,RuntimeRoofHulls())&&
                    terrain.Contains(session.feet.x,session.feet.z)&&session.feet.y>=terrain.SampleHeight(session.feet.x,session.feet.z);
                if(const auto body=Mistwatch::PairContact::LiveBody(session,encounter)){
                    BoxCylinderContact3D contact;
                    clear=clear&&QueryBoxCylinderContact3D(controller.Center(),controller.Extents(),{},*body,contact)&&
                        contact.kind==BoxCylinderContactKind3D::Clear;
                }
                recovered=clear&&UpdatePlayerCamera(dt);
                if(recovered){
                    message="Safe retry - expedition progress retained";
                    playableFrame->Commit();ResetPlayablePairRuntime();tuningInputRepeat.Reset();fieldJournalOpen=false;actionFeedback.Clear();
                }
            }catch(...){recovered=false;}
            if(recovered){TickAudio(e,dt);return;}
            if(!recovered){playableFrame->Rollback();gameAudio.director=previousAudioDirector;
                playableOutcome=PlayableOutcome::InitialPairRecovery;message="Safe retry unavailable - ESC pauses; prior state retained";return;}
        }
        if(in.Pressed('R') && session.beaconLit) {
            session={};solids=RuntimeGameplayBody(false,session.feet);ResetController();encounter={};ResetBoarPose();evasion={};orbit={};message="New expedition";resetTick=true;
        }
        if(in.Pressed('R') && !session.Alive()) {
            session.Retry();if(matureIndexedDomain)solids=RuntimeGameplayBody(session.bridgeRepaired,session.feet);ResetController();
            // 重試保留已完成任務；已擊敗的野豬不能重設為未擊敗而破壞檢查點一致性。
            if(!session.guardianDefeated)encounter={};
            ResetBoarPose();evasion={};resetTick=true;
        }
        if(resetTick){
            tuningInputRepeat.Reset();
            fieldJournalOpen=false;
            actionFeedback.Clear();
            if(!UpdatePlayerCamera(dt))throw std::runtime_error("Reset-tick player camera unavailable");
            ResetPlayablePairRuntime();
            TickAudio(e,dt);return;
        }
        if(!session.Alive() || session.beaconLit){
            tuningInputRepeat.Reset();
            playerLocomotion.Suspend();
            // 死亡只推進視覺骨架；不再推進玩家、敵人或任務。暫停已在上方返回。
            if(!session.Alive()&&!explorer.UpdateDeath(dt,session.feet,terrain,solids))
                throw std::runtime_error("Explorer death pose failed");
            if(in.Pressed(VK_F5) && !save.empty())
                SaveCheckpoint();
            if(!playerCameraMotion.Initialized()&&!UpdatePlayerCamera(dt))throw std::runtime_error("Loaded terminal-session camera unavailable");
            TickAudio(e,dt);return;
        }
        if(!playableFrame){message="Frame transaction unavailable";return;}
        const auto edges=playableInputs.Front();
        // 此副本只在世界交易成功時發布；鏡頭或身體拒絕不得偷走緩衝時間。
        auto nextPickupIntent=pickupIntent;
        const bool allowPickupIntent=!fieldJournalOpen&&!in.Pressed('J')&&!in.Pressed(VK_F5)&&
            !in.Pressed(VK_F9)&&!edges.Pressed(Mistwatch::FrameEdge::Save);
        if(!allowPickupIntent)nextPickupIntent.Clear();
        bool cameraAttempted=false,worldCommitted=false,saveAfterCommit=false,retainCounterAttack=false;
        playableLastContact.reset();playableLastPairStatus.reset();
        const auto orbitInput=[&]{orbit.Update(playableInputs.Drag(),playableInputs.Wheel(),
            cameraTurn(),dt,playableInputs.RecenterRequested(),explorer.yaw,
            cameraOptions.Sensitivity(),cameraOptions.invertVertical);};
        const auto holdFrame=[&](const char* reason,bool contactPolicy=false,
            PlayableRecoveryCause recoveryCause=PlayableRecoveryCause::None){
            // 先還原整個世界候選，再由原camera owner只算camera-only候選；兩者不累加dt。
            playableFrame->Rollback();
            const bool cameraAccepted=TryUpdateCameraOnly(playableInputs.Drag(),playableInputs.Wheel(),
                cameraTurn(),dt,&cameraAttempted,playableInputs.RecenterRequested());
            if(cameraAccepted){playableInputs.CommitCamera();playableCameraOnlyCommitted=true;}
            playableRecoveryCause=recoveryCause;playableNeedsContactPolicy=contactPolicy;
            playableOutcome=recoveryCause!=PlayableRecoveryCause::None?PlayableOutcome::InitialPairRecovery:contactPolicy?PlayableOutcome::NeedsContactPolicy:
                cameraAccepted?PlayableOutcome::CameraOnly:PlayableOutcome::Held;
            inputReplayMotionAccepted=false;inputReplayMotionSteps=0;inputReplayLastDashing=false;inputReplayLastDamage=0;
            // camera-only成功不等同世界成功；既有欄位仍標記本幀世界候選已回滾。
            inputReplayCameraAttempted=cameraAttempted;inputReplayCameraRollback=cameraAttempted;
            message=reason;
        };
        try{
            // 只把幀開始已存在的非法pair交給R。站起候選或落下接觸不得冒充初始損壞。
            const auto oldBody=Mistwatch::PairContact::LiveBody(session,encounter);
            if(!session.Valid()||!encounter.Valid()){
                holdFrame("Invalid expedition state - prior frame retained");return;
            }
            if(oldBody){
                BoxCylinderContact3D contact;
                if(!QueryBoxCylinderContact3D(controller.Center(),controller.Extents(),{},*oldBody,contact)){
                    holdFrame("Contact query unavailable - controls retained",true);return;
                }
                if(contact.kind==BoxCylinderContactKind3D::StartedOverlapping){
                    holdFrame("Initial body overlap - R: safe retry / ESC: pause",false,PlayableRecoveryCause::InitialBodyOverlap);return;
                }
            }
            orbitInput();
            const float yaw=Mistwatch::PlayerCameraMovementYaw(playerCameraMotion.Camera(),orbit.yaw);
            const float x=float(in.Down('D'))-float(in.Down('A')),z=float(in.Down('W'))-float(in.Down('S'));
            const float previousHeight=controller.Settings().height;
            Mistwatch::PlayerLocomotionInput movementInput;
            movementInput.movement={x*std::cos(yaw)-z*std::sin(yaw),0,x*std::sin(yaw)+z*std::cos(yaw)};
            movementInput.dodgeDirection=edges.Pressed(Mistwatch::FrameEdge::Evade)?edges.dodgeDirection:movementInput.movement;
            movementInput.speedMultiplier=in.Down(VK_SHIFT)?1.5f:1.f;
            movementInput.jumpPressed=edges.Pressed(Mistwatch::FrameEdge::Jump);
            movementInput.evadePressed=edges.Pressed(Mistwatch::FrameEdge::Evade);
            movementInput.crouched=in.Down(VK_CONTROL);movementInput.running=in.Down(VK_SHIFT);
            const std::span<const DynamicCylinder3D> dynamic=oldBody?std::span<const DynamicCylinder3D>(&*oldBody,1):std::span<const DynamicCylinder3D>{};
            CharacterDynamicContactState3D contacts;
            std::optional<CharacterController3D> fullController;std::optional<Mistwatch::PlayerLocomotion> fullOwner;std::optional<Mistwatch::Explorer> fullPose;std::optional<Mistwatch::Evasion> fullEvasion;
            if(indexedGameplayProofEnabled){fullController.emplace(controller);fullOwner.emplace(playerLocomotion);fullPose.emplace(explorer);fullEvasion.emplace(evasion);}
            auto candidate=playerLocomotion.Advance(controller,evasion,explorer,movementInput,dt,
                solids,RuntimeRoofHulls(),terrain,[]{return true;},dynamic,CharacterDynamicVerticalResponse3D::BlockOnly,&contacts);
            if(indexedGameplayProofEnabled){CharacterDynamicContactState3D referenceContacts;
                const auto oracle=fullOwner->Advance(*fullController,*fullEvasion,*fullPose,movementInput,dt,matureIndexedDomain->Registered(session.bridgeRepaired),RuntimeRoofHulls(),terrain,[]{return true;},dynamic,CharacterDynamicVerticalResponse3D::BlockOnly,&referenceContacts);
                MatureContact::Require(candidate.motion.accepted==oracle.motion.accepted&&candidate.motion.steps==oracle.motion.steps&&candidate.motion.poseRejected==oracle.motion.poseRejected&&candidate.contact==oracle.contact&&
                    controller.StateHash()==fullController->StateHash()&&playerLocomotion.StateHash()==fullOwner->StateHash()&&MatureSamePose(explorer,*fullPose),"NORMAL_ONUPDATE_OWNER_POSE_DIFFERS_FROM_COMPLETE_WORLD");
                ++normalBodyOracleMatches;normalMotionFixedSteps+=candidate.motion.steps;
            }
            playableLastContact=candidate.contact;
            const auto& motion=candidate.motion;
            inputReplayMotionAttempted=true;
            if(!motion.accepted){
                const bool unresolved=candidate.contact&&*candidate.contact!=CharacterDynamicStepStatus3D::Applied;
                // 新BlockOnly/TOI控制器應在這裡正常Applied；目前保留typed診斷，不fatal、不授NPC支撐。
                holdFrame(unresolved?"Body contact policy pending - controls retained; ESC pauses":
                    motion.poseRejected?"Foot placement unavailable - controls retained":"Movement unavailable - controls retained",unresolved);
                return;
            }
            session.feet=controller.Position();
            if(motion.lowCeiling){message="Low ceiling - move clear before standing";
                if(!inputReplayDirectory.empty())inputReplayLowRoofObserved=true;}
            else if(previousHeight<1.5f&&controller.Settings().height>=1.5f&&message=="Low ceiling - move clear before standing")
                message="Headroom clear - standing restored";
            const bool crouched=controller.Settings().height<1.5f;
            const bool terminalFall=session.feet.y<-5.f;
            std::optional<std::vector<BoxCollider3D>> pendingWorld;
            std::span<const BoxCollider3D> activeWorld=solids;
            unsigned damage{};
            if(terminalFall){session.Damage(100);message="Fell into the ravine - R to retry";}
            else{
                unsigned tuningPressed{},tuningDown{};
                for(unsigned lever=0;lever<3;++lever){
                    if(edges.Pressed(static_cast<Mistwatch::FrameEdge>(unsigned(Mistwatch::FrameEdge::Tune1)+lever)))tuningPressed|=1u<<lever;
                    if(in.Down('1'+lever))tuningDown|=1u<<lever;
                }
                const auto tuningContext=in.Focused()&&!in.FocusLost()?Mistwatch::FieldInput::RepeatContext(session):0;
                auto tuningKeys=tuningInputRepeat.Tick((std::min)(double(dt),.1),tuningContext,tuningPressed,tuningDown);
                // 明示確認不被同 tick 到期的長按連發搶先改值；新按調整仍照原順序先執行。
                // 計時器照常前進，失敗確認不重啟延遲，也不在下一 tick 補發被略過的連發。
                if(edges.Pressed(Mistwatch::FrameEdge::Interact))tuningKeys&=tuningPressed;
                for(unsigned lever=0;lever<3;++lever)if(tuningKeys&(1u<<lever)){
                    if(session.CurrentStage()==Mistwatch::Stage::Relays){
                        const auto result=Mistwatch::FieldInput::Lever(session,lever);
                        if(result!=Mistwatch::FieldInput::Result::None)message=Mistwatch::FieldInput::Feedback(result);
                    }else if(session.CycleFuseDial(lever))message="Cabinet dial turned - match the ranger log, then E";
                    else if(session.ToggleCircuit(lever))message="Circuit switched - light all three lamps";
                }
                if(edges.Pressed(Mistwatch::FrameEdge::Interact)){
                    nextPickupIntent.Clear();
                    const unsigned suppliesBefore=session.collected,materialsBefore=session.repairMaterials;
                    bool changed=false,bridgeCameraRejected=false,bridgePlayerRejected=false;
                    for(unsigned i=0;i<3;++i)changed=session.Collect(i)||changed;
                    for(unsigned i=0;i<3;++i)changed=session.CollectRepairMaterial(i,crouched)||changed;
                    auto bridgeCandidate=session;
                    if(bridgeCandidate.RepairBridge()){
                        auto nextWorld=RuntimeGameplayBody(true,controller.Position());
                        const auto bridgePlayer=matureIndexedDomain?PrepareIndexedGameplayBridge():
                            Mistwatch::PrepareBridgeRepairPlayer(controller,explorer,nextWorld,RuntimeRoofHulls(),terrain,playerCameraMotion.Camera(),RuntimeCameraOnly());
                        if(bridgePlayer){
                            pendingWorld.emplace(std::move(nextWorld));activeWorld=*pendingWorld;session=bridgeCandidate;changed=true;
                        }else if(bridgePlayer.reason==Mistwatch::BridgeRepairPlayerReason::CameraBlocked)bridgeCameraRejected=true;
                        else bridgePlayerRejected=true;
                    }
                    changed=session.ReadFuseClue()||changed;changed=session.CollectFuse()||changed;changed=session.RepairGenerator()||changed;
                    const auto fieldResult=Mistwatch::FieldInput::Interact(session);
                    changed=(fieldResult!=Mistwatch::FieldInput::Result::None&&fieldResult!=Mistwatch::FieldInput::Result::Invalid)||changed;
                    changed=session.LightBeacon()||changed;
                    message=bridgePlayerRejected?"Move character clear before bridge repair":
                        bridgeCameraRejected?"Move camera clear before bridge repair":changed?"Objective updated":session.CurrentStage()==Mistwatch::Stage::RepairMaterials?
                        "Find tools in west shelter (CTRL), and both timber caches":
                        session.CurrentStage()==Mistwatch::Stage::Fuse&&session.NearInterior(Mistwatch::Session::fuse)?
                        "Cabinet locked - read ranger log, set dials 1 / 2 / 3":
                        session.CurrentStage()==Mistwatch::Stage::Relays?"Ridge relay: 1 left / 2 right, green signal lock, then E":"Move closer to the objective";
                    if(!bridgePlayerRejected&&!bridgeCameraRejected&&
                       (fieldResult!=Mistwatch::FieldInput::Result::None||!changed))
                        if(const auto* feedback=Mistwatch::FieldInput::InteractionFeedback(session,fieldResult))
                            message=feedback;
                    if(!bridgePlayerRejected&&!bridgeCameraRejected&&message=="Objective updated")
                        if(const auto pickup=Mistwatch::PickupFeedback::Build(suppliesBefore,session.collected,materialsBefore,session.repairMaterials))
                            message=*pickup;
                    // 舊 FIFO 邊沿仍照原規則嘗試互動，但不能在稍後另取得新的提前按鍵寬容。
                    if(!changed&&allowPickupIntent&&in.Pressed('E')&&playableInputs.Count()==1){
                        const auto guide=Mistwatch::BuildObjectiveGuide(session,encounter,evasion,lastRenderedCamera,
                            crouched,controller.Grounded(),RuntimeGuideSource(activeWorld),terrain);
                        nextPickupIntent.Arm(session,guide);
                    }
                }else if(allowPickupIntent&&nextPickupIntent.Armed()){
                    const unsigned suppliesBefore=session.collected,materialsBefore=session.repairMaterials;
                    const auto guide=Mistwatch::BuildObjectiveGuide(session,encounter,evasion,lastRenderedCamera,
                        crouched,controller.Grounded(),RuntimeGuideSource(activeWorld),terrain);
                    // 只重試已鎖定的單項拾取，不重跑修橋、機關或戰鬥的通用 E 分支。
                    if(nextPickupIntent.Advance(session,guide,(std::min)(double(dt),.1)))
                        if(const auto pickup=Mistwatch::PickupFeedback::Build(suppliesBefore,session.collected,materialsBefore,session.repairMaterials))
                            message=*pickup;
                }
                const bool activeEncounter=session.CurrentStage()==Mistwatch::Stage::Encounter;
                const auto previous=encounter.feet;
                const auto beforeEncounter=encounter;
                // E先完成；Encounter只預覽/Tick一次。NPC每個實際static/terrain子段由pair檢查玩家final體積。
                const auto pair=Mistwatch::PairContact::AdvanceNpc(playablePairState,session,encounter,controller,dt,
                    edges.Pressed(Mistwatch::FrameEdge::Attack)&&!motion.dashing,playablePairState.lastSerial+1,RuntimeNpcSource(activeWorld),terrain,
                    [&](const Mistwatch::Encounter& next){
                        if(!activeEncounter&&Mistwatch::PairContact::Same(next.feet,previous))return true;
                        const auto look=(next.phase==Mistwatch::EnemyPhase::Windup?next.strikeTarget:session.feet)-next.feet;
                        return boar.Update(next.feet-previous,(std::min)(dt,.1f),next.phase,(std::max)(0.f,next.phaseTime),look)&&
                            boar.PlaceFeet(next.feet,terrain);
                    });
                playableLastPairStatus=pair.status;
                if(!pair.inputConsumed){
                    const bool noLegalYield=pair.status==Mistwatch::PairContact::Status::RecoveryRequired;
                    holdFrame(noLegalYield?
                        "NPC has no legal yield route - R: safe retry / ESC: pause":
                        pair.status==Mistwatch::PairContact::Status::PoseRejected?"Boar footing unavailable - controls retained":
                        "NPC movement unavailable - controls retained",true,
                        noLegalYield?PlayableRecoveryCause::NpcNoLegalYield:PlayableRecoveryCause::None);return;
                }
                damage=pair.motion.damage;session.Damage(damage);
                if(activeEncounter)Mistwatch::UpdateEncounterFeedback(session,encounter,message,motion.lowCeiling);
                // 只保留真開窗邊界的一次 F；拒絕幀不提交，下一成功世界步必須消耗。
                if(activeEncounter&&session.Alive()&&damage==0&&!motion.dashing&&
                   edges.Pressed(Mistwatch::FrameEdge::Attack)&&!edges.deferredAttack&&
                   beforeEncounter.phase==Mistwatch::EnemyPhase::Windup&&
                   encounter.phase==Mistwatch::EnemyPhase::Recovery&&encounter.health==beforeEncounter.health){
                    retainCounterAttack=Mistwatch::BuildObjectiveGuide(session,encounter,evasion,lastRenderedCamera,
                        controller.Settings().height<1.5f,controller.Grounded(),RuntimeGuideSource(activeWorld),terrain).counterReady;
                }
            }
            cameraAttempted=true;
            if(!UpdatePlayerCamera(dt,activeWorld)){holdFrame("Camera clearance - movement held");return;}
            // 可見狀態只在這一點提交；通常只借原solids，真正修橋才交換新vector。
            if(pendingWorld)solids.swap(*pendingWorld);
            playableFrame->Commit();playableInputs.CommitWorld(retainCounterAttack);
            if(!session.Alive()||session.beaconLit)nextPickupIntent.Clear();
            pickupIntent=nextPickupIntent;
            if(!Mistwatch::CanTrackRelay(session,trackedRelay))trackedRelay=Mistwatch::NoTrackedRelay;
            if(!Mistwatch::CanTrackRelay(session,previewRelay))previewRelay=Mistwatch::NoTrackedRelay;
            playableCommittedContact=contacts;
            worldCommitted=true;saveAfterCommit=!terminalFall&&edges.Pressed(Mistwatch::FrameEdge::Save)&&!save.empty();
            playableRecoveryCause=PlayableRecoveryCause::None;playableNeedsContactPolicy=false;playableOutcome=PlayableOutcome::WorldCommitted;
            inputReplayMotionAccepted=true;inputReplayMotionSteps=motion.steps;inputReplayLastDashing=motion.dashing;
            inputReplayLastDamage=damage;inputReplayCameraAttempted=true;inputReplayCameraRollback=false;
        }catch(const std::exception& error){runtimeSourceFailure=error.what();holdFrame("Frame candidate unavailable - prior state and controls retained");}
        catch(...){runtimeSourceFailure="unknown frame exception";holdFrame("Frame candidate unavailable - prior state and controls retained");}
        // 檔案與音效只在成功提交後執行；不能把提交後I/O例外當成世界已回滾。
        if(!worldCommitted)return;
        RecordIndexedGameplayCommit();
        if(saveAfterCommit)SaveCheckpoint();
        TickAudio(e,dt);
    }
    void OnRender(Engine& e,float) override {
        counterCueSubmitted=0;
        surveyMapRequested=false;surveyMapDraw={};
        RecordInputReplayCost(e);
        if(captureBenchmark){
            // renderer上一輪已完成的收據在每次渲染前收集；不依賴固定更新是否執行。
            const auto stats=e.GetRendererStats();
            captureGpuBenchmark.Observe(stats.lastCompletedGpuTimingReceipt,stats.submittedFrames);
            if(captureCostEnabled&&captureGpuBenchmark.Complete()&&!benchmarkCompleteNs)benchmarkCompleteNs=CaptureElapsedNs();
        }
        if(captureFrameCamera){
            if(capture.empty()||captureWalk||captureSequenceFrames||captureAudioProbe||captureWildlife||captureFocusProbe)
                throw std::runtime_error("Fixed-frame camera is only supported for static comparison");
            // 僅外部靜態A/B工具每render步進一次，排除OnUpdate排程次數差異；普通遊玩不走此路徑。
            // 靜態比較在固定46步後凍結鏡頭，GPU收據回收延遲不可再改鏡頭狀態。
            if(!capturePending&&cameraFixedTicks<46&&!UpdatePlayerCamera(1.f/60.f))throw std::runtime_error("Diagnostic render-step camera rejected");
        }
        scene.Clear();
        entranceRoadSubmitted=0;
        if(entranceRoadEnabled){
            if(!scene.Mesh(entranceRoadMeshes[0],{},TerrainRenderMaterial())||
                !scene.Mesh(entranceRoadMeshes[1],{},entranceRoadSurface))
                throw std::runtime_error("Entrance road domain scene submission failed");
            entranceRoadSubmitted=2;
            if(settlementSurfaceEnabled){
                if(!scene.Mesh(entranceRoadMeshes[2],{},settlementSurfaceMaterial))throw std::runtime_error("Settlement gravel submission rejected");
                ++entranceRoadSubmitted;
            }
        }else scene.Mesh(terrainMesh,{},TerrainRenderMaterial());
        scene.Mesh(landscapeMesh,{},TerrainRenderMaterial());
        submittedCanopy=0;
        if(canopyEnabled)for(std::size_t i=0;i<canopyPopulation.plants.size();++i)for(unsigned slot=0;slot<3;++slot){
            const auto mesh=slot?canopyMeshes[slot]:canopyRootMeshes[i];
            if(!scene.Mesh(mesh,canopyPopulation.plants[i].pose,canopySurfaces[slot],{},{{0x43414e4f50594658ull,i*3+slot+1},1}))
                throw std::runtime_error("Canopy-world submission failed");
            ++submittedCanopy;
        }
        submittedShrubs=0;
        if(shrubsEnabled)for(std::size_t index=0;index<shrubCanopy.plants.size();++index){
            const auto& plant=shrubCanopy.plants[index];
            for(unsigned slot=0;slot<2;++slot){const auto material=slot?broadleafSurface[1]:barkMaterial;
                if(!scene.Mesh(shrubMeshes[plant.variant][slot],plant.pose,material,{},{{0x4d57534852554253ull,index*2+slot+1},1}))throw std::runtime_error("Shrub submission failed");}
            ++submittedShrubs;
        }
        submittedEntranceUnderstory=0;
        if(shrubsEnabled)for(std::size_t index=0;index<entranceUnderstory.plants.size();++index){
            const auto& plant=entranceUnderstory.plants[index];
            for(unsigned slot=0;slot<2;++slot){
                const auto material=slot?broadleafSurface[1]:barkMaterial;
                if(!scene.Mesh(shrubMeshes[plant.variant][slot],plant.pose,material,{},{{0x4d57454e54555354ull,index*2+slot+1},1}))
                    throw std::runtime_error("Entrance understory submission failed");
            }
            ++submittedEntranceUnderstory;
        }
        coreHerbFrame={};
        if(coreHerbEnabled)for(unsigned i=0;i<coreHerbs.plants.size();++i){
            if(!scene.Mesh(coreHerbMeshes[i],coreHerbs.plants[i].pose,roadsideBroadleafSurface,{},{{CoreHerbIdentityNamespace,i+1},1}))
                throw std::runtime_error("Core herb scene insertion failed");
            ++coreHerbFrame.accepted;
        }
        if(coreHerbFrame.accepted!=(coreHerbEnabled?Mistwatch::CoreHerbClusters::Target:0u))
            throw std::runtime_error("Core herb accepted population mismatch");
        routeGrassFrame={};
        if(routeGrassEnabled)for(unsigned i=0;i<routeGrassPlan.instances.size();++i){
            if(!scene.Mesh(routeGrassMeshes[i],routeGrassPlan.instances[i].plant.pose,grassSurface,{},
                {{Mistwatch::RouteGrassDraw::InstanceNamespace,i+1},1}))throw std::runtime_error("Route grass scene insertion failed");
            ++routeGrassFrame.accepted;
        }
        if(routeGrassFrame.accepted!=(routeGrassEnabled?128u:0u))throw std::runtime_error("Route grass population mismatch");
        // 背景林使用既有 CC0 冷杉最低網格細節；不加入任務區的角色碰撞體。
        landscapeFrame={};
        for(unsigned ordinal=0;ordinal<landscapeTrees.size();++ordinal){
            if(shoulderGroveEnabled&&Mistwatch::ShoulderGrove::IsSelected(shoulderGrove,ordinal))continue;
            const auto selected=std::find_if(landscapePlanting.plants.begin(),landscapePlanting.plants.end(),
                [&](const auto& p){return p.sourceOrdinal==ordinal;});
            if(selected==landscapePlanting.plants.end()){
                for(unsigned slot=0;slot<4;++slot){
                    const RenderObjectIdentity3D identity{{LandscapeIdentityNamespace,ordinal*4+slot+1},landscapeCrownGrowth?4u:1u};
                    if(slot==1&&landscapeTwigDistanceLod){
                        const std::array<MeshLodLevel3D,2> levels{{
                            {BaseLandscapeTwig(),firMaterials[slot],landscapeCrownRange.switchDistance,landscapeCrownRange.transitionWidth},
                            {BaseLandscapeFir(slot),firMaterials[slot]}}};
                        if(!scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,landscapeTrees[ordinal],{},identity))
                            throw std::runtime_error("Landscape twig LOD scene insertion failed");
                    }else if(!scene.Mesh(BaseLandscapeFir(slot),landscapeTrees[ordinal],firMaterials[slot],{},identity))
                        throw std::runtime_error("Landscape fir scene insertion failed");
                    ++landscapeFrame.acceptedFir;
                }
            }else{
                const auto index=static_cast<std::size_t>(selected-landscapePlanting.plants.begin());
                for(unsigned slot=0;slot<3;++slot){
                    const auto& mesh=slot==2?landscapeRootMeshes[index]:broadleafMeshes[slot];
                    const RenderObjectIdentity3D identity{{LandscapeIdentityNamespace,ordinal*4+slot+1},mixedLandscapeCanopy?3u:2u};
                    if(!scene.Mesh(mesh,selected->pose,broadleafSurface[slot],{},identity))
                        throw std::runtime_error("Landscape broadleaf scene insertion failed");
                    ++landscapeFrame.acceptedBroadleaf;
                }
            }
        }
        if(landscapeFrame.acceptedFir!=((authoredLandscapePlanting||mixedLandscapeCanopy)?2016u:2080u-shoulderSelectedBase*4)||
           landscapeFrame.acceptedBroadleaf!=((authoredLandscapePlanting||mixedLandscapeCanopy)?48u:0u))
            throw std::runtime_error("Landscape population submission mismatch");
        for(const auto& tree:broadleafTrees)matureStudy.Draw(scene,tree.transform,tree.trunk,matureContactWorld.Original(false));
        // 中層候選共用既有合法闊葉網格與材質；真樹幹亦加入角色及鏡頭碰撞。
        submittedMiddleLayer=0;submittedMiddleLayerTriangles=0;
        for(const auto& tree:middleLayer.trees) {
            for(unsigned slot=0;slot<3;++slot) {
                if(!scene.Mesh(broadleafMeshes[slot],tree.transform,broadleafSurface[slot]))
                    throw std::runtime_error("Middle-layer scene insertion failed");
                submittedMiddleLayerTriangles+=broadleafMeshes[slot].TriangleCount();
            }
            ++submittedMiddleLayer;
        }
        scene.Mesh(riverMesh,{},Mistwatch::RiverMaterial());
        foliageViewRequests.clear();std::uint64_t firViewOrdinal{};
        // 鏡頭到樹冠包絡距離決定針葉細節，六公尺過渡帶保留互補色彩與陰影。
        const auto submitFir=[&](Transform3D transform) {
            const auto leafIdentity=Mistwatch::FoliageView::LeafIdentity(forestPlayableWorld,++firViewOrdinal);
            if(foliageViewEnabled&&started&&session.Alive()){
                auto bounds=firMeshes[1].bounds;
                if(nearFirTwigLod){
                    const auto other=detailedFirTwig.bounds;const auto lo=bounds.center-bounds.extents,hi=bounds.center+bounds.extents;
                    const auto a=other.center-other.extents,b=other.center+other.extents;
                    const Vec3 low{(std::min)(lo.x,a.x),(std::min)(lo.y,a.y),(std::min)(lo.z,a.z)};
                    const Vec3 high{(std::max)(hi.x,b.x),(std::max)(hi.y,b.y),(std::max)(hi.z,b.z)};
                    bounds.center=(low+high)*.5f;bounds.extents=(high-low)*.5f;bounds.radius=Length(bounds.extents);
                }
                const float opacity=Mistwatch::FoliageView::Opacity(playerCameraMotion.Camera(),session.feet,
                    controller.Settings().height,TransformBounds(bounds,transform));
                if(opacity<1.f)foliageViewRequests.push_back({leafIdentity,opacity});
            }
            for(unsigned slot=0;slot<4;++slot) {
                const auto identity=slot==1?leafIdentity:RenderObjectIdentity3D{};
                if(slot==1&&nearFirTwigLod) {
                    const std::array<MeshLodLevel3D,2> levels{{
                        {detailedFirTwig,firMaterials[slot],12.f,6.f},
                        {firMeshes[slot],firMaterials[slot]}}};
                    if(!scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,transform,{},identity))
                        throw std::runtime_error("Fir twig LOD insertion failed");
                } else if(!scene.Mesh(firMeshes[slot],transform,firMaterials[slot],{},identity))
                    throw std::runtime_error("Fir scene insertion failed");
            }
        };
        for(const auto& tree:firLayout.firs)submitFir(tree.render);
        submittedSaplings=0;
        for(const auto& tree:understory.saplings) {
            submitFir(tree);
            ++submittedSaplings;
        }
        Material3D fern{{1,1,1,1},{.92f,0,0,.65f}};
        fern.materialClass=MaterialClass3D::Foliage;
        fern.doubleSided=true;
        submittedFerns=0;submittedFernTriangles=0;
        submittedColonyFerns=0;submittedColonyFernTriangles=0;
        submittedColonyVariants={};
        submittedOriginalFernVariants={};
        for(std::size_t index=0;index<understory.ferns.size();++index) {
            auto plant=understory.ferns[index];
            const float visibility=Mistwatch::UnderstoryVisibility(Length(plant.position-session.feet));
            if(visibility<=0)continue;
            plant.scale=plant.scale*visibility;
            const auto& mesh=diagnosticLegacyFern?fernMesh:scannedFerns[index%4];
            if(!scene.Mesh(mesh,plant,diagnosticLegacyFern?fern:fernSurface))
                throw std::runtime_error("Original fern scene insertion failed");
            submittedFernTriangles+=mesh.TriangleCount();
            ++submittedOriginalFernVariants[index%4];
            ++submittedFerns;
        }
        // 共用既有四個真蕨網格與遮罩材質；新增的是固定世界群落，不是相機附著葉片。
        for(const auto& fernPlant:fernColonies.plants) {
            auto plant=fernPlant.transform;
            const float visibility=Mistwatch::UnderstoryVisibility(Length(plant.position-session.feet));
            if(visibility<=0)continue;
            plant.scale=plant.scale*visibility;
            const auto& mesh=scannedFerns[fernPlant.variant];
            if(!scene.Mesh(mesh,plant,fernSurface))throw std::runtime_error("Fern colony scene insertion failed");
            submittedFernTriangles+=mesh.TriangleCount();
            submittedColonyFernTriangles+=mesh.TriangleCount();
            ++submittedColonyVariants[fernPlant.variant];
            ++submittedFerns;++submittedColonyFerns;
        }
        const float grassTintScale=diagnosticDimGrass?.7f:1.f;
        Material3D grass{{.32f*grassTintScale,.43f*grassTintScale,.18f*grassTintScale,1},{.9f,0,0,.9f}};
        grass.materialClass=MaterialClass3D::Foliage;
        grass.doubleSided=true;
        submittedGrass=0;
        submittedGrassTriangles=0;
        submittedGrassVariants={};
        submittedNettles=0;submittedNettleTriangles=0;
        grassFixtureState={};
        const auto grassSelectionFeet=captureGrassDistance?grassFixtureSelectionFeet:session.feet;
        for(std::size_t index=0;index<groundCover.size();++index) {
            // 高灌叢接替的軟地被不重疊提交；作者資料與其他位置保持原樣。
            if(entranceUnderstoryEnabled&&std::binary_search(entranceUnderstory.suppressedGrassIds.begin(),
                entranceUnderstory.suppressedGrassIds.end(),unsigned(index)))continue;
            const auto plant=groundCover[index];
            const float scale=.5f+.25f*std::sin(plant.x*1.7f+plant.z);
            const bool selected=Mistwatch::GrassRootSelected(plant,grassSelectionFeet);
            const bool fixtureTarget=captureGrassDistance&&index==grassFixtureIndex;
            if(fixtureTarget){grassFixtureState.root=plant;grassFixtureState.feet=grassSelectionFeet;
                grassFixtureState.scale=scale;grassFixtureState.yaw=plant.x+plant.z;
                grassFixtureState.playerDistance=Length(plant-grassSelectionFeet);grassFixtureState.selected=selected;}
            if(!selected||(fixtureTarget&&!grassFixtureEnabled))continue;
            const unsigned variant=forestCover.plants.empty()?unsigned(index%5):forestCover.plants[index].variant;
            const auto nettleVariant=diagnosticNettleMix?nettleMix.renderVariants[index]:Mistwatch::NettleVariantCount;
            const bool nettle=nettleVariant<Mistwatch::NettleVariantCount;
            const auto& mesh=nettle?nettleMeshes[nettleVariant]:diagnosticProceduralGrass?grassMesh:diagnosticCurvedScannedGrass?curvedScannedGrass[variant]:scannedGrass[variant];
            const RenderObjectIdentity3D identity=fixtureTarget?RenderObjectIdentity3D{{5572150615523553875ull,index+1},1}:RenderObjectIdentity3D{};
            const auto accepted=scene.Mesh(mesh,{plant,{0,plant.x+plant.z,0},{scale,scale,scale}},nettle?nettleSurface:diagnosticProceduralGrass?grass:grassSurface,{},identity);
            if(!accepted)throw std::runtime_error("Grass scene insertion failed");
            if(fixtureTarget)grassFixtureState.sceneAccepted=true;
            submittedGrassTriangles+=mesh.TriangleCount();
            if(nettle){++submittedNettles;submittedNettleTriangles+=mesh.TriangleCount();}
            ++submittedGrassVariants[variant];
            ++submittedGrass;
        }
        submittedRoadside=0;submittedRoadsideTriangles=0;
        for(unsigned index=0;index<roadsideCover.plants.size();++index){
            const auto& plant=roadsideCover.plants[index];
            if(!Mistwatch::GrassRootSelected(plant.transform.position,session.feet))continue;
            const auto& mesh=scannedGrass[plant.variant];
            if(!scene.Mesh(mesh,plant.transform,grassSurface,{},{{0x4d57524f41444752ull,index+1},1}))
                throw std::runtime_error("Roadside scanned grass scene insertion failed");
            ++submittedRoadside;submittedRoadsideTriangles+=mesh.TriangleCount();
        }
        submittedTrailLitter=0;
        submittedRoadsideBroadleaf=0;submittedRoadsideBroadleafTriangles=0;
        for(unsigned index=0;index<roadsideBroadleaf.plants.size();++index){
            if(entranceUnderstoryEnabled&&std::binary_search(entranceUnderstory.suppressedBroadleafIds.begin(),
                entranceUnderstory.suppressedBroadleafIds.end(),index))continue;
            const auto& plant=roadsideBroadleaf.plants[index];
            if(!Mistwatch::GrassRootSelected(plant.transform.position,session.feet))continue;
            const auto& mesh=roadsideBroadleafMeshes[plant.variant];
            if(!scene.Mesh(mesh,plant.transform,roadsideBroadleafSurface,{},{{0x4d5752424c454146ull,index+1},1}))
                throw std::runtime_error("Roadside broadleaf scene insertion failed");
            ++submittedRoadsideBroadleaf;submittedRoadsideBroadleafTriangles+=mesh.TriangleCount();
        }
        if(trailLitterEnabled){
            for(unsigned index=0;index<2;++index){
                auto material=index==0?barkMaterial:rockMaterial;
                material.textureScale=1.f;
                if(!scene.Mesh(trailLitterMeshes[index],{},material,{},{{0x4d57545241494c4cull,index+1},1}))
                    throw std::runtime_error("Trail litter scene insertion failed");
                ++submittedTrailLitter;
            }
        }
        submittedFallenLogs=0;submittedFallenTriangles=0;
        if(rockSegmentEnabled)rockSegment.Submit(scene);
        if(valleyOutcropPreview)valleyOutcrop.Submit(scene);
        if(valleySlopeGrove)slopeGrove.Submit(scene);
        for(std::size_t index=0;index<forestProps.size();++index){
            const auto& prop=forestProps[index];
            if(prop.FallenLog()&&fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1){
                if(!scene.Mesh(fallenLogs.mesh,prop.transform,fallenLogs.material,{},{{0x4d5746414c4c454eull,(entranceWorld.applied?entranceWorld.propSourceIds[index]:index)+1},1}))
                    throw std::runtime_error("Scanned fallen-log scene insertion failed");
                ++submittedFallenLogs;submittedFallenTriangles+=fallenLogs.mesh.TriangleCount();
            }else if(!scene.Mesh(propMeshes[prop.FallenLog()?1:0],prop.transform,prop.FallenLog()?barkMaterial:boulderMaterial))
                throw std::runtime_error("Retained forest-prop scene insertion failed");
        }
        submittedSupplies=0;
        auto supplyWood=woodMaterial;
        supplyWood.useUv=true;
        Material3D supplyMetal{{1,1,1,1},{.65f,.7f,0,1}};
        for(unsigned i=0;i<3;++i) if(!(session.collected&(1u<<i))) {
            const Transform3D transform{Mistwatch::Session::supplies[i],{},{1,1,1}};
            if(!scene.Mesh(supplyMeshes[0],transform,supplyWood)||!scene.Mesh(supplyMeshes[1],transform,supplyMetal))
                throw std::runtime_error("Supply crate scene insertion failed");
            ++submittedSupplies;
        }
        submittedBridgeWorksite=0;
        if(session.bridgeRepaired) {
            if(bearingBridge)Mistwatch::BearingBridge::Submit(scene,*bearingBridge,woodMaterial,boulderMaterial);
            else scene.MaterialBox({0,-.1f,36},{4,.2f,8},woodMaterial);
        }
        else {
            const Transform3D pose{Mistwatch::Session::bridge+Vec3{1,0,0},{},{1,1,1}};
            auto bridgeWood=woodMaterial;bridgeWood.useUv=true;
            for(unsigned index=0;index<2;++index) {
                const auto material=index?supplyMetal:bridgeWood;
                if(!scene.Mesh(bridgeWorksiteMeshes[index],pose,material,{},
                    {{0x4d57425249444745ull,index+1},1}))
                    throw std::runtime_error("Bridge worksite scene insertion failed");
                ++submittedBridgeWorksite;
            }
        }
        submittedPlankedShelter=0;
        submittedShelterDetail=0;
        if(shelterDetailEnabled){
            // 程序金屬使用真波紋幾何，原木屋頂只保留身份，不重疊繪製。
            const Material3D roof{{.18f,.21f,.23f,1},{.72f,.65f,0,1}};
            for(unsigned i=0;i<2;++i){
                if(!scene.Mesh(shelterDetailMeshes[i],{},i?roof:hutMaterial,{},{{0x4d575348454c5444ull,i+1},1}))
                    throw std::runtime_error("Shelter detail scene insertion failed");
                ++submittedShelterDetail;
            }
        }
        else if(plankedShelter){if(!scene.Mesh(plankedShelterMesh,{},hutMaterial))throw std::runtime_error("Planked shelter actual submission failed");++submittedPlankedShelter;}
        else for(const auto solid:Mistwatch::RepairShelterSolids())scene.MaterialBox(solid.center,solid.extents*2.f,woodMaterial);
        repairToolsSubmitted=0;
        repairSignSubmitted=repairSignVisibleSites=0;
        for(unsigned site=0;site<3;++site) {
            const auto point=Mistwatch::Session::repairSites[site];
            if(site==0){
                if(repairToolsDetailed){
                    Material3D metal{{1,1,1,1},{.65f,.7f,0,1}},handle{{1,1,1,1},{.72f,0,0,1}};
                    if(session.repairMaterials&1){metal.tint=handle.tint={.3f,.3f,.25f,1};}
                    const Transform3D pose{point+Mistwatch::RepairToolsOffset,{},{1,1,1}};
                    if(!scene.Mesh(repairToolsMeshes[0],pose,metal)||!scene.Mesh(repairToolsMeshes[1],pose,handle))
                        throw std::runtime_error("Repair tools actual scene submission failed");
                    repairToolsSubmitted=2;
                }else scene.Box(point+Vec3{0,.49f,0},{.5f,.06f,.3f},(session.repairMaterials&1)?Color{.3f,.3f,.25f,1}:Color{.82f,.65f,.15f,1});
            }
            if(session.repairMaterials&(1u<<site))continue;
            if(site!=0)for(unsigned beam=0;beam<3;++beam)
                scene.Mesh(propMeshes[1],{point+Vec3{0,.18f+.22f*float(beam),0},{0,0,1.5707963f},{.22f,1.8f,.22f}},woodMaterial);
            // 小型路標讓材料位置可辨識，不用懸浮的大型目標方塊遮住場景。
            const auto sign=point+Vec3{site==0?2.8f:1.3f,0,0};
            const Transform3D signPose{sign,{},{1,1,1}};
            for(unsigned group=0;group<repairSignMeshes.size();++group) {
                if(!scene.Mesh(repairSignMeshes[group],signPose,repairSignMaterials[group]))
                    throw std::runtime_error("Repair sign actual scene submission failed");
                ++repairSignSubmitted;
            }
            ++repairSignVisibleSites;
        }
        unsigned expectedSignSites{};
        for(unsigned site=0;site<3;++site)if(!(session.repairMaterials&(1u<<site)))++expectedSignSites;
        if(repairSignVisibleSites!=expectedSignSites||repairSignSubmitted!=3u*expectedSignSites)
            throw std::runtime_error("Repair sign visible mask submission mismatch");
        if(session.CurrentStage()==Mistwatch::Stage::Encounter && encounter.phase==Mistwatch::EnemyPhase::Windup) {
            const float progress=std::clamp(1.f-encounter.phaseTime/Mistwatch::Encounter::WindupDuration,0.f,1.f);
            for(unsigned segment=0;segment<48;++segment) {
                const auto point=[&](unsigned index,float radius){
                    const float angle=float(index)*6.2831853f/48.f;
                    Vec3 p=encounter.strikeTarget+Vec3{std::cos(angle)*radius,0,std::sin(angle)*radius};
                    p.y=terrain.SampleHeight(p.x,p.z)+.06f;return p;
                };
                scene.Line(point(segment,Mistwatch::Encounter::StrikeRadius),point(segment+1,Mistwatch::Encounter::StrikeRadius),4.f,{1,.25f,.04f,1});
                scene.Line(point(segment,Mistwatch::Encounter::StrikeRadius*progress),point(segment+1,Mistwatch::Encounter::StrikeRadius*progress),2.f,{1,.75f,.12f,1});
            }
        }
        if(!Mistwatch::SubmitBeaconDetail(scene,session))
            throw std::runtime_error("Beacon detail scene insertion failed");
        submittedWorkshop=0;
        if(plankedWorkshop){
            if(!scene.Mesh(workshopMesh,{},hutMaterial))throw std::runtime_error("Workshop actual submission failed");
            ++submittedWorkshop;
        }else for(const auto solid:Mistwatch::WorkshopSolids())scene.MaterialBox(solid.center,solid.extents*2.f,woodMaterial);
        const auto cabinet=Mistwatch::FuseCaseSolid();
        scene.Box(cabinet.center,cabinet.extents*2.f,{.18f,.25f,.26f,1});
        const auto fuseDialVisuals=Mistwatch::CreateFuseDialVisuals(session,cabinet.center);
        if(!fuseDialVisuals)throw std::runtime_error("Fuse dial visual state invalid");
        for(const auto& dial:*fuseDialVisuals) {
            scene.Box(dial.centre,dial.panelSize,dial.panelColor);
            for(const auto& triangle:dial.triangles)
                scene.Triangle(triangle.a,triangle.b,triangle.c,triangle.color);
        }
        if(!scene.Mesh(rangerHutMesh,{Mistwatch::Session::rangerLog,{},{1,1,1}},hutMaterial))
            throw std::runtime_error("Ranger hut scene insertion failed");
        const auto desk=Mistwatch::RangerHutSolids()[2];
        submittedRangerDesk=0;
        if(!scene.Mesh(rangerDeskMesh,{desk.center,{},{1,1,1}},woodMaterial))
            throw std::runtime_error("Ranger desk scene insertion failed");
        ++submittedRangerDesk;
        scene.Box(Mistwatch::Session::rangerLog+Vec3{.9f,1.02f,0},{.55f,.03f,.65f},{.85f,.82f,.65f,1});
        scene.MaterialBox({2,.8f,79},{.12f,1.6f,.12f},woodMaterial);
        scene.Box({2,1.5f,79},{1,.35f,.12f},{.65f,.62f,.25f,1});
        for(std::size_t group=0;group<generatorDetailMeshes.size();++group)
            if(!scene.Mesh(generatorDetailMeshes[group],{Mistwatch::Session::generator,{}, {1,1,1}},generatorDetailMaterials[group]))
                throw std::runtime_error("Generator detail scene insertion failed");
        submittedGeneratorLensTriangles=0;
        for(unsigned lamp=0;lamp<3;++lamp)
            for(const auto& triangle:Mistwatch::GeneratorIndicatorLens(lamp)) {
                scene.Triangle(Mistwatch::Session::generator+triangle[0],Mistwatch::Session::generator+triangle[1],
                    Mistwatch::Session::generator+triangle[2],Mistwatch::GeneratorIndicatorColour(session.circuit,lamp));
                ++submittedGeneratorLensTriangles;
            }
        Material3D antenna{{1,1,1,1},{.45f,.65f,0,1},false};antenna.doubleSided=true;
        for(unsigned index=0;index<3;++index) {
            const auto p=Mistwatch::Session::relaySites[index];
            const auto relaySolids=Mistwatch::RelaySolids(index);
            scene.MaterialBox(relaySolids[0].center,relaySolids[0].extents*2.f,rockMaterial);
            scene.Box(relaySolids[1].center,relaySolids[1].extents*2.f,{.24f,.28f,.26f,1});
            const float angle=float(session.RelaySector(index))*6.2831853f/16.f;
            const Vec3 direction{std::sin(angle),0,std::cos(angle)};
            scene.Mesh(relayDish,{p+Vec3{0,2.15f,0}+direction*.5f,{0,angle,0},{1,1,1}},antenna);
            scene.Mesh(box,{p+Vec3{0,2.15f,0}+direction*.15f,{0,angle,0},{.065f,.065f,.3f}},antenna);
            const bool linked=(session.relayLinked&(1u<<index))!=0;
            const bool aligned=session.RelaySector(index)==Mistwatch::Session::RelayTargetSector(index);
            const Color lamp=linked?Color{.1f,1,.4f,1}:session.powerRestored&&aligned?Color{.4f,1,.2f,1}:Color{1,.55f,.08f,1};
            scene.Box(p+Vec3{index==1?-.69f:.69f,.7f,0},{.08f,.18f,.25f},lamp);
            scene.Line(p+Vec3{0,.74f,0},p+Vec3{direction.x,.74f,direction.z},3.f,lamp);
        }
        if(!playerCameraMotion.Initialized())throw std::runtime_error("Player camera not initialized");
        auto camera=playerCameraMotion.Camera();
        if(!ThirdPersonCameraPathClear3D(camera.position,camera.position,solids,terrain,
            RuntimeCameraOnly(),.155f))
            throw std::runtime_error("Changed world invalidated rendered player camera");
        if(captureRangerHutOverview&&!capture.empty()) {
            camera.target=Mistwatch::Session::rangerLog+Vec3{0,1.6f,0};
            camera.position=camera.target+Vec3{-7,3.2f,-7};
        }
        if(diagnosticCrown && !capture.empty()) camera=MakeThirdPersonCamera({-7,6,-5},0,.1f,8);
        // 遠景只用於地形影像驗收，不改變玩家可操作鏡頭的距離上限。
        if(captureRiverOverview && !capture.empty()) {camera.position={-22,28,8};camera.target={0,-2,36};}
        if(captureShelterOverview && !capture.empty()) {camera.position={-3,1.1f,17};camera.target={-7.25f,.7f,18};}
        if(captureWorkshopOverview&&!capture.empty()){
            camera.target=Mistwatch::Session::fuse+Vec3{0,1.4f,0};
            camera.position=camera.target+(captureWorkshopGroundWide?Vec3{11,5,-10}:Vec3{7,3,-7});
        }
        if(captureBoarOverview && !capture.empty()) {camera.position=encounter.feet+Vec3{3,1.4f,1};camera.target=encounter.feet+Vec3{0,.55f,0};}
        if(captureBoulderOverview && !capture.empty()) {
            const auto rock=std::find_if(forestProps.begin(),forestProps.end(),[](const auto& p){return !p.FallenLog();});
            if(rock==forestProps.end())throw std::runtime_error("Boulder detail fixture missing");
            camera.target=rock->collider.center;
            camera.position=camera.target+Vec3{2.f,1.1f,-2.f};
        }
        if(captureGrassOverview && !capture.empty()) {
            camera.target=Mistwatch::GrassCaptureReference::Target();
            camera.position=Mistwatch::GrassCaptureReference::Position();
        }
        if(captureSupplyOverview&&!capture.empty()) {
            camera.target=Mistwatch::Session::supplies[0]+Vec3{0,.4f,0};
            camera.position=camera.target+Vec3{1.25f,.8f,-1.6f};
        }
        if(captureGrassDistance&&!capture.empty()) {
            camera.target=groundCover[grassFixtureIndex]+Vec3{0,.3f,0};
            // 固定方向與目標，僅改鏡頭距離；不把合成腳位置當可行走地形驗收。
            camera.position=camera.target+Normalize(Vec3{1.15f,.55f,-1.15f})*grassFixtureCameraDistance;
            grassFixtureState.cameraDistance=Length(camera.position-camera.target);
        }
        if(captureFernOverview && !capture.empty()) {
            const auto plant=std::min_element(understory.ferns.begin(),understory.ferns.end(),[&](const auto& a,const auto& b){
                return Length(a.position-session.feet)<Length(b.position-session.feet);
            });
            if(plant==understory.ferns.end())throw std::runtime_error("Fern detail fixture missing");
            camera.target=plant->position+Vec3{0,.2f,0};
            camera.position=camera.target+Vec3{.95f,.55f,-1.2f};
        }
        lastRenderedCamera=camera;
        if(captureTrailOverview&&!capture.empty()){
            if(trailLitter.patches.empty())throw std::runtime_error("Trail litter profile missing");
            const auto p=trailLitter.patches.front().centre;
            camera.target=p+Vec3{0,.04f,0};camera.position=p+Vec3{.9f,.6f,-1.2f};
            lastRenderedCamera=camera;
        }
        if(captureShrubOverview&&!capture.empty()){
            const auto p=shrubCanopy.plants.front().pose.position;
            camera.target=p+Vec3{0,.65f,0};camera.position=camera.target+Vec3{1.35f,.5f,-1.9f};lastRenderedCamera=camera;
        }
        if(captureCanopyOverview&&!capture.empty()){
            camera.target=canopyPose.position+Vec3{0,canopyViewTargetHeight,0};
            camera.position=canopyPose.position+Vec3{std::sin(canopyViewYaw)*canopyViewDistance,canopyViewHeight,-std::cos(canopyViewYaw)*canopyViewDistance};
            lastRenderedCamera=camera;
        }
        if(captureFallenView>=0&&!capture.empty()){
            // 兩側共用封存掃描配置鏡頭，不依各自網格自動取景。
            const auto& solid=Mistwatch::FallenLogPlacementForId(static_cast<unsigned>(captureFallenView)).collider;
            camera.target=solid.center;
            const float distance=(std::max)(3.f,2.f*Length(solid.extents));
            camera.position=camera.target+Normalize(Vec3{1.f,.55f,-1.f})*distance;
            lastRenderedCamera=camera;
        }
        if(captureLandscapeDetail>=0&&!capture.empty()){
            // 配對鏡位固定於原 ordinal，不因兩側樹種或冠幅不同而自動改變構圖。
            const auto root=landscapeTrees.at(177).position;
            camera.target=root+Vec3{0,captureLandscapeDetail==0?4.f:.12f,0};
            camera.position=root+(captureLandscapeDetail==0?Vec3{8.f,4.4f,-12.f}:Vec3{1.8f,.85f,-2.4f});
            if(captureLandscapeTwigDistance>=0.f){
                // 配對鏡位以固定來源兩端的聯集包絡為基準，不以診斷選中的細節調整距離。
                const auto nearSource=firMeshes[1];
                const auto bounds=Mistwatch::LandscapeTwigDraw::UnionBounds(nearSource,landscapeFirMeshes[1]);
                const auto worldBounds=TransformSphereBounds(bounds,landscapeTrees.at(177));
                camera.target=worldBounds.center;
                camera.position=worldBounds.center+Normalize(Vec3{8.f,.4f,-12.f})*(worldBounds.radius+captureLandscapeTwigDistance);
            }
            lastRenderedCamera=camera;
        }
        if(captureDeciduousRootView&&!capture.empty()){
            const auto root=broadleafTrees.at(5).transform.position;
            camera.target=root+Vec3{0,.15f,0};camera.position=root+Vec3{4.f,3.f,-4.f};
            lastRenderedCamera=camera;
        }
        // 固定成對的世界診斷鏡位，不冒稱玩家自由鏡頭或改動操作相機限制。
        if(captureShoulderView>=0&&!capture.empty()){
            const float side=captureShoulderView==0?-1.f:1.f;
            camera.target={side*66.f,10.f,60.f};camera.position={side*41.f,15.f,36.f};
            if(captureShoulderEyeLevel){
                // 核心地表上方1.65m的固定診斷眼高，保留原FOV；不是玩家路線可達性的證明。
                // 不因候選植被的位置／高度重新取景，前後畫面使用同一水平視向。
                const auto& comparisonTerrain=cliffCanonicalTerrain?*cliffCanonicalTerrain:terrain;
                camera.position={side*47.f,comparisonTerrain.SampleHeight(side*47.f,56.f)+1.65f,56.f};
                camera.target=camera.position+Vec3{side*22.f,0.f,4.f};
            }
            lastRenderedCamera=camera;
        }
        auto& draw=e.Draw();
        draw.SetEnvironmentTime(waterTime);
        draw.SetWaterSurface(Mistwatch::RiverSurface());
        draw.SetWaterOptics({true,.018f,.35f,.035f});
        draw.SetScreenSpaceReflections({true,.7f,32.f,.65f,.28f,12});
        if(diagnosticNoAo) draw.SetAmbientOcclusion({false});
        if(diagnosticNoShadows)draw.SetDirectionalShadowMapEnabled(false);
        if(diagnosticNoBloom)draw.SetBloom({false});
        draw.SetDirectionalLight(Mistwatch::ForestLighting::Light(forestLightingContrast));
        draw.SetExposureCompensation(.5f);
        draw.SetAtmosphere({{.42f,.53f,.58f,.65f},camera.position,45,200,true,.0025f,.09f,-2.f,.76f});
        draw.SetTerrainDistanceMaterial(groundDistanceSettings);
        draw.SetTerrainParallax(Mistwatch::ForestSurfaceProfile::HeightSettings(forestSurfaceSelection));
        // 相機外物件仍投影；關閉陰影時不復活舊式投影 Overlay。
        Scene3DSubmissionOptions submission{};
        submission.shadows.enabled=shadowRetentionEnabled&&draw.DirectionalShadowMapEnabled();
        submission.shadows.maximumDistance=55.f;
        submission.maximumLodTransitionInstances=512;
        submission.maximumShadowOnlyInstances=4096;
        // 最後才加肩坡 chain；替換會改變後续 LOD chain 索引，保留身分但不宣稱抖動種子不變。
        standFrame={};
        if(landscapeStandFill)for(unsigned index=0;index<Mistwatch::LandscapeStandFill::Target;++index){
            if(shoulderGroveEnabled&&Mistwatch::ShoulderGrove::IsSelected(shoulderGrove,Mistwatch::LandscapeStandFill::BaseCount+index))continue;
            const auto& pose=landscapeStandPlan.combined.at(Mistwatch::LandscapeStandFill::BaseCount+index);
            for(unsigned slot=0;slot<4;++slot){const RenderObjectIdentity3D identity{{StandIdentityNamespace,index*4+slot+1},1};
                if(slot==1){const std::array<MeshLodLevel3D,2> levels{{
                    {landscapeDetailedFirTwig,firMaterials[slot],landscapeCrownRange.switchDistance,landscapeCrownRange.transitionWidth},
                    {landscapeFirMeshes[slot],firMaterials[slot]}}};
                    if(!scene.MeshLodChain(levels,MeshLodDistanceMetric3D::BoundsSurface,pose,{},identity))
                        throw std::runtime_error("Stand crown insertion failed");
                }else if(!scene.Mesh(landscapeFirMeshes[slot],pose,firMaterials[slot],{},identity))
                    throw std::runtime_error("Stand mesh insertion failed");
                ++standFrame.accepted;
            }
        }
        if(standFrame.accepted!=(landscapeStandFill?384u-shoulderSelectedStand*4:0u))throw std::runtime_error("Stand accepted count mismatch");
        std::array<Mistwatch::ShoulderDraw::Expected,Mistwatch::ShoulderDraw::SlotCount> shoulderExpected;
        shoulderFrame={};
        unsigned shoulderAccepted{};
        if(shoulderGroveEnabled)for(std::size_t index=0;index<shoulderGrove.plants.size();++index){
            for(unsigned slot=0;slot<3;++slot){
                const auto& mesh=slot==2?shoulderRootMeshes[index]:broadleafMeshes[slot];
                const RenderObjectIdentity3D identity{{Mistwatch::ShoulderGrove::InstanceNamespace,index*3+slot+1},1};
                if(!scene.Mesh(mesh,shoulderGrove.plants[index].pose,broadleafSurface[slot],{},identity))
                    throw std::runtime_error("Shoulder grove insertion failed");
                shoulderExpected[index*3+slot]={mesh,shoulderGrove.plants[index].pose,broadleafSurface[slot]};
                ++shoulderAccepted;
            }
        }
        if(shoulderAccepted!=(shoulderGroveEnabled?48u:0u)||
            (shoulderGroveEnabled&&(landscapeFrame.acceptedFir+standFrame.accepted!=2400u||shoulderSelectedBase+shoulderSelectedStand!=16u)))
            throw std::runtime_error("Shoulder grove population submission mismatch");
        std::array<Mistwatch::ShoulderGroundcover::Expected,Mistwatch::ShoulderGroundcover::SlotCount> groundcoverExpected;
        groundcoverFrame={};
        if(shoulderGroundcoverEnabled)for(std::size_t i=0;i<shoulderGroundcoverMeshes.size();++i){
            const auto& p=Mistwatch::ShoulderGroundcover::Recipes[i];const auto& mesh=shoulderGroundcoverMeshes[i];
            const auto& material=GroundcoverMaterial(p.slot);
            if(!scene.Mesh(mesh,p.pose,material,{},{{Mistwatch::ShoulderGroundcover::InstanceNamespace,i+1},1}))
                throw std::runtime_error("Groundcover scene insertion failed");
            groundcoverExpected[i]={mesh,p.pose,material};++groundcoverFrame.accepted;
        }
        const bool groundcoverTreePopulation=mixedLandscapeCanopy?
            landscapePlanting.complete&&landscapePlanting.plants.size()==16&&landscapeFrame.acceptedFir==2016u&&
                landscapeFrame.acceptedBroadleaf==48u&&standFrame.accepted==384u:
            landscapeFrame.acceptedFir+standFrame.accepted==2464u;
        if(groundcoverFrame.accepted!=(shoulderGroundcoverEnabled?78u:0u)||
           (shoulderGroundcoverEnabled&&!groundcoverTreePopulation))
            throw std::runtime_error("Groundcover retained tree submission mismatch");
        const auto guide=Mistwatch::BuildObjectiveGuide(session,encounter,evasion,camera,
            controller.Settings().height<1.5f,controller.Grounded(),RuntimeGuideSource(solids),terrain,trackedRelay);
        const auto counterCue=Mistwatch::CounterWindowCue::Build(session,guide,encounter,terrain,started,paused);
        const auto counterCueReceipt=Mistwatch::CounterWindowCue::Submit(scene,counterCue,terrain);
        counterCueSubmitted=counterCueReceipt.submitted;
        if(counterCueSubmitted>Mistwatch::CounterWindowCue::MaximumLines)
            throw std::runtime_error("Counter window cue line budget exceeded");
        const auto sceneStats=scene.Submit(draw,camera,{1280,720},submission);
        if(foliageViewWindowEnabled&&started&&session.Alive()){
            const float height=controller.Settings().height;
            draw.SetScreenOcclusionWindow(ProjectScreenOcclusionWindow3D(draw.ViewProjection(),
                session.feet+Vec3{0,height*.5f,0},{.50f,height*.5f+.10f,.50f}));
        }
        std::uint64_t fadedColors{},preservedShadows{};float minimumOpacity=1.f;
        for(const auto& request:foliageViewRequests){
            if(foliageViewWindowEnabled&&!draw.ScreenOcclusionWindow().enabled)continue;
            const auto result=ApplyStaticColorOcclusion3D(draw.meshInstances,request.identity,request.opacity);
            if(result.status==StaticColorOcclusionStatus3D::NotVisible)continue;
            if(result.status!=StaticColorOcclusionStatus3D::Applied)throw std::runtime_error("Foliage color-only occlusion update rejected");
            fadedColors+=result.matchedColor;preservedShadows+=result.shadowCopies;
            minimumOpacity=(std::min)(minimumOpacity,request.opacity);
        }
        ++foliageViewFrames;foliageViewFadedFrames+=fadedColors?1:0;
        foliageViewColorInstances+=fadedColors;foliageViewShadowCopies+=preservedShadows;
        foliageViewMinimumOpacity=(std::min)(foliageViewMinimumOpacity,minimumOpacity);
        if(inputReplay.GroveRoute()){
            const std::string phase(inputReplay.Phase());
            if(foliageViewPhases.empty()||foliageViewPhases.back().name!=phase){
                if(foliageViewPhases.size()>=128)throw std::runtime_error("Foliage view phase evidence bound");
                foliageViewPhases.push_back({phase});
            }
            auto& proof=foliageViewPhases.back();++proof.frames;proof.colors+=fadedColors;proof.shadows+=preservedShadows;
            proof.minimum=(std::min)(proof.minimum,minimumOpacity);
        }
        firLodSubmission=sceneStats;
        landscapeFrame.sceneSerial=sceneStats.submissionSerial;landscapeFrame.camera=camera;
        landscapeFrame.cameraHash=14695981039346656037ull;
        for(const auto byte:std::as_bytes(std::span{&draw.ViewProjection(),1})){
            landscapeFrame.cameraHash^=std::to_integer<std::uint8_t>(byte);landscapeFrame.cameraHash*=1099511628211ull;
        }
        Mistwatch::ShoulderDraw::Observer shoulderObserver(shoulderGroveEnabled?
            std::span<const Mistwatch::ShoulderDraw::Expected>(shoulderExpected):std::span<const Mistwatch::ShoulderDraw::Expected>{});
        for(const auto& item:draw.meshInstances)shoulderObserver.Observe(item);
        shoulderFrame.accepted=shoulderAccepted;shoulderFrame.draws=shoulderObserver.Finish();
        coreHerbFrame.sceneSerial=landscapeFrame.sceneSerial;coreHerbFrame.cameraHash=landscapeFrame.cameraHash;
        std::array<Mistwatch::CoreHerbDraw::Expected,Mistwatch::CoreHerbClusters::Target> herbExpected{};
        if(coreHerbEnabled)for(unsigned i=0;i<coreHerbs.plants.size();++i)
            herbExpected[i]={coreHerbMeshes[i],coreHerbs.plants[i].pose,roadsideBroadleafSurface};
        Mistwatch::CoreHerbDraw::Observer herbObserver(coreHerbEnabled?
            std::span<const Mistwatch::CoreHerbDraw::Expected>(herbExpected):std::span<const Mistwatch::CoreHerbDraw::Expected>{});
        for(const auto& item:draw.meshInstances){
            herbObserver.Observe(item);
            if(item.identity.id.high==CoreHerbIdentityNamespace)Mistwatch::CoreHerbDraw::ValidateCameraPolicy(
                item,herbExpected[static_cast<unsigned>(item.identity.id.low-1)],camera,submission,draw.DirectionalShadowMapEnabled());
        }
        const auto herbDraws=herbObserver.Finish();
        ObserveRouteGrass(draw,camera,submission);
        coreHerbFrame.draws=herbDraws.draws;coreHerbFrame.opaque=herbDraws.opaque;
        coreHerbFrame.shadowOnly=herbDraws.shadowOnly;coreHerbFrame.triangles=herbDraws.triangles;
        if(coreHerbEnabled)for(unsigned i=0;i<coreHerbs.plants.size();++i){
            coreHerbFrame.visibility[i]=herbDraws.visibility[i];
            if(!herbDraws.visibility[i])coreHerbFrame.cullReasons[i]=Mistwatch::CoreHerbDraw::MissingReason(
                herbExpected[i],camera,submission,draw.DirectionalShadowMapEnabled());
        }
        shoulderFrame.sceneSerial=landscapeFrame.sceneSerial;shoulderFrame.cameraHash=landscapeFrame.cameraHash;
        Mistwatch::ShoulderGroundcover::Observer groundcoverObserver(shoulderGroundcoverEnabled?
            std::span<const Mistwatch::ShoulderGroundcover::Expected>(groundcoverExpected):
            std::span<const Mistwatch::ShoulderGroundcover::Expected>{});
        for(const auto& item:draw.meshInstances)groundcoverObserver.Observe(item);
        groundcoverFrame.draws=groundcoverObserver.Finish();
        if(shoulderGroundcoverEnabled)for(std::size_t i=0;i<groundcoverExpected.size();++i){
            if(!submission.gpuInstancing)throw std::runtime_error("Groundcover candidate requires actual GPU instances");
            if(!groundcoverFrame.draws.visibility[i]){
                groundcoverFrame.cullReasons[i]=Mistwatch::ShoulderGroundcover::CullReason(groundcoverExpected[i],camera,submission);
                if(!groundcoverFrame.cullReasons[i])throw std::runtime_error("Groundcover visible mesh missing after submission");
            }
        }
        groundcoverFrame.sceneSerial=landscapeFrame.sceneSerial;groundcoverFrame.cameraHash=landscapeFrame.cameraHash;
        cliffFrame={};
        if(cliffDiagnostic||valleyMaterialAudit){
            cliffFrame.camera=camera;cliffFrame.cameraHash=landscapeFrame.cameraHash;cliffFrame.sceneSerial=landscapeFrame.sceneSerial;
            RECT viewport{};if(!GetClientRect(e.Window(),&viewport)||viewport.right<=0||viewport.bottom<=0)
                throw std::runtime_error("Cliff viewport unavailable");
            cliffFrame.width=static_cast<unsigned>(viewport.right);cliffFrame.height=static_cast<unsigned>(viewport.bottom);
            const std::array<const MeshResource3D*,2> meshes{&entranceRoadMeshes[0],&landscapeMesh};
            for(const auto& item:draw.meshInstances)for(unsigned i=0;i<2;++i)if(item.mesh==meshes[i]->handle){
                const auto& terrainSurface=TerrainRenderMaterial();
                if(++cliffFrame.draws[i]!=1||!Mistwatch::LandscapeTwigDraw::ExactResourceBindings(item,meshes[i]->handle,terrainSurface.textures)||
                   item.materialClass!=terrainSurface.materialClass||item.normalScale!=terrainSurface.normalScale||item.useUv!=terrainSurface.useUv)
                    throw std::runtime_error("Cliff actual draw binding mismatch");
                const auto identity=Matrix4::Identity();
                for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
                    if(item.transform[row][col]!=identity[row][col])throw std::runtime_error("Cliff actual draw is not identity");
            }
        }
        rockSegmentFrame={};
        cliffMaterialFrame={};
        if(cliffMaterialDiagnostic||valleyMaterialAudit){
            cliffMaterialFrame.cameraHash=landscapeFrame.cameraHash;cliffMaterialFrame.serial=landscapeFrame.sceneSerial;
            const std::array<const MeshResource3D*,4> meshes{&entranceRoadMeshes[0],&landscapeMesh,&entranceRoadMeshes[1],&entranceRoadMeshes[2]};
            const std::array<const Material3D*,4> surfaces{&TerrainRenderMaterial(),&TerrainRenderMaterial(),&entranceRoadSurface,&settlementSurfaceMaterial};
            for(const auto& item:draw.meshInstances)for(unsigned i=0;i<4;++i)if(item.mesh==meshes[i]->handle){
                const auto& m=*surfaces[i];
                if(++cliffMaterialFrame.draws[i]!=1||!Mistwatch::LandscapeTwigDraw::ExactResourceBindings(item,meshes[i]->handle,m.textures)||
                   item.tint!=PackColorRgba8(m.tint)||item.surface!=PackSurface3D(m.surface)||
                   item.materialClass!=m.materialClass||
                   item.normalScale!=m.normalScale||item.textureScale!=m.textureScale||item.useUv!=m.useUv||
                   item.alphaMode!=m.alphaMode||item.alphaCutoff!=Clamp(m.alphaCutoff,0.f,1.f)||item.doubleSided!=m.doubleSided||
                   item.lodCoverage!=1.f||item.lodSeed||item.motionValid||item.occlusionOpacity!=1.f)
                    throw std::runtime_error("Cliff material actual draw mismatch");
                if((item.layer!=MeshRenderLayer3D::Opaque&&item.layer!=MeshRenderLayer3D::ShadowOnly)||
                   (item.castsShadow&&!m.castsShadow)||(item.layer==MeshRenderLayer3D::ShadowOnly&&!item.castsShadow)||
                   (item.castsShadow?item.shadowCascadeMask==0:item.shadowCascadeMask!=0)||
                   (item.shadowCascadeMask&~DirectionalShadowAllCascades3D)!=0||
                   item.shadowCascadeSelection!=ShadowCascadeSelection3D::Automatic)
                    throw std::runtime_error("Cliff material shadow binding mismatch");
                cliffMaterialFrame.textures[i]=item.textures.semanticIdentity;
                const auto identity=Matrix4::Identity();
                for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
                    if(item.transform[r][c]!=identity[r][c]||item.previousTransform[r][c]!=identity[r][c]||item.normalTransform[r][c]!=identity[r][c])
                        throw std::runtime_error("Cliff material actual transform mismatch");
            }
            ++cliffMaterialObservedFrames;
            for(unsigned i=0;i<4;++i)cliffMaterialObservedDraws[i]+=cliffMaterialFrame.draws[i];
        }
        if(rockSegmentDiagnostic){
            if(!submission.gpuInstancing)throw std::runtime_error("Rock segment requires GPU instances");
            rockSegmentFrame=rockSegment.Observe(draw.meshInstances,camera,submission,landscapeFrame.cameraHash,landscapeFrame.sceneSerial);
        }
        if(valleyOutcropPreview)valleyOutcrop.Observe(draw.meshInstances);
        if(valleySlopeGrove)slopeGrove.Observe(draw.meshInstances);
        for(auto& observer:landscapeTwigObservers)observer.reset();
        if(landscapeTwigDistanceLod)for(unsigned ordinal=0;ordinal<landscapeTrees.size();++ordinal){
            if(shoulderGroveEnabled&&Mistwatch::ShoulderGrove::IsSelected(shoulderGrove,ordinal))continue;
            if(std::any_of(landscapePlanting.plants.begin(),landscapePlanting.plants.end(),
                [&](const auto& plant){return plant.sourceOrdinal==ordinal;}))continue;
            const auto worldBounds=TransformSphereBounds(landscapeCrownGrowth?grownTwigUnionBounds:landscapeTwigUnionBounds,landscapeTrees[ordinal]);
            const std::array<MeshLodLevel3D,2> levels{{
                {BaseLandscapeTwig(),firMaterials[1],landscapeCrownRange.switchDistance,landscapeCrownRange.transitionWidth},
                {BaseLandscapeFir(1),firMaterials[1]}}};
            const auto selection=EvaluateMeshLodChain3D(Length(worldBounds.center-camera.position),worldBounds.radius,
                levels,MeshLodDistanceMetric3D::BoundsSurface);
            landscapeTwigObservers[ordinal].emplace(Mistwatch::LandscapeTwigDraw::Expected{
                BaseLandscapeTwig().handle,BaseLandscapeFir(1).handle,BaseLandscapeTwig().TriangleCount(),
                BaseLandscapeFir(1).TriangleCount(),selection,{{LandscapeIdentityNamespace,ordinal*4+2},landscapeCrownGrowth?4u:1u},firMaterials[1]});
            if(ordinal==177){landscapeFrame.targetTwigDistance=(std::max)(0.f,Length(worldBounds.center-camera.position)-worldBounds.radius);
                landscapeFrame.targetTwigCoverage=selection.farCoverage;}
        }
        for(const auto& item:draw.meshInstances)if(item.identity.id.high==LandscapeIdentityNamespace){
            const auto id=item.identity.id.low;
            if(!id||id>landscapeFrame.visibility.size())throw std::runtime_error("Landscape post-cull identity invalid");
            const auto key=static_cast<unsigned>(id-1),ordinal=key/4,slot=key%4;
            if(shoulderGroveEnabled&&Mistwatch::ShoulderGrove::IsSelected(shoulderGrove,ordinal))
                throw std::runtime_error("Shoulder replaced base fir still drawn");
            const auto selected=std::find_if(landscapePlanting.plants.begin(),landscapePlanting.plants.end(),
                [&](const auto& p){return p.sourceOrdinal==ordinal;});
            const bool replacement=selected!=landscapePlanting.plants.end();
            if((replacement&&slot>=3)||item.identity.generation!=(replacement?(mixedLandscapeCanopy?3u:2u):landscapeCrownGrowth?4u:1u))
                throw std::runtime_error("Landscape post-cull duplicate or generation mismatch");
            if(landscapeCrownGrowth){
                if(replacement)throw std::runtime_error("Crown growth unexpected replacement");
                const auto& actualMesh=slot==1&&item.mesh==BaseLandscapeTwig().handle?BaseLandscapeTwig():BaseLandscapeFir(slot);
                RequireCrownGrowthDraw(item,actualMesh,landscapeTrees[ordinal],firMaterials[slot]);
                if(slot!=1&&(item.lodCoverage!=1.f||item.lodSeed!=0))
                    throw std::runtime_error("Crown growth non-twig coverage mismatch");
                ++landscapeFrame.grownDraws;
                ++crownGrowthObservedDraws;
            }
            if(!replacement&&slot==1&&landscapeTwigDistanceLod){
                if(!landscapeTwigObservers[ordinal])throw std::runtime_error("Landscape twig observer missing");
                landscapeTwigObservers[ordinal]->Observe(item);continue;
            }
            if(landscapeFrame.visibility[key])throw std::runtime_error("Landscape post-cull duplicate slot");
            const auto& mesh=replacement?(slot==2?landscapeRootMeshes[static_cast<std::size_t>(selected-landscapePlanting.plants.begin())]:
                broadleafMeshes[slot]):BaseLandscapeFir(slot);
            if(item.mesh!=mesh.handle)throw std::runtime_error("Landscape post-cull resource mismatch");
            if(replacement&&mixedLandscapeCanopy){
                const auto matrix=selected->pose.Matrix(),normal=CreateNormalMatrix(matrix);
                const auto& m=broadleafSurface[slot];
                if(std::memcmp(&item.transform,&matrix,sizeof(Matrix4))||
                    std::memcmp(&item.previousTransform,&matrix,sizeof(Matrix4))||
                    std::memcmp(&item.normalTransform,&normal,sizeof(Matrix4))||
                    !Mistwatch::LandscapeTwigDraw::ExactResourceBindings(item,mesh.handle,m.textures)||
                    item.tint!=PackColorRgba8(m.tint)||item.surface!=PackSurface3D(m.surface)||
                    item.textureScale!=(std::max)(.001f,m.textureScale)||item.normalScale!=Clamp(m.normalScale,-16.f,15.96875f)||
                    item.useUv!=m.useUv||item.alphaMode!=m.alphaMode||item.alphaCutoff!=Clamp(m.alphaCutoff,0.f,1.f)||
                    item.doubleSided!=m.doubleSided||item.materialClass!=m.materialClass||item.lodCoverage!=1.f||
                    item.lodSeed!=0||item.occlusionOpacity!=1.f)
                    throw std::runtime_error("Mixed canopy post-cull actual pose or material mismatch");
            }
            if(!replacement&&(item.textures!=firMaterials[slot].textures||
               item.alphaMode!=firMaterials[slot].alphaMode||item.alphaCutoff!=firMaterials[slot].alphaCutoff||
               item.doubleSided!=firMaterials[slot].doubleSided||item.materialClass!=firMaterials[slot].materialClass))
                throw std::runtime_error("Landscape post-cull material mismatch");
            if(item.layer==MeshRenderLayer3D::Opaque){++landscapeFrame.opaque;landscapeFrame.visibility[key]=1;}
            else if(item.layer==MeshRenderLayer3D::ShadowOnly){++landscapeFrame.shadowOnly;landscapeFrame.visibility[key]=2;}
            else throw std::runtime_error("Landscape post-cull unexpected layer");
            landscapeFrame.triangles+=mesh.TriangleCount();
        }
        if(landscapeTwigDistanceLod)for(unsigned ordinal=0;ordinal<landscapeTrees.size();++ordinal){
            if(!landscapeTwigObservers[ordinal])continue;
            const auto result=landscapeTwigObservers[ordinal]->Finish();
            if(result.culled)continue;
            const unsigned key=ordinal*4+1;
            if(result.layer==MeshRenderLayer3D::Opaque){++landscapeFrame.opaque;landscapeFrame.visibility[key]=1;}
            else if(result.layer==MeshRenderLayer3D::ShadowOnly){++landscapeFrame.shadowOnly;landscapeFrame.visibility[key]=2;}
            else throw std::runtime_error("Landscape twig unexpected layer");
            landscapeFrame.triangles+=result.triangles;landscapeFrame.nearTwigDraws+=result.nearDraws;
            landscapeFrame.farTwigDraws+=result.farDraws;landscapeFrame.transitionTwigSlots+=static_cast<unsigned>(result.transition);
            if(ordinal==177){landscapeFrame.targetNearTwig=result.nearDraws;landscapeFrame.targetFarTwig=result.farDraws;}
        }
        standFrame.sceneSerial=landscapeFrame.sceneSerial;standFrame.cameraHash=landscapeFrame.cameraHash;
        for(auto& observer:standObservers)observer.reset();
        if(landscapeStandFill){
            const std::array<MeshLodLevel3D,2> levels{{
                {landscapeDetailedFirTwig,firMaterials[1],landscapeCrownRange.switchDistance,landscapeCrownRange.transitionWidth},
                {landscapeFirMeshes[1],firMaterials[1]}}};
            for(unsigned index=0;index<Mistwatch::LandscapeStandFill::Target;++index){
                if(shoulderGroveEnabled&&Mistwatch::ShoulderGrove::IsSelected(shoulderGrove,Mistwatch::LandscapeStandFill::BaseCount+index))continue;
                const auto& pose=landscapeStandPlan.combined.at(Mistwatch::LandscapeStandFill::BaseCount+index);
                const auto bounds=TransformSphereBounds(landscapeTwigUnionBounds,pose);
                const auto selection=EvaluateMeshLodChain3D(Length(bounds.center-camera.position),bounds.radius,levels,MeshLodDistanceMetric3D::BoundsSurface);
                standObservers[index].emplace(Mistwatch::LandscapeTwigDraw::Expected{
                    landscapeDetailedFirTwig.handle,landscapeFirMeshes[1].handle,landscapeDetailedFirTwig.TriangleCount(),
                    landscapeFirMeshes[1].TriangleCount(),selection,{{StandIdentityNamespace,index*4+2},1},firMaterials[1]});
            }
        }
        std::array<std::optional<Matrix4>,Mistwatch::LandscapeStandFill::Target> standExpectedTransforms{};
        for(const auto& item:draw.meshInstances)if(item.identity.id.high==StandIdentityNamespace){
            const auto id=item.identity.id.low;
            if(!landscapeStandFill||!id||id>standFrame.visibility.size()||item.identity.generation!=1)
                throw std::runtime_error("Stand post-cull identity mismatch");
            const auto key=static_cast<unsigned>(id-1),index=key/4,slot=key%4;
            if(shoulderGroveEnabled&&Mistwatch::ShoulderGrove::IsSelected(shoulderGrove,Mistwatch::LandscapeStandFill::BaseCount+index))
                throw std::runtime_error("Shoulder replaced stand fir still drawn");
            auto& expected=standExpectedTransforms[index];
            if(!expected)expected.emplace(landscapeStandPlan.combined.at(Mistwatch::LandscapeStandFill::BaseCount+index).Matrix());
            const auto& expectedTransform=*expected;
            if(std::memcmp(&item.transform,&expectedTransform,sizeof(Matrix4)))throw std::runtime_error("Stand post-cull pose mismatch");
            if(slot==1){standObservers[index]->Observe(item);continue;}
            const auto& mesh=landscapeFirMeshes[slot];const auto& m=firMaterials[slot];
            if(standFrame.visibility[key]||!Mistwatch::LandscapeTwigDraw::ExactResourceBindings(item,mesh.handle,m.textures)||
                item.tint!=PackColorRgba8(m.tint)||item.surface!=PackSurface3D(m.surface)||
                item.textureScale!=(std::max)(.001f,m.textureScale)||item.normalScale!=Clamp(m.normalScale,-16.f,15.96875f)||
                item.useUv!=m.useUv||item.alphaMode!=m.alphaMode||item.alphaCutoff!=Clamp(m.alphaCutoff,0.f,1.f)||
                item.doubleSided!=m.doubleSided||item.materialClass!=m.materialClass||item.lodCoverage!=1.f||item.lodSeed!=0||item.occlusionOpacity!=1.f)
                throw std::runtime_error("Stand post-cull resource/material mismatch");
            if(item.layer==MeshRenderLayer3D::Opaque){++standFrame.opaque;standFrame.visibility[key]=1;}
            else if(item.layer==MeshRenderLayer3D::ShadowOnly&&item.castsShadow){++standFrame.shadowOnly;standFrame.visibility[key]=2;}
            else throw std::runtime_error("Stand post-cull layer mismatch");
            ++standFrame.draws;standFrame.triangles+=mesh.TriangleCount();
        }
        if(landscapeStandFill)for(unsigned index=0;index<Mistwatch::LandscapeStandFill::Target;++index){
            if(shoulderGroveEnabled&&Mistwatch::ShoulderGrove::IsSelected(shoulderGrove,Mistwatch::LandscapeStandFill::BaseCount+index))continue;
            const auto r=standObservers[index]->Finish();if(r.culled)continue;const auto key=index*4+1;
            if(r.layer==MeshRenderLayer3D::Opaque){++standFrame.opaque;standFrame.visibility[key]=1;}
            else if(r.layer==MeshRenderLayer3D::ShadowOnly){++standFrame.shadowOnly;standFrame.visibility[key]=2;}
            else throw std::runtime_error("Stand crown layer mismatch");
            standFrame.draws+=r.draws;standFrame.triangles+=r.triangles;
            standFrame.nearTwig+=r.nearDraws;standFrame.farTwig+=r.farDraws;standFrame.pairs+=static_cast<unsigned>(r.transition);
        }
        if(diagnosticNoShadows&&(sceneStats.shadowOnlyInstances||sceneStats.shadowInstances))
            throw std::runtime_error("Disabled shadows unexpectedly submitted");
        // 容量退回是有效降級路徑；驗收明確拒絕，不中止正常遊玩。
        if(!CapturePath().empty()&&nearFirTwigLod&&sceneStats.lodTransitionBudgetFallbacks)
            throw std::runtime_error("Fir twig LOD transition budget fallback");
        if(!inputReplayDirectory.empty()&&!inputReplayTwigReceiptWritten){
            WriteCrownGrowthReceipt(inputReplayDirectory/"first-frame",landscapeFrame);
            WriteCoreHerbReceipt(inputReplayDirectory/"first-frame",coreHerbFrame);
            WriteRouteGrassReceipt(inputReplayDirectory/"first-frame",routeGrassFrame);
            WriteShoulderReceipt(inputReplayDirectory/"first-frame",shoulderFrame);
            WriteGroundcoverReceipt(inputReplayDirectory/"first-frame",groundcoverFrame);
            WriteCliffReceipt(inputReplayDirectory/"first-frame",cliffFrame);
            WriteCliffMaterialReceipt(inputReplayDirectory/"first-frame",cliffMaterialFrame);
            // 收據來自正常玩家真正提交的第一幀，不把初始化開關當成渲染成功。
            std::ofstream out(inputReplayDirectory/"twig-first-frame.txt");
            out<<"capture_mode="<<!capture.empty()<<"\nscene_serial="<<landscapeFrame.sceneSerial
                <<"\naccepted_fir_slots="<<landscapeFrame.acceptedFir
                <<"\nnear_draws="<<landscapeFrame.nearTwigDraws<<"\nfar_draws="<<landscapeFrame.farTwigDraws
                <<"\ntransition_slots="<<landscapeFrame.transitionTwigSlots
                <<"\nbudget_fallbacks="<<sceneStats.lodTransitionBudgetFallbacks
                <<"\nstand_accepted_slots="<<standFrame.accepted
                <<"\nstand_opaque_slots="<<standFrame.opaque<<"\nstand_shadow_only_slots="<<standFrame.shadowOnly
                <<"\nstand_actual_draws="<<standFrame.draws<<"\nstand_triangles="<<standFrame.triangles
                <<"\nstand_near_twig_draws="<<standFrame.nearTwig<<"\nstand_far_twig_draws="<<standFrame.farTwig
                <<"\nstand_transition_pairs="<<standFrame.pairs<<"\nstand_scene_serial="<<standFrame.sceneSerial
                <<"\nstand_camera_hash="<<standFrame.cameraHash<<"\ncamera_hash="<<landscapeFrame.cameraHash
                <<"\nstand_proposals="<<landscapeStandPlan.generated<<std::setprecision(9)
                <<"\nstand_union_center="<<landscapeStandBounds.center.x<<','<<landscapeStandBounds.center.y<<','<<landscapeStandBounds.center.z
                <<"\nstand_union_extents="<<landscapeStandBounds.extents.x<<','<<landscapeStandBounds.extents.y<<','<<landscapeStandBounds.extents.z
                <<"\nstand_union_radius="<<landscapeStandBounds.radius<<'\n';
            for(unsigned b=0;b<Mistwatch::LandscapeStandFill::BandCount;++b){const auto& c=landscapeStandPlan.bands[b];
                out<<"stand_band_"<<b<<"_accepted="<<c.accepted<<"\nstand_band_"<<b<<"_proposals="<<c.proposals
                    <<"\nstand_band_"<<b<<"_slope="<<c.slope<<"\nstand_band_"<<b<<"_core="<<c.core
                    <<"\nstand_band_"<<b<<"_edge="<<c.edge<<"\nstand_band_"<<b<<"_spacing="<<c.spacing<<'\n';
            }
            out.close();if(!out)throw std::runtime_error("Normal twig draw receipt failed");
            WriteWoodlandReceipt(inputReplayDirectory/"world-start");
            inputReplayTwigReceiptWritten=true;
        }
        if(captureGrassDistance) {
            grassFixtureState.sceneSerial=sceneStats.submissionSerial;
            for(const auto& item:draw.meshInstances)if(item.identity==RenderObjectIdentity3D{{5572150615523553875ull,grassFixtureIndex+1},1}){
                if(item.layer==MeshRenderLayer3D::Opaque)++grassFixtureState.opaque;
                else if(item.layer==MeshRenderLayer3D::ShadowOnly)++grassFixtureState.shadowOnly;
                grassFixtureState.triangles+=Mistwatch::GrassTriangleCounts[grassFixtureIndex%5];
            }
            auto hash=1469598103934665603ull;
            const auto& matrix=draw.ViewProjection();
            for(const auto byte:std::as_bytes(std::span{&matrix,1})){hash^=std::to_integer<std::uint8_t>(byte);hash*=1099511628211ull;}
            grassFixtureState.cameraHash=hash;
        }
        if(!session.guardianDefeated) {
            // 暫停時正式冷卻凍結，保持目前受擊姿態；渲染不推進另一份計時。
            const auto hitPlan=Mistwatch::BoarHitReaction::Build(encounter,started&&session.Alive());
            if(hitPlan.status==Mistwatch::BoarHitReaction::Status::Invalid)
                throw std::runtime_error("Boar hit render state invalid");
            const AnimationPose3D* animalPose=&boar.pose;
            if(hitPlan.status==Mistwatch::BoarHitReaction::Status::Active){
                if(!Mistwatch::BoarHitReaction::Apply(boar.model,boar.pose,hitPlan,boarHitScratch))
                    throw std::runtime_error("Boar hit render pose rejected");
                animalPose=&boarHitScratch.pose;
            }
            const auto animalPalette=AddSkinPaletteFromPose(draw,boar.model,*animalPose,boarSkinning);
            if(!draw.SkinnedMeshInstance(boarMesh.handle,animalPalette,CreateTransformMatrix(encounter.feet,{0,boar.yaw,0},{1,1,1})))
                throw std::runtime_error("Boar skin submission failed");
        }
        const auto palette=AddSkinPaletteFromPose(draw,explorer.model,explorer.pose,explorerSkinning);
        const auto explorerTransform=CreateTransformMatrix(session.feet,{0,explorer.yaw,0},{1,1,1});
        if(!Mistwatch::ExplorerSurface::Submit(draw,explorer.model,explorer.art,explorerMesh.handle,palette,
            explorerTransform,explorerMaterials,explorerSurfaceSubmission))
            throw std::runtime_error("Explorer surface window submission failed");
        auto fieldHud=Mistwatch::FieldPresentation::Build(session,guide,encounter,started,paused,fieldJournalOpen);
        Mistwatch::FieldPresentation::PresentCameraSettings(fieldHud,cameraOptions,started,paused);
        Mistwatch::FieldPresentation::Draw(draw,fieldHud,session,Mistwatch::FieldPresentation::LastAction(message),
            actionFeedback,e.Audio().GetStats().deviceAvailable,gameAudio.options.muted,
            unsigned(gameAudio.options.volume*100+.5f),started,paused);
        surveyMapRequested=Mistwatch::SurveyMap::ShouldShow(started,paused,session.Alive(),fieldJournalOpen);
        Mistwatch::ObjectivePointer::Submit(draw,
            Mistwatch::ObjectivePointer::Build(session,guide,started,paused,fieldJournalOpen));
        if(surveyMapRequested){
            const auto hudLayout=Mistwatch::FieldPresentation::MeasureLayout(draw,fieldHud,session,
                Mistwatch::FieldPresentation::LastAction(message),actionFeedback,started,paused,
                e.Audio().GetStats().deviceAvailable,gameAudio.options.muted,unsigned(gameAudio.options.volume*100+.5f));
            // 與實際字型排版核對，不以估計高度覆蓋操作結果。
            if(hudLayout.showToast&&hudLayout.toast.Bottom()+8>Mistwatch::SurveyMap::Top)
                throw std::runtime_error("Survey map overlaps action feedback");
            const auto survey=Mistwatch::SurveyMap::Build(session,guide,camera,terrain,trackedRelay,previewRelay,true);
            surveyMapDraw=Mistwatch::SurveyMap::Draw(draw,survey);
            if(!surveyMapDraw.drawn){
                // 不可靠座標不能畫成假位置；地圖不可用也不能結束玩家的既有遊戲。
                draw.Quad({640,368},{1264,384},{.025f,.045f,.055f,.96f});
                draw.SystemText({348,184},"地圖資料暫不可用；原任務狀態已保留。",18.2f,{.9f,.95f,.96f,1});
            }
        }
        RenderManualEntrySmoke(e,draw.ViewProjection());
        if(inputReplayCaptureRequested){
            inputReplayCaptureBase=e.GetRendererStats().successfulFrameCaptures;
            const auto path=inputReplayDirectory/(std::to_string(inputReplayImages)+"-"+inputReplayPhase+".png");
            if(!e.CaptureNextFrame3D(path))throw std::runtime_error("Input replay capture request rejected");
            auto statePath=path;statePath+=L".view.txt";std::ofstream state(statePath);state.precision(9);
            state<<"expected_render_serial="<<e.GetRendererStats().submittedFrames+1<<"\nrequested_unjittered_vp_bits=";
            for(const auto& row:draw.ViewProjection().values)for(float v:row)state<<std::bit_cast<std::uint32_t>(v)<<',';
            state<<"\nsubject="<<session.feet.x<<','<<session.feet.y<<','<<session.feet.z<<','<<controller.Settings().height
                <<"\nwindow=";
            for(float v:PackScreenOcclusionWindow3D(draw.ScreenOcclusionWindow()))state<<v<<',';
            state<<'\n';state.close();if(!state)throw std::runtime_error("Capture view request receipt failed");
            inputReplayCapturePending=true;inputReplayCaptureRequested=false;
        }
        if(!capture.empty() && !capturePending && ++captureFrames>=45 &&
           (!captureLandscapeMotion||captureCameraTicks>=30) &&
           (!captureAudioProbe||captureAudioTicks>=180) &&
           (!captureBenchmark || captureGpuBenchmark.Complete())) {
            if(diagnosticHdrStage&&(captureCompleted==18||captureCompleted==27)) {
                const auto hash=[](const auto& value) {
                    std::uint64_t result=1469598103934665603ull;
                    const auto bytes=std::as_bytes(std::span{&value,1});
                    for(const auto byte:bytes){result^=std::to_integer<std::uint8_t>(byte);result*=1099511628211ull;}
                    return result;
                };
                const auto cameraHash=hash(draw.ViewProjection());
                const std::array<std::uint64_t,3> state{hash(waterTime),std::uint64_t(session.CurrentStage()),session.repairMaterials};
                if(!e.RequestLinearHdrReadback3D({captureFrames,cameraHash,hash(state)}))throw std::runtime_error("HDR diagnostic request rejected");
            }
            if(captureGrassDistance)grassFixtureFrozen=grassFixtureState;
            landscapeFrozen=landscapeFrame;
            standFrozen=standFrame;
            shoulderFrozen=shoulderFrame;
            groundcoverFrozen=groundcoverFrame;
            coreHerbFrozen=coreHerbFrame;
            routeGrassFrozen=routeGrassFrame;
            cliffFrozen=cliffFrame;
            cliffMaterialFrozen=cliffMaterialFrame;
            rockSegmentFrozen=rockSegmentFrame;
            capturePending=e.CaptureNextFrame3D(CapturePath());
            if(captureCostEnabled&&!captureRequestNs)captureRequestNs=CaptureElapsedNs();
            if(!capturePending)throw std::runtime_error("Frame capture request rejected");
        }
    }
    void OnStop(Engine& e) override {
        matureStudy.Shutdown(e,indexedGameplayOutput,!inputReplayDirectory.empty());
        const auto rollbackIbl=[&e](MistwatchGame* game)noexcept {game->RollbackForestIbl(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackIbl)> iblStopGuard(this,rollbackIbl);
        const auto rollbackRouteGrass=[&e](MistwatchGame* game)noexcept {game->RollbackRouteGrass(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackRouteGrass)> routeGrassStopGuard(this,rollbackRouteGrass);
        const auto rollbackHerbs=[&e](MistwatchGame* game)noexcept {game->RollbackCoreHerbs(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackHerbs)> herbStopGuard(this,rollbackHerbs);
        // 後續任一清理或寫檔報錯仍嘗試釋放五份新增網格。
        const auto rollbackCrown=[&e](MistwatchGame* game)noexcept {game->RollbackCrownGrowth(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackCrown)> crownStopGuard(this,rollbackCrown);
        rockSegment.Release(e);
        // 任何較早的清理或收據錯誤都不能遺漏新增屋頂的 GPU 參照及 CPU 凸包。
        const auto rollbackWorkshopRoof=[&e](MistwatchGame* game)noexcept {
            if(!game->workshopRoof)return;
            if(game->workshopMesh)e.DestroyMesh3D(game->workshopMesh.handle);
            game->workshopMesh={};game->workshopRoof.reset();
        };
        std::unique_ptr<MistwatchGame,decltype(rollbackWorkshopRoof)> workshopRoofStopGuard(this,rollbackWorkshopRoof);
        const auto rollbackShoulder=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseShoulderRoots(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackShoulder)> shoulderStopGuard(this,rollbackShoulder);
        const auto rollbackGroundcover=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseShoulderGroundcover(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackGroundcover)> groundcoverStopGuard(this,rollbackGroundcover);
        // 舊資源清理或證據寫入報錯時，棚屋仍需完成回收。
        const auto rollbackShelter=[&e](MistwatchGame* game)noexcept {game->RollbackShelterDetail(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackShelter)> shelterStopGuard(this,rollbackShelter);
        const auto rollbackRoad=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseEntranceRoad(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackRoad)> roadStopGuard(this,rollbackRoad);
        // 舊 cleanup 若中途報錯，新增群落仍由此界線負責釋放。
        const auto rollbackRoadside=[&e](MistwatchGame* game)noexcept {(void)game->ReleaseRoadsideBroadleaf(e);};
        std::unique_ptr<MistwatchGame,decltype(rollbackRoadside)> roadsideStopGuard(this,rollbackRoadside);
        // 診斷失敗不能跳過原資源清理；先保留有界證據，正常清理後仍報失敗。
        bool costWritten=true;
        bool forestSurfaceReceiptWritten=true;
        // 寫檔失敗先記錄，既有場景及 GPU 資源完整回收後才回報失敗。
        try{
            if(!inputReplayDirectory.empty()){
                std::ofstream proof(inputReplayDirectory/"foliage-view.txt");proof.precision(9);
                proof<<"profile="<<(foliageViewWindowEnabled?"subject-window-needles-v2":foliageViewEnabled?"foreground-needles-color-v1":"original")
                    <<"\nrendered_frames="<<foliageViewFrames<<"\nfaded_frames="<<foliageViewFadedFrames
                    <<"\ncolor_instances="<<foliageViewColorInstances<<"\npreserved_shadow_copies="<<foliageViewShadowCopies
                    <<"\nminimum_opacity="<<foliageViewMinimumOpacity<<"\nphysics_modified=0\nalpha_map_visibility_query=0\n";
                proof.close();if(!proof)throw std::runtime_error("Foliage view receipt write failed");
                std::ofstream phases(inputReplayDirectory/"foliage-view-phases.tsv");phases.precision(9);
                phases<<"phase\tframes\tcolor_instances\tshadow_copies\tminimum_opacity\n";
                for(const auto& row:foliageViewPhases)phases<<row.name<<'\t'<<row.frames<<'\t'<<row.colors<<'\t'<<row.shadows<<'\t'<<row.minimum<<'\n';
                phases.close();if(!phases)throw std::runtime_error("Foliage view phase receipt failed");
            }
            if(!manualForestSurfaceReceiptPrefix.empty())WriteForestSurfaceReceipt(manualForestSurfaceReceiptPrefix,e.GetRendererStats());
            else if(!inputReplayDirectory.empty())WriteForestSurfaceReceipt(inputReplayDirectory/"last-render",e.GetRendererStats());
        }catch(...){forestSurfaceReceiptWritten=false;}
        if(inputReplayCostEnabled||captureCostEnabled){
            try{RecordInputReplayCost(e);}catch(...){inputReplayCostFailed=true;}
            try{costWritten=inputReplayCost.Write(captureCostEnabled?captureCostDirectory:inputReplayDirectory,inputReplayCostFailed);}catch(...){costWritten=false;}
            if(captureCostEnabled){
                try{
                    std::ofstream timeline(captureCostDirectory/"capture-timeline.txt");
                    timeline<<"schema=mistwatch.capture-timeline.1\nclock=steady-relative-to-original-capture-start\n"
                        <<"ready_ns="<<captureReadyNs<<"\nbenchmark_complete_ns="<<benchmarkCompleteNs
                        <<"\ncapture_request_ns="<<captureRequestNs<<"\ncapture_observed_ns="<<captureObservedNs
                        <<"\nreceipt_end_ns="<<captureReceiptEndNs<<"\nquit_ns="<<captureQuitNs<<"\ntimeout_ns="<<captureTimeoutNs
                        <<"\nsamples="<<captureGpuBenchmark.Count()<<"\nwrite_timing=on-stop-only\n";
                    timeline.close();costWritten=costWritten&&bool(timeline);
                }catch(...){costWritten=false;}
            }
        }
        StopGameAudio(e);scene.Clear();scene.SetBoxMesh({});
        const auto groundcoverBefore=e.GetRendererStats();
        const auto groundcoverReleased=ReleaseShoulderGroundcover(e);
        if(groundcoverReleased)e.WaitRendererIdle3D();
        const auto groundcoverAfter=e.GetRendererStats();
        const auto shoulderBefore=e.GetRendererStats();
        const auto shoulderReleased=ReleaseShoulderRoots(e);
        if(shoulderReleased)e.WaitRendererIdle3D();
        const auto shoulderAfter=e.GetRendererStats();
        const auto landscapeBefore=e.GetRendererStats();unsigned landscapeReleased{};
        std::vector<std::uint64_t> landscapeReleasedHandles;
        for(auto& mesh:landscapeRootMeshes){
            if(mesh){landscapeReleasedHandles.push_back(mesh.handle.value);e.DestroyMesh3D(mesh.handle);++landscapeReleased;}mesh={};
        }
        if(landscapeReleased)e.WaitRendererIdle3D();
        const auto landscapeAfter=e.GetRendererStats();
        std::array<std::uint64_t,3> repairSignHandles{};
        unsigned repairSignDestroyed{};
        for(unsigned group=0;group<repairSignMeshes.size();++group) {
            auto& mesh=repairSignMeshes[group];
            repairSignHandles[group]=mesh.handle.value;
            if(mesh){e.DestroyMesh3D(mesh.handle);++repairSignDestroyed;}
            mesh={};
        }
        repairSignMaterials={};
        const bool repairSignsCleared=!repairSignMeshes[0]&&!repairSignMeshes[1]&&!repairSignMeshes[2];
        if(repairSignDestroyed!=repairSignMeshes.size()||!repairSignsCleared)
            throw std::runtime_error("Repair sign actual cleanup mismatch");
        if(!capture.empty()) {
            auto signPath=capture;signPath+=".repair-sign-cleanup.txt";
            if(std::filesystem::exists(signPath))throw std::runtime_error("Repair sign cleanup refuses overwrite");
            std::ofstream signOut(signPath);
            signOut<<"wood_handle="<<repairSignHandles[0]<<"\npaint_handle="<<repairSignHandles[1]
                <<"\nsteel_handle="<<repairSignHandles[2]<<"\ndestroy_calls="<<repairSignDestroyed
                <<"\ncleared="<<repairSignsCleared<<'\n';
            signOut.close();if(!signOut)throw std::runtime_error("Repair sign cleanup write failed");
        }
        unsigned generatorDestroyed{};
        for(auto& mesh:generatorDetailMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++generatorDestroyed;}mesh={};}
        const auto canopyReferencesBefore=e.GetRendererStats().residentMeshReferences;
        std::array<std::uint64_t,Mistwatch::WoodlandCanopy::TotalCount> releasedRootHandles{};
        unsigned canopyDestroyed{};for(auto& mesh:canopyMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++canopyDestroyed;}mesh={};}
        for(unsigned i=0;i<canopyRootMeshes.size();++i){auto& mesh=canopyRootMeshes[i];releasedRootHandles[i]=mesh.handle.value;
            if(mesh){e.DestroyMesh3D(mesh.handle);++canopyDestroyed;}mesh={};}
        canopyMaterials.Shutdown();
        if(woodlandWorld){
            e.WaitRendererIdle3D();const auto after=e.GetRendererStats().residentMeshReferences;
            if(canopyDestroyed!=15||after+canopyDestroyed!=canopyReferencesBefore)throw std::runtime_error("Woodland actual reference release mismatch");
            auto path=!capture.empty()?capture:!inputReplayDirectory.empty()?inputReplayDirectory/"world-start":std::filesystem::path{};
            if(!path.empty()){path+=".woodland-cleanup.txt";
                if(std::filesystem::exists(path))throw std::runtime_error("Woodland cleanup refuses overwrite");
                std::ofstream out(path);
                out<<"schema=woodland-reference-cleanup-v1\nreleased_references="<<canopyDestroyed<<"\nreferences_before="<<canopyReferencesBefore
                    <<"\nreferences_after="<<after<<"\nremaining_owned=0\nmaterials_remaining="<<canopyMaterials.Stats().materials<<"\ncleanup=success\n";
                for(unsigned i=0;i<releasedRootHandles.size();++i)out<<"root_"<<i<<"_handle="<<releasedRootHandles[i]<<'\n';
                out.close();if(!out)throw std::runtime_error("Woodland cleanup receipt failed");}
        }
        unsigned trailDestroyed{};
        unsigned shrubDestroyed{};for(auto& pair:shrubMeshes)for(auto& mesh:pair){if(mesh){e.DestroyMesh3D(mesh.handle);++shrubDestroyed;}mesh={};}
        for(auto& mesh:trailLitterMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++trailDestroyed;}mesh={};}
        unsigned suppliesDestroyed{};
        unsigned bridgeWorksiteDestroyed{};
        for(auto& mesh:bridgeWorksiteMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++bridgeWorksiteDestroyed;}mesh={};}
        if(bridgeWorksiteDestroyed!=2)throw std::runtime_error("Bridge worksite cleanup incomplete");
        if(!capture.empty()) {
            auto path=capture;path+=".bridge-worksite-cleanup.txt";
            std::ofstream out(path);out<<"mesh_destroy_calls=2\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Bridge worksite cleanup receipt failed");
        }
        if(!rangerDeskMesh)throw std::runtime_error("Ranger desk cleanup incomplete");
        const auto deskDestroyedHandle=rangerDeskMesh.handle.value;
        unsigned deskDestroyed{};
        e.DestroyMesh3D(rangerDeskMesh.handle);++deskDestroyed;rangerDeskMesh={};
        const bool deskCleanupOk=deskDestroyed==1&&!rangerDeskMesh;
        if(!capture.empty()) {
            auto path=capture;path+=".desk-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Ranger desk cleanup refuses overwrite");
            std::ofstream out(path);out<<"mesh_destroy_calls="<<deskDestroyed<<"\ndestroyed_handle="<<deskDestroyedHandle
                <<"\ncleared_resources="<<!rangerDeskMesh<<"\ncleanup="<<(deskCleanupOk?"success":"failed")<<'\n';
            out.close();if(!out)throw std::runtime_error("Ranger desk cleanup receipt failed");
        }
        if(!deskCleanupOk)throw std::runtime_error("Ranger desk cleanup failed");
        const auto toolMetalHandle=repairToolsMeshes[0].handle.value,toolGripHandle=repairToolsMeshes[1].handle.value;
        unsigned toolsDestroyed=0;for(auto& mesh:repairToolsMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++toolsDestroyed;}mesh={};}
        if(toolsDestroyed!=(repairToolsDetailed?2u:0u))throw std::runtime_error("Repair tools cleanup mismatch");
        if(!capture.empty()){auto toolsPath=capture;toolsPath+=".repair-tools-cleanup.txt";
            if(std::filesystem::exists(toolsPath))throw std::runtime_error("Repair tools cleanup refuses overwrite");
            std::ofstream toolsOut(toolsPath);toolsOut<<"metal_handle="<<toolMetalHandle<<"\ngrip_handle="<<toolGripHandle
                <<"\ndestroy_calls="<<toolsDestroyed<<"\ncleared="<<(!repairToolsMeshes[0]&&!repairToolsMeshes[1])<<'\n';
            toolsOut.close();if(!toolsOut)throw std::runtime_error("Repair tools cleanup write failed");}
        const auto shelterHandle=plankedShelterMesh.handle.value;unsigned shelterDestroyed=0;
        const auto detailDestroyed=ReleaseShelterDetail(e);
        if(detailDestroyed!=(shelterDetailEnabled?2u:0u))throw std::runtime_error("Shelter detail cleanup mismatch");
        if(!capture.empty()){
            auto path=capture;path+=".shelter-detail-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Shelter detail cleanup refuses overwrite");
            std::ofstream out(path);out<<"mesh_destroy_calls="<<detailDestroyed<<"\nremaining_owned=0\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Shelter detail cleanup write failed");
        }
        if(plankedShelterMesh){e.DestroyMesh3D(plankedShelterMesh.handle);++shelterDestroyed;plankedShelterMesh={};}
        if(shelterDestroyed!=unsigned(plankedShelter))throw std::runtime_error("Planked shelter cleanup mismatch");
        if(!capture.empty()){auto shelterPath=capture;shelterPath+=".planked-shelter-cleanup.txt";
            if(std::filesystem::exists(shelterPath))throw std::runtime_error("Shelter cleanup refuses overwrite");
            std::ofstream shelterOut(shelterPath);shelterOut<<"destroyed_handle="<<shelterHandle<<"\ndestroy_calls="<<shelterDestroyed<<"\ncleared="<<!plankedShelterMesh<<"\n";
            shelterOut.close();if(!shelterOut)throw std::runtime_error("Shelter cleanup write failed");}
        const auto workshopHandle=workshopMesh.handle.value;unsigned workshopDestroyed=0;
        if(workshopMesh){e.DestroyMesh3D(workshopMesh.handle);++workshopDestroyed;workshopMesh={};}
        if(workshopDestroyed!=unsigned(plankedWorkshop))throw std::runtime_error("Workshop cleanup mismatch");
        if(!capture.empty()){
            auto path=capture;path+=".workshop-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Workshop cleanup refuses overwrite");
            std::ofstream out(path);out<<"destroyed_handle="<<workshopHandle<<"\ndestroy_calls="<<workshopDestroyed<<"\ncleared="<<!workshopMesh<<'\n';
            out.close();if(!out)throw std::runtime_error("Workshop cleanup write failed");
        }
        if(!rangerHutMesh)throw std::runtime_error("Ranger hut cleanup incomplete");
        e.DestroyMesh3D(rangerHutMesh.handle);rangerHutMesh={};
        if(!capture.empty()) {
            auto path=capture;path+=".hut-cleanup.txt";
            std::ofstream out(path);out<<"mesh_destroy_calls=1\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Ranger hut cleanup receipt failed");
        }
        for(auto& mesh:supplyMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++suppliesDestroyed;}mesh={};}
        if(suppliesDestroyed!=2)throw std::runtime_error("Supply crate cleanup incomplete");
        if(!capture.empty()) {
            auto path=capture;path+=".supply-cleanup.txt";
            std::ofstream out(path);out<<"mesh_destroy_calls=2\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Supply cleanup receipt failed");
        }
        e.DestroySkinnedMesh3D(explorerMesh.handle);e.DestroySkinnedMesh3D(boarMesh.handle);
        if(valleyMaterialAudit){
            const auto path=valleyEvidenceDirectory/"valley-rock-material-runtime.txt";
            if(std::filesystem::exists(path)||!cliffMaterialObservedFrames)throw std::runtime_error("Valley rock material runtime evidence unavailable");
            std::ofstream out(path);out<<"schema=valley-rock-material-v1\nprofile="<<(cliffMaterialEnabled?"rock-face-03-2k":"original")
                <<"\nrendered_frames="<<cliffMaterialObservedFrames<<"\nterrain_texture="<<TerrainRenderMaterial().textures.semanticIdentity
                <<"\noriginal_texture="<<groundMaterial.textures.semanticIdentity<<"\nroad_texture="<<entranceRoadSurface.textures.semanticIdentity
                <<"\ngravel_texture="<<settlementSurfaceMaterial.textures.semanticIdentity<<"\nworld="<<valleySaveDomain->Identity()<<'\n';
            const std::array<const char*,4> names{"core","ring","road","gravel"};
            for(unsigned i=0;i<4;++i)out<<names[i]<<"_observed_frames="<<cliffMaterialObservedDraws[i]<<'\n';
            out.close();if(!out)throw std::runtime_error("Valley rock material evidence write failed");
        }
        if(cliffWorldEnabled&&!inputReplayDirectory.empty()){
            const auto path=inputReplayDirectory/"cliff-world-runtime.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Cliff runtime receipt refuses overwrite");
            std::ofstream out(path);out<<"schema=cliff-world-runtime-v1\nprofile=rock-face-03-world-v1\nrendered_frames="<<cliffMaterialObservedFrames
                <<"\nterrain_texture="<<TerrainRenderMaterial().textures.semanticIdentity
                <<"\noriginal_texture="<<groundMaterial.textures.semanticIdentity
                <<"\nroad_texture="<<entranceRoadSurface.textures.semanticIdentity
                <<"\ngravel_texture="<<settlementSurfaceMaterial.textures.semanticIdentity<<'\n';
            const std::array<const char*,4> names{"core","ring","road","gravel"};
            for(unsigned i=0;i<4;++i)out<<names[i]<<"_observed_frames="<<cliffMaterialObservedDraws[i]<<'\n';
            out<<"capture_mode="<<!capture.empty()<<"\nfixed_camera="<<captureFrameCamera<<"\nart_accepted=0\n";
            out.close();if(!out)throw std::runtime_error("Cliff runtime receipt write failed");
        }
        if(valleyOutcropPreview){
            valleyOutcrop.Release(e);
            if(valleyOutcrop.mesh||valleyOutcrop.acquired!=1||valleyOutcrop.released!=1)
                throw std::runtime_error("Outcrop exact resource release incomplete");
            valleyOutcrop.Write(valleyEvidenceDirectory);
        }
        if(valleySlopeGrove){
            slopeGrove.Release(e);
            if(slopeGrove.acquired!=8||slopeGrove.released!=8)throw std::runtime_error("Slope grove exact cleanup");
            slopeGrove.Write(valleyEvidenceDirectory,!inputReplayDirectory.empty());
        }
        materials.Shutdown();forestMaterials.Shutdown();broadleafMaterials.Shutdown();boulderMaterials.Shutdown();
        fernMaterials.Shutdown();
        grassMaterials.Shutdown();
        const auto roadsideBroadleafDestroyed=ReleaseRoadsideBroadleaf(e);
        const auto entranceRoadDestroyed=ReleaseEntranceRoad(e);
        nettleMaterials.Shutdown();
        unsigned nettleDestroyed{};
        for(auto& mesh:nettleMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++nettleDestroyed;}mesh={};}
        if(nettleMaterials.Stats().materials||nettleDestroyed!=(diagnosticNettleMix?6u:0u))
            throw std::runtime_error("Nettle exact cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".nettle-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls="<<nettleDestroyed<<"\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Nettle cleanup receipt failed");
        }
        unsigned grassDestroyed{};
        for(auto& mesh:scannedGrass){if(mesh){e.DestroyMesh3D(mesh.handle);++grassDestroyed;}mesh={};}
        unsigned curvedGrassDestroyed{};
        for(auto& mesh:curvedScannedGrass){if(mesh){e.DestroyMesh3D(mesh.handle);++curvedGrassDestroyed;}mesh={};}
        if(grassMaterials.Stats().materials||grassDestroyed!=5)throw std::runtime_error("Scanned grass cleanup incomplete");
        if(curvedGrassDestroyed!=(diagnosticCurvedScannedGrass?5u:0u))throw std::runtime_error("Curved grass cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".grass-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls="<<grassDestroyed+curvedGrassDestroyed
                <<"\ncanonical_mesh_destroy_calls="<<grassDestroyed<<"\ncandidate_mesh_destroy_calls="<<curvedGrassDestroyed
                <<"\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Grass cleanup receipt failed");
        }
        unsigned fernDestroyed{};
        for(auto& mesh:scannedFerns){if(mesh){e.DestroyMesh3D(mesh.handle);++fernDestroyed;}mesh={};}
        unsigned broadleafDestroyed{};
        for(auto& mesh:broadleafMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++broadleafDestroyed;}mesh={};}
        if(broadleafMaterials.Stats().materials||broadleafDestroyed!=3)throw std::runtime_error("Broadleaf cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".broadleaf-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls=3\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Broadleaf cleanup receipt failed");
        }
        e.DestroyMesh3D(box.handle);for(auto& mesh:firMeshes)e.DestroyMesh3D(mesh.handle);
        e.DestroyMesh3D(detailedFirTwig.handle);detailedFirTwig={};
        unsigned landscapeFirDestroyed{};
        const auto crownGrowthReleased=ReleaseCrownGrowth(e);
        const auto herbReleased=ReleaseCoreHerbs(e);
        const auto routeReleased=ReleaseRouteGrass(e);
        if(routeReleased!=routeGrassAcquisitions||routeReleased!=(routeGrassEnabled?128u:0u))
            throw std::runtime_error("Route grass exact cleanup incomplete");
        const auto routeCleanupPrefix=capture.empty()?inputReplayDirectory/"first-frame":capture;
        if(!capture.empty()||!inputReplayDirectory.empty()){
            auto path=routeCleanupPrefix;path+=L".route-grass-cleanup.txt";
            std::ofstream out(path);out<<"acquisitions="<<routeGrassAcquisitions<<"\nrelease_calls="<<routeReleased
                <<"\nremaining_owned=0\ncleanup=success\n";out.close();if(!out)throw std::runtime_error("Route grass cleanup write failed");
        }
        (void)routeGrassStopGuard.release();
        (void)herbStopGuard.release();
        if(herbReleased!=coreHerbAcquisitions||herbReleased!=(coreHerbEnabled?Mistwatch::CoreHerbClusters::Target:0u))
            throw std::runtime_error("Core herb exact cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=L".core-herbs-cleanup.txt";
            std::ofstream out(path);out<<"acquisitions="<<coreHerbAcquisitions<<"\nrelease_calls="<<herbReleased
                <<"\nremaining_owned=0\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Core herb cleanup receipt failed");
        }
        (void)crownStopGuard.release();
        if(crownGrowthReleased!=(landscapeCrownGrowth?5u:0u))throw std::runtime_error("Crown growth exact cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=L".crown-growth-cleanup.txt";
            std::ofstream out(path);out<<"acquisitions="<<crownGrowthAcquisitions<<"\nrelease_calls="<<crownGrowthReleased
                <<"\nremaining_owned=0\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Crown growth cleanup receipt failed");
        }
        for(auto& mesh:landscapeFirMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++landscapeFirDestroyed;}mesh={};}
        if(landscapeDetailedFirTwig){e.DestroyMesh3D(landscapeDetailedFirTwig.handle);++landscapeFirDestroyed;}landscapeDetailedFirTwig={};
        if(landscapeFirDestroyed!=(landscapeTwigDistanceLod?5u:4u))throw std::runtime_error("Landscape fir exact cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".landscape-fir-cleanup.txt";
            std::ofstream out(path);out<<"mesh_destroy_calls="<<landscapeFirDestroyed<<"\nremaining_owned=0\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Landscape fir cleanup receipt failed");
        }
        const auto fallenHandle=fallenLogs.mesh.handle.value;
        const auto fallenDestroyed=fallenLogs.Shutdown(e);
        const bool scanned=fallenLogProfile==Mistwatch::FallenLogProfile::ScannedStraightV1;
        if(fallenLogs.mesh||fallenLogs.materials.Stats().materials||fallenDestroyed!=(scanned?1u:0u))
            throw std::runtime_error("Fallen-log cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".fallen-log-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Fallen-log cleanup refuses overwrite");
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls="<<fallenDestroyed
                <<"\ndestroyed_handle="<<fallenHandle<<"\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Fallen-log cleanup receipt failed");
        }
        unsigned propsDestroyed{};
        for(auto& mesh:propMeshes){if(mesh){e.DestroyMesh3D(mesh.handle);++propsDestroyed;}mesh={};}
        if(boulderMaterials.Stats().materials||propsDestroyed!=2)throw std::runtime_error("Forest prop cleanup incomplete");
        if(!capture.empty()) {
            auto path=capture;path+=".boulder-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls=2\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Boulder cleanup receipt failed");
        }
        e.DestroyMesh3D(terrainMesh.handle);e.DestroyMesh3D(landscapeMesh.handle);e.DestroyMesh3D(riverMesh.handle);
        e.DestroyMesh3D(grassMesh.handle);e.DestroyMesh3D(fernMesh.handle);e.DestroyMesh3D(relayDish.handle);
        if(rockSegmentDiagnostic)rockSegment.WriteCleanup(CapturePath());
        // 新蕨株檢查放在其餘正常釋放之後；receipt 不宣稱全引擎 GPU 零洩漏。
        if(fernMaterials.Stats().materials||fernDestroyed!=4)throw std::runtime_error("Scanned fern cleanup incomplete");
        if(!capture.empty()) {
            auto path=capture;path+=".fern-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls=4\ncleanup=success\n";out.close();
            if(!out)throw std::runtime_error("Fern cleanup receipt failed");
        }
        // 新碎物檢查與證據寫入在正常釋放完成後，不增加其餘資源的提前退場點。
        if(trailDestroyed!=2)throw std::runtime_error("Trail litter cleanup incomplete");
        if(shrubDestroyed!=8)throw std::runtime_error("Shrub cleanup incomplete");
        if(canopyDestroyed!=(Mistwatch::WorldProfileHasCanopy(worldProfile)?unsigned(3+canopyPopulation.plants.size()):0u)||canopyMaterials.Stats().materials)throw std::runtime_error("Canopy preview normal cleanup incomplete");
        if(!capture.empty()){auto path=capture;path+=".canopy-cleanup.txt";std::ofstream out(path);out<<"mesh_destroy_calls="<<canopyDestroyed<<"\nmaterials_remaining=0\ncleanup=success\n";out.close();if(!out)throw std::runtime_error("Canopy preview cleanup receipt failed");}
        if(!capture.empty()){auto path=capture;path+=".shrub-cleanup.txt";std::ofstream out(path);out<<"mesh_destroy_calls=8\ncleanup=success\n";out.close();if(!out)throw std::runtime_error("Shrub cleanup receipt failed");}
        if(!capture.empty()){
            auto path=capture;path+=".trail-cleanup.txt";
            std::ofstream out(path);out<<"mesh_destroy_calls=2\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Trail litter cleanup receipt failed");
        }
        const auto generatorCleared=static_cast<std::size_t>(std::count_if(generatorDetailMeshes.begin(),generatorDetailMeshes.end(),
            [](const auto& mesh){return !mesh;}));
        const bool generatorCleanupOk=generatorDestroyed==Mistwatch::GeneratorDetailGroups&&
            generatorCleared==Mistwatch::GeneratorDetailGroups;
        if(!capture.empty()) {
            auto path=capture;path+=".generator-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Generator cleanup refuses overwrite");
            std::ofstream out(path);out<<"mesh_destroy_calls="<<generatorDestroyed
                <<"\ncleared_resources="<<generatorCleared<<"\ncleanup="<<(generatorCleanupOk?"success":"failed")<<"\n";
            out.close();if(!out)throw std::runtime_error("Generator cleanup receipt failed");
        }
        if(!generatorCleanupOk)throw std::runtime_error("Generator detail cleanup incomplete");
        if(landscapeReleased!=landscapeRootAcquisitions||
           landscapeAfter.residentMeshReferences+landscapeReleased!=landscapeBefore.residentMeshReferences)
            throw std::runtime_error("Landscape acquisition reference cleanup mismatch");
        if(!capture.empty()){
            auto path=capture;path+=L".landscape-planting-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Landscape cleanup refuses overwrite");
            std::ofstream out(path);out<<"created_acquisitions="<<landscapeRootAcquisitions<<"\nrelease_calls="<<landscapeReleased
                <<"\nremaining_owned=0\nreferences_before="<<landscapeBefore.residentMeshReferences
                <<"\nreferences_after="<<landscapeAfter.residentMeshReferences
                <<"\npending_release_bytes_after_idle="<<landscapeAfter.pendingGpuReleaseBytes<<"\ncleanup=success\n";
            for(std::size_t i=0;i<landscapeReleasedHandles.size();++i)out<<"released_"<<i<<"="<<landscapeReleasedHandles[i]
                <<"\nreleased_"<<i<<"_ordinal="<<landscapePlanting.plants.at(i).sourceOrdinal<<'\n';
            out.close();if(!out)throw std::runtime_error("Landscape cleanup receipt failed");
        }
        landscapePlanting={};landscapeBroadleafSource={};landscapeRootAcquisitions=0;landscapeFrame={};landscapeFrozen={};
        const auto shoulderRemaining=std::count_if(shoulderRootMeshes.begin(),shoulderRootMeshes.end(),[](const auto& mesh){return bool(mesh);});
        const auto shoulderReleaseFailures=std::count(shoulderReleaseFailed.begin(),shoulderReleaseFailed.end(),true);
        const bool shoulderCleanupOk=!shoulderRemaining&&!shoulderReleaseFailures&&shoulderReleased==shoulderRootAcquisitions&&
            shoulderAfter.residentMeshReferences+shoulderReleased==shoulderBefore.residentMeshReferences;
        const auto shoulderCleanupBase=!capture.empty()?capture:(!inputReplayDirectory.empty()?inputReplayDirectory/"cleanup":std::filesystem::path{});
        if(!shoulderCleanupBase.empty()){
            auto path=shoulderCleanupBase;path+=L".shoulder-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Shoulder cleanup refuses overwrite");
            std::ofstream out(path);out<<"created_acquisitions="<<shoulderRootAcquisitions<<"\nrelease_calls="<<shoulderReleased
                <<"\nremaining_owned="<<shoulderRemaining<<"\nrelease_failures="<<shoulderReleaseFailures
                <<"\nreferences_before="<<shoulderBefore.residentMeshReferences
                <<"\nreferences_after="<<shoulderAfter.residentMeshReferences
                <<"\npending_release_bytes_after_idle="<<shoulderAfter.pendingGpuReleaseBytes
                <<"\ncleanup="<<(shoulderCleanupOk?"success":"failed")<<'\n';
            out.close();if(!out)throw std::runtime_error("Shoulder cleanup receipt failed");
        }
        if(!shoulderCleanupOk)throw std::runtime_error("Shoulder exact resource cleanup mismatch");
        shoulderGrove={};shoulderRootAcquisitions=0;shoulderSelectedBase=0;shoulderSelectedStand=0;
        shoulderFrame={};shoulderFrozen={};(void)shoulderStopGuard.release();
        const auto groundcoverRemaining=std::count_if(shoulderGroundcoverMeshes.begin(),shoulderGroundcoverMeshes.end(),[](const auto& m){return bool(m);});
        const auto groundcoverFailures=std::count(shoulderGroundcoverReleaseFailed.begin(),shoulderGroundcoverReleaseFailed.end(),true);
        const bool groundcoverCleanupOk=!groundcoverRemaining&&!groundcoverFailures&&groundcoverReleased==shoulderGroundcoverAcquisitions&&
            groundcoverAfter.residentMeshReferences+groundcoverReleased==groundcoverBefore.residentMeshReferences;
        if(!shoulderCleanupBase.empty()){
            auto path=shoulderCleanupBase;path+=L".shoulder-groundcover-cleanup.txt";
            if(std::filesystem::exists(path))throw std::runtime_error("Groundcover cleanup refuses overwrite");
            std::ofstream out(path);out<<"created_acquisitions="<<shoulderGroundcoverAcquisitions<<"\nrelease_calls="<<groundcoverReleased
                <<"\nremaining_owned="<<groundcoverRemaining<<"\nrelease_failures="<<groundcoverFailures
                <<"\nreferences_before="<<groundcoverBefore.residentMeshReferences<<"\nreferences_after="<<groundcoverAfter.residentMeshReferences
                <<"\npending_release_bytes_after_idle="<<groundcoverAfter.pendingGpuReleaseBytes
                <<"\ncleanup="<<(groundcoverCleanupOk?"success":"failed")<<'\n';
            out.close();if(!out)throw std::runtime_error("Groundcover cleanup receipt write failed");
        }
        if(!groundcoverCleanupOk)throw std::runtime_error("Groundcover cleanup incomplete");
        (void)groundcoverStopGuard.release();
        // 新增候選的檢查及收據不打斷既有正常釋放流程。
        if(roadsideBroadleafMaterials.Stats().materials||roadsideBroadleafDestroyed!=(roadsideBroadleafEnabled?2u:0u))
            throw std::runtime_error("Roadside broadleaf exact cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".roadside-broadleaf-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls="<<roadsideBroadleafDestroyed<<"\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Roadside broadleaf cleanup receipt failed");
        }
        (void)roadsideStopGuard.release();
        if(entranceRoadMaterials.Stats().materials||entranceRoadDestroyed!=(entranceRoadEnabled?(settlementSurfaceEnabled?3u:2u):0u))
            throw std::runtime_error("Entrance road cleanup incomplete");
        if(!capture.empty()){
            auto path=capture;path+=".entrance-road-cleanup.txt";
            std::ofstream out(path);out<<"materials=0\nmesh_destroy_calls="<<entranceRoadDestroyed<<"\ncleanup=success\n";
            out.close();if(!out)throw std::runtime_error("Entrance road cleanup receipt failed");
        }
        (void)roadStopGuard.release();
        (void)shelterStopGuard.release();
        // 先完成其他場景資源釋放；IBL 收據寫入失敗不得跳過其清理責任。
        ReleaseForestIbl(e);(void)iblStopGuard.release();
        if(!inputReplayDirectory.empty()){
            std::ofstream out(inputReplayDirectory/"cleanup.txt");
            out<<"normal_on_stop_completed=1\nresult_recorded="<<inputReplayRecorded<<'\n';
            out.close();if(!out)throw std::runtime_error("Input replay cleanup receipt write failed");
        }
        if(inputReplayCostFailed||!costWritten)throw std::runtime_error("Input replay frame cost diagnostic failed after cleanup");
        if(!forestSurfaceReceiptWritten)throw std::runtime_error("Forest surface receipt failed after cleanup");
    }
};
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR commandLine,int show) {
    try {
        const std::wstring_view arguments=commandLine?commandLine:L"";
        const auto first=arguments.find_first_not_of(L" \t");
        const auto last=arguments.find_last_not_of(L" \t");
        const auto argument=first==std::wstring_view::npos?std::wstring_view{}:arguments.substr(first,last-first+1);
        if(!argument.empty()&&argument!=L"--woodland-candidate"&&argument!=L"--workshop-roof-candidate"&&argument!=L"--shoulder-grove-candidate"&&argument!=L"--cliff-world-candidate"&&argument!=L"--cliff-relief-candidate"&&argument!=L"--forest-surface-candidate"&&argument!=L"--valley-world-candidate"&&argument!=L"--valley-terrace-candidate"&&argument!=Mistwatch::ForestPlayable::Argument)throw std::runtime_error("WOODLAND_COMMAND_LINE_INVALID");
        const bool groveCandidate=argument==Mistwatch::ForestPlayable::Argument;
        const bool valleyCandidate=argument==L"--valley-world-candidate";
        const bool terraceCandidate=argument==L"--valley-terrace-candidate";
        const bool surfaceCandidate=argument==L"--forest-surface-candidate";
        const bool manualCandidate=!argument.empty()&&!surfaceCandidate&&!valleyCandidate&&!terraceCandidate&&!groveCandidate;
        const bool reliefCandidate=argument==L"--cliff-relief-candidate";
        const bool cliffCandidate=argument==L"--cliff-world-candidate"||reliefCandidate;
        const bool shoulderCandidate=argument==L"--shoulder-grove-candidate";
        const bool roofCandidate=argument==L"--workshop-roof-candidate"||shoulderCandidate;
        EngineConfig config{L"Aevrynd - Mistwatch",1280,720};
        if(manualCandidate)config.title=L"Aevrynd - Mistwatch [Woodland candidate WIP]";
        if(roofCandidate)config.title=L"Aevrynd - Mistwatch [Workshop roof candidate WIP]";
        if(shoulderCandidate)config.title=L"Aevrynd - Mistwatch [Shoulder grove candidate WIP]";
        if(cliffCandidate)config.title=L"Aevrynd - Mistwatch [Cliff world candidate WIP]";
        if(reliefCandidate)config.title=L"Aevrynd - Mistwatch [Cliff relief candidate WIP]";
        if(surfaceCandidate)config.title=L"Aevrynd - Mistwatch [Forest surface candidate WIP]";
        if(valleyCandidate)config.title=L"Aevrynd - Mistwatch [Valley candidate WIP - separate saves]";
        if(terraceCandidate)config.title=L"Aevrynd - Mistwatch [Authored valley terrace WIP - separate saves]";
        if(groveCandidate)config.title=L"Aevrynd - Mistwatch [Forest grove WIP - separate saves]";
        wchar_t costProbe[8]{};
        const auto costCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_REPLAY_COST_PROBE",costProbe,8);
        if(costCount&&(costCount!=1||costProbe[0]!=L'1'))throw std::runtime_error("Invalid frame cost probe option");
        wchar_t captureCost[8]{};
        const auto captureCostCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_COST_PROBE",captureCost,8);
        if(captureCostCount&&(captureCostCount!=1||captureCost[0]!=L'1'||costCount))throw std::runtime_error("Invalid capture frame cost probe option");
        config.enableFrameCostProbe=costCount==1||captureCostCount==1;
        wchar_t windowOption[32]{};
        const auto windowCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_REPLAY_WINDOW",windowOption,32);
        if(windowCount>=32)throw std::runtime_error("Invalid replay window option length");
        const auto replayCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_INPUT_REPLAY",nullptr,0);
        const auto captureCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE",nullptr,0);
        config.windowMode=DiagnosticWindow3D::SelectMode(std::wstring_view(windowOption,windowCount),replayCount>0,config.enableFrameCostProbe,captureCount>0);
        wchar_t presentOption[32]{};
        const auto presentCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_REPLAY_PRESENT",presentOption,32);
        if(presentCount>=32)throw std::runtime_error("Invalid replay present option length");
        config.enableUnsynchronizedDiagnosticPresent=DiagnosticWindow3D::SelectUnsynchronizedPresent(std::wstring_view(presentOption,presentCount),config.windowMode,replayCount>0,config.enableFrameCostProbe,captureCount>0);
        wchar_t capturePresent[32]{};
        const auto capturePresentCount=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_CAPTURE_PRESENT",capturePresent,32);
        if(capturePresentCount>=32)throw std::runtime_error("Invalid capture presentation option length");
        const auto exactFlag=[](const wchar_t* name){wchar_t value[3]{};return GetEnvironmentVariableW(name,value,3)==1&&value[0]==L'1';};
        if(Mistwatch::SelectCaptureBenchmarkPresentation(std::wstring_view(capturePresent,capturePresentCount),captureCount>0,captureCostCount==1,
            exactFlag(L"AEVRYND_MISTWATCH_BENCHMARK"),exactFlag(L"AEVRYND_MISTWATCH_CAPTURE_FRAME_CAMERA"),replayCount>0,windowCount>0,presentCount>0)){
            config.windowMode=EngineWindowMode3D::OffscreenNoActivateDiagnostic;
            config.enableUnsynchronizedDiagnosticPresent=true;
        }
        wchar_t manualSmokeOutput[32768]{};const auto manualSmokeLength=GetEnvironmentVariableW(L"AEVRYND_MISTWATCH_MANUAL_SMOKE_OUTPUT",manualSmokeOutput,32768);
        if(manualSmokeLength){
            const std::filesystem::path path(manualSmokeOutput);
            if(manualSmokeLength>=32768||!groveCandidate||replayCount||captureCount||windowCount||presentCount||captureCostCount||costCount||!path.is_absolute()||std::filesystem::exists(path))throw std::runtime_error("MANUAL_SMOKE_REQUIRES_FRESH_ISOLATED_MANUAL_FOREST_CONTEXT");
            config.windowMode=EngineWindowMode3D::OffscreenNoActivateDiagnostic;config.enableUnsynchronizedDiagnosticPresent=false;
        }
        Engine engine(config);
        return engine.Run(std::make_unique<MistwatchGame>(manualCandidate,roofCandidate,shoulderCandidate,cliffCandidate,reliefCandidate,surfaceCandidate,valleyCandidate,terraceCandidate,groveCandidate),instance,show);
    } catch(const std::exception& error) {
        std::ofstream log("mistwatch-startup-error.txt");
        log<<error.what()<<'\n';
        return 1;
    }
}
