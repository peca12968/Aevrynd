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
