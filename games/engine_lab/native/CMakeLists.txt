add_executable(EngineLab WIN32
  app.manifest
  material_assets.rc
  src/main.cpp
  src/EngineLabGame.cpp
)
target_include_directories(EngineLab PRIVATE include)
target_compile_options(EngineLab PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(EngineLab PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(EngineLab PRIVATE AevryndEngine Ole32)
set_target_properties(EngineLab PROPERTIES OUTPUT_NAME "Aevrynd-Engine-Lab" VS_DPI_AWARE "PerMonitor")
if(WIN32)
  add_test(NAME EngineLabNetworkCaptureSmoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/EngineLabNetworkCaptureSmoke.ps1"
      -Executable "$<TARGET_FILE:EngineLab>")
  set_tests_properties(EngineLabNetworkCaptureSmoke PROPERTIES TIMEOUT 420)
  add_test(NAME EngineLabShadowCaptureNormalSmoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/EngineLabShadowCaptureSmoke.ps1"
      -Executable "$<TARGET_FILE:EngineLab>" -SourceRoot "${CMAKE_SOURCE_DIR}" -Mode normal -CascadeCount 4)
  add_test(NAME EngineLabShadowCaptureFallbackSmoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/EngineLabShadowCaptureSmoke.ps1"
      -Executable "$<TARGET_FILE:EngineLab>" -SourceRoot "${CMAKE_SOURCE_DIR}" -Mode normal -CascadeCount 2)
  add_test(NAME EngineLabShadowCaptureGbvSmoke
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File
      "${CMAKE_CURRENT_SOURCE_DIR}/tests/EngineLabShadowCaptureSmoke.ps1"
      -Executable "$<TARGET_FILE:EngineLab>" -SourceRoot "${CMAKE_SOURCE_DIR}" -Mode gbv -CascadeCount 4)
  set_tests_properties(EngineLabShadowCaptureNormalSmoke EngineLabShadowCaptureFallbackSmoke EngineLabShadowCaptureGbvSmoke PROPERTIES TIMEOUT 420 RUN_SERIAL TRUE)
endif()
add_custom_command(TARGET EngineLab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:EngineLab>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/../assets/engine_probe.obj"
    "$<TARGET_FILE_DIR:EngineLab>/assets/engine_probe.obj")
aevrynd_package_streamline_runtime(EngineLab)
