cmake_minimum_required(VERSION 3.25)
project(Aevrynd VERSION 2.86.0 LANGUAGES CXX)
enable_testing()
option(AEVRYND_WATER_COMPONENT_DIAGNOSTICS "Build isolated water component capture diagnostics" OFF)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
option(AEVRYND_BUILD_MISTWATCH_SERVER_ONLY "Build the Mistwatch dedicated server and headless tests" OFF)
if(AEVRYND_BUILD_MISTWATCH_SERVER_ONLY)
  if(NOT WIN32)
    message(FATAL_ERROR "The Mistwatch dedicated server requires Windows Winsock and SQLite")
  endif()
  add_subdirectory(engine/native/headless)
  set(MISTWATCH_RUNTIME_LIBRARY AevryndReplicationCore)
  set(MISTWATCH_ENABLE_RIVERBANK_BATTLE ON)
  set(MISTWATCH_ENABLE_RIVERBANK_PROFILES ON)
  set(MISTWATCH_TEST_RIVERBANK_ENCOUNTER ON)
  add_subdirectory(games/mistwatch/mmorpg)
  return()
endif()

set(STREAMLINE_ROOT "${CMAKE_SOURCE_DIR}/engine/third_party/streamline" CACHE PATH "Local NVIDIA Streamline SDK root")
option(AEVRYND_ENABLE_STREAMLINE "Enable NVIDIA Streamline integration when the local SDK is available" ON)
option(AEVRYND_ENABLE_STREAMLINE_RUNTIME "Package optional signed Streamline runtime redistributables" OFF)
set(AEVRYND_STREAMLINE_RUNTIME_FILES sl.interposer.dll sl.common.dll sl.dlss.dll nvngx_dlss.dll NvLowLatencyVk.dll)
set(AEVRYND_STREAMLINE_NOTICE_FILES license.txt 3rd-party-licenses.md bin/x64/nvngx_dlss.license.txt bin/x64/reflex.license.txt)
if(AEVRYND_ENABLE_STREAMLINE_RUNTIME)
  if(NOT AEVRYND_ENABLE_STREAMLINE)
    message(FATAL_ERROR "Streamline runtime packaging requires AEVRYND_ENABLE_STREAMLINE=ON")
  endif()
  foreach(required IN LISTS AEVRYND_STREAMLINE_RUNTIME_FILES)
    if(NOT EXISTS "${STREAMLINE_ROOT}/bin/x64/${required}")
      message(FATAL_ERROR "Incomplete Streamline runtime package: missing ${required}")
    endif()
  endforeach()
  foreach(metadata IN LISTS AEVRYND_STREAMLINE_NOTICE_FILES)
    if(NOT EXISTS "${STREAMLINE_ROOT}/${metadata}")
      message(FATAL_ERROR "Streamline redistribution metadata is missing: ${metadata}")
    endif()
  endforeach()
  if(NOT EXISTS "${STREAMLINE_ROOT}/AEVRYND_REDISTRIBUTION_APPROVED.manifest")
    message(FATAL_ERROR "Streamline runtime redistribution is unresolved; missing AEVRYND_REDISTRIBUTION_APPROVED.manifest")
  endif()
endif()
function(aevrynd_package_streamline_runtime target)
  if(AEVRYND_ENABLE_STREAMLINE_RUNTIME)
    foreach(runtime_file IN LISTS AEVRYND_STREAMLINE_RUNTIME_FILES)
      add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${STREAMLINE_ROOT}/bin/x64/${runtime_file}" "$<TARGET_FILE_DIR:${target}>/${runtime_file}")
    endforeach()
    foreach(notice_file IN LISTS AEVRYND_STREAMLINE_NOTICE_FILES)
      get_filename_component(notice_name "${notice_file}" NAME)
      add_custom_command(TARGET ${target} POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:${target}>/licenses/streamline" COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${STREAMLINE_ROOT}/${notice_file}" "$<TARGET_FILE_DIR:${target}>/licenses/streamline/${notice_name}")
    endforeach()
  endif()
endfunction()
option(AEVRYND_STREAMLINE_PACKAGE_TEST_TARGET "Create the isolated Streamline packaging fixture target" OFF)
if(AEVRYND_STREAMLINE_PACKAGE_TEST_TARGET)
  file(WRITE "${CMAKE_BINARY_DIR}/streamline-package-fixture.cpp" "int main(){return 0;}\n")
  add_executable(AevryndStreamlinePackageFixture "${CMAKE_BINARY_DIR}/streamline-package-fixture.cpp")
  aevrynd_package_streamline_runtime(AevryndStreamlinePackageFixture)
  # 固定測試入口與完整建置共用唯一資產部署者，先於各遊戲設定。
  include("${CMAKE_SOURCE_DIR}/cmake/AevryndReleaseGateAssets.cmake")
