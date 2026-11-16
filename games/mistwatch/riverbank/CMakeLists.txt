# Explicit development target: original Mistwatch entry and old423 cohort are unchanged.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
# 河岸版使用自己的角色標頭編譯後頸毛髮綁定，確保姿態定義在所有編譯單元一致。
add_library(MistwatchRiverbankExplorerNapeHairBinding OBJECT ../native/src/MistwatchExplorerNapeHairBinding.cpp)
target_include_directories(MistwatchRiverbankExplorerNapeHairBinding BEFORE PRIVATE include)
target_compile_features(MistwatchRiverbankExplorerNapeHairBinding PRIVATE cxx_std_20)
target_compile_options(MistwatchRiverbankExplorerNapeHairBinding PRIVATE /W4 /WX /permissive- /utf-8 /fp:precise /MP1)
target_link_libraries(MistwatchRiverbankExplorerNapeHairBinding PRIVATE AevryndEngine)
add_executable(MistwatchRiverbank WIN32 src/main.cpp)
target_sources(MistwatchRiverbank PRIVATE $<TARGET_OBJECTS:MistwatchRiverbankExplorerNapeHairBinding>)
target_sources(MistwatchRiverbank PRIVATE $<TARGET_OBJECTS:MistwatchWaterlineBankConstruction>)
target_sources(MistwatchRiverbank PRIVATE $<TARGET_OBJECTS:MistwatchBankRocksConstruction>)
target_compile_features(MistwatchRiverbank PRIVATE cxx_std_20)
target_compile_options(MistwatchRiverbank PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_include_directories(MistwatchRiverbank BEFORE PRIVATE include)
target_link_libraries(MistwatchRiverbank PRIVATE AevryndEngine d3d12 dxgi windowscodecs ole32 user32 gdi32 psapi xaudio2 xinput9_1_0 bcrypt dwmapi)
target_link_options(MistwatchRiverbank PRIVATE /STACK:16777216 /INCREMENTAL:NO "/MAP:${CMAKE_CURRENT_BINARY_DIR}/MistwatchRiverbank.map")
set_target_properties(MistwatchRiverbank PROPERTIES OUTPUT_NAME "Aevrynd-Mistwatch-Riverbank"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/riverbank/$<CONFIG>"
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL INTERPROCEDURAL_OPTIMIZATION FALSE)
add_custom_command(TARGET MistwatchRiverbank POST_BUILD
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/package_runtime.py"
    --manifest "${CMAKE_CURRENT_SOURCE_DIR}/runtime-identity.json"
    --source "${CMAKE_CURRENT_SOURCE_DIR}/assets" --output "$<TARGET_FILE_DIR:MistwatchRiverbank>/assets" VERBATIM)
add_test(NAME MistwatchRiverbankRuntimeIdentity
  COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/package_runtime.py"
    --manifest "${CMAKE_CURRENT_SOURCE_DIR}/runtime-identity.json"
    --source "${CMAKE_CURRENT_SOURCE_DIR}/assets" --output "$<TARGET_FILE_DIR:MistwatchRiverbank>/assets" --verify-only)
set_tests_properties(MistwatchRiverbankRuntimeIdentity PROPERTIES TIMEOUT 60 RUN_SERIAL TRUE LABELS "riverbank-development;runtime-identity")
add_executable(MistwatchRiverbankLocomotionTest tests/RiverbankLocomotionTest.cpp)
target_sources(MistwatchRiverbankLocomotionTest PRIVATE $<TARGET_OBJECTS:MistwatchRiverbankExplorerNapeHairBinding>)
target_compile_features(MistwatchRiverbankLocomotionTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRiverbankLocomotionTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_include_directories(MistwatchRiverbankLocomotionTest BEFORE PRIVATE include)
target_link_libraries(MistwatchRiverbankLocomotionTest PRIVATE AevryndEngine)
target_link_options(MistwatchRiverbankLocomotionTest PRIVATE /STACK:16777216 /INCREMENTAL:NO)
set_target_properties(MistwatchRiverbankLocomotionTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL INTERPROCEDURAL_OPTIMIZATION FALSE)
add_test(NAME MistwatchRiverbankLocomotionTest COMMAND MistwatchRiverbankLocomotionTest
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-heightfield.tsv"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mature-world-before.f32"
  "${CMAKE_CURRENT_BINARY_DIR}/bank-trace.tsv")
set_tests_properties(MistwatchRiverbankLocomotionTest PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS "riverbank-development;changed-terrain;locomotion")

add_executable(MistwatchRiverbankShelterGuidanceTest tests/ShelterGuidanceTest.cpp)
target_include_directories(MistwatchRiverbankShelterGuidanceTest BEFORE PRIVATE include)
target_compile_features(MistwatchRiverbankShelterGuidanceTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRiverbankShelterGuidanceTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchRiverbankShelterGuidanceTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchRiverbankShelterGuidanceTest PRIVATE AevryndEngine)
set_target_properties(MistwatchRiverbankShelterGuidanceTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchRiverbankShelterGuidanceTest COMMAND MistwatchRiverbankShelterGuidanceTest)
set_tests_properties(MistwatchRiverbankShelterGuidanceTest PROPERTIES TIMEOUT 30 LABELS "riverbank-development;gameplay-guidance;cpu")

add_executable(MistwatchRiverbankPlaytestDebriefTest tests/PlaytestDebriefTest.cpp)
target_include_directories(MistwatchRiverbankPlaytestDebriefTest BEFORE PRIVATE include)
target_compile_features(MistwatchRiverbankPlaytestDebriefTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRiverbankPlaytestDebriefTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchRiverbankPlaytestDebriefTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchRiverbankPlaytestDebriefTest PRIVATE AevryndEngine)
set_target_properties(MistwatchRiverbankPlaytestDebriefTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchRiverbankPlaytestDebriefTest COMMAND MistwatchRiverbankPlaytestDebriefTest)
set_tests_properties(MistwatchRiverbankPlaytestDebriefTest PROPERTIES TIMEOUT 30 LABELS "riverbank-development;playtest-debrief;cpu")

add_executable(MistwatchRiverbankRelayJournalTest tests/RelayJournalTest.cpp)
target_include_directories(MistwatchRiverbankRelayJournalTest BEFORE PRIVATE include)
target_compile_features(MistwatchRiverbankRelayJournalTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRiverbankRelayJournalTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchRiverbankRelayJournalTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchRiverbankRelayJournalTest PRIVATE AevryndEngine)
set_target_properties(MistwatchRiverbankRelayJournalTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchRiverbankRelayJournalTest COMMAND MistwatchRiverbankRelayJournalTest)
set_tests_properties(MistwatchRiverbankRelayJournalTest PROPERTIES TIMEOUT 30 LABELS "riverbank-development;relay-journal;cpu")

add_executable(MistwatchRiverbankSaveProfileRecoveryTest tests/SaveProfileRecoveryTest.cpp)
target_include_directories(MistwatchRiverbankSaveProfileRecoveryTest BEFORE PRIVATE include)
target_compile_features(MistwatchRiverbankSaveProfileRecoveryTest PRIVATE cxx_std_20)
target_compile_options(MistwatchRiverbankSaveProfileRecoveryTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchRiverbankSaveProfileRecoveryTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_link_libraries(MistwatchRiverbankSaveProfileRecoveryTest PRIVATE AevryndEngine)
set_target_properties(MistwatchRiverbankSaveProfileRecoveryTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchRiverbankSaveProfileRecoveryTest COMMAND MistwatchRiverbankSaveProfileRecoveryTest)
set_tests_properties(MistwatchRiverbankSaveProfileRecoveryTest PROPERTIES TIMEOUT 30 LABELS "riverbank-development;save-profile;cpu")

add_executable(MistwatchBearingBridgeContractTest tests/BearingBridgeConstructionTest.cpp)
target_compile_features(MistwatchBearingBridgeContractTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBearingBridgeContractTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchBearingBridgeContractTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MistwatchBearingBridgeContractTest BEFORE PRIVATE include)
target_link_libraries(MistwatchBearingBridgeContractTest PRIVATE AevryndEngine)
set_target_properties(MistwatchBearingBridgeContractTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchBearingBridgeContractTest COMMAND MistwatchBearingBridgeContractTest
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-heightfield.tsv")
set_tests_properties(MistwatchBearingBridgeContractTest PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS "riverbank-development;finite-bridge;rail-contact")

add_executable(MistwatchBridgeTimberVisualTest tests/BridgeTimberVisualTest.cpp)
target_compile_features(MistwatchBridgeTimberVisualTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBridgeTimberVisualTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchBridgeTimberVisualTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MistwatchBridgeTimberVisualTest BEFORE PRIVATE include)
target_link_libraries(MistwatchBridgeTimberVisualTest PRIVATE AevryndEngine)
set_target_properties(MistwatchBridgeTimberVisualTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchBridgeTimberVisualTest COMMAND MistwatchBridgeTimberVisualTest
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-heightfield.tsv")
set_tests_properties(MistwatchBridgeTimberVisualTest PROPERTIES TIMEOUT 30 LABELS "riverbank-development;bridge-timber;cpu")

add_executable(MistwatchStormWaterContractTest tests/StormWaterContractTest.cpp)
target_compile_features(MistwatchStormWaterContractTest PRIVATE cxx_std_20)
target_compile_options(MistwatchStormWaterContractTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchStormWaterContractTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MistwatchStormWaterContractTest BEFORE PRIVATE include)
target_link_libraries(MistwatchStormWaterContractTest PRIVATE AevryndEngine)
set_target_properties(MistwatchStormWaterContractTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchStormWaterContractTest COMMAND MistwatchStormWaterContractTest
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-heightfield.tsv")
set_tests_properties(MistwatchStormWaterContractTest PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS "riverbank-development;storm-water;shore-wave")

# Keep the tested construction in its own translation unit. The monolithic
# candidate encountered a compiler AV; no optimization/quality gate is reduced.
add_library(MistwatchWaterlineBankConstruction OBJECT src/ErodedBankConstruction.cpp)
target_compile_features(MistwatchWaterlineBankConstruction PRIVATE cxx_std_20)
target_compile_options(MistwatchWaterlineBankConstruction PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchWaterlineBankConstruction PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MistwatchWaterlineBankConstruction BEFORE PRIVATE include)
target_link_libraries(MistwatchWaterlineBankConstruction PRIVATE AevryndEngine)
set_target_properties(MistwatchWaterlineBankConstruction PROPERTIES
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL INTERPROCEDURAL_OPTIMIZATION FALSE)

add_executable(MistwatchWaterlineBankContractTest tests/WaterlineBankConstructionTest.cpp)
target_sources(MistwatchWaterlineBankContractTest PRIVATE $<TARGET_OBJECTS:MistwatchWaterlineBankConstruction>)
target_compile_features(MistwatchWaterlineBankContractTest PRIVATE cxx_std_20)
target_compile_options(MistwatchWaterlineBankContractTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchWaterlineBankContractTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MistwatchWaterlineBankContractTest BEFORE PRIVATE include)
target_link_libraries(MistwatchWaterlineBankContractTest PRIVATE AevryndEngine)
set_target_properties(MistwatchWaterlineBankContractTest PROPERTIES
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL INTERPROCEDURAL_OPTIMIZATION FALSE)
add_test(NAME MistwatchWaterlineBankContractTest COMMAND MistwatchWaterlineBankContractTest
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-heightfield.tsv"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-guards.tsv")
set_tests_properties(MistwatchWaterlineBankContractTest PROPERTIES
  TIMEOUT 120 RUN_SERIAL TRUE LABELS "riverbank-development;waterline-bank;protected-roots")

# Explicit unaccepted preview only; the original strict height rejection remains.
add_library(MistwatchBankRocksConstruction OBJECT src/BankRocks.cpp)
target_compile_features(MistwatchBankRocksConstruction PRIVATE cxx_std_20)
target_compile_options(MistwatchBankRocksConstruction PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchBankRocksConstruction PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MistwatchBankRocksConstruction BEFORE PRIVATE include)
target_link_libraries(MistwatchBankRocksConstruction PRIVATE AevryndEngine)
set_target_properties(MistwatchBankRocksConstruction PROPERTIES
  MSVC_RUNTIME_LIBRARY MultiThreadedDLL INTERPROCEDURAL_OPTIMIZATION FALSE)
add_executable(MistwatchBankRocksPreviewTest tests/BankRocksPreviewTest.cpp)
target_sources(MistwatchBankRocksPreviewTest PRIVATE
  $<TARGET_OBJECTS:MistwatchBankRocksConstruction> $<TARGET_OBJECTS:MistwatchWaterlineBankConstruction>)
target_compile_features(MistwatchBankRocksPreviewTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBankRocksPreviewTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_compile_definitions(MistwatchBankRocksPreviewTest PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MistwatchBankRocksPreviewTest BEFORE PRIVATE include)
target_link_libraries(MistwatchBankRocksPreviewTest PRIVATE AevryndEngine bcrypt)
set_target_properties(MistwatchBankRocksPreviewTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchBankRocksPreviewTest COMMAND MistwatchBankRocksPreviewTest
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-heightfield.tsv"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/riverbank-guards.tsv"
  "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/mature-world-before.f32"
  "${CMAKE_CURRENT_SOURCE_DIR}/assets/mistwatch-boulders/meshes/boulder-01-lod0-node0-mesh0-primitive0-material0-boulder-01.aemesh"
  --no-report --rebuild-current-final-bank)
set_tests_properties(MistwatchBankRocksPreviewTest PROPERTIES TIMEOUT 120 RUN_SERIAL TRUE LABELS "riverbank-development;unaccepted-bank-rocks;finite-proxy")
add_executable(MistwatchBankRocksPreviewModeTest tests/BankRocksPreviewModeTest.cpp)
target_compile_features(MistwatchBankRocksPreviewModeTest PRIVATE cxx_std_20)
target_compile_options(MistwatchBankRocksPreviewModeTest PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
target_include_directories(MistwatchBankRocksPreviewModeTest BEFORE PRIVATE include)
set_target_properties(MistwatchBankRocksPreviewModeTest PROPERTIES MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
add_test(NAME MistwatchBankRocksPreviewModeTest COMMAND MistwatchBankRocksPreviewModeTest)
set_tests_properties(MistwatchBankRocksPreviewModeTest PROPERTIES TIMEOUT 30 LABELS "riverbank-development;preview-opt-in;cpu")

if(TARGET MistwatchLocalWorldClient)
  # 驗證多人設定的輸入阻擋、偏好存讀與基本環境音事件。
  add_executable(MistwatchRiverbankCombatCameraSmoke tests/RiverbankCombatCameraSmoke.cpp)
  target_include_directories(MistwatchRiverbankCombatCameraSmoke PRIVATE include)
  target_compile_definitions(MistwatchRiverbankCombatCameraSmoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(MistwatchRiverbankCombatCameraSmoke PRIVATE /W4 /WX /permissive- /utf-8)
  target_link_libraries(MistwatchRiverbankCombatCameraSmoke PRIVATE AevryndEngine)
  add_test(NAME MistwatchRiverbankCombatCameraTest COMMAND MistwatchRiverbankCombatCameraSmoke)
  set_tests_properties(MistwatchRiverbankCombatCameraTest PROPERTIES TIMEOUT 10 LABELS "mistwatch;client;camera")

  add_executable(MistwatchRiverbankSettingsMenuSmoke tests/RiverbankSettingsMenuSmoke.cpp)
  target_include_directories(MistwatchRiverbankSettingsMenuSmoke PRIVATE include ../mmorpg)
  target_compile_definitions(MistwatchRiverbankSettingsMenuSmoke PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(MistwatchRiverbankSettingsMenuSmoke PRIVATE /W4 /WX /permissive- /utf-8)
  target_link_libraries(MistwatchRiverbankSettingsMenuSmoke PRIVATE AevryndEngine)
  add_test(NAME MistwatchRiverbankSettingsMenuSmoke COMMAND MistwatchRiverbankSettingsMenuSmoke "${CMAKE_CURRENT_BINARY_DIR}/multiplayer-settings-test")
  set_tests_properties(MistwatchRiverbankSettingsMenuSmoke PROPERTIES TIMEOUT 20 LABELS "mistwatch;client;settings")
  add_executable(MistwatchRiverbankRouteMapTrackingTest tests/RiverbankRouteMapTrackingTest.cpp)
  target_include_directories(MistwatchRiverbankRouteMapTrackingTest PRIVATE include ../mmorpg)
  target_compile_features(MistwatchRiverbankRouteMapTrackingTest PRIVATE cxx_std_20)
  target_compile_definitions(MistwatchRiverbankRouteMapTrackingTest PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(MistwatchRiverbankRouteMapTrackingTest PRIVATE /W4 /WX /permissive- /utf-8)
  target_link_libraries(MistwatchRiverbankRouteMapTrackingTest PRIVATE AevryndEngine)
  add_test(NAME MistwatchRiverbankRouteMapTrackingTest COMMAND MistwatchRiverbankRouteMapTrackingTest)
  set_tests_properties(MistwatchRiverbankRouteMapTrackingTest PROPERTIES TIMEOUT 10 LABELS "mistwatch;client;map")

  add_executable(MistwatchRiverbankLocalWorld WIN32 src/main.cpp)
  target_sources(MistwatchRiverbankLocalWorld PRIVATE $<TARGET_OBJECTS:MistwatchRiverbankExplorerNapeHairBinding>
    $<TARGET_OBJECTS:MistwatchWaterlineBankConstruction> $<TARGET_OBJECTS:MistwatchBankRocksConstruction>)
  target_include_directories(MistwatchRiverbankLocalWorld BEFORE PRIVATE include)
  target_compile_features(MistwatchRiverbankLocalWorld PRIVATE cxx_std_20)
  target_compile_options(MistwatchRiverbankLocalWorld PRIVATE /W4 /WX /permissive- /utf-8 /MP1)
  target_compile_definitions(MistwatchRiverbankLocalWorld PRIVATE AEVRYND_MISTWATCH_MMO_CLIENT)
  target_link_libraries(MistwatchRiverbankLocalWorld PRIVATE MistwatchLocalWorldClient)
  target_link_options(MistwatchRiverbankLocalWorld PRIVATE /STACK:16777216 /INCREMENTAL:NO)
  set_target_properties(MistwatchRiverbankLocalWorld PROPERTIES OUTPUT_NAME "Aevrynd-Mistwatch-Local-World"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/riverbank-local-world/$<CONFIG>" MSVC_RUNTIME_LIBRARY MultiThreadedDLL)
  add_custom_command(TARGET MistwatchRiverbankLocalWorld POST_BUILD
    COMMAND "${Python3_EXECUTABLE}" -B "${CMAKE_CURRENT_SOURCE_DIR}/tools/package_runtime.py"
      --manifest "${CMAKE_CURRENT_SOURCE_DIR}/runtime-identity.json"
      --source "${CMAKE_CURRENT_SOURCE_DIR}/assets" --output "$<TARGET_FILE_DIR:MistwatchRiverbankLocalWorld>/assets" VERBATIM)
  if(MISTWATCH_LOCAL_WORLD_AVATAR)
    target_compile_definitions(MistwatchRiverbankLocalWorld PRIVATE AEVRYND_MISTWATCH_RIVERBANK_AVATAR=1)
    set_property(TARGET MistwatchRiverbankLocalWorld APPEND PROPERTY LINK_DEPENDS "${MISTWATCH_LOCAL_WORLD_AVATAR}")
    add_custom_command(TARGET MistwatchRiverbankLocalWorld POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "$<TARGET_FILE_DIR:MistwatchRiverbankLocalWorld>/character"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${MISTWATCH_LOCAL_WORLD_AVATAR}"
        "$<TARGET_FILE_DIR:MistwatchRiverbankLocalWorld>/character/RiverbankExplorer.aeskin" VERBATIM)
    if(MISTWATCH_RIVERBANK_WARDROBES)
      target_compile_definitions(MistwatchRiverbankLocalWorld PRIVATE AEVRYND_MISTWATCH_RIVERBANK_WARDROBE=1)
      foreach(wardrobe IN LISTS MISTWATCH_RIVERBANK_WARDROBES)
        set_property(TARGET MistwatchRiverbankLocalWorld APPEND PROPERTY LINK_DEPENDS "${wardrobe}")
        add_custom_command(TARGET MistwatchRiverbankLocalWorld POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${wardrobe}"
            "$<TARGET_FILE_DIR:MistwatchRiverbankLocalWorld>/character/" VERBATIM)
      endforeach()
    endif()
  endif()
endif()
