add_executable(FrontierWorld WIN32 app.manifest src/main.cpp src/FrontierWorldGame.cpp)
target_include_directories(FrontierWorld PRIVATE include)
target_compile_options(FrontierWorld PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(FrontierWorld PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(FrontierWorld PRIVATE AevryndEngine)
set_target_properties(FrontierWorld PROPERTIES OUTPUT_NAME "Aevrynd-Frontier-World" VS_DPI_AWARE "PerMonitor")
add_custom_command(TARGET FrontierWorld POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:FrontierWorld>/assets"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/material-packs" "$<TARGET_FILE_DIR:FrontierWorld>/assets/material-packs"
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/engine/assets/model-packs" "$<TARGET_FILE_DIR:FrontierWorld>/assets/model-packs"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:FrontierWorld>/assets/model-packs/poly-haven/quiver_tree_02/source"
  COMMAND ${CMAKE_COMMAND} -E remove_directory "$<TARGET_FILE_DIR:FrontierWorld>/assets/model-packs/poly-haven/rock_09/source")
aevrynd_package_streamline_runtime(FrontierWorld)
add_executable(FrontierWorldRulesTest tests/FrontierWorldRulesSmoke.cpp)
target_include_directories(FrontierWorldRulesTest PRIVATE include)
target_compile_options(FrontierWorldRulesTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(FrontierWorldRulesTest PRIVATE AevryndEngine)
add_test(NAME FrontierWorldRulesTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:FrontierWorldRulesTest>'")
add_executable(FrontierWorldShowcaseBenchmark tests/FrontierWorldShowcaseBenchmark.cpp)
target_compile_options(FrontierWorldShowcaseBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(FrontierWorldShowcaseBenchmark PRIVATE AevryndEngine)
add_test(NAME FrontierWorldShowcaseBenchmark COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:FrontierWorldShowcaseBenchmark>'")
add_test(NAME FrontierWorldShowcaseCapture COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/tests/FrontierWorldShowcaseCaptureSmoke.ps1" -Executable "$<TARGET_FILE:FrontierWorld>")
set_tests_properties(FrontierWorldShowcaseCapture PROPERTIES TIMEOUT 390)
