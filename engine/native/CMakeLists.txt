add_library(AevryndEngine STATIC
  src/Animation3D.cpp
  src/Audio.cpp
  src/CharacterController3D.cpp
  src/CollisionWorld3D.cpp
  src/ECS.cpp
  src/Effects2D.cpp
  src/Engine.cpp
  src/EnvironmentMeshes3D.cpp
  src/HeightField3D.cpp
  src/Memory.cpp
  src/MaterialLibrary3D.cpp
  src/D3D12Renderer.cpp
  src/MeshLoader.cpp
  src/Navigation3D.cpp
  src/PhysicsWorld3D.cpp
  src/Serialization.cpp
  src/Spatial2D.cpp
  src/WorldBuilder3D.cpp
  src/WorldStreamer3D.cpp
)

set(AEVRYND_STREAMLINE_AVAILABLE OFF)
if(AEVRYND_ENABLE_STREAMLINE AND EXISTS "${STREAMLINE_ROOT}/include/sl.h" AND EXISTS "${STREAMLINE_ROOT}/bin/x64/sl.interposer.dll")
  set(AEVRYND_STREAMLINE_AVAILABLE ON)
  target_sources(AevryndEngine PRIVATE src/StreamlineDLSS.cpp)
else()
  target_sources(AevryndEngine PRIVATE src/StreamlineDLSSStub.cpp)
endif()
set(AEVRYND_STREAMLINE_AVAILABLE "${AEVRYND_STREAMLINE_AVAILABLE}" CACHE INTERNAL "NVIDIA Streamline SDK availability" FORCE)

cmake_host_system_information(RESULT WINDOWS_KITS_ROOT QUERY WINDOWS_REGISTRY "HKLM/SOFTWARE/Microsoft/Windows Kits/Installed Roots" VALUE "KitsRoot10")
set(AEVRYND_FXC "${WINDOWS_KITS_ROOT}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64/fxc.exe")
if(NOT EXISTS "${AEVRYND_FXC}")
  file(GLOB AEVRYND_FXC_CANDIDATES "${WINDOWS_KITS_ROOT}/bin/*/x64/fxc.exe")
  list(SORT AEVRYND_FXC_CANDIDATES COMPARE NATURAL ORDER DESCENDING)
  if(AEVRYND_FXC_CANDIDATES)
    list(GET AEVRYND_FXC_CANDIDATES 0 AEVRYND_FXC)
  else()
    message(FATAL_ERROR "Windows SDK shader compiler fxc.exe was not found")
  endif()
endif()

