add_executable(CharacterLab WIN32
  app.manifest
  src/main.cpp
  src/CharacterLabGame.cpp
)
target_include_directories(CharacterLab PRIVATE include)
target_compile_options(CharacterLab PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(CharacterLab PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(CharacterLab PRIVATE AevryndEngine bcrypt)
set_target_properties(CharacterLab PROPERTIES OUTPUT_NAME "Aevrynd-Character-Lab" VS_DPI_AWARE "PerMonitor")
add_executable(CharacterLabDemoLauncher WIN32 src/DemoLauncher.cpp)
target_compile_options(CharacterLabDemoLauncher PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(CharacterLabDemoLauncher PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(CharacterLabDemoLauncher PRIVATE bcrypt)
set_target_properties(CharacterLabDemoLauncher PROPERTIES OUTPUT_NAME "Aevrynd-Character-Lab-Launcher" VS_DPI_AWARE "PerMonitor")
if(TARGET AevryndReleaseGateAssets)
  add_dependencies(CharacterLab AevryndReleaseGateAssets)
else()
foreach(CHARACTER_ASSET realistic-female realistic-male)
  add_custom_command(TARGET CharacterLab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
      "$<TARGET_FILE_DIR:CharacterLab>/assets/model-packs/${CHARACTER_ASSET}"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      "${CMAKE_SOURCE_DIR}/games/character_lab/assets/model-packs/${CHARACTER_ASSET}/${CHARACTER_ASSET}.aeskin"
      "$<TARGET_FILE_DIR:CharacterLab>/assets/model-packs/${CHARACTER_ASSET}/${CHARACTER_ASSET}.aeskin")
  if(CHARACTER_ASSET STREQUAL "realistic-female")
    add_custom_command(TARGET CharacterLab POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:CharacterLab>/assets/model-packs/realistic-female/source"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/games/character_lab/assets/model-packs/realistic-female/realistic-female.provenance.json"
        "$<TARGET_FILE_DIR:CharacterLab>/assets/model-packs/realistic-female/realistic-female.provenance.json"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/games/character_lab/assets/model-packs/realistic-female/source/realistic-female.gltf"
        "$<TARGET_FILE_DIR:CharacterLab>/assets/model-packs/realistic-female/source/realistic-female.gltf"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/games/character_lab/assets/model-packs/realistic-female/source/realistic-female.bin"
        "$<TARGET_FILE_DIR:CharacterLab>/assets/model-packs/realistic-female/source/realistic-female.bin")
  endif()
endforeach()
add_custom_command(TARGET CharacterLab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/games/character_lab/assets/material-packs"
    "$<TARGET_FILE_DIR:CharacterLab>/assets/material-packs")
endif()
aevrynd_package_streamline_runtime(CharacterLab)

add_executable(CharacterLabAutomationTest tests/CharacterLabAutomationSmoke.cpp)
target_include_directories(CharacterLabAutomationTest PRIVATE include)
target_compile_options(CharacterLabAutomationTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(CharacterLabAutomationTest PRIVATE AevryndEngine)
add_test(NAME CharacterLabAutomationTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:CharacterLabAutomationTest>'")
