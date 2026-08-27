add_executable(CharacterLab WIN32
  app.manifest
  src/main.cpp
  src/CharacterLabGame.cpp
)
target_include_directories(CharacterLab PRIVATE include)
target_compile_options(CharacterLab PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(CharacterLab PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(CharacterLab PRIVATE AevryndEngine)
set_target_properties(CharacterLab PROPERTIES OUTPUT_NAME "Aevrynd-Character-Lab" VS_DPI_AWARE "PerMonitor")
if(AEVRYND_STREAMLINE_AVAILABLE)
  foreach(SL_DLL sl.interposer.dll sl.common.dll sl.dlss.dll nvngx_dlss.dll)
    add_custom_command(TARGET CharacterLab POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${STREAMLINE_ROOT}/bin/x64/${SL_DLL}"
        "$<TARGET_FILE_DIR:CharacterLab>/${SL_DLL}")
  endforeach()
endif()

add_executable(CharacterLabAutomationTest tests/CharacterLabAutomationSmoke.cpp)
target_include_directories(CharacterLabAutomationTest PRIVATE include)
target_compile_options(CharacterLabAutomationTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(CharacterLabAutomationTest PRIVATE AevryndEngine)
add_test(NAME CharacterLabAutomationTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:CharacterLabAutomationTest>'")
