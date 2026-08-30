add_executable(GameplayLab WIN32 app.manifest src/main.cpp src/GameplayLabGame.cpp src/GameplayRules3D.cpp src/GameplayCombat3D.cpp src/GameplayAnimation3D.cpp src/ObjectPaletteAdapter3D.cpp src/GameplayShowcaseEnvironment3D.cpp src/GameplayObjectPaletteUi3D.cpp)
set(CHARACTER_PRESENTATION_FINGERPRINT_FILES
  engine/native/CMakeLists.txt engine/native/include/Aevrynd/CharacterPresentation3D.h
  engine/native/src/CharacterPresentation3D.cpp engine/native/src/Engine.cpp
  engine/native/tests/CharacterPresentation3DSmoke.cpp games/character_lab/native/src/CharacterLabGame.cpp
  engine/native/include/Aevrynd/EnvironmentCollisionStreaming3D.h
  engine/native/src/EnvironmentCollisionStreaming3D.cpp
  engine/native/tests/EnvironmentCollisionStreaming3DSmoke.cpp
  games/gameplay_lab/game.json games/gameplay_lab/native/CMakeLists.txt
  games/gameplay_lab/native/include/GameplayAnimation3D.h games/gameplay_lab/native/include/GameplayCombat3D.h
  games/gameplay_lab/native/include/GameplayLabGame.h games/gameplay_lab/native/src/GameplayAnimation3D.cpp
  games/gameplay_lab/native/include/GameplayShowcaseEnvironment3D.h games/gameplay_lab/native/src/GameplayShowcaseEnvironment3D.cpp
  games/gameplay_lab/native/include/GameplayObjectPaletteUi3D.h games/gameplay_lab/native/src/GameplayObjectPaletteUi3D.cpp
  games/gameplay_lab/native/src/GameplayLabGame.cpp games/gameplay_lab/native/src/main.cpp
  games/gameplay_lab/native/tests/GameplayCombatAnimationSmoke.cpp games/gameplay_lab/native/tests/GameplayLabCaptureSmoke.ps1)
set(CHARACTER_PRESENTATION_FINGERPRINT_INPUT "")
foreach(CHARACTER_PRESENTATION_FILE IN LISTS CHARACTER_PRESENTATION_FINGERPRINT_FILES)
  file(SHA256 "${CMAKE_SOURCE_DIR}/${CHARACTER_PRESENTATION_FILE}" CHARACTER_PRESENTATION_FILE_SHA)
  string(APPEND CHARACTER_PRESENTATION_FINGERPRINT_INPUT "${CHARACTER_PRESENTATION_FILE}:${CHARACTER_PRESENTATION_FILE_SHA}\n")
endforeach()
string(SHA256 CHARACTER_PRESENTATION_FINGERPRINT "${CHARACTER_PRESENTATION_FINGERPRINT_INPUT}")
target_include_directories(GameplayLab PRIVATE include)
target_compile_options(GameplayLab PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(GameplayLab PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX AEVRYND_CHARACTER_BUILD_FINGERPRINT="${CHARACTER_PRESENTATION_FINGERPRINT}")
target_link_libraries(GameplayLab PRIVATE AevryndEngine Ole32)
set_target_properties(GameplayLab PROPERTIES OUTPUT_NAME "Aevrynd-Gameplay-Lab" VS_DPI_AWARE "PerMonitor")
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
add_executable(GameplayLabRulesTest tests/GameplayLabRulesSmoke.cpp src/GameplayRules3D.cpp)
target_include_directories(GameplayLabRulesTest PRIVATE include)
target_compile_options(GameplayLabRulesTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayLabRulesTest PRIVATE AevryndEngine)
add_test(NAME GameplayLabRulesTest COMMAND GameplayLabRulesTest)
add_executable(GameplayShowcaseEnvironmentTest tests/GameplayShowcaseEnvironment3DSmoke.cpp src/GameplayShowcaseEnvironment3D.cpp)
target_include_directories(GameplayShowcaseEnvironmentTest PRIVATE include)
target_compile_options(GameplayShowcaseEnvironmentTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(GameplayShowcaseEnvironmentTest PRIVATE AevryndEngine)
add_test(NAME GameplayShowcaseEnvironmentTest COMMAND GameplayShowcaseEnvironmentTest)
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
add_test(NAME GameplayLabCaptureSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>")
set_tests_properties(GameplayLabCaptureSmoke PROPERTIES TIMEOUT 180)
add_test(NAME GameplayLabGpuValidationCaptureSmoke COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/GameplayLabCaptureSmoke.ps1" -Executable "$<TARGET_FILE:GameplayLab>" -GpuValidation)
set_tests_properties(GameplayLabGpuValidationCaptureSmoke PROPERTIES TIMEOUT 180)
add_executable(GameplayLabCombatAnimationTest tests/GameplayCombatAnimationSmoke.cpp src/GameplayCombat3D.cpp src/GameplayAnimation3D.cpp)
target_include_directories(GameplayLabCombatAnimationTest PRIVATE include)
target_compile_options(GameplayLabCombatAnimationTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(GameplayLabCombatAnimationTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(GameplayLabCombatAnimationTest PRIVATE AevryndEngine)
add_test(NAME GameplayLabCombatAnimationTest COMMAND GameplayLabCombatAnimationTest)
