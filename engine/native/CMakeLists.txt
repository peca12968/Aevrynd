add_library(AevryndEngine STATIC
  src/Audio.cpp
  src/CharacterController3D.cpp
  src/Engine.cpp
  src/D3D12Renderer.cpp
  src/MeshLoader.cpp
  src/StreamlineDLSS.cpp
)
set(STREAMLINE_ROOT "${CMAKE_SOURCE_DIR}/engine/third_party/streamline")
target_include_directories(AevryndEngine PUBLIC include PRIVATE "${STREAMLINE_ROOT}/include")
target_compile_definitions(AevryndEngine PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(AevryndEngine PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEngine PUBLIC "${STREAMLINE_ROOT}/lib/x64/sl.interposer.lib" d3dcompiler user32 gdi32 xaudio2 xinput9_1_0)

add_executable(AevryndDrawListTest tests/DrawListSmoke.cpp)
target_compile_options(AevryndDrawListTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndDrawListTest PRIVATE AevryndEngine)
add_test(NAME AevryndDrawListTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndDrawListTest>'")

add_executable(AevryndCameraTest tests/CameraSmoke.cpp)
target_compile_options(AevryndCameraTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndCameraTest PRIVATE AevryndEngine)
add_test(NAME AevryndCameraTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndCameraTest>'")

add_executable(AevryndCharacterControllerTest tests/CharacterControllerSmoke.cpp)
target_compile_options(AevryndCharacterControllerTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndCharacterControllerTest PRIVATE AevryndEngine)
add_test(NAME AevryndCharacterControllerTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndCharacterControllerTest>'")

add_executable(AevryndScene3DTest tests/Scene3DSmoke.cpp)
target_compile_options(AevryndScene3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndScene3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndScene3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndScene3DTest>'")

add_executable(AevryndMesh3DTest tests/Mesh3DSmoke.cpp)
target_compile_options(AevryndMesh3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndMesh3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndMesh3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndMesh3DTest>'")

add_executable(AevryndAudioTest tests/AudioSmoke.cpp)
target_compile_options(AevryndAudioTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndAudioTest PRIVATE AevryndEngine)
add_test(NAME AevryndAudioTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndAudioTest>'")

add_executable(AevryndRendererBenchmark tests/RendererBenchmark.cpp)
target_compile_options(AevryndRendererBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndRendererBenchmark PRIVATE AevryndEngine)

add_executable(AevryndScene3DBenchmark tests/Scene3DBenchmark.cpp)
target_compile_options(AevryndScene3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndScene3DBenchmark PRIVATE AevryndEngine)