endif()
if(WIN32)
  add_test(NAME AevryndStreamlineRuntimePackageGateTest COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
    "${CMAKE_SOURCE_DIR}/tools/ci/test-streamline-runtime-package.ps1" -SourceRoot "${CMAKE_SOURCE_DIR}" -Generator "${CMAKE_GENERATOR}")
  # Fresh fixture 會逐一重建所有缺件矩陣；依 234.303 秒實測保留有界裕度。
  set_tests_properties(AevryndStreamlineRuntimePackageGateTest PROPERTIES TIMEOUT 360)
  add_test(NAME AevryndStreamlineRuntimeOffOutputTest COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
    "${CMAKE_SOURCE_DIR}/tools/ci/test-streamline-runtime-off-output.ps1"
    -EngineLab "$<TARGET_FILE:EngineLab>" -FrontierWorld "$<TARGET_FILE:FrontierWorld>"
    -Riftwarden "$<TARGET_FILE:Riftwarden>" -CharacterLab "$<TARGET_FILE:CharacterLab>")
endif()
add_subdirectory(engine/native)
add_subdirectory(games/riftwarden/native)
add_subdirectory(games/engine_lab/native)
add_subdirectory(games/character_lab/native)
add_subdirectory(games/frontier_world/native)
add_subdirectory(games/gameplay_lab/native)
add_subdirectory(games/mistwatch/native)

