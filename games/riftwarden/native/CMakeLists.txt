add_executable(Riftwarden WIN32
  app.manifest
  src/main.cpp
  src/RiftwardenGame.cpp
  src/LocalQwen.cpp
)
target_include_directories(Riftwarden PRIVATE include)
target_compile_options(Riftwarden PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(Riftwarden PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(Riftwarden PRIVATE AevryndEngine winhttp)
set_target_properties(Riftwarden PROPERTIES OUTPUT_NAME "Riftwarden-DX12")
set_target_properties(Riftwarden PROPERTIES VS_DPI_AWARE "PerMonitor")
aevrynd_package_streamline_runtime(Riftwarden)

add_executable(RiftwardenQwenTest src/QwenSmoke.cpp src/LocalQwen.cpp)
target_include_directories(RiftwardenQwenTest PRIVATE include)
target_compile_options(RiftwardenQwenTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(RiftwardenQwenTest PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(RiftwardenQwenTest PRIVATE winhttp)

add_executable(RiftwardenGameplayRulesTest tests/GameplayRulesSmoke.cpp)
target_include_directories(RiftwardenGameplayRulesTest PRIVATE include)
target_compile_options(RiftwardenGameplayRulesTest PRIVATE /W4 /permissive- /utf-8)
add_test(NAME RiftwardenGameplayRulesTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:RiftwardenGameplayRulesTest>'")
