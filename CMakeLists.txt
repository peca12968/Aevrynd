cmake_minimum_required(VERSION 3.25)
project(Aevrynd VERSION 2.86.0 LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
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

# 受限遠端 worker 只允許建置這個既有固定 target；在 gate preset 下，
# 將它擴充為 13 個無互動 CPU gate 的聚合建置入口。
if(AEVRYND_STREAMLINE_PACKAGE_TEST_TARGET)
  add_dependencies(AevryndStreamlinePackageFixture
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
    AevryndWorldPartition3DFakeSourceTest
    AevryndWorldPartitionDeltaGateFakeTest)
endif()
