# 新可玩林帶的入口選擇，不取消舊原型的拒絕條件。
add_executable(MistwatchForestPlayableTest tests/MistwatchForestPlayableTest.cpp)
target_include_directories(MistwatchForestPlayableTest PRIVATE include)
target_compile_features(MistwatchForestPlayableTest PRIVATE cxx_std_20)
target_compile_options(MistwatchForestPlayableTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
add_test(NAME MistwatchForestPlayableTest COMMAND MistwatchForestPlayableTest)
set_tests_properties(MistwatchForestPlayableTest PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-profile")
# 灌木擴增須保留同配置的真64株前綴，不允許跨關卡冒用快取。
foreach(prefix_mode IN ITEMS MistwatchShrubPrefixLayoutTest MistwatchShrubPrefixPilotLayoutTest)
  add_executable(${prefix_mode} tests/MistwatchShrubPrefixLayoutTest.cpp)
  target_include_directories(${prefix_mode} PRIVATE include)
  target_compile_features(${prefix_mode} PRIVATE cxx_std_20)
  target_compile_options(${prefix_mode} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${prefix_mode} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(prefix_mode STREQUAL "MistwatchShrubPrefixPilotLayoutTest")
    target_compile_definitions(${prefix_mode} PRIVATE AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT=1)
  endif()
  set_property(TARGET ${prefix_mode} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${prefix_mode} PRIVATE /STACK:16777216)
  target_link_libraries(${prefix_mode} PRIVATE AevryndEngine)
  add_test(NAME ${prefix_mode} COMMAND ${prefix_mode}
    "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/meshes"
    "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh")
  set_tests_properties(${prefix_mode} PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-layout-candidate")
endforeach()
# 保留快取的原／新配置身份及合法轉換；不冒稱完整新世界證書。
foreach(retention_mode IN ITEMS MistwatchRetentionLayoutIdentityTest MistwatchRetentionLayoutPilotIdentityTest)
  add_executable(${retention_mode} tests/MistwatchRetentionLayoutIdentityTest.cpp)
  target_include_directories(${retention_mode} PRIVATE include)
  target_compile_features(${retention_mode} PRIVATE cxx_std_20)
  target_compile_options(${retention_mode} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${retention_mode} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(retention_mode STREQUAL "MistwatchRetentionLayoutPilotIdentityTest")
    target_compile_definitions(${retention_mode} PRIVATE AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT=1)
  endif()
  set_property(TARGET ${retention_mode} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${retention_mode} PRIVATE /STACK:16777216)
  target_link_libraries(${retention_mode} PRIVATE AevryndEngine)
  add_test(NAME ${retention_mode} COMMAND ${retention_mode} "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/meshes")
  set_tests_properties(${retention_mode} PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-layout-candidate")
endforeach()
# 真植物資產的小批量配置單元；不取代完整森林與實機驗收。
foreach(vegetation_mode IN ITEMS MistwatchVegetationLayoutPolicyTest MistwatchVegetationLayoutPilotPolicyTest)
  add_executable(${vegetation_mode} tests/MistwatchVegetationLayoutPolicyTest.cpp)
  target_include_directories(${vegetation_mode} PRIVATE include)
  target_compile_features(${vegetation_mode} PRIVATE cxx_std_20)
  target_compile_options(${vegetation_mode} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${vegetation_mode} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(vegetation_mode STREQUAL "MistwatchVegetationLayoutPilotPolicyTest")
    target_compile_definitions(${vegetation_mode} PRIVATE AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT=1)
  endif()
  set_property(TARGET ${vegetation_mode} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${vegetation_mode} PRIVATE /STACK:16777216)
  target_link_libraries(${vegetation_mode} PRIVATE AevryndEngine)
  add_test(NAME ${vegetation_mode} COMMAND ${vegetation_mode}
    "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/meshes"
    "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/meshes"
    "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-primitive1-material1-tree-small-02-leaves.aemesh")
  set_tests_properties(${vegetation_mode} PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-layout-candidate")
endforeach()
# 林床與樹冠配置傳遞的小型單元；不取代正式資產／完整世界驗收。
foreach(policy_mode IN ITEMS MistwatchForestLayoutPolicyTest MistwatchForestLayoutPilotPolicyTest)
  add_executable(${policy_mode} tests/MistwatchForestLayoutPolicyTest.cpp)
  target_include_directories(${policy_mode} PRIVATE include)
  target_compile_features(${policy_mode} PRIVATE cxx_std_20)
  target_compile_options(${policy_mode} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${policy_mode} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(policy_mode STREQUAL "MistwatchForestLayoutPilotPolicyTest")
    target_compile_definitions(${policy_mode} PRIVATE AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT=1)
  endif()
  set_property(TARGET ${policy_mode} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${policy_mode} PRIVATE /STACK:16777216)
  target_link_libraries(${policy_mode} PRIVATE AevryndEngine)
  add_test(NAME ${policy_mode} COMMAND ${policy_mode})
  set_tests_properties(${policy_mode} PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-layout-candidate")
endforeach()
# 原入口作者在兩配置下重建原來源；不把此結果當作完整新配置世界。
foreach(author_mode IN ITEMS MistwatchOriginalEntranceSourceTest MistwatchOriginalEntrancePilotSourceTest)
  add_executable(${author_mode} tests/MistwatchOriginalEntranceSourceTest.cpp)
  target_include_directories(${author_mode} PRIVATE include)
  target_compile_features(${author_mode} PRIVATE cxx_std_20)
  target_compile_options(${author_mode} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${author_mode} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(author_mode STREQUAL "MistwatchOriginalEntrancePilotSourceTest")
    target_compile_definitions(${author_mode} PRIVATE AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT=1)
  endif()
  set_property(TARGET ${author_mode} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${author_mode} PRIVATE /STACK:16777216)
  target_link_libraries(${author_mode} PRIVATE AevryndEngine)
  add_test(NAME ${author_mode} COMMAND ${author_mode} --fern-dir "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/meshes")
  set_tests_properties(${author_mode} PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-layout-candidate")
endforeach()
# 原森林來源須在兩種入口配置下保留；不向正式Game傳入候選profile。
foreach(source_mode IN ITEMS MistwatchOriginalForestSourceTest MistwatchOriginalForestPilotSourceTest)
  add_executable(${source_mode} tests/MistwatchOriginalForestSourceTest.cpp)
  target_include_directories(${source_mode} PRIVATE include)
  target_compile_features(${source_mode} PRIVATE cxx_std_20)
  target_compile_options(${source_mode} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${source_mode} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(source_mode STREQUAL "MistwatchOriginalForestPilotSourceTest")
    target_compile_definitions(${source_mode} PRIVATE AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT=1)
  endif()
  set_property(TARGET ${source_mode} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${source_mode} PRIVATE /STACK:16777216)
  target_link_libraries(${source_mode} PRIVATE AevryndEngine)
  add_test(NAME ${source_mode} COMMAND ${source_mode})
  set_tests_properties(${source_mode} PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-layout-candidate")
endforeach()
# 獨立CPU候選只驗同源互動/存檔；不向正式Game傳入候選profile。
foreach(entry_mode IN ITEMS MistwatchEntryLayoutRuntimeTest MistwatchEntryLayoutPilotTest)
  add_executable(${entry_mode} tests/MistwatchEntryLayoutPilotTest.cpp)
  target_include_directories(${entry_mode} PRIVATE include)
  target_compile_features(${entry_mode} PRIVATE cxx_std_20)
  target_compile_options(${entry_mode} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${entry_mode} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  if(entry_mode STREQUAL "MistwatchEntryLayoutPilotTest")
    target_compile_definitions(${entry_mode} PRIVATE AEVRYND_MISTWATCH_ENTRY_LAYOUT_PILOT=1)
  endif()
  set_property(TARGET ${entry_mode} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${entry_mode} PRIVATE /STACK:16777216)
  target_link_libraries(${entry_mode} PRIVATE AevryndEngine)
  add_test(NAME ${entry_mode} COMMAND ${entry_mode})
  set_tests_properties(${entry_mode} PROPERTIES TIMEOUT 30 LABELS "forest-diagnostics;entry-layout-candidate")
endforeach()
# 入口站點共用來源，原關卡座標與存檔語意保持，不啟用未驗收的新layout。
add_executable(MistwatchEntryLayoutTest tests/MistwatchEntryLayoutTest.cpp)
target_include_directories(MistwatchEntryLayoutTest PRIVATE include)
target_compile_features(MistwatchEntryLayoutTest PRIVATE cxx_std_20)
target_compile_options(MistwatchEntryLayoutTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchEntryLayoutTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchEntryLayoutTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchEntryLayoutTest PRIVATE AevryndEngine)
add_test(NAME MistwatchEntryLayoutTest COMMAND MistwatchEntryLayoutTest)
set_tests_properties(MistwatchEntryLayoutTest PROPERTIES TIMEOUT 30 LABELS "release-gates;gameplay;entry-layout")
add_executable(MistwatchPlaybackTest tests/MistwatchPlaybackTest.cpp)
add_executable(MistwatchCameraSettingsTest tests/MistwatchCameraSettingsTest.cpp)
target_include_directories(MistwatchCameraSettingsTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchCameraSettingsTest PRIVATE cxx_std_20)
target_compile_definitions(MistwatchCameraSettingsTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(MistwatchCameraSettingsTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
set_property(TARGET MistwatchCameraSettingsTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchCameraSettingsTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCameraSettingsTest COMMAND MistwatchCameraSettingsTest)
set_tests_properties(MistwatchCameraSettingsTest PROPERTIES TIMEOUT 30 LABELS "release-gates;gameplay;camera-settings")
# 肩坡混合樹群為明確候選；CPU 配方與逐幀身分驗收不冒稱正式發布門已通過。
foreach(shoulder_test IN ITEMS MistwatchShoulderGroveTest MistwatchShoulderDrawTest MistwatchCliffCoverageTest MistwatchCliffCollisionTest)
  add_executable(${shoulder_test} tests/${shoulder_test}.cpp)
  target_include_directories(${shoulder_test} PRIVATE include)
  target_compile_features(${shoulder_test} PRIVATE cxx_std_20)
  target_compile_options(${shoulder_test} PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
  target_compile_definitions(${shoulder_test} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  set_property(TARGET ${shoulder_test} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_libraries(${shoulder_test} PRIVATE AevryndEngine)
  target_link_options(${shoulder_test} PRIVATE /STACK:16777216)
endforeach()
add_test(NAME MistwatchShoulderDrawTest COMMAND MistwatchShoulderDrawTest)
add_test(NAME MistwatchCliffCoverageTest COMMAND MistwatchCliffCoverageTest)
add_test(NAME MistwatchCliffCollisionTest COMMAND MistwatchCliffCollisionTest)
set_tests_properties(MistwatchCliffCollisionTest PROPERTIES LABELS "forest-diagnostics;cliff-collision-candidate" TIMEOUT 120)
set_tests_properties(MistwatchCliffCoverageTest PROPERTIES LABELS "forest-diagnostics;cliff-coverage-candidate" TIMEOUT 120)
set_tests_properties(MistwatchShoulderDrawTest PROPERTIES LABELS "forest-diagnostics;shoulder-candidate" TIMEOUT 120)
add_test(NAME MistwatchShoulderGroveTest COMMAND MistwatchShoulderGroveTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchShoulderGroveTest PROPERTIES LABELS "forest-diagnostics;shoulder-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
# 工棚逐面米制木紋：保留負向歷史算法，獨立量測世界距離，不依賴外部證據目錄。
add_executable(MistwatchWorkshopWoodUvTest tests/MistwatchWorkshopWoodUvTest.cpp)
target_include_directories(MistwatchWorkshopWoodUvTest PRIVATE include)
target_compile_features(MistwatchWorkshopWoodUvTest PRIVATE cxx_std_20)
target_compile_options(MistwatchWorkshopWoodUvTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchWorkshopWoodUvTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
set_property(TARGET MistwatchWorkshopWoodUvTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchWorkshopWoodUvTest PRIVATE AevryndEngine)
add_test(NAME MistwatchWorkshopWoodUvTest COMMAND MistwatchWorkshopWoodUvTest)
set_tests_properties(MistwatchWorkshopWoodUvTest PROPERTIES TIMEOUT 120 LABELS "release-gates;workshop-wood-uv")
add_executable(MistwatchSettlementSurfaceTest tests/MistwatchSettlementSurfaceTest.cpp)
target_include_directories(MistwatchSettlementSurfaceTest PRIVATE include)
target_compile_features(MistwatchSettlementSurfaceTest PRIVATE cxx_std_20)
target_compile_options(MistwatchSettlementSurfaceTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchSettlementSurfaceTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
set_property(TARGET MistwatchSettlementSurfaceTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_options(MistwatchSettlementSurfaceTest PRIVATE /STACK:16777216)
target_link_libraries(MistwatchSettlementSurfaceTest PRIVATE AevryndEngine)
add_test(NAME MistwatchSettlementSurfaceTest COMMAND MistwatchSettlementSurfaceTest)
set_tests_properties(MistwatchSettlementSurfaceTest PROPERTIES TIMEOUT 120 LABELS "release-gates;settlement-surface")
# 固定肩坡樹群先以真實五個網格驗證，不放寬原 520 株世界契約。
add_executable(MistwatchLandscapeStandFillTest tests/MistwatchLandscapeStandFillTest.cpp)
target_include_directories(MistwatchLandscapeStandFillTest PRIVATE include)
target_compile_features(MistwatchLandscapeStandFillTest PRIVATE cxx_std_20)
target_compile_options(MistwatchLandscapeStandFillTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchLandscapeStandFillTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchLandscapeStandFillTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchLandscapeStandFillTest PRIVATE AevryndEngine)
add_test(NAME MistwatchLandscapeStandFillTest COMMAND MistwatchLandscapeStandFillTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchLandscapeStandFillTest PROPERTIES LABELS "forest-diagnostics;landscape-stand-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
# 外圈地表使用同一批真實樹姿態，先驗證候選再以原生鏡位比較。
add_executable(MistwatchOuterForestFloorTest tests/MistwatchOuterForestFloorTest.cpp)
target_include_directories(MistwatchOuterForestFloorTest PRIVATE include)
target_compile_features(MistwatchOuterForestFloorTest PRIVATE cxx_std_20)
target_compile_options(MistwatchOuterForestFloorTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchOuterForestFloorTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchOuterForestFloorTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchOuterForestFloorTest PRIVATE AevryndEngine)
add_test(NAME MistwatchOuterForestFloorTest COMMAND MistwatchOuterForestFloorTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchOuterForestFloorTest PROPERTIES LABELS "forest-diagnostics;outer-floor-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
# 林下群落先驗證完整三角支撐與固定配額，不改正常遊玩預設。
add_executable(MistwatchOuterUnderstoryTest tests/MistwatchOuterUnderstoryTest.cpp)
target_include_directories(MistwatchOuterUnderstoryTest PRIVATE include)
target_compile_features(MistwatchOuterUnderstoryTest PRIVATE cxx_std_20)
target_compile_options(MistwatchOuterUnderstoryTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchOuterUnderstoryTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchOuterUnderstoryTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchOuterUnderstoryTest PRIVATE AevryndEngine)
add_test(NAME MistwatchOuterUnderstoryTest COMMAND MistwatchOuterUnderstoryTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchOuterUnderstoryTest PROPERTIES LABELS "forest-diagnostics;outer-understory-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
add_test(NAME MistwatchOuterUnderstorySourceFeasibilityTest COMMAND MistwatchOuterUnderstoryTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets" --source-feasibility)
set_tests_properties(MistwatchOuterUnderstorySourceFeasibilityTest PROPERTIES LABELS "forest-diagnostics;source-feasibility-only" TIMEOUT 180 RUN_SERIAL TRUE)
add_test(NAME MistwatchOuterUnderstoryPrefixDiagnosticTest COMMAND MistwatchOuterUnderstoryTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets" --candidate-prefix)
set_tests_properties(MistwatchOuterUnderstoryPrefixDiagnosticTest PROPERTIES LABELS "forest-diagnostics;candidate-prefix-only" TIMEOUT 60 RUN_SERIAL TRUE)
find_package(Python3 3.10 REQUIRED COMPONENTS Interpreter)
set(MISTWATCH_CANOPY_BATCH "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch")
set(MISTWATCH_ROOT_SOURCE "${MISTWATCH_CANOPY_BATCH}/cooked/meshes/island-tree-01/island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh")
# 每個 fresh build 自有派生目錄；作者工具拒絕覆寫非空輸出。
set(MISTWATCH_ROOT_OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated-root-stem")
set(MISTWATCH_ROOT_COOKED "${MISTWATCH_ROOT_OUTPUT}/island-tree-01-lod2-trunk-root-stem.aemesh")
set(MISTWATCH_ROOT_RECEIPT "${MISTWATCH_ROOT_OUTPUT}/author-receipt.json")
add_custom_command(OUTPUT "${MISTWATCH_ROOT_COOKED}" "${MISTWATCH_ROOT_RECEIPT}"
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/author_root_stem.py"
    --source "${MISTWATCH_ROOT_SOURCE}" --output-dir "${MISTWATCH_ROOT_OUTPUT}"
  DEPENDS "${MISTWATCH_ROOT_SOURCE}" "${CMAKE_CURRENT_SOURCE_DIR}/../tools/author_root_stem.py"
  VERBATIM)
add_custom_target(MistwatchRootStemAsset DEPENDS "${MISTWATCH_ROOT_COOKED}" "${MISTWATCH_ROOT_RECEIPT}")
add_executable(MistwatchRootStemTest tests/MistwatchRootStemTest.cpp)
target_include_directories(MistwatchRootStemTest PRIVATE include)
target_compile_options(MistwatchRootStemTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchRootStemTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" MISTWATCH_DERIVED_STEM="${MISTWATCH_ROOT_COOKED}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchRootStemTest PRIVATE AevryndEngine)
add_dependencies(MistwatchRootStemTest MistwatchRootStemAsset)
add_test(NAME MistwatchRootStemTest COMMAND MistwatchRootStemTest)
set_tests_properties(MistwatchRootStemTest PROPERTIES LABELS "release-gates" TIMEOUT 60)
add_executable(MistwatchWorldProfileTest tests/MistwatchWorldProfileTest.cpp)
add_executable(MistwatchPlayableArtProfileTest tests/MistwatchPlayableArtProfileTest.cpp)
target_include_directories(MistwatchPlayableArtProfileTest PRIVATE include)
target_compile_features(MistwatchPlayableArtProfileTest PRIVATE cxx_std_20)
target_compile_options(MistwatchPlayableArtProfileTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
add_test(NAME MistwatchPlayableArtProfileTest COMMAND MistwatchPlayableArtProfileTest)
set_tests_properties(MistwatchPlayableArtProfileTest PROPERTIES LABELS "release-gates;playable-defaults" TIMEOUT 60)
# 入口八株作者與原四株基準分開驗證，不改舊 generator 的身分門檻。
add_executable(MistwatchEntranceAuthoredWorldTest tests/MistwatchEntranceAuthoredWorldTest.cpp)
target_include_directories(MistwatchEntranceAuthoredWorldTest PRIVATE include)
target_compile_options(MistwatchEntranceAuthoredWorldTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchEntranceAuthoredWorldTest PRIVATE
  MISTWATCH_RUNTIME_ASSETS="$<TARGET_FILE_DIR:Mistwatch>/assets" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchEntranceAuthoredWorldTest PRIVATE AevryndEngine)
add_dependencies(MistwatchEntranceAuthoredWorldTest Mistwatch)
add_test(NAME MistwatchEntranceAuthoredWorldTest COMMAND MistwatchEntranceAuthoredWorldTest)
set_tests_properties(MistwatchEntranceAuthoredWorldTest PROPERTIES LABELS "release-gates;entrance-world" TIMEOUT 600 RUN_SERIAL TRUE)
add_executable(MistwatchGrassCaptureReferenceTest tests/MistwatchGrassCaptureReferenceTest.cpp)
target_include_directories(MistwatchGrassCaptureReferenceTest PRIVATE include)
target_compile_options(MistwatchGrassCaptureReferenceTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchGrassCaptureReferenceTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchGrassCaptureReferenceTest PRIVATE AevryndEngine)
add_test(NAME MistwatchGrassCaptureReferenceTest COMMAND MistwatchGrassCaptureReferenceTest)
set_tests_properties(MistwatchGrassCaptureReferenceTest PROPERTIES LABELS "release-gates;entrance-world" TIMEOUT 120)
add_executable(MistwatchForestGrassShapeTest tests/MistwatchForestGrassShapeTest.cpp)
target_include_directories(MistwatchForestGrassShapeTest PRIVATE include)
target_compile_features(MistwatchForestGrassShapeTest PRIVATE cxx_std_20)
target_compile_options(MistwatchForestGrassShapeTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchForestGrassShapeTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchForestGrassShapeTest PRIVATE AevryndEngine)
add_test(NAME MistwatchForestGrassShapeTest COMMAND MistwatchForestGrassShapeTest)
set_tests_properties(MistwatchForestGrassShapeTest PROPERTIES LABELS "release-gates;entrance-world" TIMEOUT 120)
add_executable(MistwatchNettleAssetTest tests/MistwatchNettleAssetTest.cpp)
target_include_directories(MistwatchNettleAssetTest PRIVATE include)
target_compile_features(MistwatchNettleAssetTest PRIVATE cxx_std_20)
target_compile_options(MistwatchNettleAssetTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchNettleAssetTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchNettleAssetTest PRIVATE AevryndEngine)
add_test(NAME MistwatchNettleAssetTest COMMAND MistwatchNettleAssetTest)
set_tests_properties(MistwatchNettleAssetTest PROPERTIES LABELS "release-gates;understory-assets" TIMEOUT 120)
add_executable(MistwatchForestCoverRetentionTest tests/MistwatchForestCoverRetentionTest.cpp)
target_include_directories(MistwatchForestCoverRetentionTest PRIVATE include)
target_compile_options(MistwatchForestCoverRetentionTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchForestCoverRetentionTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchForestCoverRetentionTest PRIVATE AevryndEngine)
add_test(NAME MistwatchForestCoverRetentionTest COMMAND MistwatchForestCoverRetentionTest)
set_tests_properties(MistwatchForestCoverRetentionTest PROPERTIES LABELS "release-gates;entrance-world" TIMEOUT 120)
target_include_directories(MistwatchWorldProfileTest PRIVATE include)
target_compile_options(MistwatchWorldProfileTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME MistwatchWorldProfileTest COMMAND MistwatchWorldProfileTest)
set_tests_properties(MistwatchWorldProfileTest PROPERTIES LABELS "release-gates" TIMEOUT 60)
add_executable(MistwatchFirLayoutTest tests/MistwatchFirLayoutTest.cpp)
target_include_directories(MistwatchFirLayoutTest PRIVATE include)
target_compile_options(MistwatchFirLayoutTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchFirLayoutTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchFirLayoutTest PRIVATE AevryndEngine)
add_test(NAME MistwatchFirLayoutTest COMMAND MistwatchFirLayoutTest)
set_tests_properties(MistwatchFirLayoutTest PROPERTIES LABELS "release-gates" TIMEOUT 60)

add_executable(MistwatchObjectiveGuideTest tests/MistwatchObjectiveGuideTest.cpp)
target_include_directories(MistwatchObjectiveGuideTest PRIVATE include)
target_compile_options(MistwatchObjectiveGuideTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchObjectiveGuideTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchObjectiveGuideTest PRIVATE AevryndEngine)
add_test(NAME MistwatchObjectiveGuideTest COMMAND MistwatchObjectiveGuideTest)
set_tests_properties(MistwatchObjectiveGuideTest PROPERTIES LABELS "release-gates" TIMEOUT 60)

add_executable(MistwatchForestFloorMaskTest tests/MistwatchForestFloorMaskTest.cpp)
add_executable(MistwatchForestHabitatFloorTest tests/MistwatchForestHabitatFloorTest.cpp)
target_include_directories(MistwatchForestHabitatFloorTest PRIVATE include)
target_compile_options(MistwatchForestHabitatFloorTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchForestHabitatFloorTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchForestHabitatFloorTest PRIVATE AevryndEngine)
add_test(NAME MistwatchForestHabitatFloorTest COMMAND MistwatchForestHabitatFloorTest)
set_tests_properties(MistwatchForestHabitatFloorTest PROPERTIES LABELS "release-gates;entrance-world" TIMEOUT 120)
target_include_directories(MistwatchForestFloorMaskTest PRIVATE include)
target_compile_options(MistwatchForestFloorMaskTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchForestFloorMaskTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchForestFloorMaskTest PRIVATE AevryndEngine)
add_test(NAME MistwatchForestFloorMaskTest COMMAND MistwatchForestFloorMaskTest)
set_tests_properties(MistwatchForestFloorMaskTest PROPERTIES LABELS "release-gates" TIMEOUT 60)
add_executable(MistwatchCanopyCollisionTest tests/MistwatchCanopyCollisionTest.cpp)
target_include_directories(MistwatchCanopyCollisionTest PRIVATE include)
target_compile_options(MistwatchCanopyCollisionTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchCanopyCollisionTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" MISTWATCH_DERIVED_STEM="${MISTWATCH_ROOT_COOKED}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchCanopyCollisionTest PRIVATE AevryndEngine)
add_dependencies(MistwatchCanopyCollisionTest MistwatchRootStemAsset)
add_test(NAME MistwatchCanopyCollisionTest COMMAND MistwatchCanopyCollisionTest)
set_tests_properties(MistwatchCanopyCollisionTest PROPERTIES LABELS "release-gates" TIMEOUT 60)
add_executable(MistwatchCanopyVolumeClearanceTest tests/MistwatchCanopyVolumeClearanceTest.cpp)
target_include_directories(MistwatchCanopyVolumeClearanceTest PRIVATE include)
target_compile_options(MistwatchCanopyVolumeClearanceTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchCanopyVolumeClearanceTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchCanopyVolumeClearanceTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCanopyVolumeClearanceTest COMMAND MistwatchCanopyVolumeClearanceTest)
set_tests_properties(MistwatchCanopyVolumeClearanceTest PROPERTIES LABELS "release-gates" TIMEOUT 60)
add_executable(MistwatchShrubCanopyTest tests/MistwatchShrubCanopyTest.cpp)
target_include_directories(MistwatchShrubCanopyTest PRIVATE include)
target_compile_options(MistwatchShrubCanopyTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(MistwatchShrubCanopyTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(MistwatchShrubCanopyTest PRIVATE AevryndEngine)
add_test(NAME MistwatchShrubCanopyTest COMMAND MistwatchShrubCanopyTest)
set_tests_properties(MistwatchShrubCanopyTest PROPERTIES LABELS "release-gates" TIMEOUT 60)
add_executable(MistwatchTrailLitterTest tests/MistwatchTrailLitterTest.cpp)
target_include_directories(MistwatchTrailLitterTest PRIVATE include)
target_compile_options(MistwatchTrailLitterTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchTrailLitterTest PRIVATE AevryndEngine)
add_test(NAME MistwatchTrailLitterTest COMMAND MistwatchTrailLitterTest)
set_tests_properties(MistwatchTrailLitterTest PROPERTIES LABELS "release-gates")
add_executable(MistwatchBroadleafTest tests/MistwatchBroadleafTest.cpp)
target_include_directories(MistwatchBroadleafTest PRIVATE include)
target_compile_options(MistwatchBroadleafTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(MistwatchBroadleafTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(MistwatchBroadleafTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBroadleafTest COMMAND MistwatchBroadleafTest)
add_executable(MistwatchBankLayerCoverageTest tests/MistwatchBankLayerCoverageTest.cpp)
target_include_directories(MistwatchBankLayerCoverageTest PRIVATE include)
target_compile_options(MistwatchBankLayerCoverageTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchBankLayerCoverageTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBankLayerCoverageTest COMMAND MistwatchBankLayerCoverageTest)
set_tests_properties(MistwatchBankLayerCoverageTest PROPERTIES LABELS "release-gates")
target_include_directories(MistwatchPlaybackTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(MistwatchPlaybackTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME MistwatchPlaybackTest COMMAND MistwatchPlaybackTest)
add_executable(MistwatchSessionTest tests/MistwatchSessionTest.cpp)
target_include_directories(MistwatchSessionTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(MistwatchSessionTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME MistwatchSessionTest COMMAND MistwatchSessionTest)
add_executable(Mistwatch WIN32 src/main.cpp)
add_executable(MistwatchForestSurfaceProfileTest tests/MistwatchForestSurfaceProfileTest.cpp)
target_include_directories(MistwatchForestSurfaceProfileTest PRIVATE include)
target_compile_features(MistwatchForestSurfaceProfileTest PRIVATE cxx_std_20)
set_property(TARGET MistwatchForestSurfaceProfileTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_compile_options(MistwatchForestSurfaceProfileTest PRIVATE /W4 /WX /fp:strict /permissive- /utf-8)
target_link_libraries(MistwatchForestSurfaceProfileTest PRIVATE AevryndEngine)
add_test(NAME MistwatchForestSurfaceProfileTest COMMAND MistwatchForestSurfaceProfileTest)
set_tests_properties(MistwatchForestSurfaceProfileTest PROPERTIES LABELS "release-gates;terrain-surface" TIMEOUT 30)
add_dependencies(Mistwatch MistwatchRootStemAsset)
# 短髮純作者只在專用 TU 使用 precise；原 Main 與所有原測試編譯選項不變。
add_library(MistwatchExplorerNapeHairBinding OBJECT src/MistwatchExplorerNapeHairBinding.cpp)
target_include_directories(MistwatchExplorerNapeHairBinding PRIVATE include)
target_compile_features(MistwatchExplorerNapeHairBinding PRIVATE cxx_std_20)
target_compile_options(MistwatchExplorerNapeHairBinding PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_link_libraries(MistwatchExplorerNapeHairBinding PRIVATE AevryndEngine)
target_sources(Mistwatch PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
add_executable(MistwatchSupplyCrateTest tests/MistwatchSupplyCrateTest.cpp)
target_include_directories(MistwatchSupplyCrateTest PRIVATE include)
target_compile_options(MistwatchSupplyCrateTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchSupplyCrateTest PRIVATE AevryndEngine)
add_test(NAME MistwatchSupplyCrateTest COMMAND MistwatchSupplyCrateTest)
add_executable(MistwatchSupplyCrateBoardUvTest tests/MistwatchSupplyCrateBoardUvTest.cpp)
target_include_directories(MistwatchSupplyCrateBoardUvTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchSupplyCrateBoardUvTest PRIVATE cxx_std_20)
target_compile_options(MistwatchSupplyCrateBoardUvTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchSupplyCrateBoardUvTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchSupplyCrateBoardUvTest PRIVATE AevryndEngine)
add_test(NAME MistwatchSupplyCrateBoardUvTest COMMAND MistwatchSupplyCrateBoardUvTest)
set_tests_properties(MistwatchSupplyCrateBoardUvTest PROPERTIES LABELS "release-gates;current-prop-art" TIMEOUT 120)
add_executable(MistwatchRangerHutTest tests/MistwatchRangerHutTest.cpp)
target_include_directories(MistwatchRangerHutTest PRIVATE include)
target_compile_options(MistwatchRangerHutTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchRangerHutTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRangerHutTest COMMAND MistwatchRangerHutTest)
add_executable(MistwatchRelayCameraTest tests/MistwatchRelayCameraTest.cpp)
target_include_directories(MistwatchRelayCameraTest PRIVATE include)
target_compile_options(MistwatchRelayCameraTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchRelayCameraTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRelayCameraTest COMMAND MistwatchRelayCameraTest)
add_executable(MistwatchCameraMotionTest tests/MistwatchCameraMotionTest.cpp)
target_include_directories(MistwatchCameraMotionTest PRIVATE include)
target_compile_options(MistwatchCameraMotionTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchCameraMotionTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCameraMotionTest COMMAND MistwatchCameraMotionTest)
add_executable(MistwatchCameraOrbitTest tests/MistwatchCameraOrbitTest.cpp)
target_include_directories(MistwatchCameraOrbitTest PRIVATE include)
target_compile_options(MistwatchCameraOrbitTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchCameraOrbitTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCameraOrbitTest COMMAND MistwatchCameraOrbitTest)
add_executable(MistwatchCameraRouteContextTest tests/MistwatchCameraRouteContextTest.cpp)
target_include_directories(MistwatchCameraRouteContextTest PRIVATE include)
target_compile_options(MistwatchCameraRouteContextTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchCameraRouteContextTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCameraRouteContextTest COMMAND MistwatchCameraRouteContextTest)
foreach(save_test IN ITEMS MistwatchSaveGameTest MistwatchSaveFileTest MistwatchLoadPreparationTest)
  add_executable(${save_test} tests/${save_test}.cpp)
  target_include_directories(${save_test} PRIVATE include)
  target_compile_options(${save_test} PRIVATE /W4 /WX /permissive- /utf-8)
  target_link_libraries(${save_test} PRIVATE AevryndEngine)
endforeach()
add_test(NAME MistwatchSaveGameTest COMMAND MistwatchSaveGameTest)
add_test(NAME MistwatchSaveFileTest COMMAND MistwatchSaveFileTest "${CMAKE_CURRENT_BINARY_DIR}/save-file-evidence")
add_test(NAME MistwatchLoadPreparationTest COMMAND MistwatchLoadPreparationTest)
add_executable(MistwatchMiddleLayerTest tests/MistwatchMiddleLayerTest.cpp)
target_include_directories(MistwatchMiddleLayerTest PRIVATE include)
target_compile_options(MistwatchMiddleLayerTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(MistwatchMiddleLayerTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchMiddleLayerTest PRIVATE AevryndEngine)
add_test(NAME MistwatchMiddleLayerTest COMMAND MistwatchMiddleLayerTest)
set_tests_properties(MistwatchMiddleLayerTest PROPERTIES TIMEOUT 60)
add_executable(MistwatchSaveInputAudit WIN32 tests/MistwatchSaveInputAudit.cpp)
target_include_directories(MistwatchSaveInputAudit PRIVATE include)
target_compile_options(MistwatchSaveInputAudit PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(MistwatchSaveInputAudit PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchSaveInputAudit PRIVATE AevryndEngine shell32)
target_sources(MistwatchSaveInputAudit PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
add_dependencies(MistwatchSaveInputAudit Mistwatch)
target_include_directories(Mistwatch PRIVATE include)
target_compile_options(Mistwatch PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(Mistwatch PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(Mistwatch PRIVATE AevryndEngine)

# 預設不核准外部Cook；只有完成當前來源建置/獨立讀回的管線才提供這兩份固定身分。
set(AEVRYND_ROUTE_GRASS_APPROVED_BUNDLE_SHA256 "" CACHE STRING "Approved route grass bundle SHA256")
set(AEVRYND_ROUTE_GRASS_APPROVED_BUILD_SHA256 "" CACHE STRING "Approved route grass source/runtime build SHA256")
set(AEVRYND_ROUTE_GRASS_COOKED_TEST_DIRECTORY "" CACHE PATH "Fresh qualified route grass cook fixture")
if(NOT AEVRYND_ROUTE_GRASS_APPROVED_BUNDLE_SHA256 STREQUAL "" OR NOT AEVRYND_ROUTE_GRASS_APPROVED_BUILD_SHA256 STREQUAL "")
  foreach(hash_var AEVRYND_ROUTE_GRASS_APPROVED_BUNDLE_SHA256 AEVRYND_ROUTE_GRASS_APPROVED_BUILD_SHA256)
    string(LENGTH "${${hash_var}}" hash_length)
    if(NOT hash_length EQUAL 64 OR NOT "${${hash_var}}" MATCHES "^[0-9a-f]+$")
      message(FATAL_ERROR "Both compiler-approved lowercase SHA256 identities are required")
    endif()
  endforeach()
  target_compile_definitions(Mistwatch PRIVATE
    AEVRYND_ROUTE_GRASS_APPROVED_BUNDLE_SHA256="${AEVRYND_ROUTE_GRASS_APPROVED_BUNDLE_SHA256}"
    AEVRYND_ROUTE_GRASS_APPROVED_BUILD_SHA256="${AEVRYND_ROUTE_GRASS_APPROVED_BUILD_SHA256}")
endif()
if(NOT AEVRYND_ROUTE_GRASS_COOKED_TEST_DIRECTORY STREQUAL "")
  if(NOT IS_ABSOLUTE "${AEVRYND_ROUTE_GRASS_COOKED_TEST_DIRECTORY}" OR
      NOT EXISTS "${AEVRYND_ROUTE_GRASS_COOKED_TEST_DIRECTORY}/route-grass.aecook")
    message(FATAL_ERROR "A fresh absolute cook fixture is required for admission tests")
  endif()
  add_executable(MistwatchRouteGrassCookLoadTest tests/MistwatchRouteGrassCookLoadTest.cpp)
  target_include_directories(MistwatchRouteGrassCookLoadTest PRIVATE include)
  target_compile_features(MistwatchRouteGrassCookLoadTest PRIVATE cxx_std_20)
  target_compile_options(MistwatchRouteGrassCookLoadTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
  target_compile_definitions(MistwatchRouteGrassCookLoadTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_property(TARGET MistwatchRouteGrassCookLoadTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(MistwatchRouteGrassCookLoadTest PRIVATE /STACK:16777216)
  target_link_libraries(MistwatchRouteGrassCookLoadTest PRIVATE AevryndEngine)
  add_test(NAME MistwatchRouteGrassCookLoadTest COMMAND MistwatchRouteGrassCookLoadTest --cooked-dir "${AEVRYND_ROUTE_GRASS_COOKED_TEST_DIRECTORY}")
  set_tests_properties(MistwatchRouteGrassCookLoadTest PROPERTIES TIMEOUT 180 LABELS "route-grass-cooked-admission")
endif()
set_target_properties(Mistwatch PROPERTIES OUTPUT_NAME "Aevrynd-Mistwatch")
# 明確另封候選，不污染原八組133檔的正常驗收 runtime；輸出已存在即拒絕。
add_custom_target(MistwatchForestSurfacePackage
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../tools/package_forest_surface.py"
    --source-root "${CMAKE_SOURCE_DIR}"
    --runtime-dir "$<TARGET_FILE_DIR:Mistwatch>"
    --output "${CMAKE_BINARY_DIR}/forest-surface-$<CONFIG>"
  DEPENDS Mistwatch
  VERBATIM)
# 可攜式倒木套件僅新增六個執行期檔案；既有部署原文不變。
set(MISTWATCH_FALLEN_SOURCE "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fallen-logs")
set(MISTWATCH_FALLEN_RUNTIME "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-fallen-logs")
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTWATCH_FALLEN_RUNTIME}/meshes" "${MISTWATCH_FALLEN_RUNTIME}/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_FALLEN_SOURCE}/meshes/dead-tree-trunk-lod2-node0-mesh0-primitive0-materialnone-material-none.aemesh"
    "${MISTWATCH_FALLEN_RUNTIME}/meshes/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_FALLEN_SOURCE}/materials/slot-0-base.dds"
    "${MISTWATCH_FALLEN_SOURCE}/materials/slot-0-arm.dds"
    "${MISTWATCH_FALLEN_SOURCE}/materials/slot-0-normal.dds"
    "${MISTWATCH_FALLEN_RUNTIME}/materials/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_FALLEN_SOURCE}/fallen-log-package-manifest.json"
    "${MISTWATCH_FALLEN_SOURCE}/LICENSE.md"
    "${MISTWATCH_FALLEN_RUNTIME}/"
  VERBATIM)
# runtime 只複製實際三槽與九個原材質，不帶整個 LOD 資料夾。
set(MISTWATCH_CANOPY_RUNTIME "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-canopy-preview")
set(MISTWATCH_LEAF_CLUSTER_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../assets/leaf-clusters")
add_custom_target(MistwatchLeafClusterAssets
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-leaf-clusters"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_LEAF_CLUSTER_SOURCE}/leaves-v1.aemesh"
    "${MISTWATCH_LEAF_CLUSTER_SOURCE}/PROVENANCE.txt"
    "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-leaf-clusters/"
  DEPENDS "${MISTWATCH_LEAF_CLUSTER_SOURCE}/leaves-v1.aemesh" "${MISTWATCH_LEAF_CLUSTER_SOURCE}/PROVENANCE.txt"
  VERBATIM)
add_dependencies(Mistwatch MistwatchLeafClusterAssets)
add_executable(MistwatchLeafClusterTest tests/MistwatchLeafClusterTest.cpp)
target_include_directories(MistwatchLeafClusterTest PRIVATE include)
target_compile_features(MistwatchLeafClusterTest PRIVATE cxx_std_20)
target_compile_options(MistwatchLeafClusterTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchLeafClusterTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
set_property(TARGET MistwatchLeafClusterTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchLeafClusterTest PRIVATE AevryndEngine)
add_dependencies(MistwatchLeafClusterTest Mistwatch)
add_test(NAME MistwatchLeafClusterTest COMMAND MistwatchLeafClusterTest "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchLeafClusterTest PROPERTIES LABELS "release-gates;foliage-assets" TIMEOUT 60 RUN_SERIAL TRUE)
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTWATCH_CANOPY_RUNTIME}/meshes" "${MISTWATCH_CANOPY_RUNTIME}/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_CANOPY_BATCH}/LICENSE.md"
    "${MISTWATCH_CANOPY_BATCH}/runtime-asset-manifest.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/../ROOT_DERIVATION.md" "${MISTWATCH_CANOPY_RUNTIME}/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MISTWATCH_ROOT_COOKED}" "${MISTWATCH_CANOPY_RUNTIME}/meshes/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MISTWATCH_ROOT_RECEIPT}" "${MISTWATCH_CANOPY_RUNTIME}/root-stem-derivation.json"
  VERBATIM)
foreach(canopy_mesh IN ITEMS
    island-tree-01-lod2-node0-mesh0-primitive0-material0-island-tree-01.aemesh
    island-tree-01-lod1-node0-mesh0-primitive1-material1-island-tree-01-leaves.aemesh
    island-tree-01-lod2-node0-mesh0-primitive2-material2-island-tree-01-branches.aemesh)
  add_custom_command(TARGET Mistwatch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${MISTWATCH_CANOPY_BATCH}/cooked/meshes/island-tree-01/${canopy_mesh}" "${MISTWATCH_CANOPY_RUNTIME}/meshes/"
    VERBATIM)
endforeach()
foreach(canopy_slot IN ITEMS trunk leaves branches)
  foreach(canopy_channel IN ITEMS base normal arm)
    add_custom_command(TARGET Mistwatch POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${MISTWATCH_CANOPY_BATCH}/cooked/materials/island-tree-01/${canopy_slot}-${canopy_channel}.dds" "${MISTWATCH_CANOPY_RUNTIME}/materials/"
      VERBATIM)
  endforeach()
endforeach()
set(AEVRYND_MISTWATCH_RUNTIME_ASSET_COHORT "authored-forest-grove-full-v6" CACHE STRING "Explicit legacy418 compatibility or current423 forest runtime asset cohort")
set_property(CACHE AEVRYND_MISTWATCH_RUNTIME_ASSET_COHORT PROPERTY STRINGS
  authored-current-forest-full-v5 authored-forest-grove-full-v6)
if(NOT AEVRYND_MISTWATCH_RUNTIME_ASSET_COHORT STREQUAL "authored-current-forest-full-v5" AND
   NOT AEVRYND_MISTWATCH_RUNTIME_ASSET_COHORT STREQUAL "authored-forest-grove-full-v6")
  message(FATAL_ERROR "Unknown Mistwatch runtime asset cohort")
endif()
set(MISTWATCH_VALLEY_STARTUP_BAKE_FILE "" CACHE FILEPATH "Explicit reviewed v1 data asset; never an imported native binary")
set(MISTWATCH_RUNTIME_MATERIALS ambientcg/ground037 ambientcg/woodsiding006 poly-haven/bark_brown_02 poly-haven/rock_surface)
if(AEVRYND_MISTWATCH_RUNTIME_ASSET_COHORT STREQUAL "authored-forest-grove-full-v6")
  list(APPEND MISTWATCH_RUNTIME_MATERIALS poly-haven/rock_face_03)
  if(MISTWATCH_VALLEY_STARTUP_BAKE_FILE)
    if(NOT EXISTS "${MISTWATCH_VALLEY_STARTUP_BAKE_FILE}" OR IS_DIRECTORY "${MISTWATCH_VALLEY_STARTUP_BAKE_FILE}")
      message(FATAL_ERROR "Explicit reviewed Mistwatch v1 startup data asset not found")
    endif()
    file(SIZE "${MISTWATCH_VALLEY_STARTUP_BAKE_FILE}" MISTWATCH_STARTUP_BAKE_BYTES)
    file(SHA256 "${MISTWATCH_VALLEY_STARTUP_BAKE_FILE}" MISTWATCH_STARTUP_BAKE_SHA)
    if(NOT MISTWATCH_STARTUP_BAKE_BYTES EQUAL 7948440 OR NOT MISTWATCH_STARTUP_BAKE_SHA STREQUAL "cc01776a1f99929a6ac61792bfa0b5ea7a73b72587e328ca727641722d559242")
      message(FATAL_ERROR "Mistwatch startup data asset identity differs; do not silently regenerate or accept another cache")
    endif()
    add_custom_command(TARGET Mistwatch POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-world"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MISTWATCH_VALLEY_STARTUP_BAKE_FILE}"
        "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-world/valley-startup-v1.bin" VERBATIM)
  endif()
elseif(MISTWATCH_VALLEY_STARTUP_BAKE_FILE)
  message(FATAL_ERROR "Legacy418 compatibility must use a separate build without the v6 startup data asset")
endif()
foreach(material IN LISTS MISTWATCH_RUNTIME_MATERIALS)
  add_custom_command(TARGET Mistwatch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/material-packs/${material}" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/${material}")
endforeach()
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/material-packs/LICENSE.md" "${CMAKE_SOURCE_DIR}/engine/assets/material-packs/material-packs.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/")
# 普通遊玩只部署兩張既有高度圖；來源封印與授權另置 bin 同層，不混入資產清單。
set(MISTWATCH_SURFACE_HEIGHT_SOURCE "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-surface-height")
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/moss-height"
    "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/leaf-height"
    "$<TARGET_FILE_DIR:Mistwatch>/surface-provenance"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_SURFACE_HEIGHT_SOURCE}/moss-height/Ground037_SurfaceOrmHeight_Level0.png"
    "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/moss-height/Ground037_SurfaceOrmHeight_Level0.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_SURFACE_HEIGHT_SOURCE}/leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png"
    "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/leaf-height/ForestLeaves04_SurfaceOrmHeight_Level0.png"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_SURFACE_HEIGHT_SOURCE}/manifest.json"
    "${MISTWATCH_SURFACE_HEIGHT_SOURCE}/LICENSE.txt"
    "$<TARGET_FILE_DIR:Mistwatch>/surface-provenance/"
  VERBATIM)
# 巡林屋使用獨立風化木板槽；只打包三張原圖，不帶來源審查與下載記錄。
set(MISTWATCH_HUT_MATERIAL "${CMAKE_SOURCE_DIR}/engine/assets/material-packs/poly-haven/worn_planks/1k")
set(MISTWATCH_HUT_RUNTIME "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/poly-haven/worn_planks/1k")
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTWATCH_HUT_RUNTIME}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_HUT_MATERIAL}/base-color.jpg"
    "${MISTWATCH_HUT_MATERIAL}/roughness.jpg"
    "${MISTWATCH_HUT_MATERIAL}/normal-dx.png"
    "${MISTWATCH_HUT_RUNTIME}/"
  VERBATIM)
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/materials/forest-ground-04" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/poly-haven/forest-ground-04"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/LICENSE.md" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/FOREST_GROUND_LICENSE.md"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/forest-ground-render-manifest.json")
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/materials/ground108" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/ambientcg/ground108"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/LICENSE.md" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/GROUND_SOIL_LICENSE.md"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/runtime-render-manifest.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/ground-soil-render-manifest.json")
add_executable(MistwatchGroundMaterialTest tests/MistwatchGroundMaterialTest.cpp)
target_include_directories(MistwatchGroundMaterialTest PRIVATE include)
target_compile_options(MistwatchGroundMaterialTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(MistwatchGroundMaterialTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(MistwatchGroundMaterialTest PRIVATE AevryndEngine)
add_test(NAME MistwatchGroundMaterialTest COMMAND MistwatchGroundMaterialTest)
add_executable(MistwatchGroundForestWoodArtTest tests/MistwatchGroundForestWoodArtTest.cpp)
target_include_directories(MistwatchGroundForestWoodArtTest PRIVATE include)
target_compile_features(MistwatchGroundForestWoodArtTest PRIVATE cxx_std_20)
target_compile_options(MistwatchGroundForestWoodArtTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise)
target_compile_definitions(MistwatchGroundForestWoodArtTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchGroundForestWoodArtTest PRIVATE AevryndEngine windowscodecs ole32)
add_test(NAME MistwatchGroundForestWoodArtTest COMMAND MistwatchGroundForestWoodArtTest)
set_tests_properties(MistwatchGroundForestWoodArtTest PROPERTIES LABELS "release-gates" TIMEOUT 300)
add_executable(MistwatchHabitatLeafMaterialTest tests/MistwatchHabitatLeafMaterialTest.cpp)
target_include_directories(MistwatchHabitatLeafMaterialTest PRIVATE include)
target_compile_features(MistwatchHabitatLeafMaterialTest PRIVATE cxx_std_20)
target_compile_options(MistwatchHabitatLeafMaterialTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise)
target_compile_definitions(MistwatchHabitatLeafMaterialTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchHabitatLeafMaterialTest PRIVATE AevryndEngine windowscodecs ole32)
add_test(NAME MistwatchHabitatLeafMaterialTest COMMAND MistwatchHabitatLeafMaterialTest)
set_tests_properties(MistwatchHabitatLeafMaterialTest PROPERTIES LABELS "release-gates;entrance-world" TIMEOUT 300)
# 棲地落葉只部署三張無損圖與來源說明；不覆寫原苔地、碎石土或岩石來源。
set(MISTWATCH_LEAF_SOURCE "${CMAKE_SOURCE_DIR}/engine/assets/material-packs/poly-haven/forest_leaves_04")
set(MISTWATCH_LEAF_RUNTIME "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-materials/poly-haven/forest_leaves_04")
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MISTWATCH_LEAF_RUNTIME}/2k"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${MISTWATCH_LEAF_SOURCE}/2k/forest_leaves_04_diff_2k.png"
    "${MISTWATCH_LEAF_SOURCE}/2k/forest_leaves_04_rough_2k.png"
    "${MISTWATCH_LEAF_SOURCE}/2k/forest_leaves_04_nor_dx_2k.png"
    "${MISTWATCH_LEAF_RUNTIME}/2k/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${MISTWATCH_LEAF_SOURCE}/LICENSE.txt" "${MISTWATCH_LEAF_SOURCE}/DERIVATION.txt" "${MISTWATCH_LEAF_RUNTIME}/"
  VERBATIM)
add_executable(MistwatchGrassTest tests/MistwatchGrassTest.cpp)
target_include_directories(MistwatchGrassTest PRIVATE include)
target_compile_options(MistwatchGrassTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchGrassTest PRIVATE AevryndEngine)
add_test(NAME MistwatchGrassTest COMMAND MistwatchGrassTest)
add_executable(MistwatchScannedGrassTest tests/MistwatchScannedGrassTest.cpp)
target_include_directories(MistwatchScannedGrassTest PRIVATE include)
target_compile_options(MistwatchScannedGrassTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(MistwatchScannedGrassTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(MistwatchScannedGrassTest PRIVATE AevryndEngine)
add_test(NAME MistwatchScannedGrassTest COMMAND MistwatchScannedGrassTest)
add_executable(MistwatchForestCoverTest tests/MistwatchForestCoverTest.cpp)
target_include_directories(MistwatchForestCoverTest PRIVATE include)
target_compile_options(MistwatchForestCoverTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchForestCoverTest PRIVATE AevryndEngine)
add_custom_command(TARGET MistwatchForestCoverTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/meshes" "$<TARGET_FILE_DIR:MistwatchForestCoverTest>/assets/mistwatch-forest-cover-test/meshes")
add_test(NAME MistwatchForestCoverTest COMMAND MistwatchForestCoverTest "$<TARGET_FILE_DIR:MistwatchForestCoverTest>/assets/mistwatch-forest-cover-test/meshes")
add_test(NAME MistwatchMiddleLayerForestCoverTest COMMAND MistwatchForestCoverTest "$<TARGET_FILE_DIR:MistwatchForestCoverTest>/assets/mistwatch-forest-cover-test/meshes" --middle-layer)
add_executable(MistwatchFernColoniesTest tests/MistwatchFernColoniesTest.cpp)
target_include_directories(MistwatchFernColoniesTest PRIVATE include)
target_compile_options(MistwatchFernColoniesTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchFernColoniesTest PRIVATE AevryndEngine)
add_custom_command(TARGET MistwatchFernColoniesTest POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/meshes" "$<TARGET_FILE_DIR:MistwatchFernColoniesTest>/assets/mistwatch-forest-cover-test/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/meshes" "$<TARGET_FILE_DIR:MistwatchFernColoniesTest>/assets/mistwatch-fern-colonies-test/meshes")
add_test(NAME MistwatchFernColoniesTest COMMAND MistwatchFernColoniesTest "$<TARGET_FILE_DIR:MistwatchFernColoniesTest>/assets/mistwatch-forest-cover-test/meshes" "$<TARGET_FILE_DIR:MistwatchFernColoniesTest>/assets/mistwatch-fern-colonies-test/meshes")
add_test(NAME MistwatchMiddleLayerFernColoniesTest COMMAND MistwatchFernColoniesTest "$<TARGET_FILE_DIR:MistwatchFernColoniesTest>/assets/mistwatch-forest-cover-test/meshes" "$<TARGET_FILE_DIR:MistwatchFernColoniesTest>/assets/mistwatch-fern-colonies-test/meshes" --middle-layer)
set_tests_properties(MistwatchMiddleLayerForestCoverTest MistwatchMiddleLayerFernColoniesTest PROPERTIES TIMEOUT 60)
find_program(MISTWATCH_RECEIPT_PWSH NAMES pwsh REQUIRED)
add_test(NAME MistwatchForestCoverReceiptTest COMMAND "${MISTWATCH_RECEIPT_PWSH}" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/MistwatchForestCoverReceiptTest.ps1")
set_tests_properties(MistwatchForestCoverReceiptTest PROPERTIES TIMEOUT 30)
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/meshes" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-grass/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/materials" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-grass/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/LICENSE.md" "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-grass-medium-02/download-receipt.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-grass/")
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-nettle-plant/meshes" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-nettle/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-nettle-plant/materials" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-nettle/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-nettle-plant/LICENSE.txt" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-nettle/")
add_executable(MistwatchFernTest tests/MistwatchFernTest.cpp)
target_include_directories(MistwatchFernTest PRIVATE include)
target_compile_options(MistwatchFernTest PRIVATE /W4 /WX /permissive- /utf-8)
target_compile_definitions(MistwatchFernTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(MistwatchFernTest PRIVATE AevryndEngine)
add_test(NAME MistwatchFernTest COMMAND MistwatchFernTest)
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/meshes" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-ferns/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/materials" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-ferns/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/LICENSE.md" "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/download-receipt.json" "${CMAKE_SOURCE_DIR}/engine/assets/mistwatch-fern-02/material-bake-receipt.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-ferns/")
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/materials/fir-tree-01-c" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-forest/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/meshes/fir-tree-01-c" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-forest/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/LICENSE.md" "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/runtime-asset-manifest.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-forest/")
add_executable(MistwatchBridgeTest tests/MistwatchBridgeTest.cpp)
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-broadleaf/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/materials/tree-small-02" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-broadleaf/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/LICENSE.md" "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/tree-small-02/tree-small-02-provenance-receipt.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-broadleaf/")
foreach(slot IN ITEMS "primitive0-material0-tree-small-02-branches" "primitive1-material1-tree-small-02-leaves" "primitive2-material2-tree-small-02-trunk")
  add_custom_command(TARGET Mistwatch POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/meshes/tree-small-02/tree-small-02-lod2-node0-mesh0-${slot}.aemesh" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-broadleaf/meshes/")
endforeach()
target_include_directories(MistwatchBridgeTest PRIVATE include)
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-boulders/meshes"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/materials/boulder-01" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-boulders/materials"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod2-node0-mesh0-primitive0-material0-boulder-01.aemesh" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-boulders/meshes/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/cooked/meshes/boulder-01/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-boulders/meshes/"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/LICENSE.md" "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/runtime-asset-manifest.json" "${CMAKE_SOURCE_DIR}/engine/assets/realistic-cc0-first-batch/boulder-01/boulder-01-lod-receipt.json" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-boulders/")
target_compile_options(MistwatchBridgeTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchBridgeTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBridgeTest COMMAND MistwatchBridgeTest)
add_executable(MistwatchEncounterTest tests/MistwatchEncounterTest.cpp)
target_include_directories(MistwatchEncounterTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(MistwatchEncounterTest PRIVATE /W4 /WX /permissive- /utf-8)
add_test(NAME MistwatchEncounterTest COMMAND MistwatchEncounterTest)
add_executable(MistwatchEncounterMotionTest tests/MistwatchEncounterMotionTest.cpp)
target_include_directories(MistwatchEncounterMotionTest PRIVATE include)
target_compile_options(MistwatchEncounterMotionTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchEncounterMotionTest PRIVATE AevryndEngine)
add_test(NAME MistwatchEncounterMotionTest COMMAND MistwatchEncounterMotionTest)
add_executable(MistwatchRouteTest tests/MistwatchRouteTest.cpp)
target_include_directories(MistwatchRouteTest PRIVATE include)
target_compile_definitions(MistwatchRouteTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(MistwatchRouteTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchRouteTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRouteTest COMMAND MistwatchRouteTest)
add_test(NAME MistwatchMiddleLayerRouteTest COMMAND MistwatchRouteTest --middle-layer)
set_tests_properties(MistwatchMiddleLayerRouteTest PROPERTIES TIMEOUT 60)
add_executable(MistwatchExplorerTest tests/MistwatchExplorerTest.cpp)
target_include_directories(MistwatchExplorerTest PRIVATE include)
target_compile_options(MistwatchExplorerTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchExplorerTest PRIVATE AevryndEngine)
add_test(NAME MistwatchExplorerTest COMMAND MistwatchExplorerTest)
add_executable(MistwatchExplorerDeathTest tests/MistwatchExplorerDeathTest.cpp $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
target_include_directories(MistwatchExplorerDeathTest PRIVATE include)
target_compile_options(MistwatchExplorerDeathTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_compile_definitions(MistwatchExplorerDeathTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchExplorerDeathTest PRIVATE AevryndEngine)
add_test(NAME MistwatchExplorerDeathTest COMMAND MistwatchExplorerDeathTest)
set_tests_properties(MistwatchExplorerDeathTest PROPERTIES TIMEOUT 60)
add_executable(MistwatchEvasionTest tests/MistwatchEvasionTest.cpp)
add_executable(MistwatchLocomotionMotorTest tests/MistwatchLocomotionMotorTest.cpp)
target_include_directories(MistwatchLocomotionMotorTest PRIVATE include)
target_compile_features(MistwatchLocomotionMotorTest PRIVATE cxx_std_20)
target_compile_options(MistwatchLocomotionMotorTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchLocomotionMotorTest PRIVATE AevryndEngine)
add_test(NAME MistwatchLocomotionMotorTest COMMAND MistwatchLocomotionMotorTest)

add_executable(MistwatchJumpGraceTest tests/MistwatchJumpGraceTest.cpp)
target_include_directories(MistwatchJumpGraceTest PRIVATE include)
target_compile_features(MistwatchJumpGraceTest PRIVATE cxx_std_20)
target_compile_options(MistwatchJumpGraceTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchJumpGraceTest PRIVATE AevryndEngine)
add_test(NAME MistwatchJumpGraceTest COMMAND MistwatchJumpGraceTest)
set_tests_properties(MistwatchJumpGraceTest PROPERTIES TIMEOUT 30 LABELS "gameplay;locomotion")

add_executable(MistwatchStationaryPivotTest tests/MistwatchStationaryPivotTest.cpp)
target_include_directories(MistwatchStationaryPivotTest PRIVATE include)
target_compile_features(MistwatchStationaryPivotTest PRIVATE cxx_std_20)
target_compile_options(MistwatchStationaryPivotTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchStationaryPivotTest PRIVATE AevryndEngine)
add_test(NAME MistwatchStationaryPivotTest COMMAND MistwatchStationaryPivotTest)
set_tests_properties(MistwatchStationaryPivotTest PROPERTIES TIMEOUT 45 LABELS "gameplay;locomotion")

add_executable(MistwatchSteepSlideMotorTest tests/MistwatchSteepSlideMotorTest.cpp)
target_include_directories(MistwatchSteepSlideMotorTest PRIVATE include)
target_compile_features(MistwatchSteepSlideMotorTest PRIVATE cxx_std_20)
target_compile_options(MistwatchSteepSlideMotorTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchSteepSlideMotorTest PRIVATE AevryndEngine)
add_test(NAME MistwatchSteepSlideMotorTest COMMAND MistwatchSteepSlideMotorTest)
set_tests_properties(MistwatchSteepSlideMotorTest PROPERTIES LABELS "terrain-locomotion")

add_executable(MistwatchSteepSlideOwnerTest tests/MistwatchSteepSlideOwnerTest.cpp)
target_include_directories(MistwatchSteepSlideOwnerTest PRIVATE include)
target_compile_features(MistwatchSteepSlideOwnerTest PRIVATE cxx_std_20)
target_compile_options(MistwatchSteepSlideOwnerTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchSteepSlideOwnerTest PRIVATE AevryndEngine)
add_test(NAME MistwatchSteepSlideOwnerTest COMMAND MistwatchSteepSlideOwnerTest)
set_tests_properties(MistwatchSteepSlideOwnerTest PROPERTIES LABELS "terrain-locomotion")
add_executable(MistwatchPlayerLocomotionTest tests/MistwatchPlayerLocomotionTest.cpp)
target_include_directories(MistwatchPlayerLocomotionTest PRIVATE include)
target_compile_features(MistwatchPlayerLocomotionTest PRIVATE cxx_std_20)
target_compile_options(MistwatchPlayerLocomotionTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchPlayerLocomotionTest PRIVATE AevryndEngine)
add_test(NAME MistwatchPlayerLocomotionTest COMMAND MistwatchPlayerLocomotionTest)
add_executable(MistwatchMotionCheckpointTest tests/MistwatchMotionCheckpointTest.cpp)
target_include_directories(MistwatchMotionCheckpointTest PRIVATE include)
target_compile_features(MistwatchMotionCheckpointTest PRIVATE cxx_std_20)
target_compile_options(MistwatchMotionCheckpointTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchMotionCheckpointTest PRIVATE AevryndEngine)
add_test(NAME MistwatchMotionCheckpointTest COMMAND MistwatchMotionCheckpointTest)
add_executable(MistwatchMotionLoadTest tests/MistwatchMotionLoadTest.cpp)
target_include_directories(MistwatchMotionLoadTest PRIVATE include)
target_compile_features(MistwatchMotionLoadTest PRIVATE cxx_std_20)
target_compile_options(MistwatchMotionLoadTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchMotionLoadTest PRIVATE AevryndEngine)
add_test(NAME MistwatchMotionLoadTest COMMAND MistwatchMotionLoadTest)

# 原生完整流程使用的轉向／停站回歸納入正式建置，不依賴 Evidence 目錄。
foreach(test_name IN ITEMS MistwatchPoseCouplingTest MistwatchSettlingRegressionTest MistwatchTurnBoundaryTest MistwatchSlopeFootReachTest MistwatchWestCliffReplayTest MistwatchNearTurnReplayTest)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_include_directories(${test_name} PRIVATE include)
  target_compile_features(${test_name} PRIVATE cxx_std_20)
  target_compile_options(${test_name} PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
  target_compile_definitions(${test_name} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_property(TARGET ${test_name} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(${test_name} PRIVATE /STACK:16777216)
  target_link_libraries(${test_name} PRIVATE AevryndEngine)
  add_test(NAME ${test_name} COMMAND ${test_name})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 120 LABELS "release-gates;locomotion-regression")
endforeach()
foreach(test_name IN ITEMS MistwatchLocomotionMotorTest MistwatchPlayerLocomotionTest MistwatchMotionCheckpointTest MistwatchMotionLoadTest)
  set_property(TEST ${test_name} APPEND PROPERTY LABELS "release-gates;locomotion-regression")
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 120)
endforeach()
target_include_directories(MistwatchEvasionTest PRIVATE include)
target_compile_options(MistwatchEvasionTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchEvasionTest PRIVATE AevryndEngine)
add_test(NAME MistwatchEvasionTest COMMAND MistwatchEvasionTest)
add_executable(MistwatchForestPropsTest tests/MistwatchForestPropsTest.cpp)
target_include_directories(MistwatchForestPropsTest PRIVATE include)
target_compile_definitions(MistwatchForestPropsTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}")
target_compile_options(MistwatchForestPropsTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchForestPropsTest PRIVATE AevryndEngine)
add_test(NAME MistwatchForestPropsTest COMMAND MistwatchForestPropsTest)
add_executable(MistwatchRiverTest tests/MistwatchRiverTest.cpp)
target_include_directories(MistwatchRiverTest PRIVATE include)
target_compile_options(MistwatchRiverTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchRiverTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRiverTest COMMAND MistwatchRiverTest)
foreach(river_test IN ITEMS MistwatchRiverBankRegressionTest MistwatchRiverQuestGateTest)
  add_executable(${river_test} tests/${river_test}.cpp)
  target_include_directories(${river_test} PRIVATE include)
  target_compile_options(${river_test} PRIVATE /W4 /WX /permissive- /utf-8)
  target_compile_definitions(${river_test} PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(${river_test} PRIVATE AevryndEngine)
  add_test(NAME ${river_test} COMMAND ${river_test})
  set_tests_properties(${river_test} PROPERTIES TIMEOUT 60)
endforeach()
add_executable(MistwatchRepairExplorationTest tests/MistwatchRepairExplorationTest.cpp)
target_include_directories(MistwatchRepairExplorationTest PRIVATE include)
target_compile_options(MistwatchRepairExplorationTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchRepairExplorationTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRepairExplorationTest COMMAND MistwatchRepairExplorationTest)
add_executable(MistwatchCameraTest tests/MistwatchCameraTest.cpp)
target_include_directories(MistwatchCameraTest PRIVATE include)
target_compile_options(MistwatchCameraTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchCameraTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCameraTest COMMAND MistwatchCameraTest)
add_executable(MistwatchSettlementTest tests/MistwatchSettlementTest.cpp)
target_include_directories(MistwatchSettlementTest PRIVATE include)
target_compile_options(MistwatchSettlementTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchSettlementTest PRIVATE AevryndEngine)
add_test(NAME MistwatchSettlementTest COMMAND MistwatchSettlementTest)
add_executable(MistwatchUnderstoryTest tests/MistwatchUnderstoryTest.cpp)
target_include_directories(MistwatchUnderstoryTest PRIVATE include)
target_compile_options(MistwatchUnderstoryTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchUnderstoryTest PRIVATE AevryndEngine)
add_test(NAME MistwatchUnderstoryTest COMMAND MistwatchUnderstoryTest)
add_executable(MistwatchBoarAsset tools/export_boar.cpp)
target_include_directories(MistwatchBoarAsset PRIVATE include)
target_compile_options(MistwatchBoarAsset PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchBoarAsset PRIVATE AevryndEngine)
add_dependencies(Mistwatch MistwatchBoarAsset)
add_custom_command(TARGET Mistwatch POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife"
  COMMAND $<TARGET_FILE:MistwatchBoarAsset> "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin")
add_executable(MistwatchBoarTest tests/MistwatchBoarTest.cpp)
target_include_directories(MistwatchBoarTest PRIVATE include)
target_compile_options(MistwatchBoarTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchBoarTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBoarTest COMMAND MistwatchBoarTest)
# 真Main暫停選站；斷言分離TU，且每次CTest使用獨立存檔目錄。
add_executable(MistwatchRelayTrackingTest tests/MistwatchRelayTrackingTest.cpp tests/MistwatchRelayTrackingAssert.cpp $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
target_include_directories(MistwatchRelayTrackingTest PRIVATE include)
target_compile_features(MistwatchRelayTrackingTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRelayTrackingTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /O2 /Ob2 /GL- /MP1)
target_compile_definitions(MistwatchRelayTrackingTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchRelayTrackingTest PRIVATE AevryndEngine shell32)
target_link_options(MistwatchRelayTrackingTest PRIVATE /STACK:16777216 /INCREMENTAL:NO /LTCG:OFF)
set_target_properties(MistwatchRelayTrackingTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL INTERPROCEDURAL_OPTIMIZATION FALSE)
add_dependencies(MistwatchRelayTrackingTest Mistwatch)
add_test(NAME MistwatchRelayTrackingTest COMMAND "${Python3_EXECUTABLE}" -B -c
  "import os,sys,uuid,subprocess;from pathlib import Path;r=Path(sys.argv[3])/('run-'+uuid.uuid4().hex);r.mkdir(parents=True);e=dict(os.environ);[(r/n).mkdir() for n in ('local','roaming','temp')];e.update(LOCALAPPDATA=str(r/'local'),APPDATA=str(r/'roaming'),TEMP=str(r/'temp'),TMP=str(r/'temp'));p=subprocess.run([sys.argv[1],sys.argv[2],str(r/'evidence')],cwd=r,env=e,creationflags=getattr(subprocess,'CREATE_NO_WINDOW',0),timeout=110,stdout=subprocess.PIPE,stderr=subprocess.PIPE);sys.stdout.buffer.write(p.stdout);sys.stderr.buffer.write(p.stderr);sys.exit(p.returncode)"
  "$<TARGET_FILE:MistwatchRelayTrackingTest>" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin" "${CMAKE_BINARY_DIR}/relay-tracking-cpu")
set_tests_properties(MistwatchRelayTrackingTest PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}" LABELS "release-gates;gameplay;navigation")
# 真 Main 單次 F 開窗銜接；不啟動視窗、Renderer 或音效裝置。
add_executable(MistwatchCounterBoundaryTest tests/MistwatchCounterBoundaryTest.cpp $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
target_include_directories(MistwatchCounterBoundaryTest PRIVATE include)
target_compile_features(MistwatchCounterBoundaryTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCounterBoundaryTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchCounterBoundaryTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchCounterBoundaryTest PRIVATE AevryndEngine shell32)
target_link_options(MistwatchCounterBoundaryTest PRIVATE /STACK:16777216)
add_dependencies(MistwatchCounterBoundaryTest Mistwatch)
add_test(NAME MistwatchCounterBoundaryTest COMMAND MistwatchCounterBoundaryTest "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin")
set_tests_properties(MistwatchCounterBoundaryTest PROPERTIES TIMEOUT 120 LABELS "release-gates;gameplay;combat")
# 只驗證命中後渲染姿態，不改正式碰撞、接地或戰鬥規則。
add_executable(MistwatchBoarHitReactionTest tests/MistwatchBoarHitReactionTest.cpp)
target_include_directories(MistwatchBoarHitReactionTest PRIVATE include)
target_compile_features(MistwatchBoarHitReactionTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBoarHitReactionTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchBoarHitReactionTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchBoarHitReactionTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBoarHitReactionTest COMMAND MistwatchBoarHitReactionTest)
set_tests_properties(MistwatchBoarHitReactionTest PROPERTIES TIMEOUT 180 LABELS "release-gates;gameplay;combat")
add_executable(MistwatchRelayTest tests/MistwatchRelayTest.cpp)
target_include_directories(MistwatchRelayTest PRIVATE include)
target_compile_options(MistwatchRelayTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchRelayTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRelayTest COMMAND MistwatchRelayTest)
add_executable(MistwatchLandscapeTest tests/MistwatchLandscapeTest.cpp)
target_include_directories(MistwatchLandscapeTest PRIVATE include)
target_compile_options(MistwatchLandscapeTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchLandscapeTest PRIVATE AevryndEngine)
add_test(NAME MistwatchLandscapeTest COMMAND MistwatchLandscapeTest)
add_executable(MistwatchAudioTest tests/MistwatchAudioTest.cpp)
target_include_directories(MistwatchAudioTest PRIVATE include)
target_compile_options(MistwatchAudioTest PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchAudioTest PRIVATE AevryndEngine)
add_test(NAME MistwatchAudioTest COMMAND MistwatchAudioTest)
add_executable(MistwatchAudioPreview tools/MistwatchAudioPreview.cpp)
target_include_directories(MistwatchAudioPreview PRIVATE include)
target_compile_options(MistwatchAudioPreview PRIVATE /W4 /WX /permissive- /utf-8)
target_link_libraries(MistwatchAudioPreview PRIVATE AevryndEngine)
if(AEVRYND_STREAMLINE_PACKAGE_TEST_TARGET)
  add_executable(MistwatchEncounterSightTest tests/MistwatchEncounterSightTest.cpp)
  target_include_directories(MistwatchEncounterSightTest PRIVATE include)
  target_compile_options(MistwatchEncounterSightTest PRIVATE /W4 /WX /permissive- /utf-8)
  target_link_libraries(MistwatchEncounterSightTest PRIVATE AevryndEngine)
  add_test(NAME MistwatchEncounterSightTest COMMAND MistwatchEncounterSightTest)
  set_tests_properties(MistwatchEncounterSightTest PROPERTIES LABELS "release-gates")
  set_tests_properties(MistwatchEncounterMotionTest PROPERTIES LABELS "release-gates")
  set_tests_properties(MistwatchSupplyCrateTest MistwatchSessionTest MistwatchRouteTest MistwatchRangerHutTest MistwatchRelayCameraTest MistwatchCameraMotionTest MistwatchCameraOrbitTest MistwatchCameraRouteContextTest MistwatchCameraTest MistwatchSaveGameTest MistwatchSaveFileTest MistwatchLoadPreparationTest MistwatchGroundMaterialTest MistwatchBridgeTest MistwatchLandscapeTest MistwatchForestCoverTest MistwatchFernColoniesTest MistwatchForestCoverReceiptTest
    PROPERTIES LABELS "release-gates")
  set_tests_properties(MistwatchMiddleLayerTest PROPERTIES LABELS "release-gates")
  set_tests_properties(MistwatchMiddleLayerRouteTest PROPERTIES LABELS "release-gates")
endif()
option(AEVRYND_MISTWATCH_NATIVE_GATES "Full production Mistwatch native scene matrices" OFF)
if(AEVRYND_MISTWATCH_NATIVE_GATES)
  if(AEVRYND_INTERNAL_RENDERER_TEST_HOOKS)
    message(FATAL_ERROR "Mistwatch production matrices require renderer hooks OFF")
  endif()
  find_program(MISTWATCH_PWSH NAMES pwsh REQUIRED)
  add_test(NAME MistwatchNativeNormal32
    COMMAND "${MISTWATCH_PWSH}" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_native_ctest.ps1"
      -SourceRoot "${CMAKE_SOURCE_DIR}" -BuildRoot "${CMAKE_BINARY_DIR}"
      -ExecutablePath "$<TARGET_FILE:Mistwatch>" -Mode normal)
  add_test(NAME MistwatchNativeGbv17
    COMMAND "${MISTWATCH_PWSH}" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
      -File "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_native_ctest.ps1"
      -SourceRoot "${CMAKE_SOURCE_DIR}" -BuildRoot "${CMAKE_BINARY_DIR}"
      -ExecutablePath "$<TARGET_FILE:Mistwatch>" -Mode gbv)
  set_tests_properties(MistwatchNativeNormal32 PROPERTIES
    LABELS "mistwatch-native-normal" WORKING_DIRECTORY "$<TARGET_FILE_DIR:Mistwatch>"
    RUN_SERIAL TRUE RESOURCE_LOCK "mistwatch-native-gpu" TIMEOUT 1560)
  set_tests_properties(MistwatchNativeGbv17 PROPERTIES
    LABELS "mistwatch-native-gbv" WORKING_DIRECTORY "$<TARGET_FILE_DIR:Mistwatch>"
    RUN_SERIAL TRUE RESOURCE_LOCK "mistwatch-native-gpu" TIMEOUT 2670)
endif()

# 離線草叢建置必須由外層可追溯流程提供整批來源與資產的SHA256，不能使用任意safe旗標。
set(AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256 "" CACHE STRING "Pinned source and runtime cook build SHA256")
if(NOT AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256 STREQUAL "")
  string(LENGTH "${AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256}" cook_hash_length)
  if(NOT cook_hash_length EQUAL 64 OR NOT AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256 MATCHES "^[0-9a-f]+$")
    message(FATAL_ERROR "A lowercase 64-character cook SHA256 is required")
  endif()
  add_executable(MistwatchRouteGrassCook EXCLUDE_FROM_ALL tools/CookRouteGrass.cpp)
  target_include_directories(MistwatchRouteGrassCook PRIVATE include)
  target_compile_features(MistwatchRouteGrassCook PRIVATE cxx_std_20)
  target_compile_options(MistwatchRouteGrassCook PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
  target_compile_definitions(MistwatchRouteGrassCook PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
    AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256="${AEVRYND_ROUTE_GRASS_COOK_BUILD_SHA256}")
  set_property(TARGET MistwatchRouteGrassCook PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_options(MistwatchRouteGrassCook PRIVATE /STACK:16777216)
  target_link_libraries(MistwatchRouteGrassCook PRIVATE AevryndEngine)
endif()

# 背景冷杉距離細節的實際雙端提交收據，不取代既有場景或陰影測試。
add_executable(MistwatchLandscapeTwigDrawTest tests/MistwatchLandscapeTwigDrawTest.cpp)
target_include_directories(MistwatchLandscapeTwigDrawTest PRIVATE include)
target_compile_features(MistwatchLandscapeTwigDrawTest PRIVATE cxx_std_20)
target_compile_options(MistwatchLandscapeTwigDrawTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchLandscapeTwigDrawTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchLandscapeTwigDrawTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchLandscapeTwigDrawTest PRIVATE AevryndEngine)
add_test(NAME MistwatchLandscapeTwigDrawTest COMMAND MistwatchLandscapeTwigDrawTest)
set_tests_properties(MistwatchLandscapeTwigDrawTest PROPERTIES LABELS "release-gates;current-landscape-twig" TIMEOUT 60)

# 河岸沉積遮罩候選僅做真實幾何 CPU 回歸，不代表視覺驗收。
add_executable(MistwatchBankCoverageCandidateTest tests/MistwatchBankCoverageCandidateTest.cpp)
target_include_directories(MistwatchBankCoverageCandidateTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_options(MistwatchBankCoverageCandidateTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchBankCoverageCandidateTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchBankCoverageCandidateTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBankCoverageCandidateTest COMMAND MistwatchBankCoverageCandidateTest)
set_tests_properties(MistwatchBankCoverageCandidateTest PROPERTIES LABELS "release-gates" TIMEOUT 120)

# 轉盤數字與指針的純CPU完整回歸；保留原66項release門檻。
add_executable(MistwatchFuseDialVisualTest tests/MistwatchFuseDialVisualTest.cpp)
target_include_directories(MistwatchFuseDialVisualTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchFuseDialVisualTest PRIVATE cxx_std_20)
target_compile_options(MistwatchFuseDialVisualTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchFuseDialVisualTest PRIVATE NOMINMAX)
add_test(NAME MistwatchFuseDialVisualTest COMMAND MistwatchFuseDialVisualTest)
set_tests_properties(MistwatchFuseDialVisualTest PROPERTIES LABELS "release-gates" TIMEOUT 15)

# 真CPU quiet與拒絕更新合約；完整保留原67項。
add_executable(MistwatchCameraQuietContractTest tests/MistwatchCameraQuietContractTest.cpp)
target_include_directories(MistwatchCameraQuietContractTest PRIVATE "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchCameraQuietContractTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCameraQuietContractTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchCameraQuietContractTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchCameraQuietContractTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCameraQuietContractTest COMMAND MistwatchCameraQuietContractTest)
set_tests_properties(MistwatchCameraQuietContractTest PROPERTIES LABELS "release-gates" TIMEOUT 30)

# 真信標世界部件 CPU 合約；既有68項保留原順序。
add_executable(MistwatchBeaconWorldDetailTest tests/MistwatchBeaconWorldDetailTest.cpp)
target_include_directories(MistwatchBeaconWorldDetailTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchBeaconWorldDetailTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBeaconWorldDetailTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchBeaconWorldDetailTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchBeaconWorldDetailTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBeaconWorldDetailTest COMMAND MistwatchBeaconWorldDetailTest)
set_tests_properties(MistwatchBeaconWorldDetailTest PROPERTIES LABELS "release-gates" TIMEOUT 60)

# 遭遇完成提示合約；保留所有原69項。
add_executable(MistwatchEncounterFeedbackTest tests/MistwatchEncounterFeedbackTest.cpp)
target_include_directories(MistwatchEncounterFeedbackTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchEncounterFeedbackTest PRIVATE cxx_std_20)
target_compile_options(MistwatchEncounterFeedbackTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchEncounterFeedbackTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchEncounterFeedbackTest PRIVATE AevryndEngine)
add_test(NAME MistwatchEncounterFeedbackTest COMMAND MistwatchEncounterFeedbackTest)
set_tests_properties(MistwatchEncounterFeedbackTest PROPERTIES LABELS "release-gates" TIMEOUT 60)

# 發電機作者幾何與既有電路／碰撞合約；完整保留原70項。
add_executable(MistwatchGeneratorDetailTest tests/MistwatchGeneratorDetailTest.cpp)
target_include_directories(MistwatchGeneratorDetailTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchGeneratorDetailTest PRIVATE cxx_std_20)
target_compile_options(MistwatchGeneratorDetailTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchGeneratorDetailTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchGeneratorDetailTest PRIVATE AevryndEngine)
add_test(NAME MistwatchGeneratorDetailTest COMMAND MistwatchGeneratorDetailTest)
set_tests_properties(MistwatchGeneratorDetailTest PROPERTIES LABELS "release-gates" TIMEOUT 45)

# 人物與物件作者合約；原有71項測試及標籤全部保留。
add_executable(MistwatchExplorerArtCpuTest tests/art/ExplorerArtCpuTest.cpp)
add_executable(MistwatchExplorerFaceArtCpuTest tests/art/ExplorerFaceArtCpuTest.cpp)
add_executable(MistwatchPropArtCpuTest tests/art/PropArtCpuTest.cpp tests/art/FormalWorldSnapshotIntegration.cpp)
add_executable(MistwatchHutMaterialCpuTest tests/art/HutMaterialCpuTest.cpp)
target_compile_definitions(MistwatchPropArtCpuTest PRIVATE
  PROP_ART_FORMAL_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/tests/art/formal-before"
  PROP_ART_CANDIDATE_ROOT="${CMAKE_SOURCE_DIR}")
foreach(art_test IN ITEMS MistwatchExplorerArtCpuTest MistwatchExplorerFaceArtCpuTest MistwatchPropArtCpuTest MistwatchHutMaterialCpuTest)
  target_include_directories(${art_test} PRIVATE include tests/art/fixtures "${CMAKE_SOURCE_DIR}/engine/native/include")
  target_compile_features(${art_test} PRIVATE cxx_std_20)
  target_compile_options(${art_test} PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
  target_compile_definitions(${art_test} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  target_link_libraries(${art_test} PRIVATE AevryndEngine)
  add_test(NAME ${art_test} COMMAND ${art_test})
  set_tests_properties(${art_test} PROPERTIES LABELS "release-gates" TIMEOUT 60)
endforeach()

# 新木屋以完整現行作者合約及原物理回歸驗收；不把歷史資產檢查當現行資格。
add_executable(MistwatchHutWholeArtCpuTest tests/art/HutWholeArtCpuTest.cpp tests/art/FormalWorldSnapshotIntegration.cpp)
target_include_directories(MistwatchHutWholeArtCpuTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchHutWholeArtCpuTest PRIVATE cxx_std_20)
target_compile_options(MistwatchHutWholeArtCpuTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchHutWholeArtCpuTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchHutWholeArtCpuTest PRIVATE AevryndEngine)
add_test(NAME MistwatchHutWholeArtCpuTest COMMAND MistwatchHutWholeArtCpuTest)
set_tests_properties(MistwatchHutWholeArtCpuTest PROPERTIES LABELS "release-gates;current-hut-art" TIMEOUT 60)

# 真桌面／桌腳與可見桌下空間；碰撞代理仍由原世界負責。
add_executable(MistwatchRangerDeskCpuTest tests/art/RangerDeskCpuTest.cpp)
target_include_directories(MistwatchRangerDeskCpuTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRangerDeskCpuTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRangerDeskCpuTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRangerDeskCpuTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchRangerDeskCpuTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRangerDeskCpuTest COMMAND MistwatchRangerDeskCpuTest)
set_tests_properties(MistwatchRangerDeskCpuTest PROPERTIES LABELS "release-gates;current-prop-art" TIMEOUT 15)
# 只隔離兩舊測試內的歷史木屋 API；不改 include 搜尋順序或現行 generator/world。
set_tests_properties(MistwatchHutMaterialCpuTest MistwatchPropArtCpuTest
  PROPERTIES LABELS "release-gates;historical-hut-reference")

# 僅在V291表面候選另經authoritative native接受及正式整合後新增。
add_executable(MistwatchExplorerSurfaceCpuTest tests/art/ExplorerSurfaceCpuTest.cpp)
target_include_directories(MistwatchExplorerSurfaceCpuTest PRIVATE include tests/art/fixtures "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchExplorerSurfaceCpuTest PRIVATE cxx_std_20)
target_compile_options(MistwatchExplorerSurfaceCpuTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchExplorerSurfaceCpuTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchExplorerSurfaceCpuTest PRIVATE AevryndEngine)
add_test(NAME MistwatchExplorerSurfaceCpuTest COMMAND MistwatchExplorerSurfaceCpuTest)
set_tests_properties(MistwatchExplorerSurfaceCpuTest PROPERTIES LABELS "release-gates" TIMEOUT 60)

# 開放枝冠保留完整作者足跡與整葉拓撲，不替換任何既有回歸。
add_executable(MistwatchShrubOpenCrownTest tests/MistwatchShrubOpenCrownTest.cpp)
target_compile_features(MistwatchShrubOpenCrownTest PRIVATE cxx_std_20)
target_include_directories(MistwatchShrubOpenCrownTest PRIVATE include)
target_compile_options(MistwatchShrubOpenCrownTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise)
target_compile_definitions(MistwatchShrubOpenCrownTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchShrubOpenCrownTest PRIVATE AevryndEngine)
add_test(NAME MistwatchShrubOpenCrownTest COMMAND MistwatchShrubOpenCrownTest)
set_tests_properties(MistwatchShrubOpenCrownTest PROPERTIES LABELS "release-gates" TIMEOUT 60)

# 工棚分板視覺共用引擎幾何；既有五個碰撞體及原測試全部保留。
add_executable(MistwatchRepairShelterVisualTest tests/MistwatchRepairShelterVisualTest.cpp)
target_include_directories(MistwatchRepairShelterVisualTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRepairShelterVisualTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRepairShelterVisualTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRepairShelterVisualTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchRepairShelterVisualTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRepairShelterVisualTest COMMAND MistwatchRepairShelterVisualTest)
set_tests_properties(MistwatchRepairShelterVisualTest PROPERTIES LABELS "release-gates;current-prop-art" TIMEOUT 30)

# 保留原工棚測試，另外檢查直壁紋理投影與完整幾何不變量。
add_executable(MistwatchRepairShelterBoardUvTest tests/MistwatchRepairShelterBoardUvTest.cpp)
target_include_directories(MistwatchRepairShelterBoardUvTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRepairShelterBoardUvTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRepairShelterBoardUvTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRepairShelterBoardUvTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchRepairShelterBoardUvTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRepairShelterBoardUvTest COMMAND MistwatchRepairShelterBoardUvTest)
set_tests_properties(MistwatchRepairShelterBoardUvTest PROPERTIES LABELS "release-gates;current-prop-art" TIMEOUT 30)

# 真工具道具幾何；保留原五個碰撞體與站立／蹲下拾取規則。
add_executable(MistwatchRepairToolsVisualTest tests/MistwatchRepairToolsVisualTest.cpp)
target_include_directories(MistwatchRepairToolsVisualTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRepairToolsVisualTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRepairToolsVisualTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRepairToolsVisualTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchRepairToolsVisualTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRepairToolsVisualTest COMMAND MistwatchRepairToolsVisualTest)
set_tests_properties(MistwatchRepairToolsVisualTest PROPERTIES LABELS "release-gates;current-prop-art" TIMEOUT 30)

# 屋頂獨立投影；原牆板與幾何測試原樣先執行，不放寬受保護屋頂的舊合同。
add_executable(MistwatchRepairShelterRoofUvTest tests/MistwatchRepairShelterRoofUvTest.cpp)
target_include_directories(MistwatchRepairShelterRoofUvTest PRIVATE include tests "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRepairShelterRoofUvTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRepairShelterRoofUvTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRepairShelterRoofUvTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchRepairShelterRoofUvTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRepairShelterRoofUvTest COMMAND MistwatchRepairShelterRoofUvTest)
set_tests_properties(MistwatchRepairShelterRoofUvTest PROPERTIES LABELS "release-gates;current-prop-art" TIMEOUT 30)

# 原木牌候選整合的完整來源幾何測試；所有既有93項及原門檻保留。
add_executable(MistwatchRepairSignVisualTest tests/MistwatchRepairSignVisualTest.cpp)
target_include_directories(MistwatchRepairSignVisualTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRepairSignVisualTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRepairSignVisualTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRepairSignVisualTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchRepairSignVisualTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRepairSignVisualTest COMMAND MistwatchRepairSignVisualTest)
set_tests_properties(MistwatchRepairSignVisualTest PROPERTIES LABELS "release-gates;current-prop-art" TIMEOUT 30)

# 真 production Apply 邊界、canonical、來源拒絕及applied模型讀檔交易；不取代原93項。
add_executable(MistwatchExplorerNapeHairTest tests/MistwatchExplorerNapeHairTest.cpp $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
target_include_directories(MistwatchExplorerNapeHairTest PRIVATE include)
target_compile_features(MistwatchExplorerNapeHairTest PRIVATE cxx_std_20)
target_compile_options(MistwatchExplorerNapeHairTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchExplorerNapeHairTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchExplorerNapeHairTest PRIVATE AevryndEngine bcrypt)
add_test(NAME MistwatchExplorerNapeHairTest COMMAND MistwatchExplorerNapeHairTest)
set_tests_properties(MistwatchExplorerNapeHairTest PROPERTIES LABELS "release-gates;current-character-art" TIMEOUT 83)

# 主屋坡板統一來源色帶；原95項、完整木屋物理與負向驗證全部保留。
add_executable(MistwatchRangerRoofBandCpuTest tests/art/RangerRoofBandCpuTest.cpp tests/art/FormalWorldSnapshotIntegration.cpp)
target_include_directories(MistwatchRangerRoofBandCpuTest PRIVATE include tests/art "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRangerRoofBandCpuTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRangerRoofBandCpuTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRangerRoofBandCpuTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchRangerRoofBandCpuTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRangerRoofBandCpuTest COMMAND MistwatchRangerRoofBandCpuTest)
set_tests_properties(MistwatchRangerRoofBandCpuTest PROPERTIES LABELS "release-gates;current-hut-art" TIMEOUT 60)

# 原完整木屋與屋頂回歸保留，另驗固定退襯的板面可見性與封縫。
add_executable(MistwatchRangerHutLiningCpuTest tests/art/RangerHutLiningCpuTest.cpp tests/art/FormalWorldSnapshotIntegration.cpp)
target_include_directories(MistwatchRangerHutLiningCpuTest PRIVATE include tests tests/art "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchRangerHutLiningCpuTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRangerHutLiningCpuTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:precise)
target_compile_definitions(MistwatchRangerHutLiningCpuTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchRangerHutLiningCpuTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRangerHutLiningCpuTest COMMAND MistwatchRangerHutLiningCpuTest)
set_tests_properties(MistwatchRangerHutLiningCpuTest PROPERTIES LABELS "release-gates;current-hut-art" TIMEOUT 60)

# 可攜式完整背包回歸；保留33負控、接觸精度與242姿態／存讀檔。
add_executable(MistwatchBackpackFitCpuTest tests/art/BackpackFitCpuTest.cpp src/MistwatchExplorerNapeHairBinding.cpp)
target_include_directories(MistwatchBackpackFitCpuTest PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchBackpackFitCpuTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBackpackFitCpuTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchBackpackFitCpuTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchBackpackFitCpuTest PRIVATE AevryndEngine)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/backpack-artifacts")
add_test(NAME MistwatchBackpackFitCpuTest COMMAND MistwatchBackpackFitCpuTest "${CMAKE_CURRENT_BINARY_DIR}/backpack-artifacts")
set_tests_properties(MistwatchBackpackFitCpuTest PROPERTIES LABELS "release-gates;current-character-art" TIMEOUT 90 RUN_SERIAL TRUE)

# 新增孤橋復聯三項 CPU 回歸；保留既有 target、依存、labels 與測試門。
add_executable(MistwatchPlayableFlowRulesTest1420 tests/MistwatchPlayableFlowRulesTest1420.cpp)
target_include_directories(MistwatchPlayableFlowRulesTest1420 PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchPlayableFlowRulesTest1420 PRIVATE cxx_std_20)
target_compile_options(MistwatchPlayableFlowRulesTest1420 PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchPlayableFlowRulesTest1420 PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0602)
set_property(TARGET MistwatchPlayableFlowRulesTest1420 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
target_link_libraries(MistwatchPlayableFlowRulesTest1420 PRIVATE AevryndEngine)
add_test(NAME MistwatchPlayableFlowRulesTest1420 COMMAND MistwatchPlayableFlowRulesTest1420)
set_tests_properties(MistwatchPlayableFlowRulesTest1420 PROPERTIES LABELS "release-gates;playable-flow" TIMEOUT 300)

add_executable(MistwatchPlayableFlowSaveTest1420 tests/MistwatchPlayableFlowSaveTest1420.cpp)
target_include_directories(MistwatchPlayableFlowSaveTest1420 PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchPlayableFlowSaveTest1420 PRIVATE cxx_std_20)
target_compile_options(MistwatchPlayableFlowSaveTest1420 PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchPlayableFlowSaveTest1420 PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0602)
set_property(TARGET MistwatchPlayableFlowSaveTest1420 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
target_link_libraries(MistwatchPlayableFlowSaveTest1420 PRIVATE AevryndEngine)
add_test(NAME MistwatchPlayableFlowSaveTest1420 COMMAND "${CMAKE_COMMAND}"
    "-DTEST_EXECUTABLE=$<TARGET_FILE:MistwatchPlayableFlowSaveTest1420>"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunPlayableFlowSaveTest.cmake")
set_tests_properties(MistwatchPlayableFlowSaveTest1420 PROPERTIES LABELS "release-gates;playable-flow" TIMEOUT 300 RUN_SERIAL TRUE)

add_executable(MistwatchPlayableFlowUiTest1436 tests/MistwatchPlayableFlowUiTest1436.cpp)
target_include_directories(MistwatchPlayableFlowUiTest1436 PRIVATE include "${CMAKE_SOURCE_DIR}/engine/native/include")
target_compile_features(MistwatchPlayableFlowUiTest1436 PRIVATE cxx_std_20)
target_compile_options(MistwatchPlayableFlowUiTest1436 PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchPlayableFlowUiTest1436 PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0602)
set_property(TARGET MistwatchPlayableFlowUiTest1436 PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
target_link_libraries(MistwatchPlayableFlowUiTest1436 PRIVATE AevryndEngine gdi32 user32)
add_test(NAME MistwatchPlayableFlowUiTest1436 COMMAND MistwatchPlayableFlowUiTest1436)
set_tests_properties(MistwatchPlayableFlowUiTest1436 PROPERTIES LABELS "release-gates;playable-flow" TIMEOUT 300)

# 部署版本與世界版本分開驗證；不以實際檔案數量猜測版本。
set(AEVRYND_MISTWATCH_QUALIFICATION_RUNTIME_PROFILE "authored-leaf-cluster-full-v3" CACHE STRING
  "Explicit reviewed runtime deployment profile for qualification")
set_property(CACHE AEVRYND_MISTWATCH_QUALIFICATION_RUNTIME_PROFILE PROPERTY STRINGS
  legacy-full-v1 habitat-full-v1 authored-nettle-full-v1 authored-mistwatch-only-v1 authored-boulder-full-v2 authored-boulder-mistwatch-only-v2 authored-leaf-cluster-full-v3)
if(NOT AEVRYND_MISTWATCH_QUALIFICATION_RUNTIME_PROFILE MATCHES
    "^(legacy-full-v1|habitat-full-v1|authored-nettle-full-v1|authored-mistwatch-only-v1|authored-boulder-full-v2|authored-boulder-mistwatch-only-v2|authored-leaf-cluster-full-v3)$")
  message(FATAL_ERROR "Unknown Mistwatch qualification runtime profile")
endif()

# 完整倒木世界 CPU 資格使用本輪正式引擎與資產，不讀舊 Evidence 收據。
# 舊部署使用獨立實體副本，不能與 current418 共用目錄後忽略新增檔案。
# 縮短實體副本路徑，避免未啟用長路徑的 Windows 在複製長檔名時失敗。
# 版本仍以完整 profile 參數及收據核對；短雜湊避免不同版本共用目錄。
string(SHA256 MISTWATCH_QUALIFICATION_PROFILE_KEY "${AEVRYND_MISTWATCH_QUALIFICATION_RUNTIME_PROFILE}")
string(SUBSTRING "${MISTWATCH_QUALIFICATION_PROFILE_KEY}" 0 12 MISTWATCH_QUALIFICATION_PROFILE_KEY)
set(MISTWATCH_QUALIFICATION_FIXTURE_ROOT
  "${CMAKE_CURRENT_BINARY_DIR}/q-$<CONFIG>-${MISTWATCH_QUALIFICATION_PROFILE_KEY}")
add_test(NAME MistwatchQualificationRuntimeStage
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../tools/stage_legacy_runtime_fixture.py"
    --source-root "${CMAKE_SOURCE_DIR}" --runtime-dir "$<TARGET_FILE_DIR:Mistwatch>"
    --out "${MISTWATCH_QUALIFICATION_FIXTURE_ROOT}"
    --profile "${AEVRYND_MISTWATCH_QUALIFICATION_RUNTIME_PROFILE}")
set_tests_properties(MistwatchQualificationRuntimeStage PROPERTIES
  LABELS "qualification-runtime-fixture" TIMEOUT 120 RUN_SERIAL TRUE
  FIXTURES_SETUP MistwatchQualificationRuntimePrepared)
add_test(NAME MistwatchLegacyRuntimeFixtureIoTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_legacy_runtime_fixture.py"
    --helper-source-root "${CMAKE_SOURCE_DIR}"
    --stage-script "${CMAKE_CURRENT_SOURCE_DIR}/../tools/stage_legacy_runtime_fixture.py")
set_tests_properties(MistwatchLegacyRuntimeFixtureIoTest PROPERTIES
  LABELS "qualification-runtime-fixture-io" TIMEOUT 120 RUN_SERIAL TRUE)

add_executable(MistwatchFallenWorldQualificationTest tests/MistwatchFallenWorldQualificationTest.cpp)
target_include_directories(MistwatchFallenWorldQualificationTest PRIVATE include)
target_compile_features(MistwatchFallenWorldQualificationTest PRIVATE cxx_std_20)
target_compile_options(MistwatchFallenWorldQualificationTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchFallenWorldQualificationTest PRIVATE
  MISTWATCH_FALLEN_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
  MISTWATCH_FALLEN_CONFIGURATION="$<CONFIG>"
  _WIN32_WINNT=0x0602)
set_property(TARGET MistwatchFallenWorldQualificationTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchFallenWorldQualificationTest PRIVATE AevryndEngine)
# 部署使用明確版本清單；下方仍驗原四樹世界，不將新部署冒稱新世界資格。
add_dependencies(MistwatchFallenWorldQualificationTest Mistwatch)
add_test(NAME MistwatchFallenWorldQualificationTest
  COMMAND MistwatchFallenWorldQualificationTest
    --source-root "${CMAKE_SOURCE_DIR}"
    --runtime-assets "${MISTWATCH_QUALIFICATION_FIXTURE_ROOT}/assets"
    --out "${CMAKE_CURRENT_BINARY_DIR}/fallen-cpu-certificate"
    --runtime-profile "${AEVRYND_MISTWATCH_QUALIFICATION_RUNTIME_PROFILE}"
    --generated-root-stem "${MISTWATCH_ROOT_OUTPUT}"
    --world-cohort legacy-four-v1)
set_tests_properties(MistwatchFallenWorldQualificationTest PROPERTIES
  LABELS "release-gates;fallen-world" TIMEOUT 900 RUN_SERIAL TRUE
  FIXTURES_REQUIRED MistwatchQualificationRuntimePrepared
  FIXTURES_SETUP MistwatchFallenCpuExpected)

# 放在正式 MistwatchFallenWorldQualificationTest 與兩個原生 add_test 註冊之後。
# 同一避雨棚完整幾何、工具台與所有三角 UV 手性回歸；不是美術通過證明。
add_executable(MistwatchShelterWeatheredUvTest tests/MistwatchShelterWeatheredUvTest.cpp)
target_include_directories(MistwatchShelterWeatheredUvTest PRIVATE include)
target_compile_features(MistwatchShelterWeatheredUvTest PRIVATE cxx_std_20)
target_compile_options(MistwatchShelterWeatheredUvTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchShelterWeatheredUvTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchShelterWeatheredUvTest PRIVATE AevryndEngine)
add_test(NAME MistwatchShelterWeatheredUvTest COMMAND MistwatchShelterWeatheredUvTest)
set_tests_properties(MistwatchShelterWeatheredUvTest PROPERTIES LABELS "release-gates;current-shelter-art" TIMEOUT 60)

add_executable(MistwatchWorkshopVisualTest tests/MistwatchWorkshopVisualTest.cpp)
target_include_directories(MistwatchWorkshopVisualTest PRIVATE include)
target_compile_features(MistwatchWorkshopVisualTest PRIVATE cxx_std_20)
target_compile_options(MistwatchWorkshopVisualTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchWorkshopVisualTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchWorkshopVisualTest PRIVATE AevryndEngine)
add_test(NAME MistwatchWorkshopVisualTest COMMAND MistwatchWorkshopVisualTest)
set_tests_properties(MistwatchWorkshopVisualTest PROPERTIES LABELS "release-gates;current-workshop-art" TIMEOUT 60)

add_executable(MistwatchWorkshopFrameTest tests/MistwatchWorkshopFrameTest.cpp)
target_include_directories(MistwatchWorkshopFrameTest PRIVATE include)
target_compile_features(MistwatchWorkshopFrameTest PRIVATE cxx_std_20)
target_compile_options(MistwatchWorkshopFrameTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchWorkshopFrameTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchWorkshopFrameTest PRIVATE AevryndEngine)
add_test(NAME MistwatchWorkshopFrameTest COMMAND MistwatchWorkshopFrameTest)
set_tests_properties(MistwatchWorkshopFrameTest PROPERTIES LABELS "release-gates;current-workshop-art" TIMEOUT 60)

add_executable(MistwatchQualificationRuntimeTest tests/qualification/MistwatchQualificationRuntimeTest.cpp)
target_include_directories(MistwatchQualificationRuntimeTest PRIVATE include)
target_compile_features(MistwatchQualificationRuntimeTest PRIVATE cxx_std_20)
target_compile_options(MistwatchQualificationRuntimeTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchQualificationRuntimeTest PRIVATE
  MISTWATCH_QUALIFICATION_SOURCE_ROOT="${CMAKE_SOURCE_DIR}"
  UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchQualificationRuntimeTest PRIVATE AevryndEngine)
add_dependencies(MistwatchQualificationRuntimeTest Mistwatch)
add_test(NAME MistwatchQualificationRuntimeTest
  COMMAND MistwatchQualificationRuntimeTest
    --source-root "${CMAKE_SOURCE_DIR}"
    --runtime-assets "${MISTWATCH_QUALIFICATION_FIXTURE_ROOT}/assets"
    --runtime-profile "${AEVRYND_MISTWATCH_QUALIFICATION_RUNTIME_PROFILE}"
    --generated-root-stem "${MISTWATCH_ROOT_OUTPUT}")
set_tests_properties(MistwatchQualificationRuntimeTest PROPERTIES
  LABELS "release-gates;qualification-runtime" TIMEOUT 120 RUN_SERIAL TRUE
  FIXTURES_REQUIRED MistwatchQualificationRuntimePrepared)

# 新作者世界的完整 typed 碰撞與草來源映射；不取代原四樹世界證書。
add_executable(MistwatchAuthoredWorldQualificationTest tests/qualification/MistwatchAuthoredWorldQualificationTest.cpp)
target_include_directories(MistwatchAuthoredWorldQualificationTest PRIVATE include)
target_compile_features(MistwatchAuthoredWorldQualificationTest PRIVATE cxx_std_20)
target_compile_options(MistwatchAuthoredWorldQualificationTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchAuthoredWorldQualificationTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchAuthoredWorldQualificationTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchAuthoredWorldQualificationTest PRIVATE AevryndEngine)
add_dependencies(MistwatchAuthoredWorldQualificationTest Mistwatch)
add_test(NAME MistwatchAuthoredWorldQualificationTest
  COMMAND MistwatchAuthoredWorldQualificationTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchAuthoredWorldQualificationTest PROPERTIES
  LABELS "release-gates;authored-world-qualification" TIMEOUT 600 RUN_SERIAL TRUE)

# 灌木候選保留原64族群前綴，獨立驗證128株及兩橋完整體積，不取代原資格門。
add_executable(MistwatchShrubDensityQualificationTest tests/qualification/MistwatchShrubDensityQualificationTest.cpp)
target_include_directories(MistwatchShrubDensityQualificationTest PRIVATE include)
target_compile_features(MistwatchShrubDensityQualificationTest PRIVATE cxx_std_20)
target_compile_options(MistwatchShrubDensityQualificationTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchShrubDensityQualificationTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchShrubDensityQualificationTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchShrubDensityQualificationTest PRIVATE AevryndEngine)
add_dependencies(MistwatchShrubDensityQualificationTest Mistwatch)
add_test(NAME MistwatchShrubDensityQualificationTest
  COMMAND MistwatchShrubDensityQualificationTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchShrubDensityQualificationTest PROPERTIES
  LABELS "release-gates;shrub-density-candidate" TIMEOUT 600 RUN_SERIAL TRUE)

# 有限搜尋診斷只證明 trace 不改結果；不把128短缺改成候選通過。
add_executable(MistwatchShrubSearchTraceTest tests/qualification/MistwatchShrubSearchTraceTest.cpp)
target_include_directories(MistwatchShrubSearchTraceTest PRIVATE include)
target_compile_features(MistwatchShrubSearchTraceTest PRIVATE cxx_std_20)
target_compile_options(MistwatchShrubSearchTraceTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchShrubSearchTraceTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchShrubSearchTraceTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchShrubSearchTraceTest PRIVATE AevryndEngine)
add_dependencies(MistwatchShrubSearchTraceTest Mistwatch)
add_test(NAME MistwatchShrubSearchTraceTest
  COMMAND MistwatchShrubSearchTraceTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchShrubSearchTraceTest PROPERTIES
  LABELS "forest-diagnostics;shrub-search-trace" TIMEOUT 600 RUN_SERIAL TRUE)

# 原外環回歸與16株樹種候選資格分開；短缺不可由原外環通過掩蓋。
add_executable(MistwatchLandscapeSpeciesTest tests/MistwatchLandscapeSpeciesTest.cpp)
target_include_directories(MistwatchLandscapeSpeciesTest PRIVATE include)
target_compile_features(MistwatchLandscapeSpeciesTest PRIVATE cxx_std_20)
target_compile_options(MistwatchLandscapeSpeciesTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchLandscapeSpeciesTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchLandscapeSpeciesTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchLandscapeSpeciesTest PRIVATE AevryndEngine)
add_dependencies(MistwatchLandscapeSpeciesTest Mistwatch)
add_test(NAME MistwatchLandscapeSpeciesOriginalTest
  COMMAND MistwatchLandscapeSpeciesTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets" --mode original)
add_test(NAME MistwatchLandscapeSpeciesCandidateTest
  COMMAND MistwatchLandscapeSpeciesTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets" --mode candidate)
set_tests_properties(MistwatchLandscapeSpeciesOriginalTest MistwatchLandscapeSpeciesCandidateTest PROPERTIES
  LABELS "forest-diagnostics;landscape-species" TIMEOUT 120 RUN_SERIAL TRUE)

# 新作者位置與真正貼坡樹根獨立驗收；保留舊固定位置候選的失敗門。
add_executable(MistwatchTerrainPlantingTest tests/MistwatchTerrainPlantingTest.cpp)
target_include_directories(MistwatchTerrainPlantingTest PRIVATE include)
target_compile_features(MistwatchTerrainPlantingTest PRIVATE cxx_std_20)
target_compile_options(MistwatchTerrainPlantingTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchTerrainPlantingTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchTerrainPlantingTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchTerrainPlantingTest PRIVATE AevryndEngine)
add_dependencies(MistwatchTerrainPlantingTest Mistwatch)
add_test(NAME MistwatchTerrainPlantingTest
  COMMAND MistwatchTerrainPlantingTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchTerrainPlantingTest PROPERTIES
  LABELS "forest-diagnostics;terrain-planting" TIMEOUT 120 RUN_SERIAL TRUE)

# 原材質足帶追蹤的新支撐契約獨立測試，不取代舊水平接地門。
add_executable(MistwatchRootSupportTest tests/MistwatchRootSupportTest.cpp)
target_include_directories(MistwatchRootSupportTest PRIVATE include)
target_compile_features(MistwatchRootSupportTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRootSupportTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchRootSupportTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchRootSupportTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchRootSupportTest PRIVATE AevryndEngine)
add_dependencies(MistwatchRootSupportTest Mistwatch)
add_test(NAME MistwatchRootSupportTest COMMAND MistwatchRootSupportTest)
add_test(NAME MistwatchRootSupportAssetTest
  COMMAND MistwatchRootSupportTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchRootSupportTest MistwatchRootSupportAssetTest PROPERTIES
  LABELS "forest-diagnostics;root-support" TIMEOUT 120 RUN_SERIAL TRUE)

# 新作者十六株用正式材質地表驗證；舊候選及其失敗契約保持獨立。
add_executable(MistwatchAuthoredLandscapePlantingTest tests/MistwatchAuthoredLandscapePlantingTest.cpp)
add_executable(MistwatchMixedLandscapeCanopyTest tests/MistwatchMixedLandscapeCanopyTest.cpp)
add_executable(MistwatchLandscapeCrownGrowthTest tests/MistwatchLandscapeCrownGrowthTest.cpp)
add_executable(MistwatchCoreHerbClustersTest tests/MistwatchCoreHerbClustersTest.cpp)
target_include_directories(MistwatchCoreHerbClustersTest PRIVATE include)
target_compile_features(MistwatchCoreHerbClustersTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCoreHerbClustersTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchCoreHerbClustersTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchCoreHerbClustersTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchCoreHerbClustersTest PRIVATE AevryndEngine)
add_dependencies(MistwatchCoreHerbClustersTest Mistwatch)
add_test(NAME MistwatchCoreHerbClustersTest COMMAND MistwatchCoreHerbClustersTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchCoreHerbClustersTest PROPERTIES LABELS "forest-diagnostics;core-herb-candidate" TIMEOUT 180 RUN_SERIAL TRUE)

foreach(test_name IN ITEMS MistwatchCoreHerbDrawTest MistwatchCoreHerbSupportTest MistwatchForestLightingTest MistwatchForestLeafDomainTest MistwatchCoreHerbRouteLayoutTest MistwatchUnderstoryRetentionTest MistwatchGroundPointSamplerTest MistwatchRouteGrassDrawTest MistwatchRouteGrassSupportTest MistwatchRouteGrassContextTest)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_include_directories(${test_name} PRIVATE include)
  target_compile_features(${test_name} PRIVATE cxx_std_20)
  target_compile_options(${test_name} PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
  target_compile_definitions(${test_name} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_property(TARGET ${test_name} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_libraries(${test_name} PRIVATE AevryndEngine)
  if(test_name STREQUAL "MistwatchCoreHerbSupportTest")
    add_dependencies(${test_name} Mistwatch)
    add_test(NAME ${test_name} COMMAND ${test_name} --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
  else()
    add_test(NAME ${test_name} COMMAND ${test_name})
  endif()
  set_tests_properties(${test_name} PROPERTIES LABELS "forest-diagnostics;core-herb-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
endforeach()
target_include_directories(MistwatchLandscapeCrownGrowthTest PRIVATE include)
target_compile_features(MistwatchLandscapeCrownGrowthTest PRIVATE cxx_std_20)
target_compile_options(MistwatchLandscapeCrownGrowthTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchLandscapeCrownGrowthTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchLandscapeCrownGrowthTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchLandscapeCrownGrowthTest PRIVATE AevryndEngine)
add_dependencies(MistwatchLandscapeCrownGrowthTest Mistwatch)
add_test(NAME MistwatchLandscapeCrownGrowthTest COMMAND MistwatchLandscapeCrownGrowthTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchLandscapeCrownGrowthTest PROPERTIES LABELS "forest-diagnostics;crown-growth-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
target_include_directories(MistwatchMixedLandscapeCanopyTest PRIVATE include)
target_compile_features(MistwatchMixedLandscapeCanopyTest PRIVATE cxx_std_20)
target_compile_options(MistwatchMixedLandscapeCanopyTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchMixedLandscapeCanopyTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchMixedLandscapeCanopyTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchMixedLandscapeCanopyTest PRIVATE AevryndEngine)
add_dependencies(MistwatchMixedLandscapeCanopyTest Mistwatch)
add_test(NAME MistwatchMixedLandscapeCanopyTest COMMAND MistwatchMixedLandscapeCanopyTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchMixedLandscapeCanopyTest PROPERTIES LABELS "forest-diagnostics;mixed-landscape-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
target_include_directories(MistwatchAuthoredLandscapePlantingTest PRIVATE include)
target_compile_features(MistwatchAuthoredLandscapePlantingTest PRIVATE cxx_std_20)
target_compile_options(MistwatchAuthoredLandscapePlantingTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchAuthoredLandscapePlantingTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchAuthoredLandscapePlantingTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchAuthoredLandscapePlantingTest PRIVATE AevryndEngine)
add_dependencies(MistwatchAuthoredLandscapePlantingTest Mistwatch)
add_test(NAME MistwatchAuthoredLandscapePlantingTest
  COMMAND MistwatchAuthoredLandscapePlantingTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchAuthoredLandscapePlantingTest PROPERTIES
  LABELS "forest-diagnostics;authored-landscape" TIMEOUT 120 RUN_SERIAL TRUE)

# 新版完整原生預期由獨立 CPU context 生成；不替換原 native70／四樹門。
add_executable(MistwatchDeciduousHabitatFloorTest tests/MistwatchDeciduousHabitatFloorTest.cpp)
target_include_directories(MistwatchDeciduousHabitatFloorTest PRIVATE include)
target_compile_features(MistwatchDeciduousHabitatFloorTest PRIVATE cxx_std_20)
target_compile_options(MistwatchDeciduousHabitatFloorTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchDeciduousHabitatFloorTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchDeciduousHabitatFloorTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchDeciduousHabitatFloorTest PRIVATE AevryndEngine)
add_dependencies(MistwatchDeciduousHabitatFloorTest Mistwatch)
add_test(NAME MistwatchDeciduousHabitatFloorTest
  COMMAND MistwatchDeciduousHabitatFloorTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchDeciduousHabitatFloorTest PROPERTIES
  LABELS "forest-diagnostics;deciduous-habitat" TIMEOUT 180 RUN_SERIAL TRUE)

# 新版完整原生預期由獨立 CPU context 生成；不替換原 native70／四樹門。
add_executable(MistwatchEntranceCompositionTest tests/MistwatchEntranceCompositionTest.cpp)
target_include_directories(MistwatchEntranceCompositionTest PRIVATE include)
target_compile_features(MistwatchEntranceCompositionTest PRIVATE cxx_std_20)
target_compile_options(MistwatchEntranceCompositionTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchEntranceCompositionTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchEntranceCompositionTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchEntranceCompositionTest PRIVATE AevryndEngine)
add_dependencies(MistwatchEntranceCompositionTest Mistwatch)
add_test(NAME MistwatchEntranceCompositionTest
  COMMAND MistwatchEntranceCompositionTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchEntranceCompositionTest PROPERTIES
  LABELS "forest-diagnostics;entrance-composition" TIMEOUT 180 RUN_SERIAL TRUE)

# 新低草作者使用獨立兩態世界驗證，保留原大地物與草地品質門。
add_executable(MistwatchRoadsideCoverTest tests/MistwatchRoadsideCoverTest.cpp)
target_include_directories(MistwatchRoadsideCoverTest PRIVATE include)
target_compile_features(MistwatchRoadsideCoverTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRoadsideCoverTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchRoadsideCoverTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchRoadsideCoverTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchRoadsideCoverTest PRIVATE AevryndEngine)
add_dependencies(MistwatchRoadsideCoverTest Mistwatch)
add_test(NAME MistwatchRoadsideCoverTest COMMAND MistwatchRoadsideCoverTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchRoadsideCoverTest PROPERTIES LABELS "forest-diagnostics;roadside-cover" TIMEOUT 180 RUN_SERIAL TRUE)

add_executable(MistwatchRoadsideBroadleafTest tests/MistwatchRoadsideBroadleafTest.cpp)
target_include_directories(MistwatchRoadsideBroadleafTest PRIVATE include)
target_compile_features(MistwatchRoadsideBroadleafTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRoadsideBroadleafTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchRoadsideBroadleafTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchRoadsideBroadleafTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchRoadsideBroadleafTest PRIVATE AevryndEngine)
add_dependencies(MistwatchRoadsideBroadleafTest Mistwatch)
add_test(NAME MistwatchRoadsideBroadleafTest COMMAND MistwatchRoadsideBroadleafTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchRoadsideBroadleafTest PROPERTIES LABELS "forest-diagnostics;roadside-broadleaf" TIMEOUT 180 RUN_SERIAL TRUE)

add_executable(MistwatchAuthoredNativeExpectedTest tests/qualification/MistwatchAuthoredNativeExpectedTest.cpp)

add_executable(MistwatchRepairShelterDetailTest tests/MistwatchRepairShelterDetailTest.cpp)
target_include_directories(MistwatchRepairShelterDetailTest PRIVATE include)
target_compile_features(MistwatchRepairShelterDetailTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRepairShelterDetailTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchRepairShelterDetailTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchRepairShelterDetailTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchRepairShelterDetailTest PRIVATE AevryndEngine)
add_test(NAME MistwatchRepairShelterDetailTest COMMAND MistwatchRepairShelterDetailTest)
set_tests_properties(MistwatchRepairShelterDetailTest PROPERTIES LABELS "release-gates;current-shelter-art" TIMEOUT 60)
add_executable(MistwatchBoulderDetailTest tests/MistwatchBoulderDetailTest.cpp)
target_include_directories(MistwatchBoulderDetailTest PRIVATE include)
target_compile_features(MistwatchBoulderDetailTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBoulderDetailTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchBoulderDetailTest PRIVATE MISTWATCH_SOURCE_ROOT="${CMAKE_SOURCE_DIR}" UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchBoulderDetailTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchBoulderDetailTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBoulderDetailTest COMMAND MistwatchBoulderDetailTest)
set_tests_properties(MistwatchBoulderDetailTest PROPERTIES LABELS "release-gates;current-boulder-art" TIMEOUT 60)

add_executable(MistwatchInputReplayTest tests/MistwatchInputReplayTest.cpp)
target_include_directories(MistwatchInputReplayTest PRIVATE include)
target_compile_features(MistwatchInputReplayTest PRIVATE cxx_std_20)
target_compile_options(MistwatchInputReplayTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchInputReplayTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchInputReplayTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchInputReplayTest PRIVATE AevryndEngine)
add_test(NAME MistwatchInputReplayTest COMMAND MistwatchInputReplayTest)
set_tests_properties(MistwatchInputReplayTest PROPERTIES LABELS "release-gates;production-input-replay" TIMEOUT 30)

add_executable(MistwatchEntranceUnderstoryTest tests/MistwatchEntranceUnderstoryTest.cpp)
target_include_directories(MistwatchEntranceUnderstoryTest PRIVATE include)
target_compile_features(MistwatchEntranceUnderstoryTest PRIVATE cxx_std_20)
target_compile_options(MistwatchEntranceUnderstoryTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchEntranceUnderstoryTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchEntranceUnderstoryTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchEntranceUnderstoryTest PRIVATE AevryndEngine)
add_dependencies(MistwatchEntranceUnderstoryTest Mistwatch)
add_test(NAME MistwatchEntranceUnderstoryTest COMMAND MistwatchEntranceUnderstoryTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchEntranceUnderstoryTest PROPERTIES LABELS "forest-diagnostics;entrance-understory" TIMEOUT 180 RUN_SERIAL TRUE)

add_executable(MistwatchEntranceRoadDomainTest tests/MistwatchEntranceRoadDomainTest.cpp)
target_include_directories(MistwatchEntranceRoadDomainTest PRIVATE include)
target_compile_features(MistwatchEntranceRoadDomainTest PRIVATE cxx_std_20)
target_compile_options(MistwatchEntranceRoadDomainTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchEntranceRoadDomainTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchEntranceRoadDomainTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchEntranceRoadDomainTest PRIVATE AevryndEngine)
add_dependencies(MistwatchEntranceRoadDomainTest Mistwatch)
add_test(NAME MistwatchEntranceRoadDomainTest COMMAND MistwatchEntranceRoadDomainTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchEntranceRoadDomainTest PROPERTIES LABELS "forest-diagnostics;entrance-road" TIMEOUT 180 RUN_SERIAL TRUE)

add_executable(MistwatchSettlementGroundTest tests/MistwatchSettlementGroundTest.cpp)
target_include_directories(MistwatchSettlementGroundTest PRIVATE include)
target_compile_features(MistwatchSettlementGroundTest PRIVATE cxx_std_20)
target_compile_options(MistwatchSettlementGroundTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchSettlementGroundTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchSettlementGroundTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchSettlementGroundTest PRIVATE AevryndEngine)
add_dependencies(MistwatchSettlementGroundTest Mistwatch)
add_test(NAME MistwatchSettlementGroundTest COMMAND MistwatchSettlementGroundTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchSettlementGroundTest PROPERTIES LABELS "forest-diagnostics;settlement-ground-candidate" TIMEOUT 180 RUN_SERIAL TRUE)
add_executable(MistwatchReplayCostProbeTest tests/MistwatchReplayCostProbeTest.cpp)
target_include_directories(MistwatchReplayCostProbeTest PRIVATE include)
target_compile_features(MistwatchReplayCostProbeTest PRIVATE cxx_std_20)
target_compile_options(MistwatchReplayCostProbeTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchReplayCostProbeTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchReplayCostProbeTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchReplayCostProbeTest PRIVATE AevryndEngine)
add_test(NAME MistwatchReplayCostProbeTest COMMAND MistwatchReplayCostProbeTest)
set_tests_properties(MistwatchReplayCostProbeTest PROPERTIES LABELS "release-gates;replay-cost-diagnostics" TIMEOUT 120 RUN_SERIAL TRUE)

add_executable(MistwatchCaptureGpuBenchmarkTest tests/MistwatchCaptureGpuBenchmarkTest.cpp)
target_include_directories(MistwatchCaptureGpuBenchmarkTest PRIVATE include)
target_compile_features(MistwatchCaptureGpuBenchmarkTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCaptureGpuBenchmarkTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchCaptureGpuBenchmarkTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchCaptureGpuBenchmarkTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchCaptureGpuBenchmarkTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCaptureGpuBenchmarkTest COMMAND MistwatchCaptureGpuBenchmarkTest)
set_tests_properties(MistwatchCaptureGpuBenchmarkTest PROPERTIES LABELS "capture-benchmark-cpu" TIMEOUT 60)

add_executable(MistwatchCapturePresentationTest tests/MistwatchCapturePresentationTest.cpp)
target_include_directories(MistwatchCapturePresentationTest PRIVATE include)
target_compile_features(MistwatchCapturePresentationTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCapturePresentationTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchCapturePresentationTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchCapturePresentationTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchCapturePresentationTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCapturePresentationTest COMMAND MistwatchCapturePresentationTest)
set_tests_properties(MistwatchCapturePresentationTest PROPERTIES LABELS "capture-benchmark-cpu" TIMEOUT 60)

target_include_directories(MistwatchAuthoredNativeExpectedTest PRIVATE include)
target_compile_features(MistwatchAuthoredNativeExpectedTest PRIVATE cxx_std_20)
target_compile_options(MistwatchAuthoredNativeExpectedTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchAuthoredNativeExpectedTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchAuthoredNativeExpectedTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchAuthoredNativeExpectedTest PRIVATE AevryndEngine)
add_dependencies(MistwatchAuthoredNativeExpectedTest Mistwatch)
add_test(NAME MistwatchAuthoredNativeExpectedTest
  COMMAND MistwatchAuthoredNativeExpectedTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchAuthoredNativeExpectedTest PROPERTIES
  LABELS "release-gates;authored-native-expectation" TIMEOUT 900 RUN_SERIAL TRUE)

# 新版 JSON 與 capture 投影的有限負控，舊收據測試保持原樣。
add_test(NAME MistwatchAuthoredNativeReceiptTest
  COMMAND "${MISTWATCH_RECEIPT_PWSH}" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/MistwatchAuthoredNativeReceiptTest.ps1")
set_tests_properties(MistwatchAuthoredNativeReceiptTest PROPERTIES
  LABELS "release-gates;authored-native-expectation" TIMEOUT 120 RUN_SERIAL TRUE)

# 不取代既有 LABELS、RUN_SERIAL、RESOURCE_LOCK、TIMEOUT 與任何原本測試。
add_executable(MistwatchLandscapeCrownRangeTest tests/MistwatchLandscapeCrownRangeTest.cpp)
target_include_directories(MistwatchLandscapeCrownRangeTest PRIVATE include)
target_compile_features(MistwatchLandscapeCrownRangeTest PRIVATE cxx_std_20)
target_compile_options(MistwatchLandscapeCrownRangeTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchLandscapeCrownRangeTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchLandscapeCrownRangeTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchLandscapeCrownRangeTest PRIVATE AevryndEngine)
add_test(NAME MistwatchLandscapeCrownRangeTest COMMAND MistwatchLandscapeCrownRangeTest)
set_tests_properties(MistwatchLandscapeCrownRangeTest PROPERTIES TIMEOUT 60 LABELS "landscape-crown-range-candidate")

# 新群落先作獨立 CPU 作者驗證，不替代既有八株世界或正式執行預設。
add_executable(MistwatchWoodlandCanopyTest tests/MistwatchWoodlandCanopyTest.cpp)
target_include_directories(MistwatchWoodlandCanopyTest PRIVATE include)
target_compile_features(MistwatchWoodlandCanopyTest PRIVATE cxx_std_20)
target_compile_options(MistwatchWoodlandCanopyTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchWoodlandCanopyTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchWoodlandCanopyTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchWoodlandCanopyTest PRIVATE AevryndEngine)
add_dependencies(MistwatchWoodlandCanopyTest Mistwatch)
add_test(NAME MistwatchWoodlandCanopyTest
  COMMAND MistwatchWoodlandCanopyTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchWoodlandCanopyTest PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE LABELS "woodland-canopy-author-candidate")

# 十二株木件維持每株原10000上限，以完整有界分組驗證，不放寬舊單span介面。
add_executable(MistwatchGroundMicroreliefTest tests/MistwatchGroundMicroreliefTest.cpp)
target_include_directories(MistwatchGroundMicroreliefTest PRIVATE include)
target_compile_features(MistwatchGroundMicroreliefTest PRIVATE cxx_std_20)
target_compile_options(MistwatchGroundMicroreliefTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
set_property(TARGET MistwatchGroundMicroreliefTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchGroundMicroreliefTest COMMAND MistwatchGroundMicroreliefTest)
set_tests_properties(MistwatchGroundMicroreliefTest PROPERTIES TIMEOUT 60 LABELS "ground-microrelief-art-policy")

add_executable(MistwatchForestIblTest tests/MistwatchForestIblTest.cpp)
target_include_directories(MistwatchForestIblTest PRIVATE include)
target_compile_features(MistwatchForestIblTest PRIVATE cxx_std_20)
target_compile_options(MistwatchForestIblTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchForestIblTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchForestIblTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchForestIblTest PRIVATE AevryndEngine)
add_test(NAME MistwatchForestIblTest COMMAND MistwatchForestIblTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
set_tests_properties(MistwatchForestIblTest PROPERTIES TIMEOUT 60 LABELS "forest-ibl-identity-cpu")

# 十二株木件維持每株原10000上限，以完整有界分組驗證，不放寬舊單span介面。
add_executable(MistwatchGroupedCanopyVolumeTest tests/MistwatchGroupedCanopyVolumeTest.cpp)
target_include_directories(MistwatchGroupedCanopyVolumeTest PRIVATE include)
target_compile_features(MistwatchGroupedCanopyVolumeTest PRIVATE cxx_std_20)
target_compile_options(MistwatchGroupedCanopyVolumeTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchGroupedCanopyVolumeTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchGroupedCanopyVolumeTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchGroupedCanopyVolumeTest PRIVATE AevryndEngine)
add_test(NAME MistwatchGroupedCanopyVolumeTest COMMAND MistwatchGroupedCanopyVolumeTest)
set_tests_properties(MistwatchGroupedCanopyVolumeTest PROPERTIES TIMEOUT 60 LABELS "woodland-grouped-volume-candidate")

# 正常預設不變；純政策完整核對手動opt-in及兩種互斥隔離上下文。
add_executable(MistwatchWoodlandProfileTest tests/MistwatchWoodlandProfileTest.cpp)
target_include_directories(MistwatchWoodlandProfileTest PRIVATE include)
target_compile_features(MistwatchWoodlandProfileTest PRIVATE cxx_std_20)
target_compile_options(MistwatchWoodlandProfileTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
set_property(TARGET MistwatchWoodlandProfileTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchWoodlandProfileTest COMMAND MistwatchWoodlandProfileTest)
set_tests_properties(MistwatchWoodlandProfileTest PROPERTIES TIMEOUT 60 LABELS "woodland-profile-policy")

add_executable(MistwatchCliffWorldProfileTest tests/MistwatchCliffWorldProfileTest.cpp)
target_include_directories(MistwatchCliffWorldProfileTest PRIVATE include)
target_compile_features(MistwatchCliffWorldProfileTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCliffWorldProfileTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
set_property(TARGET MistwatchCliffWorldProfileTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchCliffWorldProfileTest COMMAND MistwatchCliffWorldProfileTest)
set_tests_properties(MistwatchCliffWorldProfileTest PROPERTIES TIMEOUT 60 LABELS "cliff-world-profile-policy")

foreach(test_name MistwatchWestCliffReliefTest MistwatchWestCliffLoadTest MistwatchWestCliffClearanceTest)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_include_directories(${test_name} PRIVATE include)
  target_link_libraries(${test_name} PRIVATE AevryndEngine)
  target_compile_features(${test_name} PRIVATE cxx_std_20)
  target_compile_options(${test_name} PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
  set_property(TARGET ${test_name} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  add_test(NAME ${test_name} COMMAND ${test_name})
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 180 LABELS "west-cliff-relief-cpu")
endforeach()

# 日常CPU整合消費已封印runtime資產，不啟動或重建Main以冒充原生畫面。
foreach(test_name IN ITEMS MistwatchGroupedVegetationApiTest MistwatchWoodlandRoadsideApiTest MistwatchWoodlandDownstreamTest)
  add_executable(${test_name} tests/${test_name}.cpp)
  target_include_directories(${test_name} PRIVATE include)
  target_compile_features(${test_name} PRIVATE cxx_std_20)
  target_compile_options(${test_name} PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
  target_compile_definitions(${test_name} PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
  set_property(TARGET ${test_name} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_libraries(${test_name} PRIVATE AevryndEngine)
  add_test(NAME ${test_name} COMMAND ${test_name} --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
  set_tests_properties(${test_name} PROPERTIES TIMEOUT 900 RUN_SERIAL TRUE LABELS "woodland-downstream-candidate")
endforeach()

add_executable(MistwatchTuningInputRepeatTest tests/MistwatchTuningInputRepeatTest.cpp)
target_include_directories(MistwatchTuningInputRepeatTest PRIVATE include)
target_compile_features(MistwatchTuningInputRepeatTest PRIVATE cxx_std_20)
target_compile_options(MistwatchTuningInputRepeatTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchTuningInputRepeatTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchTuningInputRepeatTest PRIVATE AevryndEngine)
set_property(TARGET MistwatchTuningInputRepeatTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchTuningInputRepeatTest COMMAND MistwatchTuningInputRepeatTest)
set_tests_properties(MistwatchTuningInputRepeatTest PROPERTIES TIMEOUT 30 LABELS "release-gates;gameplay-input-repeat")

if(AEVRYND_MISTWATCH_NATIVE_GATES)
  set_tests_properties(MistwatchNativeNormal32 MistwatchNativeGbv17 PROPERTIES
    FIXTURES_REQUIRED MistwatchFallenCpuExpected
    DEPENDS MistwatchFallenWorldQualificationTest)
endif()

# 新版森林獨立建立預期，不將舊四樹證書改名後用於新版場景。
add_executable(MistwatchCurrentWorldExpectedTest tests/qualification/MistwatchCurrentWorldExpectedTest.cpp)
target_include_directories(MistwatchCurrentWorldExpectedTest PRIVATE include)
target_compile_features(MistwatchCurrentWorldExpectedTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCurrentWorldExpectedTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict)
target_compile_definitions(MistwatchCurrentWorldExpectedTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchCurrentWorldExpectedTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_options(MistwatchCurrentWorldExpectedTest PRIVATE /STACK:16777216)
target_link_libraries(MistwatchCurrentWorldExpectedTest PRIVATE AevryndEngine)
add_dependencies(MistwatchCurrentWorldExpectedTest Mistwatch)

add_test(NAME MistwatchCurrentWorldReceiptTest
  COMMAND "${MISTWATCH_RECEIPT_PWSH}" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
    -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/MistwatchCurrentWorldReceiptTest.ps1")
set_tests_properties(MistwatchCurrentWorldReceiptTest PROPERTIES
  LABELS "current-world-receipt" TIMEOUT 180 RUN_SERIAL TRUE)

option(AEVRYND_MISTWATCH_CURRENT_NATIVE_GATES "Current authored forest independent CPU certificate and complete native scene matrices" OFF)
if(AEVRYND_MISTWATCH_CURRENT_NATIVE_GATES)
  if(AEVRYND_INTERNAL_RENDERER_TEST_HOOKS)
    message(FATAL_ERROR "Current Mistwatch production matrices require renderer hooks OFF")
  endif()
  set(MISTWATCH_CURRENT_CERTIFICATE "${CMAKE_CURRENT_BINARY_DIR}/current-world-certificate")
  if(AEVRYND_MISTWATCH_RUNTIME_ASSET_COHORT STREQUAL "authored-current-forest-full-v5")
    set(MISTWATCH_CURRENT_RUNTIME_STAGE_COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../tools/stage_current_runtime.py"
      --source-root "${CMAKE_SOURCE_DIR}" --runtime-dir "$<TARGET_FILE_DIR:Mistwatch>")
  else()
    if(NOT MISTWATCH_VALLEY_STARTUP_BAKE_FILE)
      message(FATAL_ERROR "Current423 full runtime gates require explicit MISTWATCH_VALLEY_STARTUP_BAKE_FILE; legacy418 is a separately named compatibility cohort")
    endif()
    set(MISTWATCH_CURRENT_RUNTIME_STAGE_COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../tools/stage_forest_grove_runtime.py"
      --source-root "${CMAKE_SOURCE_DIR}" --runtime-dir "$<TARGET_FILE_DIR:Mistwatch>"
      --mode stage --startup-bake-file "${MISTWATCH_VALLEY_STARTUP_BAKE_FILE}")
    message(STATUS "Current423 asset staging selected; original418 CPU/Native49 contracts remain legacy-cohort checks, not current423 qualification")
  endif()
  add_custom_target(MistwatchCurrentRuntimeStage
    COMMAND ${MISTWATCH_CURRENT_RUNTIME_STAGE_COMMAND}
    DEPENDS Mistwatch VERBATIM)
  add_dependencies(MistwatchCurrentWorldExpectedTest MistwatchCurrentRuntimeStage)
  add_test(NAME MistwatchCurrentRuntimeStageTest
    COMMAND ${MISTWATCH_CURRENT_RUNTIME_STAGE_COMMAND})
  set_tests_properties(MistwatchCurrentRuntimeStageTest PROPERTIES
    LABELS "current-world-cpu" TIMEOUT 120 RUN_SERIAL TRUE FIXTURES_SETUP MistwatchCurrentRuntimePrepared)
  add_test(NAME MistwatchCurrentWorldExpectedTest
    COMMAND MistwatchCurrentWorldExpectedTest --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets")
  set_tests_properties(MistwatchCurrentWorldExpectedTest PROPERTIES
    LABELS "current-world-cpu" TIMEOUT 900 RUN_SERIAL TRUE
    FIXTURES_REQUIRED MistwatchCurrentRuntimePrepared FIXTURES_SETUP MistwatchCurrentRecipeExpected)
  add_test(NAME MistwatchCurrentWorldCertificate
    COMMAND MistwatchFallenWorldQualificationTest
      --source-root "${CMAKE_SOURCE_DIR}" --runtime-assets "$<TARGET_FILE_DIR:Mistwatch>/assets"
      --out "${MISTWATCH_CURRENT_CERTIFICATE}"
      --runtime-profile authored-current-forest-full-v5 --generated-root-stem "${MISTWATCH_ROOT_OUTPUT}"
      --world-cohort entrance-current-forest-surface-v1)
  set_tests_properties(MistwatchCurrentWorldCertificate PROPERTIES
    LABELS "current-world-cpu" TIMEOUT 900 RUN_SERIAL TRUE
    FIXTURES_REQUIRED "MistwatchCurrentRuntimePrepared;MistwatchCurrentRecipeExpected"
    FIXTURES_SETUP MistwatchCurrentWorldCpuExpected DEPENDS MistwatchCurrentWorldExpectedTest)
  foreach(current_mode IN ITEMS normal gbv)
    if(current_mode STREQUAL "normal")
      set(current_test MistwatchCurrentNativeNormal32)
      set(current_timeout 1560)
    else()
      set(current_test MistwatchCurrentNativeGbv17)
      set(current_timeout 2670)
    endif()
    add_test(NAME ${current_test}
      COMMAND "${MISTWATCH_RECEIPT_PWSH}" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass
        -File "${CMAKE_CURRENT_SOURCE_DIR}/../tools/run_current_native_ctest.ps1"
        -SourceRoot "${CMAKE_SOURCE_DIR}" -BuildRoot "${CMAKE_BINARY_DIR}"
        -ExecutablePath "$<TARGET_FILE:Mistwatch>" -CertificateRoot "${MISTWATCH_CURRENT_CERTIFICATE}"
        -Mode "${current_mode}")
    set_tests_properties(${current_test} PROPERTIES
      LABELS "current-native-${current_mode}" WORKING_DIRECTORY "$<TARGET_FILE_DIR:Mistwatch>"
      RUN_SERIAL TRUE RESOURCE_LOCK "mistwatch-native-gpu" TIMEOUT ${current_timeout}
      FIXTURES_REQUIRED MistwatchCurrentWorldCpuExpected DEPENDS MistwatchCurrentWorldCertificate)
  endforeach()
endif()

# 獨立修橋安全診斷；不加入既有 release-gates 清單或改動原136項。
add_executable(MistwatchBridgeRepairPreparationTest tests/MistwatchBridgeRepairPreparationTest.cpp)
target_include_directories(MistwatchBridgeRepairPreparationTest PRIVATE include)
target_compile_features(MistwatchBridgeRepairPreparationTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBridgeRepairPreparationTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchBridgeRepairPreparationTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
set_property(TARGET MistwatchBridgeRepairPreparationTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchBridgeRepairPreparationTest PRIVATE AevryndEngine)
# 沿用原作者資產與精確度政策，避免以未套用 Nape/Backpack 的替身角色驗收。
target_sources(MistwatchBridgeRepairPreparationTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
add_dependencies(MistwatchBridgeRepairPreparationTest Mistwatch)
add_test(NAME MistwatchBridgeRepairPreparationTest COMMAND MistwatchBridgeRepairPreparationTest "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin")
set_tests_properties(MistwatchBridgeRepairPreparationTest PROPERTIES LABELS "gameplay-diagnostics;bridge-repair" TIMEOUT 120 RUN_SERIAL TRUE)

# 獨立快取邊界診斷；只抽原 Helper/Entry 344 項，不更改既有 release-gates。
add_executable(MistwatchImmutableVolumeBoundsTest tests/MistwatchImmutableVolumeBoundsTest.cpp)
target_include_directories(MistwatchImmutableVolumeBoundsTest PRIVATE include)
target_compile_features(MistwatchImmutableVolumeBoundsTest PRIVATE cxx_std_20)
target_compile_options(MistwatchImmutableVolumeBoundsTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1 /fp:strict "$<$<CONFIG:Release>:/O2>")
target_compile_definitions(MistwatchImmutableVolumeBoundsTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
set_property(TARGET MistwatchImmutableVolumeBoundsTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchImmutableVolumeBoundsTest PRIVATE AevryndEngine)
add_test(NAME MistwatchImmutableVolumeBoundsTest COMMAND MistwatchImmutableVolumeBoundsTest)
set_tests_properties(MistwatchImmutableVolumeBoundsTest PROPERTIES LABELS "gameplay-diagnostics;cover-bounds-cache" TIMEOUT 120)

# 修橋預檢不推進時間的獨立回歸；原11案原target/labels與release136不變。
add_executable(MistwatchBridgeRepairClockTest tests/MistwatchBridgeRepairClockTest.cpp)
target_include_directories(MistwatchBridgeRepairClockTest PRIVATE include)
target_compile_features(MistwatchBridgeRepairClockTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBridgeRepairClockTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1 "$<$<CONFIG:Release>:/O2>")
target_compile_definitions(MistwatchBridgeRepairClockTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
set_property(TARGET MistwatchBridgeRepairClockTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchBridgeRepairClockTest PRIVATE AevryndEngine)
# 沿用原專用precise作者object，不把Nape併入strict TU。
target_sources(MistwatchBridgeRepairClockTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
add_dependencies(MistwatchBridgeRepairClockTest Mistwatch)
add_test(NAME MistwatchBridgeRepairClockTest COMMAND MistwatchBridgeRepairClockTest "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin")
set_tests_properties(MistwatchBridgeRepairClockTest PROPERTIES LABELS "gameplay-diagnostics;bridge-repair-clock" TIMEOUT 120 RUN_SERIAL TRUE)
# 僅追加至目前games/mistwatch/native/CMakeLists.txt；保留239E原文、clock與release136。
# 不改Mistwatch production main或Engine CC的default-precise政策；下列只建原strict CPU gates。
file(GLOB_RECURSE historical_braking_input_headers CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
  ${historical_braking_input_headers}
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/PrepareMistwatchHistoricalBrakingHeaders.py")
set(historical_braking_headers "${CMAKE_CURRENT_BINARY_DIR}/historical-braking/include")
set(current_braking_control_headers "${CMAKE_CURRENT_BINARY_DIR}/current-braking-control/include")
execute_process(COMMAND "${Python3_EXECUTABLE}" -B
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/PrepareMistwatchHistoricalBrakingHeaders.py"
  --binary-root "${CMAKE_BINARY_DIR}" --output "${historical_braking_headers}"
  --current-output "${current_braking_control_headers}"
  RESULT_VARIABLE historical_braking_result)
if(NOT historical_braking_result STREQUAL "0")
  message(FATAL_ERROR "Historical braking private-header preparation failed")
endif()
foreach(pair_formal IN ITEMS MistwatchPlayerDynamicContactTest MistwatchPairContactTest
    MistwatchFrameInputLatchTest MistwatchPairLoadPreparationTest MistwatchPlayablePairFrameTest
    MistwatchGameplayOrdinaryParity MistwatchGameplayHistoricalBrakingParity MistwatchGameplayCurrentBrakingControl)
  if(pair_formal STREQUAL "MistwatchGameplayOrdinaryParity" OR
      pair_formal STREQUAL "MistwatchGameplayHistoricalBrakingParity" OR
      pair_formal STREQUAL "MistwatchGameplayCurrentBrakingControl")
    set(pair_source tests/MistwatchGameplayOrdinaryParity.cpp)
  else()
    set(pair_source tests/${pair_formal}.cpp)
  endif()
  add_executable(${pair_formal} ${pair_source} $<TARGET_OBJECTS:AevryndDynamicContactStrictCC>)
  target_include_directories(${pair_formal} PRIVATE include)
  target_compile_features(${pair_formal} PRIVATE cxx_std_20)
  target_compile_options(${pair_formal} PRIVATE /W4 /WX /permissive- /utf-8
    /fp:strict "$<$<CONFIG:Release>:/O2>" /Ob2 /EHsc /Gy /MP1 /showIncludes)
  target_compile_definitions(${pair_formal} PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
  set_property(TARGET ${pair_formal} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  target_link_libraries(${pair_formal} PRIVATE AevryndEngine)
  target_link_options(${pair_formal} PRIVATE /INCREMENTAL:NO /VERBOSE:LIB
    "/MAP:${CMAKE_BINARY_DIR}/pair-formal-maps/${pair_formal}.map")
endforeach()
target_sources(MistwatchPlayerDynamicContactTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
target_sources(MistwatchPairLoadPreparationTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
target_sources(MistwatchPlayablePairFrameTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
target_link_libraries(MistwatchPlayablePairFrameTest PRIVATE shell32)
target_compile_definitions(MistwatchPairContactTest PRIVATE CANDIDATE=1)
target_compile_definitions(MistwatchGameplayOrdinaryParity PRIVATE CANDIDATE=0)
target_compile_definitions(MistwatchGameplayHistoricalBrakingParity PRIVATE CANDIDATE=0)
target_compile_definitions(MistwatchGameplayCurrentBrakingControl PRIVATE CANDIDATE=0)
target_include_directories(MistwatchGameplayHistoricalBrakingParity BEFORE PRIVATE "${historical_braking_headers}")
target_include_directories(MistwatchGameplayCurrentBrakingControl BEFORE PRIVATE "${current_braking_control_headers}")
add_dependencies(MistwatchPairContactTest Mistwatch MistwatchGameplayOrdinaryParity)
add_dependencies(MistwatchGameplayHistoricalBrakingParity MistwatchGameplayOrdinaryParity MistwatchGameplayCurrentBrakingControl)
add_dependencies(MistwatchPairLoadPreparationTest Mistwatch)
add_dependencies(MistwatchPlayablePairFrameTest Mistwatch)

add_test(NAME MistwatchPlayerDynamicContactTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/RunMistwatchDynamicContactTests.py"
    player "$<TARGET_FILE:MistwatchPlayerDynamicContactTest>")
add_test(NAME MistwatchPairContactTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/RunMistwatchDynamicContactTests.py"
    pair "$<TARGET_FILE:MistwatchPairContactTest>" "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin"
    "$<TARGET_FILE:MistwatchGameplayOrdinaryParity>")
set_tests_properties(MistwatchPlayerDynamicContactTest MistwatchPairContactTest PROPERTIES
  LABELS "gameplay-diagnostics;dynamic-contact" TIMEOUT 120 RUN_SERIAL TRUE
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME MistwatchHistoricalBrakingParityTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tests/support/RunMistwatchDynamicContactTests.py"
    pair-history "$<TARGET_FILE:MistwatchGameplayOrdinaryParity>" "$<TARGET_FILE:MistwatchGameplayHistoricalBrakingParity>"
    "$<TARGET_FILE:MistwatchGameplayCurrentBrakingControl>")
set_tests_properties(MistwatchHistoricalBrakingParityTest PROPERTIES
  LABELS "gameplay-diagnostics;dynamic-contact;historical-braking" TIMEOUT 120 RUN_SERIAL TRUE
  WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}")
add_test(NAME MistwatchFrameInputLatchTest
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:MistwatchFrameInputLatchTest>"
    "-DMODE=Input" "-DOUTPUT_ROOT=${CMAKE_BINARY_DIR}/test-artifacts/mistwatch-input-save-controls/input"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunMistwatchInputSaveControls.cmake")
set_tests_properties(MistwatchFrameInputLatchTest PROPERTIES
  LABELS "gameplay-diagnostics;dynamic-contact;input-latch" TIMEOUT 120 RUN_SERIAL TRUE)
add_test(NAME MistwatchPairLoadPreparationTest
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:MistwatchPairLoadPreparationTest>"
    "-DMODE=Save" "-DBOAR_ASSET=$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin"
    "-DOUTPUT_ROOT=${CMAKE_BINARY_DIR}/test-artifacts/mistwatch-input-save-controls/save"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunMistwatchInputSaveControls.cmake")
set_tests_properties(MistwatchPairLoadPreparationTest PROPERTIES
  LABELS "gameplay-diagnostics;dynamic-contact;pair-save" TIMEOUT 120 RUN_SERIAL TRUE)
add_test(NAME MistwatchPlayablePairFrameTest
  COMMAND "${CMAKE_COMMAND}" "-DPROGRAM=$<TARGET_FILE:MistwatchPlayablePairFrameTest>"
    "-DBOAR_ASSET=$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin"
    "-DOUTPUT_ROOT=${CMAKE_CURRENT_BINARY_DIR}/test-artifacts/playable-pair-frame"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/RunMistwatchPlayablePairFrameTest.cmake")
set_tests_properties(MistwatchPlayablePairFrameTest PROPERTIES
  LABELS "gameplay-diagnostics;dynamic-contact" TIMEOUT 120 RUN_SERIAL TRUE)

# 首次起步與停步轉身分開驗收；四方向、固定步頻率與回滾保留正式回歸。
add_executable(MistwatchFirstInputStartupTest tests/MistwatchFirstInputStartupTest.cpp)
target_include_directories(MistwatchFirstInputStartupTest PRIVATE include)
target_compile_features(MistwatchFirstInputStartupTest PRIVATE cxx_std_20)
target_compile_options(MistwatchFirstInputStartupTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchFirstInputStartupTest PRIVATE AevryndEngine)
add_test(NAME MistwatchFirstInputStartupTest COMMAND MistwatchFirstInputStartupTest)
set_tests_properties(MistwatchFirstInputStartupTest PROPERTIES TIMEOUT 45 LABELS "gameplay;locomotion;first-input")

# 閃避末段只保留一次新跳躍，並驗證取消、載入及長落差的輸入期限。
add_executable(MistwatchDashJumpHandoffTest tests/MistwatchDashJumpHandoffTest.cpp)
target_include_directories(MistwatchDashJumpHandoffTest PRIVATE include)
target_compile_features(MistwatchDashJumpHandoffTest PRIVATE cxx_std_20)
target_compile_options(MistwatchDashJumpHandoffTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchDashJumpHandoffTest PRIVATE AevryndEngine)
add_test(NAME MistwatchDashJumpHandoffTest COMMAND MistwatchDashJumpHandoffTest)
set_tests_properties(MistwatchDashJumpHandoffTest PROPERTIES TIMEOUT 120 LABELS "release-gates;gameplay;locomotion;jump-handoff")

# 放鍵制動的新落腳須保留下一固定步的續行、反向與完整回滾。
add_executable(MistwatchBrakingFootLeadTest tests/MistwatchBrakingFootLeadTest.cpp)
target_include_directories(MistwatchBrakingFootLeadTest PRIVATE include)
target_compile_features(MistwatchBrakingFootLeadTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBrakingFootLeadTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
target_link_libraries(MistwatchBrakingFootLeadTest PRIVATE AevryndEngine)
add_test(NAME MistwatchBrakingFootLeadTest COMMAND MistwatchBrakingFootLeadTest)
set_tests_properties(MistwatchBrakingFootLeadTest PROPERTIES TIMEOUT 120 LABELS "release-gates;gameplay;locomotion;braking")

# 暫停地圖只讀已知任務與當前高度場；保留實際字型排版及禁止世界提交的檢查。
add_executable(MistwatchSurveyMapTest tests/MistwatchSurveyMapTest.cpp)
target_include_directories(MistwatchSurveyMapTest PRIVATE include)
target_compile_features(MistwatchSurveyMapTest PRIVATE cxx_std_20)
target_compile_options(MistwatchSurveyMapTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchSurveyMapTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchSurveyMapTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_options(MistwatchSurveyMapTest PRIVATE /STACK:16777216)
target_link_libraries(MistwatchSurveyMapTest PRIVATE AevryndEngine)
add_test(NAME MistwatchSurveyMapTest COMMAND MistwatchSurveyMapTest)
set_tests_properties(MistwatchSurveyMapTest PROPERTIES TIMEOUT 180 LABELS "release-gates;gameplay;navigation")

# 回正沿原安全鏡頭交易執行；真輸入、拒絕與完成分支均保留 CPU 回歸。
add_executable(MistwatchCameraRecenterTest tests/MistwatchCameraRecenterTest.cpp tests/MistwatchCameraRecenterAssert.cpp)
target_include_directories(MistwatchCameraRecenterTest PRIVATE include)
target_compile_features(MistwatchCameraRecenterTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCameraRecenterTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchCameraRecenterTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN
  "MISTWATCH_RECENTER_MAIN_SOURCE=\"${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp\"")
set_property(TARGET MistwatchCameraRecenterTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_options(MistwatchCameraRecenterTest PRIVATE /STACK:16777216)
target_link_libraries(MistwatchCameraRecenterTest PRIVATE AevryndEngine shell32)
target_sources(MistwatchCameraRecenterTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
add_dependencies(MistwatchCameraRecenterTest Mistwatch)
add_test(NAME MistwatchCameraRecenterTest COMMAND MistwatchCameraRecenterTest "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin")
set_tests_properties(MistwatchCameraRecenterTest PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS "release-gates;gameplay;camera;input")

# 連續方位只讀同一正式引導；驗證角度、固定版面與零世界寫入。
add_executable(MistwatchObjectivePointerTest tests/MistwatchObjectivePointerTest.cpp)
target_include_directories(MistwatchObjectivePointerTest PRIVATE include)
target_compile_features(MistwatchObjectivePointerTest PRIVATE cxx_std_20)
target_compile_options(MistwatchObjectivePointerTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchObjectivePointerTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchObjectivePointerTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_options(MistwatchObjectivePointerTest PRIVATE /STACK:16777216)
target_link_libraries(MistwatchObjectivePointerTest PRIVATE AevryndEngine)
add_test(NAME MistwatchObjectivePointerTest COMMAND MistwatchObjectivePointerTest)
set_tests_properties(MistwatchObjectivePointerTest PROPERTIES TIMEOUT 120 LABELS "release-gates;gameplay;navigation")

# 成功拾取才顯示物品名稱及真實進度；非法或撤銷變化不能宣稱成功。
add_executable(MistwatchPickupFeedbackTest tests/MistwatchPickupFeedbackTest.cpp)
target_include_directories(MistwatchPickupFeedbackTest PRIVATE include)
target_compile_features(MistwatchPickupFeedbackTest PRIVATE cxx_std_20)
target_compile_options(MistwatchPickupFeedbackTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict)
set_property(TARGET MistwatchPickupFeedbackTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchPickupFeedbackTest COMMAND MistwatchPickupFeedbackTest)
set_tests_properties(MistwatchPickupFeedbackTest PROPERTIES TIMEOUT 60 LABELS "release-gates;gameplay;interaction")

# 短暫提前取物只適用補給與戶外木料；純邏輯與真 Main 的取消、回滾分開驗收。
add_executable(MistwatchPickupIntentTest tests/MistwatchPickupIntentTest.cpp)
target_include_directories(MistwatchPickupIntentTest PRIVATE include)
target_compile_features(MistwatchPickupIntentTest PRIVATE cxx_std_20)
target_compile_options(MistwatchPickupIntentTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchPickupIntentTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchPickupIntentTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_libraries(MistwatchPickupIntentTest PRIVATE AevryndEngine)
add_test(NAME MistwatchPickupIntentTest COMMAND MistwatchPickupIntentTest)
set_tests_properties(MistwatchPickupIntentTest PROPERTIES TIMEOUT 60 LABELS "release-gates;gameplay;interaction")

add_executable(MistwatchPickupIntentMainTest tests/MistwatchPickupIntentMainTest.cpp)
target_include_directories(MistwatchPickupIntentMainTest PRIVATE include)
target_compile_features(MistwatchPickupIntentMainTest PRIVATE cxx_std_20)
target_compile_options(MistwatchPickupIntentMainTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchPickupIntentMainTest PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
set_property(TARGET MistwatchPickupIntentMainTest PROPERTY MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
target_link_options(MistwatchPickupIntentMainTest PRIVATE /STACK:16777216)
target_link_libraries(MistwatchPickupIntentMainTest PRIVATE AevryndEngine shell32)
target_sources(MistwatchPickupIntentMainTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
add_dependencies(MistwatchPickupIntentMainTest Mistwatch)
add_test(NAME MistwatchPickupIntentMainTest COMMAND MistwatchPickupIntentMainTest "$<TARGET_FILE_DIR:Mistwatch>/assets/mistwatch-wildlife/boar.aeskin")
set_tests_properties(MistwatchPickupIntentMainTest PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS "release-gates;gameplay;interaction;input")

# 真實可反擊狀態的有界世界圖元，不改戰鬥或輸入規則。
add_executable(MistwatchCounterWindowCueTest tests/MistwatchCounterWindowCueTest.cpp)
target_include_directories(MistwatchCounterWindowCueTest PRIVATE include)
target_compile_features(MistwatchCounterWindowCueTest PRIVATE cxx_std_20)
target_compile_options(MistwatchCounterWindowCueTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchCounterWindowCueTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchCounterWindowCueTest PRIVATE AevryndEngine)
add_test(NAME MistwatchCounterWindowCueTest COMMAND MistwatchCounterWindowCueTest)
set_tests_properties(MistwatchCounterWindowCueTest PROPERTIES TIMEOUT 180 LABELS "release-gates;gameplay;combat")

add_executable(MistwatchGroveFloorTest tests/MistwatchGroveFloorTest.cpp)
target_include_directories(MistwatchGroveFloorTest PRIVATE include)
target_compile_features(MistwatchGroveFloorTest PRIVATE cxx_std_20)
target_compile_options(MistwatchGroveFloorTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_compile_definitions(MistwatchGroveFloorTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchGroveFloorTest PRIVATE AevryndEngine)
add_test(NAME MistwatchGroveFloorTest COMMAND MistwatchGroveFloorTest)
set_tests_properties(MistwatchGroveFloorTest PROPERTIES TIMEOUT 60 LABELS "release-gates;gameplay;materials")

# 新423資產規格獨立測試；不改舊418規格，也不把身分檢查當成完整原生矩陣。
add_test(NAME MistwatchForestGroveRuntimeContractTest
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/../tools/test_forest_grove_runtime.py"
    --source-root "${CMAKE_SOURCE_DIR}")
set_tests_properties(MistwatchForestGroveRuntimeContractTest PROPERTIES
  TIMEOUT 30 RUN_SERIAL TRUE LABELS "release-gates;gameplay;runtime-lineage")

add_executable(MistwatchIndexedVolumeBoundsTest tests/MistwatchIndexedVolumeBoundsTest.cpp)
target_include_directories(MistwatchIndexedVolumeBoundsTest PRIVATE include)
target_compile_features(MistwatchIndexedVolumeBoundsTest PRIVATE cxx_std_20)
target_compile_options(MistwatchIndexedVolumeBoundsTest PRIVATE /W4 /WX /permissive- /utf-8 /fp:strict /MP1)
target_compile_definitions(MistwatchIndexedVolumeBoundsTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(MistwatchIndexedVolumeBoundsTest PRIVATE AevryndEngine)
add_test(NAME MistwatchIndexedVolumeBoundsTest COMMAND MistwatchIndexedVolumeBoundsTest)
set_tests_properties(MistwatchIndexedVolumeBoundsTest PROPERTIES TIMEOUT 120 LABELS "release-gates;gameplay;placement")