# 受限遠端 worker 只允許建置這個既有固定 target；在 gate preset 下，
# 保留既有依賴，明列 release-gates 所需的 CPU 測試與資產。
# 固定聚合原生測試執行檔；純 PowerShell 契約測試沒有額外執行檔依賴。
# 不依賴 ALL_BUILD、不在建置階段執行測試，也不納入其他 GPU 測試入口。
if(AEVRYND_STREAMLINE_PACKAGE_TEST_TARGET)
  add_dependencies(AevryndStreamlinePackageFixture
    AevryndTerrainParallaxContractTest
    AevryndCharacterQueryWindow3DDefaultTest
    AevryndCharacterQueryWindow3DStrictTest
    MistwatchForestSurfaceProfileTest
    MistwatchReplayCostProbeTest
    AevryndExactTriangleContact3DTest
    AevryndOcclusionNormalTest
    AevryndTerrainRockVariationTest
    AevryndPhaseSkyDiffuseTest
    MistwatchAuthoredNativeExpectedTest
    MistwatchAuthoredWorldQualificationTest
    MistwatchBackpackFitCpuTest
    MistwatchBoulderDetailTest
    MistwatchEntranceAuthoredWorldTest
    MistwatchExplorerArtCpuTest
    MistwatchExplorerFaceArtCpuTest
    MistwatchExplorerNapeHairTest
    MistwatchExplorerSurfaceCpuTest
    MistwatchFallenWorldQualificationTest
    MistwatchForestCoverRetentionTest
    MistwatchForestGrassShapeTest
    MistwatchForestHabitatFloorTest
    MistwatchGrassCaptureReferenceTest
    MistwatchGroundForestWoodArtTest
    MistwatchHabitatLeafMaterialTest
    MistwatchHutMaterialCpuTest
    MistwatchInputReplayTest
    MistwatchLandscapeTwigDrawTest
    MistwatchLeafClusterTest
    MistwatchNettleAssetTest
    MistwatchPlayableArtProfileTest
    MistwatchPlayableFlowRulesTest1420
    MistwatchPlayableFlowSaveTest1420
    MistwatchPlayableFlowUiTest1436
    MistwatchPropArtCpuTest
    MistwatchQualificationRuntimeTest
    MistwatchRangerHutLiningCpuTest
    MistwatchRangerRoofBandCpuTest
    MistwatchRepairShelterBoardUvTest
    MistwatchRepairShelterDetailTest
    MistwatchRepairShelterRoofUvTest
    MistwatchRepairShelterVisualTest
    MistwatchRepairSignVisualTest
    MistwatchRepairToolsVisualTest
    MistwatchShelterWeatheredUvTest
    MistwatchShrubDensityQualificationTest
    MistwatchShrubOpenCrownTest
    MistwatchWorkshopFrameTest
    MistwatchWorkshopVisualTest
    MistwatchWorkshopWoodUvTest
      MistwatchSettlementSurfaceTest
      MistwatchPoseCouplingTest
      MistwatchSettlingRegressionTest
      MistwatchTurnBoundaryTest
      MistwatchLocomotionMotorTest
      MistwatchTuningInputRepeatTest
      MistwatchPlayerLocomotionTest
      MistwatchMotionCheckpointTest
      MistwatchMotionLoadTest
      MistwatchBankCoverageCandidateTest
      MistwatchFuseDialVisualTest
      MistwatchCameraQuietContractTest
      MistwatchBeaconWorldDetailTest
      MistwatchEncounterFeedbackTest
      MistwatchGeneratorDetailTest
      Mistwatch
      MistwatchSaveInputAudit
    AevryndCharacterDeathPresentation3DTest
    GameplayForestTrial3DTest
    GameplayCottageEntry3DTest
    GameplayCottageInterior3DTest
    AevryndFootPlacement3DTest
    AevryndSkinnedMaterialSidecar3DTest
    AevryndNativeMaterialRgba8Dds3DTest
    GameplayLabRulesTest
    GameplayLabCombatAnimationTest
    GameplayCameraProductionIntegrationTest
    AevryndCharacterPresentation3DTest
    AevryndFirstPersonArmsSkinnedMesh3DTest
    GameplayFirstPersonPoseTest
    GameplayClipSegmentVisibilityTest
    AevryndHlod3DTest
    AevryndHlodSceneECS3DTest
    AevryndHlodWorldPartitionReceiptTest
    AevryndScene3DTest
    AevryndScene3DBvhTest
    AevryndWorldOrigin3DTest
    AevryndMeshRaycastBvhTest
    AevryndCollisionWorldTest
    AevryndPhysicsWorld3DTest
    AevryndNavigation3DTest
    AevryndHeightField3DTest
    AevryndHeightFieldLineOfSight3DTest
    MistwatchEncounterMotionTest
    MistwatchEncounterSightTest
    MistwatchTrailLitterTest
    MistwatchShrubCanopyTest
    MistwatchRootStemTest
    MistwatchWorldProfileTest
    MistwatchFirLayoutTest
    MistwatchObjectiveGuideTest
    MistwatchSurveyMapTest
    MistwatchPickupFeedbackTest
    MistwatchPickupIntentTest
    MistwatchPickupIntentMainTest
    MistwatchCounterWindowCueTest
    MistwatchBoarHitReactionTest
    MistwatchForestFloorMaskTest
    AevryndMeshCollisionVoxel3DTest
    AevryndMeshCollisionGridSolid3DTest
    MistwatchCanopyCollisionTest
    MistwatchCanopyVolumeClearanceTest
    AevryndTerrainLayerCoverage3DTest
    AevryndWorldPartition3DFakeSourceTest
    AevryndWorldPartitionDeltaGateFakeTest
    MistwatchSupplyCrateTest
    MistwatchSupplyCrateBoardUvTest
    MistwatchSessionTest
    MistwatchRouteTest
    MistwatchRangerHutTest
    MistwatchHutWholeArtCpuTest
    MistwatchRangerDeskCpuTest
    MistwatchRelayCameraTest
    AevryndThirdPersonFraming3DTest
    AevryndThirdPersonCameraMotion3DTest
    MistwatchCameraMotionTest
    MistwatchCameraOrbitTest
    MistwatchCameraRouteContextTest
    # 固定遠端入口也必須產生近期操作回歸的執行檔；不改測試選集或門檻。
    MistwatchSlopeFootReachTest
    MistwatchWestCliffReplayTest
    MistwatchNearTurnReplayTest
    MistwatchRelayTrackingTest
    MistwatchCounterBoundaryTest
    MistwatchDashJumpHandoffTest
    MistwatchBrakingFootLeadTest
    MistwatchCameraRecenterTest
    MistwatchObjectivePointerTest
    MistwatchSaveGameTest
    MistwatchSaveFileTest
    MistwatchLoadPreparationTest
    MistwatchMiddleLayerTest
    MistwatchGroundMaterialTest
    MistwatchBankLayerCoverageTest
    MistwatchForestCoverTest
    MistwatchFernColoniesTest
    MistwatchBridgeTest
    MistwatchLandscapeTest
    MistwatchCameraTest)
endif()