set(AEVRYND_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/shaders")
function(aevrynd_compile_shader output_name source_name entry profile symbol)
  set(output "${AEVRYND_SHADER_OUTPUT_DIR}/${output_name}")
  add_custom_command(
    OUTPUT "${output}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${AEVRYND_SHADER_OUTPUT_DIR}"
    COMMAND "${AEVRYND_FXC}" /nologo /O3 /T "${profile}" /E "${entry}" /Fh "${output}" /Vn "${symbol}" "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${source_name}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shaders/${source_name}"
    VERBATIM
  )
  set(AEVRYND_SHADER_HEADERS ${AEVRYND_SHADER_HEADERS} "${output}" PARENT_SCOPE)
endfunction()

aevrynd_compile_shader(CanvasVertexShader.h Canvas.hlsl CanvasVertexMain vs_5_0 CanvasVertexShader)
aevrynd_compile_shader(CanvasPixelShader.h Canvas.hlsl CanvasPixelMain ps_5_0 CanvasPixelShader)
aevrynd_compile_shader(WorldVertexShader.h World.hlsl WorldVertexMain vs_5_0 WorldVertexShader)
aevrynd_compile_shader(InstancedVertexShader.h World.hlsl InstancedVertexMain vs_5_0 InstancedVertexShader)
aevrynd_compile_shader(WorldPixelShader.h World.hlsl WorldPixelMain ps_5_0 WorldPixelShader)
add_custom_target(AevryndShaders DEPENDS ${AEVRYND_SHADER_HEADERS})
add_dependencies(AevryndEngine AevryndShaders)
target_sources(AevryndEngine PRIVATE shaders/Canvas.hlsl shaders/World.hlsl ${AEVRYND_SHADER_HEADERS})
set_source_files_properties(shaders/Canvas.hlsl shaders/World.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)
target_include_directories(AevryndEngine PUBLIC include PRIVATE "${AEVRYND_SHADER_OUTPUT_DIR}")
if(AEVRYND_STREAMLINE_AVAILABLE)
  target_include_directories(AevryndEngine PRIVATE "${STREAMLINE_ROOT}/include")
endif()
target_compile_definitions(AevryndEngine PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX AEVRYND_ENGINE_VERSION="${PROJECT_VERSION}" AEVRYND_STREAMLINE_AVAILABLE=$<BOOL:${AEVRYND_STREAMLINE_AVAILABLE}>)
target_compile_options(AevryndEngine PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEngine PUBLIC d3d12 dxgi windowscodecs ole32 user32 gdi32 psapi xaudio2 xinput9_1_0)

add_executable(AevryndDrawListTest tests/DrawListSmoke.cpp)
target_compile_options(AevryndDrawListTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndDrawListTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
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

add_executable(AevryndCollisionWorldTest tests/CollisionWorldSmoke.cpp)
target_compile_options(AevryndCollisionWorldTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndCollisionWorldTest PRIVATE AevryndEngine)
add_test(NAME AevryndCollisionWorldTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndCollisionWorldTest>'")

add_executable(AevryndScene3DTest tests/Scene3DSmoke.cpp)
target_compile_options(AevryndScene3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndScene3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndScene3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndScene3DTest>'")

add_executable(AevryndMesh3DTest tests/Mesh3DSmoke.cpp)
target_compile_options(AevryndMesh3DTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndMesh3DTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndMesh3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndMesh3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndMesh3DTest>'")

add_executable(AevryndEnvironmentMeshes3DTest tests/EnvironmentMeshes3DSmoke.cpp)
target_compile_options(AevryndEnvironmentMeshes3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEnvironmentMeshes3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndEnvironmentMeshes3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndEnvironmentMeshes3DTest>'")

add_executable(AevryndMeshUploadStreamingTest tests/MeshUploadStreamingSmoke.cpp)
target_compile_options(AevryndMeshUploadStreamingTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndMeshUploadStreamingTest PRIVATE AevryndEngine)
add_test(NAME AevryndMeshUploadStreamingTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndMeshUploadStreamingTest>'")

add_executable(AevryndTextureUploadStreamingTest tests/TextureUploadStreamingSmoke.cpp)
target_compile_options(AevryndTextureUploadStreamingTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndTextureUploadStreamingTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndTextureUploadStreamingTest PRIVATE AevryndEngine)
add_test(NAME AevryndTextureUploadStreamingTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndTextureUploadStreamingTest>'")

add_executable(AevryndMaterialLibrary3DTest tests/MaterialLibrary3DSmoke.cpp)
target_compile_options(AevryndMaterialLibrary3DTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndMaterialLibrary3DTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndMaterialLibrary3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndMaterialLibrary3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndMaterialLibrary3DTest>'")

add_executable(AevryndAudioTest tests/AudioSmoke.cpp)
target_compile_options(AevryndAudioTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndAudioTest PRIVATE AevryndEngine)
add_test(NAME AevryndAudioTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndAudioTest>'")

add_executable(AevryndAnimation3DTest tests/Animation3DSmoke.cpp)
target_compile_options(AevryndAnimation3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndAnimation3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndAnimation3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndAnimation3DTest>'")

add_executable(AevryndWorldStreamer3DTest tests/WorldStreamer3DSmoke.cpp)
target_compile_options(AevryndWorldStreamer3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndWorldStreamer3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndWorldStreamer3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndWorldStreamer3DTest>'")

add_executable(AevryndNavigation3DTest tests/Navigation3DSmoke.cpp)
target_compile_options(AevryndNavigation3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndNavigation3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndNavigation3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndNavigation3DTest>'")

add_executable(AevryndHeightField3DTest tests/HeightField3DSmoke.cpp)
target_compile_options(AevryndHeightField3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndHeightField3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndHeightField3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndHeightField3DTest>'")

add_executable(AevryndECSTest tests/ECSSmoke.cpp)
target_compile_options(AevryndECSTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndECSTest PRIVATE AevryndEngine)
add_test(NAME AevryndECSTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndECSTest>'")

add_executable(AevryndSerializationTest tests/SerializationSmoke.cpp)
target_compile_options(AevryndSerializationTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSerializationTest PRIVATE AevryndEngine)
add_test(NAME AevryndSerializationTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSerializationTest>'")

add_executable(AevryndPhysicsWorld3DTest tests/PhysicsWorld3DSmoke.cpp)
target_compile_options(AevryndPhysicsWorld3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndPhysicsWorld3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndPhysicsWorld3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndPhysicsWorld3DTest>'")

add_executable(AevryndRendererBenchmark tests/RendererBenchmark.cpp)
target_compile_options(AevryndRendererBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndRendererBenchmark PRIVATE AevryndEngine)

add_executable(AevryndScene3DBenchmark tests/Scene3DBenchmark.cpp)
target_compile_options(AevryndScene3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndScene3DBenchmark PRIVATE AevryndEngine)
