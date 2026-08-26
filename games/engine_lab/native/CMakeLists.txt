add_executable(EngineLab WIN32
  app.manifest
  src/main.cpp
  src/EngineLabGame.cpp
)
target_include_directories(EngineLab PRIVATE include)
target_compile_options(EngineLab PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(EngineLab PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(EngineLab PRIVATE AevryndEngine)
set_target_properties(EngineLab PROPERTIES OUTPUT_NAME "Aevrynd-Engine-Lab" VS_DPI_AWARE "PerMonitor")
foreach(SL_DLL sl.interposer.dll sl.common.dll sl.dlss.dll nvngx_dlss.dll)
  add_custom_command(TARGET EngineLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${STREAMLINE_ROOT}/bin/x64/${SL_DLL}"
      "$<TARGET_FILE_DIR:EngineLab>/${SL_DLL}")
endforeach()