# 河岸多人是獨立的開發入口，與既有單人驗收分開啟用。
option(AEVRYND_BUILD_MISTWATCH_MULTIPLAYER "Build the Mistwatch Riverbank multiplayer prototype" OFF)
if(AEVRYND_BUILD_MISTWATCH_MULTIPLAYER)
  set(MISTWATCH_ENABLE_RIVERBANK_BATTLE ON)
  set(MISTWATCH_ENABLE_RIVERBANK_PROFILES ON)
  set(MISTWATCH_TEST_RIVERBANK_ENCOUNTER ON)
  set(MISTWATCH_LOCAL_WORLD_AVATAR "${CMAKE_CURRENT_SOURCE_DIR}/games/mistwatch/mmorpg/assets/RiverbankExplorer.aeskin"
    CACHE FILEPATH "Verified Riverbank Explorer AESKIN asset; empty disables the alternate avatar")
  if(MISTWATCH_LOCAL_WORLD_AVATAR)
    file(SHA256 "${MISTWATCH_LOCAL_WORLD_AVATAR}" riverbank_avatar_sha)
    set(verified_riverbank_avatar_hashes
      "d91fd676b12027d6907fa3e45a8892e61ea7316506f081a9af9074fa3e715d7b"
      "f129fa70cbee1b7e27f990f60069b59915b9b948ac43740ecd92684fe005305b"
      "a1a1d2bd0290ba2ec01527bb6d7af10fe214514ca34a6230fab3cf874cfef42a"
      "013a9f3a37f0942652ebd79932cb33955987515ffd829f4a981add4aa7f40d2e"
      "03d30e0c5c050394391e9b18693b33616b67364ed87920eb92b8ea2a22ec5587")
    if(NOT riverbank_avatar_sha IN_LIST verified_riverbank_avatar_hashes)
      message(FATAL_ERROR "Riverbank Explorer asset hash differs from the verified export")
    endif()
    if(riverbank_avatar_sha STREQUAL "013a9f3a37f0942652ebd79932cb33955987515ffd829f4a981add4aa7f40d2e" OR
       riverbank_avatar_sha STREQUAL "03d30e0c5c050394391e9b18693b33616b67364ed87920eb92b8ea2a22ec5587")
      foreach(wardrobe_entry IN ITEMS
        "Guardian|d9e3b391ee44e852aa85cad40cb1f207acf1ef6e5d7d082d2b3f24a10c12d841"
        "Ranger|182f1baacdd1b7431aca9f49e579df150c392bdbffbb364cddc1ee77eeff5c6f"
        "Priest|0c215fa6a19b37ef02df4486262485ebc4f9df4262184d0e3da743582cb393de")
        string(REPLACE "|" ";" wardrobe_fields "${wardrobe_entry}")
        list(GET wardrobe_fields 0 wardrobe_role)
        list(GET wardrobe_fields 1 wardrobe_hash)
        set(wardrobe_path "${CMAKE_CURRENT_SOURCE_DIR}/games/mistwatch/mmorpg/assets/RiverbankExplorer-${wardrobe_role}.aeskin")
        file(SHA256 "${wardrobe_path}" wardrobe_actual)
        if(NOT wardrobe_actual STREQUAL wardrobe_hash)
          message(FATAL_ERROR "Riverbank ${wardrobe_role} wardrobe asset differs from the verified export")
        endif()
        list(APPEND MISTWATCH_RIVERBANK_WARDROBES "${wardrobe_path}")
      endforeach()
    endif()
    if(riverbank_avatar_sha STREQUAL "03d30e0c5c050394391e9b18693b33616b67364ed87920eb92b8ea2a22ec5587")
      foreach(hair_entry IN ITEMS
        "Ranger|fe27db7b117514aa5ce9fa26fa64a41b3cd3b7162d5153f56f076af14eafe6cd"
        "Priest|09eab6f46a2058c69a72b3090429a93b556d8efd1bacf921bf7ad161a901b3ec")
        string(REPLACE "|" ";" hair_fields "${hair_entry}")
        list(GET hair_fields 0 hair_role)
        list(GET hair_fields 1 hair_hash)
        set(hair_path "${CMAKE_CURRENT_SOURCE_DIR}/games/mistwatch/mmorpg/assets/RiverbankExplorer-Hair-${hair_role}.aeskin")
        file(SHA256 "${hair_path}" hair_actual)
        if(NOT hair_actual STREQUAL hair_hash)
          message(FATAL_ERROR "Riverbank ${hair_role} hair asset differs from the verified export")
        endif()
        list(APPEND MISTWATCH_RIVERBANK_HAIR_VARIANTS "${hair_path}")
      endforeach()
    endif()
  endif()
  add_subdirectory(games/mistwatch/mmorpg)
endif()

option(AEVRYND_BUILD_MISTWATCH_RIVERBANK_WIP "Build reversible riverbank gameplay WIP" OFF)
if(AEVRYND_BUILD_MISTWATCH_RIVERBANK_WIP OR AEVRYND_BUILD_MISTWATCH_MULTIPLAYER)
  add_subdirectory(games/mistwatch/riverbank)
endif()
