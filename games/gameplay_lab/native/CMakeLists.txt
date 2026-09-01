add_executable(GameplayLab WIN32 app.manifest src/main.cpp src/GameplayLabGame.cpp src/GameplayFirstPersonViewmodel3D.cpp src/GameplayContactEvidence3D.cpp src/GameplayRules3D.cpp src/GameplayCombat3D.cpp src/GameplayAnimation3D.cpp src/ObjectPaletteAdapter3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/GameplayObjectPaletteUi3D.cpp src/ShowcaseRenderAssetBindings3D.cpp src/GameplayCameraOcclusionPolicy3D.cpp src/GameplayCameraProductionBuilder3D.cpp src/GameplayOccluderMask3D.cpp tests/ux_replay/GameplayUxReplayWin32.cpp tests/ux_replay/GameplayUxReplayProduction.cpp)
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
  "${CMAKE_SOURCE_DIR}/games/gameplay_lab/native/tests/*.ps1")
list(APPEND GAMEPLAY_SOURCE_INVENTORY_FILES
  CMakeLists.txt
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
target_include_directories(GameplayLab PRIVATE include tests/ux_replay)
target_compile_options(GameplayLab PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(GameplayLab PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX AEVRYND_CHARACTER_BUILD_FINGERPRINT="${CHARACTER_PRESENTATION_FINGERPRINT}")
target_link_libraries(GameplayLab PRIVATE AevryndEngine Ole32)
set_target_properties(GameplayLab PROPERTIES OUTPUT_NAME "Aevrynd-Gameplay-Lab" VS_DPI_AWARE "PerMonitor")
add_custom_command(TARGET GameplayLab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${GAMEPLAY_SOURCE_INVENTORY_PATH}"
    "$<TARGET_FILE_DIR:GameplayLab>/GameplaySourceInventory.txt")
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

# 只封裝 authoritative runtime manifest 引用的 54 個 GPU payload；不帶入 GLB、JPG、HDR 或烘焙收據。
set(GAMEPLAY_PHOTOREAL_COOKED_ROOT "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked")
set(GAMEPLAY_PHOTOREAL_MANIFEST "${GAMEPLAY_PHOTOREAL_COOKED_ROOT}/runtime-render-manifest.json")
file(READ "${GAMEPLAY_PHOTOREAL_MANIFEST}" GAMEPLAY_PHOTOREAL_MANIFEST_JSON)
string(JSON GAMEPLAY_PHOTOREAL_FILE_COUNT LENGTH "${GAMEPLAY_PHOTOREAL_MANIFEST_JSON}" files)
if(NOT GAMEPLAY_PHOTOREAL_FILE_COUNT EQUAL 54)
  message(FATAL_ERROR "Gameplay photoreal package requires exactly 54 manifest payloads")
endif()
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

add_executable(ShowcaseRenderAssetBindings3DTest tests/ShowcaseRenderAssetBindings3DSmoke.cpp src/ShowcaseRenderAssetBindings3D.cpp src/GameplayShowcaseEnvironment3D.cpp)
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
add_executable(GameplayLabRulesTest tests/GameplayLabRulesSmoke.cpp src/GameplayFirstPersonViewmodel3D.cpp src/GameplayRules3D.cpp src/GameplayContactEvidence3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/GameplayOccluderMask3D.cpp)
target_include_directories(GameplayLabRulesTest PRIVATE include)
target_compile_options(GameplayLabRulesTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayLabRulesTest PRIVATE AevryndEngine)
add_test(NAME GameplayLabRulesTest COMMAND GameplayLabRulesTest)
add_executable(GameplayShowcaseEnvironmentTest tests/GameplayShowcaseEnvironment3DSmoke.cpp src/GameplayShowcaseEnvironment3D.cpp)
target_include_directories(GameplayShowcaseEnvironmentTest PRIVATE include)
target_compile_options(GameplayShowcaseEnvironmentTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayShowcaseEnvironmentTest PRIVATE AevryndEngine)
add_test(NAME GameplayShowcaseEnvironmentTest COMMAND GameplayShowcaseEnvironmentTest)
add_executable(GameplayCameraOcclusionPolicyTest tests/GameplayCameraOcclusionPolicySmoke.cpp src/GameplayCameraOcclusionPolicy3D.cpp)
target_include_directories(GameplayCameraOcclusionPolicyTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(GameplayCameraOcclusionPolicyTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME GameplayCameraOcclusionPolicyTest COMMAND GameplayCameraOcclusionPolicyTest)
add_executable(GameplayCameraProductionIntegrationTest tests/GameplayCameraProductionIntegrationSmoke.cpp src/GameplayFirstPersonViewmodel3D.cpp src/GameplayAnimation3D.cpp src/GameplayCameraOcclusionPolicy3D.cpp src/GameplayCameraProductionBuilder3D.cpp src/GameplayOccluderMask3D.cpp src/GameplayShowcaseEnvironment3D.cpp)
target_include_directories(GameplayCameraProductionIntegrationTest PRIVATE include)
target_compile_options(GameplayCameraProductionIntegrationTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(GameplayCameraProductionIntegrationTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(GameplayCameraProductionIntegrationTest PRIVATE AevryndEngine)
add_test(NAME GameplayCameraProductionIntegrationTest COMMAND GameplayCameraProductionIntegrationTest)
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
add_executable(GameplayLabBenchmark tests/GameplayLabBenchmark.cpp src/GameplayRules3D.cpp)
target_include_directories(GameplayLabBenchmark PRIVATE include)
target_compile_options(GameplayLabBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayLabBenchmark PRIVATE AevryndEngine)
add_test(NAME GameplayLabBenchmark COMMAND GameplayLabBenchmark)
set_tests_properties(GameplayLabBenchmark PROPERTIES TIMEOUT 30)
add_test(NAME GameplayLabCaptureEvidenceTemporalSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureEvidenceSmoke.ps1" -Section Temporal)
add_test(NAME GameplayLabCaptureEvidenceReceiptSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureEvidenceSmoke.ps1" -Section Receipt)
set_tests_properties(GameplayLabCaptureEvidenceTemporalSmoke GameplayLabCaptureEvidenceReceiptSmoke PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE)
foreach(warmup IN ITEMS 3 4 5)
  add_test(NAME GameplayThirdDayTemporalFixtureWarmup${warmup}Smoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayThirdDayTemporalFixtureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>" -Warmup ${warmup})
  set_tests_properties(GameplayThirdDayTemporalFixtureWarmup${warmup}Smoke PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endforeach()
add_test(NAME GameplaySourceInventorySmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplaySourceInventorySmoke.ps1" -SourceRoot "${CMAKE_SOURCE_DIR}" -InventoryPath "${GAMEPLAY_SOURCE_INVENTORY_PATH}")
set_tests_properties(GameplaySourceInventorySmoke PROPERTIES TIMEOUT 30)
add_test(NAME GameplayLabCaptureSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>")
set_tests_properties(GameplayLabCaptureSmoke PROPERTIES TIMEOUT 300)
add_test(NAME GameplayLabGpuValidationCaptureSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>" -GpuValidation -SingleRun)
set_tests_properties(GameplayLabGpuValidationCaptureSmoke PROPERTIES TIMEOUT 180)
add_test(NAME GameplayLabCaptureCtestContractSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureCtestContractSmoke.ps1" -CTestFile "${CMAKE_CURRENT_BINARY_DIR}/CTestTestfile.cmake")
set_tests_properties(GameplayLabCaptureCtestContractSmoke PROPERTIES TIMEOUT 10)
add_executable(GameplayLabCombatAnimationTest tests/GameplayCombatAnimationSmoke.cpp src/GameplayCombat3D.cpp src/GameplayAnimation3D.cpp)
target_include_directories(GameplayLabCombatAnimationTest PRIVATE include)
target_compile_options(GameplayLabCombatAnimationTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(GameplayLabCombatAnimationTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(GameplayLabCombatAnimationTest PRIVATE AevryndEngine)
add_test(NAME GameplayLabCombatAnimationTest COMMAND GameplayLabCombatAnimationTest)
