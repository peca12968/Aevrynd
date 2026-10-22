# Explicit development target: original Mistwatch entry and old423 cohort are unchanged.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(MistwatchRiverbank WIN32 src/main.cpp)
target_sources(MistwatchRiverbank PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
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
target_sources(MistwatchRiverbankLocomotionTest PRIVATE $<TARGET_OBJECTS:MistwatchExplorerNapeHairBinding>)
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
