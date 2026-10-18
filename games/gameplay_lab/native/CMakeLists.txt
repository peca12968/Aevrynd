add_executable(GameplayLab WIN32 app.manifest src/main.cpp src/GameplayLabGame.cpp src/GameplayCottageEntry3D.cpp src/GameplayProductionHdrSidecar3D.cpp src/GameplayFirstPersonViewmodel3D.cpp src/GameplayContactEvidence3D.cpp src/GameplayRules3D.cpp src/GameplayCombat3D.cpp src/GameplayAnimation3D.cpp src/ObjectPaletteAdapter3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp src/GameplayForestGroundCoverMesh3D.cpp src/GameplayObjectPaletteUi3D.cpp src/ShowcaseRenderAssetBindings3D.cpp src/GameplayHlodWorldRuntime3D.cpp src/GameplayCameraOcclusionPolicy3D.cpp src/GameplayCameraProductionBuilder3D.cpp src/GameplayOccluderMask3D.cpp tests/ux_replay/GameplayUxReplayWin32.cpp tests/ux_replay/GameplayUxReplayProduction.cpp)
add_custom_target(GameplayForestTerrainBindingTest
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayForestTerrainBindingSmoke.ps1" -SourceRoot "${CMAKE_SOURCE_DIR}" -AssetRoot "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets"
  VERBATIM)
add_executable(GameplayGrassCommunityMesh3DTest tests/GameplayGrassCommunityMesh3DSmoke.cpp)
target_include_directories(GameplayGrassCommunityMesh3DTest PRIVATE include)
target_compile_options(GameplayGrassCommunityMesh3DTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(GameplayGrassCommunityMesh3DTest PRIVATE AevryndEngine)
add_test(NAME GameplayGrassCommunityMesh3DSmoke COMMAND GameplayGrassCommunityMesh3DTest)
add_executable(GameplayForestGroundCoverMesh3DTest tests/GameplayForestGroundCoverMesh3DSmoke.cpp src/GameplayForestGroundCoverMesh3D.cpp)
target_include_directories(GameplayForestGroundCoverMesh3DTest PRIVATE include)
target_compile_options(GameplayForestGroundCoverMesh3DTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(GameplayForestGroundCoverMesh3DTest PRIVATE AevryndEngine)
add_test(NAME GameplayForestGroundCoverMesh3DSmoke COMMAND GameplayForestGroundCoverMesh3DTest)
add_executable(GameplayForestTrial3DTest tests/GameplayForestTrial3DSmoke.cpp)
target_include_directories(GameplayForestTrial3DTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(GameplayForestTrial3DTest PRIVATE cxx_std_20)
target_compile_options(GameplayForestTrial3DTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME GameplayForestTrial3DTest COMMAND GameplayForestTrial3DTest)
set_property(TEST GameplayForestTrial3DTest APPEND PROPERTY LABELS "release-gates")
add_executable(GameplayCottageEntry3DTest tests/GameplayCottageEntry3DSmoke.cpp src/GameplayCottageEntry3D.cpp)
target_include_directories(GameplayCottageEntry3DTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(GameplayCottageEntry3DTest PRIVATE cxx_std_20)
target_compile_options(GameplayCottageEntry3DTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(GameplayCottageEntry3DTest PRIVATE AevryndEngine)
add_test(NAME GameplayCottageEntry3DTest COMMAND GameplayCottageEntry3DTest)
set_property(TEST GameplayCottageEntry3DTest APPEND PROPERTY LABELS "release-gates")
add_executable(GameplayCottageInterior3DTest tests/GameplayCottageInterior3DSmoke.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp)
target_include_directories(GameplayCottageInterior3DTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(GameplayCottageInterior3DTest PRIVATE cxx_std_20)
target_compile_options(GameplayCottageInterior3DTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(GameplayCottageInterior3DTest PRIVATE AevryndEngine)
add_test(NAME GameplayCottageInterior3DTest COMMAND GameplayCottageInterior3DTest)
set_property(TEST GameplayCottageInterior3DTest APPEND PROPERTY LABELS "release-gates")
add_executable(GameplayBuildingIdentities3DTest tests/GameplayBuildingIdentities3DSmoke.cpp)
target_include_directories(GameplayBuildingIdentities3DTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(GameplayBuildingIdentities3DTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME GameplayBuildingIdentities3DSmoke COMMAND GameplayBuildingIdentities3DTest)
add_executable(GameplayProductionHdrSidecarValidator3D tools/GameplayProductionHdrSidecarValidator3D.cpp src/GameplayProductionHdrSidecar3D.cpp)
target_include_directories(GameplayProductionHdrSidecarValidator3D PRIVATE include)
target_compile_options(GameplayProductionHdrSidecarValidator3D PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(GameplayProductionHdrSidecarValidator3D PRIVATE AevryndEngine bcrypt)
set_target_properties(GameplayProductionHdrSidecarValidator3D PROPERTIES OUTPUT_NAME "Aevrynd-Gameplay-Hdr-Validator")
add_dependencies(GameplayLab GameplayProductionHdrSidecarValidator3D)
# Gameplay 的 build fingerprint 由一份排序、可重算、fail-closed 的完整 source inventory 產生。
file(GLOB_RECURSE GAMEPLAY_SOURCE_INVENTORY_FILES CONFIGURE_DEPENDS RELATIVE "${CMAKE_SOURCE_DIR}"
  "${CMAKE_SOURCE_DIR}/engine/native/include/*.h"
  "${CMAKE_SOURCE_DIR}/engine/native/include/*.hpp"
  "${CMAKE_SOURCE_DIR}/engine/native/include/*.cpp"
  "${CMAKE_SOURCE_DIR}/engine/native/src/*.h"
  "${CMAKE_SOURCE_DIR}/engine/native/src/*.hpp"
  "${CMAKE_SOURCE_DIR}/engine/native/src/*.cpp"
  "${CMAKE_SOURCE_DIR}/engine/native/shaders/*.hlsl"
  "${CMAKE_SOURCE_DIR}/engine/native/shaders/*.hlsli"
  "${CMAKE_SOURCE_DIR}/engine/native/tests/*.h"
  "${CMAKE_SOURCE_DIR}/engine/native/tests/*.hpp"
  "${CMAKE_SOURCE_DIR}/engine/native/tests/*.cpp"
  "${CMAKE_SOURCE_DIR}/engine/native/tests/*.ps1"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/include/*.h"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/include/*.hpp"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/include/*.cpp"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/src/*.h"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/src/*.hpp"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/src/*.cpp"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tests/*.h"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tests/*.hpp"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tests/*.cpp"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tests/*.ps1"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tools/*.h"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tools/*.hpp"
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tools/*.cpp")
list(APPEND GAMEPLAY_SOURCE_INVENTORY_FILES
  CMakeLists.txt
  cmake/AevryndReleaseGateAssets.cmake
  engine/native/CMakeLists.txt
  games/gameplay_lab/game.json
  games/gameplay_lab/native/CMakeLists.txt
  games/gameplay_lab/native/app.manifest
  engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json)
list(REMOVE_DUPLICATES GAMEPLAY_SOURCE_INVENTORY_FILES)
list(SORT GAMEPLAY_SOURCE_INVENTORY_FILES)
set(GAMEPLAY_SOURCE_INVENTORY_CONTENT "")
foreach(GAMEPLAY_SOURCE_FILE IN LISTS GAMEPLAY_SOURCE_INVENTORY_FILES)
  if(NOT EXISTS "${CMAKE_SOURCE_DIR}/${GAMEPLAY_SOURCE_FILE}")
    message(FATAL_ERROR "Gameplay source inventory file missing: ${GAMEPLAY_SOURCE_FILE}")
  endif()
  file(SHA256 "${CMAKE_SOURCE_DIR}/${GAMEPLAY_SOURCE_FILE}" GAMEPLAY_SOURCE_FILE_SHA)
  string(TOLOWER "${GAMEPLAY_SOURCE_FILE_SHA}" GAMEPLAY_SOURCE_FILE_SHA)
  string(APPEND GAMEPLAY_SOURCE_INVENTORY_CONTENT "${GAMEPLAY_SOURCE_FILE}:${GAMEPLAY_SOURCE_FILE_SHA}\n")
endforeach()
string(SHA256 CHARACTER_PRESENTATION_FINGERPRINT "${GAMEPLAY_SOURCE_INVENTORY_CONTENT}")
set(GAMEPLAY_SOURCE_INVENTORY_PATH "${CMAKE_CURRENT_BINARY_DIR}/GameplaySourceInventory.txt")
file(WRITE "${GAMEPLAY_SOURCE_INVENTORY_PATH}" "${GAMEPLAY_SOURCE_INVENTORY_CONTENT}")
target_sources(GameplayLab PRIVATE src/GameplayFirstPersonPose3D.cpp)
target_include_directories(GameplayLab PRIVATE include tests/ux_replay)
target_compile_options(GameplayLab PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(GameplayLab PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX AEVRYND_CHARACTER_BUILD_FINGERPRINT="${CHARACTER_PRESENTATION_FINGERPRINT}")
target_link_libraries(GameplayLab PRIVATE AevryndEngine Ole32)
set_target_properties(GameplayLab PROPERTIES OUTPUT_NAME "Aevrynd-Gameplay-Lab" VS_DPI_AWARE "PerMonitor")
add_custom_command(TARGET GameplayLab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GAMEPLAY_SOURCE_INVENTORY_PATH}"
    "$<TARGET_FILE_DIR:GameplayLab>/GameplaySourceInventory.txt")
if(TARGET AevryndReleaseGateAssets)
  add_dependencies(GameplayLab AevryndReleaseGateAssets)
else()
foreach(CHARACTER_ASSET realistic-female realistic-male)
  add_custom_command(TARGET GameplayLab POST_BUILD COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GameplayLab>/assets/model-packs/${CHARACTER_ASSET}" COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/games/character_lab/assets/model-packs/${CHARACTER_ASSET}/${CHARACTER_ASSET}.aeskin" "$<TARGET_FILE_DIR:GameplayLab>/assets/model-packs/${CHARACTER_ASSET}/${CHARACTER_ASSET}.aeskin")
endforeach()
add_custom_command(TARGET GameplayLab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/games/character_lab/assets/material-packs"
    "$<TARGET_FILE_DIR:GameplayLab>/assets/material-packs"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/games/character_lab/assets/model-packs/CC0-HUMAN-BASE-MESHES.md"
    "$<TARGET_FILE_DIR:GameplayLab>/assets/model-packs/CC0-HUMAN-BASE-MESHES.md")
add_custom_command(TARGET GameplayLab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/engine/assets/material-packs"
    "$<TARGET_FILE_DIR:GameplayLab>/assets/environment-material-packs"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/engine/assets/model-packs"
    "$<TARGET_FILE_DIR:GameplayLab>/assets/environment-model-packs")
endif()

# 只封裝 authoritative 12-asset production manifest 引用的 96 個 GPU payload；不帶入 GLB、JPG、HDR 或烘焙收據。
set(GAMEPLAY_PHOTOREAL_COOKED_ROOT "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked")
set(GAMEPLAY_PHOTOREAL_MANIFEST "${GAMEPLAY_PHOTOREAL_COOKED_ROOT}/runtime-render-manifest.json")
file(READ "${GAMEPLAY_PHOTOREAL_MANIFEST}" GAMEPLAY_PHOTOREAL_MANIFEST_JSON)
string(JSON GAMEPLAY_PHOTOREAL_FILE_COUNT LENGTH "${GAMEPLAY_PHOTOREAL_MANIFEST_JSON}" files)
if(NOT GAMEPLAY_PHOTOREAL_FILE_COUNT EQUAL 96)
  message(FATAL_ERROR "Gameplay 12-asset photoreal package requires exactly 96 manifest payloads")
endif()
if(NOT TARGET AevryndReleaseGateAssets)
add_custom_command(TARGET GameplayLab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GameplayLab>/assets/photoreal-cooked"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GAMEPLAY_PHOTOREAL_MANIFEST}" "$<TARGET_FILE_DIR:GameplayLab>/assets/photoreal-cooked/runtime-render-manifest.json")
math(EXPR GAMEPLAY_PHOTOREAL_FILE_LAST "${GAMEPLAY_PHOTOREAL_FILE_COUNT} - 1")
foreach(GAMEPLAY_PHOTOREAL_FILE_INDEX RANGE 0 ${GAMEPLAY_PHOTOREAL_FILE_LAST})
  string(JSON GAMEPLAY_PHOTOREAL_FILE_PATH GET "${GAMEPLAY_PHOTOREAL_MANIFEST_JSON}" files ${GAMEPLAY_PHOTOREAL_FILE_INDEX} path)
  get_filename_component(GAMEPLAY_PHOTOREAL_FILE_DIRECTORY "${GAMEPLAY_PHOTOREAL_FILE_PATH}" DIRECTORY)
  add_custom_command(TARGET GameplayLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:GameplayLab>/assets/photoreal-cooked/${GAMEPLAY_PHOTOREAL_FILE_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GAMEPLAY_PHOTOREAL_COOKED_ROOT}/${GAMEPLAY_PHOTOREAL_FILE_PATH}" "$<TARGET_FILE_DIR:GameplayLab>/assets/photoreal-cooked/${GAMEPLAY_PHOTOREAL_FILE_PATH}")
endforeach()
endif()

add_executable(ShowcaseRenderAssetBindings3DTest tests/ShowcaseRenderAssetBindings3DSmoke.cpp src/ShowcaseRenderAssetBindings3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp)
target_include_directories(ShowcaseRenderAssetBindings3DTest PRIVATE include)
target_compile_options(ShowcaseRenderAssetBindings3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(ShowcaseRenderAssetBindings3DTest PRIVATE AevryndEngine)
add_test(NAME ShowcaseRenderAssetBindings3DTest COMMAND ShowcaseRenderAssetBindings3DTest "${CMAKE_SOURCE_DIR}/engine/assets/asset-catalogs/realistic-cc0-assets.json" "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/runtime-asset-manifest.json")
add_executable(ShowcaseAssetTransform3DTest tests/ShowcaseAssetTransformSmoke.cpp src/ShowcaseRenderAssetBindings3D.cpp)
target_include_directories(ShowcaseAssetTransform3DTest PRIVATE include)
target_compile_options(ShowcaseAssetTransform3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(ShowcaseAssetTransform3DTest PRIVATE AevryndEngine)
add_test(NAME ShowcaseAssetTransform3DTest COMMAND ShowcaseAssetTransform3DTest)

add_executable(GameplayUxReplayWin32Test tests/ux_replay/GameplayUxReplayWin32Smoke.cpp tests/ux_replay/GameplayUxReplayWin32.cpp src/GameplayRules3D.cpp)
target_include_directories(GameplayUxReplayWin32Test PRIVATE include tests/ux_replay)
target_compile_options(GameplayUxReplayWin32Test PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayUxReplayWin32Test PRIVATE AevryndEngine)
add_test(NAME GameplayUxReplayWin32Test COMMAND GameplayUxReplayWin32Test)
add_executable(GameplayLabRulesTest tests/GameplayLabRulesSmoke.cpp src/GameplayFirstPersonViewmodel3D.cpp src/GameplayRules3D.cpp src/GameplayContactEvidence3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp src/GameplayOccluderMask3D.cpp)
target_include_directories(GameplayLabRulesTest PRIVATE include)
target_compile_definitions(GameplayLabRulesTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_compile_options(GameplayLabRulesTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayLabRulesTest PRIVATE AevryndEngine)
add_test(NAME GameplayLabRulesTest COMMAND GameplayLabRulesTest)
set_property(TEST GameplayLabRulesTest APPEND PROPERTY LABELS "release-gates")

add_executable(GameplayFirstPersonPoseTest tests/GameplayFirstPersonPoseSmoke.cpp src/GameplayFirstPersonPose3D.cpp src/GameplayAnimation3D.cpp)
add_executable(GameplayClipSegmentVisibilityTest tests/GameplayClipSegmentVisibilitySmoke.cpp)
target_include_directories(GameplayClipSegmentVisibilityTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(GameplayClipSegmentVisibilityTest PRIVATE cxx_std_20)
target_compile_options(GameplayClipSegmentVisibilityTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME GameplayClipSegmentVisibilityTest COMMAND GameplayClipSegmentVisibilityTest)
set_property(TEST GameplayClipSegmentVisibilityTest APPEND PROPERTY LABELS "release-gates")
target_include_directories(GameplayFirstPersonPoseTest PRIVATE include)
target_compile_options(GameplayFirstPersonPoseTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayFirstPersonPoseTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(GameplayFirstPersonPoseTest PRIVATE AevryndEngine)
add_test(NAME GameplayFirstPersonPoseTest COMMAND GameplayFirstPersonPoseTest)
set_property(TEST GameplayFirstPersonPoseTest APPEND PROPERTY LABELS "release-gates")
add_executable(GameplayShowcaseEnvironmentTest tests/GameplayShowcaseEnvironment3DSmoke.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp)
target_include_directories(GameplayShowcaseEnvironmentTest PRIVATE include)
target_compile_options(GameplayShowcaseEnvironmentTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayShowcaseEnvironmentTest PRIVATE AevryndEngine)
add_test(NAME GameplayShowcaseEnvironmentTest COMMAND GameplayShowcaseEnvironmentTest)
add_executable(ShowcaseLandEcology3DTest tests/ShowcaseLandEcology3DSmoke.cpp src/ShowcaseLandEcology3D.cpp)
target_include_directories(ShowcaseLandEcology3DTest PRIVATE include)
target_compile_options(ShowcaseLandEcology3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(ShowcaseLandEcology3DTest PRIVATE AevryndEngine)
add_test(NAME ShowcaseLandEcology3DTest COMMAND ShowcaseLandEcology3DTest)
add_executable(GameplayCameraOcclusionPolicyTest tests/GameplayCameraOcclusionPolicySmoke.cpp src/GameplayCameraOcclusionPolicy3D.cpp)
target_include_directories(GameplayCameraOcclusionPolicyTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(GameplayCameraOcclusionPolicyTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME GameplayCameraOcclusionPolicyTest COMMAND GameplayCameraOcclusionPolicyTest)
add_executable(GameplayCameraProductionIntegrationTest tests/GameplayCameraProductionIntegrationSmoke.cpp src/GameplayFirstPersonViewmodel3D.cpp src/GameplayAnimation3D.cpp src/GameplayCameraOcclusionPolicy3D.cpp src/GameplayCameraProductionBuilder3D.cpp src/GameplayOccluderMask3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp)
target_include_directories(GameplayCameraProductionIntegrationTest PRIVATE include)
target_compile_options(GameplayCameraProductionIntegrationTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayCameraProductionIntegrationTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(GameplayCameraProductionIntegrationTest PRIVATE AevryndEngine)
add_test(NAME GameplayCameraProductionIntegrationTest COMMAND GameplayCameraProductionIntegrationTest)
set_property(TEST GameplayCameraProductionIntegrationTest APPEND PROPERTY LABELS "release-gates")
add_executable(GameplayHlodCameraTransitionD3D12Test tests/GameplayHlodCameraTransitionD3D12Smoke.cpp src/GameplayCameraOcclusionPolicy3D.cpp src/GameplayCameraProductionBuilder3D.cpp src/GameplayOccluderMask3D.cpp)
target_include_directories(GameplayHlodCameraTransitionD3D12Test PRIVATE include)
target_compile_options(GameplayHlodCameraTransitionD3D12Test PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayHlodCameraTransitionD3D12Test PRIVATE AEVRYND_TRANSITION_PIXEL_RECEIPT=L"${CMAKE_CURRENT_BINARY_DIR}/gameplay-hlod-camera-transition-pixel-receipt.json")
target_link_libraries(GameplayHlodCameraTransitionD3D12Test PRIVATE AevryndEngine)
add_test(NAME GameplayHlodCameraTransitionD3D12Test COMMAND GameplayHlodCameraTransitionD3D12Test)
add_executable(GameplayHlodWorldWorkloadReadinessAudit tests/GameplayHlodWorldWorkloadReadinessAudit.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp)
target_include_directories(GameplayHlodWorldWorkloadReadinessAudit PRIVATE include)
target_compile_options(GameplayHlodWorldWorkloadReadinessAudit PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayHlodWorldWorkloadReadinessAudit PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}" AEVRYND_HLOD_WORKLOAD_READINESS_RECEIPT=L"${CMAKE_CURRENT_BINARY_DIR}/gameplay-hlod-world-workload-readiness.json")
target_link_libraries(GameplayHlodWorldWorkloadReadinessAudit PRIVATE AevryndEngine)
add_test(NAME GameplayHlodWorldWorkloadReadinessAudit COMMAND GameplayHlodWorldWorkloadReadinessAudit)
set_tests_properties(GameplayHlodWorldWorkloadReadinessAudit PROPERTIES LABELS "HLOD;NO_GO_EVIDENCE")
add_executable(GameplayHlodWorldRuntimeD3D12Test WIN32 tests/GameplayHlodWorldRuntimeD3D12Smoke.cpp src/GameplayHlodWorldRuntime3D.cpp src/ShowcaseRenderAssetBindings3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/ShowcaseLandEcology3D.cpp)
target_include_directories(GameplayHlodWorldRuntimeD3D12Test PRIVATE include)
target_compile_options(GameplayHlodWorldRuntimeD3D12Test PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayHlodWorldRuntimeD3D12Test PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}" AEVRYND_GAMEPLAY_HLOD_RUNTIME_RECEIPT=L"${CMAKE_CURRENT_BINARY_DIR}/gameplay-hlod-world-runtime-receipt.json")
target_link_libraries(GameplayHlodWorldRuntimeD3D12Test PRIVATE AevryndEngine)
add_test(NAME GameplayHlodWorldRuntimeD3D12NormalTest COMMAND ${CMAKE_COMMAND} -E env AEVRYND_EXPECT_GPU_VALIDATION_MODE=normal powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/engine/native/tools/RunWindowsGuiTest.ps1" -Executable "$<TARGET_FILE:GameplayHlodWorldRuntimeD3D12Test>")
add_test(NAME GameplayHlodWorldRuntimeD3D12GpuValidationTest COMMAND ${CMAKE_COMMAND} -E env AEVRYND_GPU_VALIDATION=1 AEVRYND_EXPECT_GPU_VALIDATION_MODE=gbv powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_SOURCE_DIR}/engine/native/tools/RunWindowsGuiTest.ps1" -Executable "$<TARGET_FILE:GameplayHlodWorldRuntimeD3D12Test>")
set_tests_properties(GameplayHlodWorldRuntimeD3D12NormalTest GameplayHlodWorldRuntimeD3D12GpuValidationTest PROPERTIES LABELS "HLOD;GAMEPLAY_PRODUCTION" TIMEOUT 180 RUN_SERIAL TRUE)
add_executable(GameplayOccluderMask3DTest tests/GameplayOccluderMaskSmoke.cpp src/GameplayOccluderMask3D.cpp)
target_include_directories(GameplayOccluderMask3DTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(GameplayOccluderMask3DTest PRIVATE /W4 /permissive- /utf-8)
add_test(NAME GameplayOccluderMask3DTest COMMAND GameplayOccluderMask3DTest)
add_executable(GameplayProductionInputReplayTest tests/GameplayProductionInputReplaySmoke.cpp src/GameplayRules3D.cpp)
target_include_directories(GameplayProductionInputReplayTest PRIVATE include tests)
target_compile_options(GameplayProductionInputReplayTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayProductionInputReplayTest PRIVATE AevryndEngine)
add_test(NAME GameplayProductionInputReplayTest COMMAND GameplayProductionInputReplayTest)
add_executable(GameplayObjectPaletteUiTest tests/GameplayObjectPaletteUi3DSmoke.cpp src/GameplayObjectPaletteUi3D.cpp src/ObjectPaletteAdapter3D.cpp src/GameplayRules3D.cpp)
target_include_directories(GameplayObjectPaletteUiTest PRIVATE include)
target_compile_options(GameplayObjectPaletteUiTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayObjectPaletteUiTest PRIVATE AevryndEngine)
add_test(NAME GameplayObjectPaletteUiTest COMMAND GameplayObjectPaletteUiTest)
add_executable(GameplayProductionInputPaletteTest tests/GameplayProductionInputPaletteSmoke.cpp src/GameplayRules3D.cpp src/ObjectPaletteAdapter3D.cpp)
target_include_directories(GameplayProductionInputPaletteTest PRIVATE include)
target_compile_options(GameplayProductionInputPaletteTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayProductionInputPaletteTest PRIVATE AevryndEngine)
add_test(NAME GameplayProductionInputPaletteTest COMMAND GameplayProductionInputPaletteTest)
add_executable(GameplayProductionHdrSidecar3DTest tests/GameplayProductionHdrSidecar3DSmoke.cpp src/GameplayProductionHdrSidecar3D.cpp)
target_include_directories(GameplayProductionHdrSidecar3DTest PRIVATE include)
target_compile_options(GameplayProductionHdrSidecar3DTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayProductionHdrSidecar3DTest PRIVATE AEVRYND_INTERNAL_RENDERER_TEST_HOOKS=0)
target_link_libraries(GameplayProductionHdrSidecar3DTest PRIVATE AevryndEngine)
add_test(NAME GameplayProductionHdrSidecar3DTest COMMAND GameplayProductionHdrSidecar3DTest)
add_executable(GameplayLabBenchmark tests/GameplayLabBenchmark.cpp src/GameplayRules3D.cpp)
target_include_directories(GameplayLabBenchmark PRIVATE include)
target_compile_options(GameplayLabBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayLabBenchmark PRIVATE AevryndEngine)
add_test(NAME GameplayLabBenchmark COMMAND GameplayLabBenchmark)
set_tests_properties(GameplayLabBenchmark PROPERTIES TIMEOUT 30)
add_executable(GameplayObjectIdentityContractProbe tests/GameplayObjectIdentityContractProbe.cpp)
target_include_directories(GameplayObjectIdentityContractProbe PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(GameplayObjectIdentityContractProbe PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayObjectIdentityContractProbe PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX AEVRYND_ENGINE_VERSION="${PROJECT_VERSION}" AEVRYND_STREAMLINE_AVAILABLE=$<BOOL:${AEVRYND_STREAMLINE_AVAILABLE}> AEVRYND_INTERNAL_RENDERER_TEST_HOOKS=$<BOOL:${AEVRYND_INTERNAL_RENDERER_TEST_HOOKS}> AEVRYND_CHARACTER_BUILD_FINGERPRINT="${CHARACTER_PRESENTATION_FINGERPRINT}")
set_target_properties(GameplayObjectIdentityContractProbe PROPERTIES OUTPUT_NAME "Aevrynd-Gameplay-Object-Identity-Contract")
add_test(NAME GameplayObjectIdentityContractProbeTest COMMAND GameplayObjectIdentityContractProbe)
add_test(NAME GameplayLabCaptureEvidenceTemporalSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureEvidenceSmoke.ps1" -Section Temporal -IdentityContractProbe "$<TARGET_FILE:GameplayObjectIdentityContractProbe>")
add_test(NAME GameplayLabCaptureEvidenceReceiptSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureEvidenceSmoke.ps1" -Section Receipt -IdentityContractProbe "$<TARGET_FILE:GameplayObjectIdentityContractProbe>")
set_tests_properties(GameplayLabCaptureEvidenceTemporalSmoke GameplayLabCaptureEvidenceReceiptSmoke PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
foreach(warmup IN ITEMS 3 4 5)
  add_test(NAME GameplayThirdDayTemporalFixtureWarmup${warmup}Smoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayThirdDayTemporalFixtureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>" -EvidenceHelper "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureEvidence.ps1" -Warmup ${warmup})
  set_tests_properties(GameplayThirdDayTemporalFixtureWarmup${warmup}Smoke PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endforeach()
add_test(NAME GameplaySourceInventorySmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplaySourceInventorySmoke.ps1" -SourceRoot "${CMAKE_SOURCE_DIR}" -InventoryPath "${GAMEPLAY_SOURCE_INVENTORY_PATH}")
set_tests_properties(GameplaySourceInventorySmoke PROPERTIES TIMEOUT 30)
add_test(NAME GameplayLabCaptureSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>" -IdentityContractProbe "$<TARGET_FILE:GameplayObjectIdentityContractProbe>" -ProcessTimeoutSeconds 120)
set_tests_properties(GameplayLabCaptureSmoke PROPERTIES TIMEOUT 420)
add_test(NAME GameplayLabGpuValidationCaptureSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>" -IdentityContractProbe "$<TARGET_FILE:GameplayObjectIdentityContractProbe>" -GpuValidation -SingleRun -ProcessTimeoutSeconds 180)
set_tests_properties(GameplayLabGpuValidationCaptureSmoke PROPERTIES TIMEOUT 240)
set_property(TEST GameplayLabCaptureSmoke GameplayLabGpuValidationCaptureSmoke APPEND PROPERTY LABELS "visual-gates")
add_test(NAME GameplayLabCaptureCtestContractSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureCtestContractSmoke.ps1" -CTestFile "${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake")
set_tests_properties(GameplayLabCaptureCtestContractSmoke PROPERTIES TIMEOUT 10)
add_test(NAME GameplayLightingPhaseEvidenceSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLightingPhaseEvidenceSmoke.ps1")
set_tests_properties(GameplayLightingPhaseEvidenceSmoke PROPERTIES TIMEOUT 10 LABELS "visual-gates")
add_custom_target(GameplayR7VisualSidecarHostileChecks
  COMMAND ${CMAKE_COMMAND} -E env "LIB=" powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayR7VisualSidecarSmoke.ps1" -Producer "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayR7VisualSidecar.ps1"
  VERBATIM)
add_test(NAME GameplayR7VisualSidecarSmoke
  COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayR7VisualSidecarSmoke.ps1" -Producer "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayR7VisualSidecar.ps1")
set_tests_properties(GameplayR7VisualSidecarSmoke PROPERTIES TIMEOUT 60 LABELS "release-gates;visual-gates")
add_executable(GameplayLabCombatAnimationTest tests/GameplayCombatAnimationSmoke.cpp src/GameplayCombat3D.cpp src/GameplayAnimation3D.cpp)
target_include_directories(GameplayLabCombatAnimationTest PRIVATE include)
target_compile_options(GameplayLabCombatAnimationTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(GameplayLabCombatAnimationTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(GameplayLabCombatAnimationTest PRIVATE AevryndEngine)
add_test(NAME GameplayLabCombatAnimationTest COMMAND GameplayLabCombatAnimationTest)
set_property(TEST GameplayLabCombatAnimationTest APPEND PROPERTY LABELS "release-gates")
