add_library(AevryndEngine STATIC
  src/ArchitectureMeshes3D.cpp
  src/Animation3D.cpp
  src/AnimationGraph3D.cpp
  src/AssetManifest.cpp
  src/Audio.cpp
  src/CharacterController3D.cpp
  src/CollisionWorld3D.cpp
  src/ECS.cpp
  src/Effects2D.cpp
  src/Engine.cpp
  src/EnvironmentMeshes3D.cpp
  src/FrameCapture3D.cpp
  src/HeightField3D.cpp
  src/Memory.cpp
  src/MaterialLibrary3D.cpp
  src/D3D12Renderer.cpp
  src/EnvironmentIbl3D.cpp
  src/MeshLoader.cpp
  src/MeshProcessing3D.cpp
  src/Navigation3D.cpp
  src/PhysicsWorld3D.cpp
  src/PbrMaterialCooker3D.cpp
  src/Profiling.cpp
  src/ProceduralPlacement3D.cpp
  src/SceneECS3D.cpp
  src/Serialization.cpp
  src/SkinnedModel3D.cpp
  src/SkinnedModelAsset.cpp
  src/SkinnedMaterialSidecar3D.cpp
  src/SkinnedRenderable3D.cpp
  src/Spatial2D.cpp
  src/StaticMeshAssetLibrary3D.cpp
  src/TerrainClipmap3D.cpp
  src/TemporalMotion3D.cpp
  src/WorldBuilder3D.cpp
  src/WorldStreamer3D.cpp
  src/VisibilityBvh3D.cpp
  src/Vegetation3D.cpp
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
aevrynd_compile_shader(VelocityWorldVertexShader.h World.hlsl VelocityWorldVertexMain vs_5_0 VelocityWorldVertexShader)
aevrynd_compile_shader(VelocityInstancedVertexShader.h World.hlsl VelocityInstancedVertexMain vs_5_0 VelocityInstancedVertexShader)
aevrynd_compile_shader(VelocitySkinnedVertexShader.h World.hlsl VelocitySkinnedVertexMain vs_5_0 VelocitySkinnedVertexShader)
aevrynd_compile_shader(VelocityPixelShader.h World.hlsl VelocityPixelMain ps_5_0 VelocityPixelShader)
aevrynd_compile_shader(VelocityMaskedInstancedVertexShader.h World.hlsl VelocityMaskedInstancedVertexMain vs_5_0 VelocityMaskedInstancedVertexShader)
aevrynd_compile_shader(VelocityMaskedSkinnedVertexShader.h World.hlsl VelocityMaskedSkinnedVertexMain vs_5_0 VelocityMaskedSkinnedVertexShader)
aevrynd_compile_shader(VelocityMaskedPixelShader.h World.hlsl VelocityMaskedPixelMain ps_5_0 VelocityMaskedPixelShader)
aevrynd_compile_shader(SkinnedVertexShader.h World.hlsl SkinnedVertexMain vs_5_0 SkinnedVertexShader)
aevrynd_compile_shader(WorldPixelShader.h World.hlsl WorldPixelMain ps_5_0 WorldPixelShader)
aevrynd_compile_shader(WorldMaskedPixelShader.h World.hlsl WorldMaskedPixelMain ps_5_0 WorldMaskedPixelShader)
aevrynd_compile_shader(ShadowWorldVertexShader.h World.hlsl ShadowWorldVertexMain vs_5_0 ShadowWorldVertexShader)
aevrynd_compile_shader(ShadowInstancedVertexShader.h World.hlsl ShadowInstancedVertexMain vs_5_0 ShadowInstancedVertexShader)
aevrynd_compile_shader(ShadowSkinnedVertexShader.h World.hlsl ShadowSkinnedVertexMain vs_5_0 ShadowSkinnedVertexShader)
aevrynd_compile_shader(ShadowSkinnedPixelShader.h World.hlsl ShadowSkinnedPixelMain ps_5_0 ShadowSkinnedPixelShader)
aevrynd_compile_shader(PostProcessVertexShader.h PostProcess.hlsl PostProcessVertexMain vs_5_0 PostProcessVertexShader)
aevrynd_compile_shader(PostProcessPixelShader.h PostProcess.hlsl PostProcessPixelMain ps_5_0 PostProcessPixelShader)
aevrynd_compile_shader(TemporalResolveVertexShader.h TemporalResolve.hlsl TemporalResolveVertexMain vs_5_0 TemporalResolveVertexShader)
aevrynd_compile_shader(TemporalResolvePixelShader.h TemporalResolve.hlsl TemporalResolvePixelMain ps_5_0 TemporalResolvePixelShader)
aevrynd_compile_shader(AmbientOcclusionVertexShader.h AmbientOcclusion.hlsl AmbientOcclusionVertexMain vs_5_0 AmbientOcclusionVertexShader)
aevrynd_compile_shader(AmbientOcclusionPixelShader.h AmbientOcclusion.hlsl AmbientOcclusionPixelMain ps_5_0 AmbientOcclusionPixelShader)
aevrynd_compile_shader(AmbientOcclusionBilateralBlurPixelShader.h AmbientOcclusion.hlsl AmbientOcclusionBilateralBlurPixelMain ps_5_0 AmbientOcclusionBilateralBlurPixelShader)
aevrynd_compile_shader(AmbientOcclusionDepthWorldVertexShader.h AmbientOcclusion.hlsl DepthWorldVertexMain vs_5_0 AmbientOcclusionDepthWorldVertexShader)
aevrynd_compile_shader(AmbientOcclusionDepthInstancedVertexShader.h AmbientOcclusion.hlsl DepthInstancedVertexMain vs_5_0 AmbientOcclusionDepthInstancedVertexShader)
aevrynd_compile_shader(AmbientOcclusionDepthSkinnedVertexShader.h AmbientOcclusion.hlsl DepthSkinnedVertexMain vs_5_0 AmbientOcclusionDepthSkinnedVertexShader)
aevrynd_compile_shader(AmbientOcclusionDepthPixelShader.h AmbientOcclusion.hlsl DepthPixelMain ps_5_0 AmbientOcclusionDepthPixelShader)
add_custom_target(AevryndShaders DEPENDS ${AEVRYND_SHADER_HEADERS})
add_dependencies(AevryndEngine AevryndShaders)
target_sources(AevryndEngine PRIVATE shaders/Canvas.hlsl shaders/World.hlsl shaders/PostProcess.hlsl shaders/AmbientOcclusion.hlsl ${AEVRYND_SHADER_HEADERS})
set_source_files_properties(shaders/Canvas.hlsl shaders/World.hlsl shaders/PostProcess.hlsl shaders/AmbientOcclusion.hlsl PROPERTIES HEADER_FILE_ONLY TRUE)
target_include_directories(AevryndEngine PUBLIC include PRIVATE "${AEVRYND_SHADER_OUTPUT_DIR}")
if(AEVRYND_STREAMLINE_AVAILABLE)
  target_include_directories(AevryndEngine PRIVATE "${STREAMLINE_ROOT}/include")
endif()
target_compile_definitions(AevryndEngine PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX AEVRYND_ENGINE_VERSION="${PROJECT_VERSION}" AEVRYND_STREAMLINE_AVAILABLE=$<BOOL:${AEVRYND_STREAMLINE_AVAILABLE}>)
target_compile_options(AevryndEngine PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEngine PUBLIC d3d12 dxgi windowscodecs ole32 user32 gdi32 psapi xaudio2 xinput9_1_0)

add_executable(AevryndDrawListTest tests/DrawListSmoke.cpp)
add_executable(AevryndVelocityReadbackTest tests/VelocityReadbackSmoke.cpp)
add_executable(AevryndTemporalAntiAliasingTest tests/TemporalAntiAliasingSmoke.cpp)
target_link_libraries(AevryndVelocityReadbackTest PRIVATE AevryndEngine)
target_compile_options(AevryndVelocityReadbackTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndVelocityReadbackTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
add_test(NAME AevryndVelocityReadbackTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndVelocityReadbackTest>'")
target_link_libraries(AevryndTemporalAntiAliasingTest PRIVATE AevryndEngine)
target_compile_options(AevryndTemporalAntiAliasingTest PRIVATE /W4 /permissive- /utf-8)
add_test(NAME AevryndTemporalAntiAliasingTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndTemporalAntiAliasingTest>'")
target_compile_options(AevryndDrawListTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndDrawListTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndDrawListTest PRIVATE AevryndEngine)
add_test(NAME AevryndDrawListTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndDrawListTest>'")

add_executable(AevryndSoftDirectionalShadow3DTest tests/SoftDirectionalShadow3DSmoke.cpp)
target_compile_options(AevryndSoftDirectionalShadow3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSoftDirectionalShadow3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndSoftDirectionalShadow3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSoftDirectionalShadow3DTest>'")

add_executable(AevryndSoftDirectionalShadow3DBenchmark tests/SoftDirectionalShadow3DBenchmark.cpp)
target_compile_options(AevryndSoftDirectionalShadow3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSoftDirectionalShadow3DBenchmark PRIVATE AevryndEngine)

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

add_executable(AevryndScene3DBvhTest tests/Scene3DBvhSmoke.cpp)
target_compile_options(AevryndScene3DBvhTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndScene3DBvhTest PRIVATE AevryndEngine)
add_test(NAME AevryndScene3DBvhTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndScene3DBvhTest>'")

add_executable(AevryndVisibilityBvh3DTest tests/VisibilityBvh3DSmoke.cpp)
target_compile_options(AevryndVisibilityBvh3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndVisibilityBvh3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndVisibilityBvh3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndVisibilityBvh3DTest>'")
add_executable(AevryndVegetation3DTest tests/Vegetation3DSmoke.cpp)
target_compile_options(AevryndVegetation3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndVegetation3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndVegetation3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndVegetation3DTest>'")
add_executable(AevryndVegetation3DBenchmark tests/Vegetation3DBenchmark.cpp)
target_compile_options(AevryndVegetation3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndVegetation3DBenchmark PRIVATE AevryndEngine)
add_test(NAME AevryndVegetation3DBenchmark COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndVegetation3DBenchmark>'")
add_executable(AevryndVegetationD3D12VisualTest tests/VegetationD3D12VisualSmoke.cpp)
target_compile_options(AevryndVegetationD3D12VisualTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndVegetationD3D12VisualTest PRIVATE AevryndEngine)
add_test(NAME AevryndVegetationD3D12VisualTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndVegetationD3D12VisualTest>'")

add_executable(AevryndEnvironmentLighting3DTest tests/EnvironmentLighting3DSmoke.cpp)
target_compile_options(AevryndEnvironmentLighting3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEnvironmentLighting3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndEnvironmentLighting3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndEnvironmentLighting3DTest>'")
add_executable(AevryndEnvironmentLightingD3D12Test tests/EnvironmentLightingD3D12Smoke.cpp)
target_compile_options(AevryndEnvironmentLightingD3D12Test PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndEnvironmentLightingD3D12Test PRIVATE AEVRYND_TESTING=1)
target_link_libraries(AevryndEnvironmentLightingD3D12Test PRIVATE AevryndEngine)
add_test(NAME AevryndEnvironmentLightingD3D12Test COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndEnvironmentLightingD3D12Test>'")

add_executable(AevryndMeshRaycastBvhTest tests/MeshRaycastBvhSmoke.cpp)
target_compile_options(AevryndMeshRaycastBvhTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndMeshRaycastBvhTest PRIVATE AevryndEngine)
add_test(NAME AevryndMeshRaycastBvhTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndMeshRaycastBvhTest>'")

add_executable(AevryndMesh3DTest tests/Mesh3DSmoke.cpp)
target_compile_options(AevryndMesh3DTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndMesh3DTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndMesh3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndMesh3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndMesh3DTest>'")

add_executable(AevryndEnvironmentMeshes3DTest tests/EnvironmentMeshes3DSmoke.cpp)
target_compile_options(AevryndEnvironmentMeshes3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEnvironmentMeshes3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndEnvironmentMeshes3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndEnvironmentMeshes3DTest>'")

add_executable(AevryndProceduralPlacement3DTest tests/ProceduralPlacement3DSmoke.cpp)
target_compile_options(AevryndProceduralPlacement3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndProceduralPlacement3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndProceduralPlacement3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndProceduralPlacement3DTest>'")

add_executable(AevryndTerrainClipmap3DTest tests/TerrainClipmap3DSmoke.cpp)
target_compile_options(AevryndTerrainClipmap3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndTerrainClipmap3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndTerrainClipmap3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndTerrainClipmap3DTest>'")

add_executable(AevryndTerrainDistanceMaterial3DTest tests/TerrainDistanceMaterial3DSmoke.cpp)
target_compile_options(AevryndTerrainDistanceMaterial3DTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndTerrainDistanceMaterial3DTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndTerrainDistanceMaterial3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndTerrainDistanceMaterial3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndTerrainDistanceMaterial3DTest>'")

add_executable(AevryndTerrainDistanceMaterial3DBenchmark tests/TerrainDistanceMaterial3DBenchmark.cpp)
target_compile_options(AevryndTerrainDistanceMaterial3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndTerrainDistanceMaterial3DBenchmark PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndTerrainDistanceMaterial3DBenchmark PRIVATE AevryndEngine)

add_executable(AevryndArchitectureMeshes3DTest tests/ArchitectureMeshes3DSmoke.cpp)
target_compile_options(AevryndArchitectureMeshes3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndArchitectureMeshes3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndArchitectureMeshes3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndArchitectureMeshes3DTest>'")

add_executable(AevryndFrameCapture3DTest tests/FrameCapture3DSmoke.cpp)
target_compile_options(AevryndFrameCapture3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndFrameCapture3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndFrameCapture3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndFrameCapture3DTest>'")

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

add_executable(AevryndStaticMeshAssetLibrary3DTest tests/StaticMeshAssetLibrary3DSmoke.cpp)
target_compile_options(AevryndStaticMeshAssetLibrary3DTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndStaticMeshAssetLibrary3DTest PRIVATE AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}")
target_link_libraries(AevryndStaticMeshAssetLibrary3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndStaticMeshAssetLibrary3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndStaticMeshAssetLibrary3DTest>'")

add_executable(AevryndAudioTest tests/AudioSmoke.cpp)
target_compile_options(AevryndAudioTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndAudioTest PRIVATE AevryndEngine)
add_test(NAME AevryndAudioTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndAudioTest>'")

add_executable(AevryndAnimation3DTest tests/Animation3DSmoke.cpp)
target_compile_options(AevryndAnimation3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndAnimation3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndAnimation3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndAnimation3DTest>'")
add_executable(AevryndAnimationGraph3DTest tests/AnimationGraph3DSmoke.cpp)
target_compile_options(AevryndAnimationGraph3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndAnimationGraph3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndAnimationGraph3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndAnimationGraph3DTest>'")
add_executable(AevryndSkinnedModelAssetV2Test tests/SkinnedModelAssetV2Smoke.cpp)
target_compile_options(AevryndSkinnedModelAssetV2Test PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSkinnedModelAssetV2Test PRIVATE AevryndEngine)
add_test(NAME AevryndSkinnedModelAssetV2Test COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinnedModelAssetV2Test>'")
add_executable(AevryndSkinnedMaterialSidecar3DTest tests/SkinnedMaterialSidecar3DSmoke.cpp)
target_compile_options(AevryndSkinnedMaterialSidecar3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSkinnedMaterialSidecar3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndSkinnedMaterialSidecar3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinnedMaterialSidecar3DTest>'")
add_executable(AevryndSkinnedModelAssetV2Inspect tests/SkinnedModelAssetV2Inspect.cpp)
target_compile_options(AevryndSkinnedModelAssetV2Inspect PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSkinnedModelAssetV2Inspect PRIVATE AevryndEngine)

add_executable(AevryndSkinnedModel3DTest tests/SkinnedModel3DSmoke.cpp)
target_compile_options(AevryndSkinnedModel3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSkinnedModel3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndSkinnedModel3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinnedModel3DTest>'")

add_executable(AevryndSkinnedRenderable3DTest tests/SkinnedRenderable3DSmoke.cpp)
target_compile_options(AevryndSkinnedRenderable3DTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndSkinnedRenderable3DTest PRIVATE AEVRYND_TESTING=1)
target_link_libraries(AevryndSkinnedRenderable3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndSkinnedRenderable3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinnedRenderable3DTest>'")
add_executable(AevryndSkinnedGroupVelocityReadbackTest tests/SkinnedGroupVelocityReadbackSmoke.cpp)
target_compile_definitions(AevryndSkinnedGroupVelocityReadbackTest PRIVATE AEVRYND_TESTING=1)
target_compile_options(AevryndSkinnedGroupVelocityReadbackTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSkinnedGroupVelocityReadbackTest PRIVATE AevryndEngine)
add_test(NAME AevryndSkinnedGroupVelocityReadbackTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinnedGroupVelocityReadbackTest>'")

add_executable(AevryndSkinnedModelAssetTest tests/SkinnedModelAssetSmoke.cpp)
target_compile_options(AevryndSkinnedModelAssetTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSkinnedModelAssetTest PRIVATE AevryndEngine)
add_test(NAME AevryndSkinnedModelAssetTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinnedModelAssetTest>'")

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

add_executable(AevryndAssetManifestTest tests/AssetManifestSmoke.cpp)
target_compile_options(AevryndAssetManifestTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndAssetManifestTest PRIVATE AevryndEngine)
add_test(NAME AevryndAssetManifestTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndAssetManifestTest>'")

add_executable(AevryndPbrMaterialCooker3DTest tests/PbrMaterialCooker3DSmoke.cpp)
target_compile_options(AevryndPbrMaterialCooker3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndPbrMaterialCooker3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndPbrMaterialCooker3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndPbrMaterialCooker3DTest>'")

add_executable(AevryndProfilingTest tests/ProfilingSmoke.cpp)
target_compile_options(AevryndProfilingTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndProfilingTest PRIVATE AevryndEngine)
add_test(NAME AevryndProfilingTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndProfilingTest>'")

add_executable(AevryndSceneECS3DTest tests/SceneECS3DSmoke.cpp)
target_compile_options(AevryndSceneECS3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSceneECS3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndSceneECS3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSceneECS3DTest>'")

add_executable(AevryndMeshProcessing3DTest tests/MeshProcessing3DSmoke.cpp)
target_compile_options(AevryndMeshProcessing3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndMeshProcessing3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndMeshProcessing3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndMeshProcessing3DTest>'")

add_executable(AevryndPbrLightingReferenceTest tests/PbrLightingReferenceSmoke.cpp)
target_compile_options(AevryndPbrLightingReferenceTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndPbrLightingReferenceTest PRIVATE AevryndEngine)
add_test(NAME AevryndPbrLightingReferenceTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndPbrLightingReferenceTest>'")

add_executable(AevryndHeightFog3DTest tests/HeightFog3DSmoke.cpp)
target_compile_options(AevryndHeightFog3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndHeightFog3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndHeightFog3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndHeightFog3DTest>'")

add_executable(AevryndHeightFog3DBenchmark tests/HeightFog3DBenchmark.cpp)
target_compile_options(AevryndHeightFog3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndHeightFog3DBenchmark PRIVATE AevryndEngine)

add_executable(AevryndVolumetricCloud3DTest tests/VolumetricCloud3DSmoke.cpp)
target_compile_options(AevryndVolumetricCloud3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndVolumetricCloud3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndVolumetricCloud3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndVolumetricCloud3DTest>'")

add_executable(AevryndVolumetricCloudD3D12VisualTest tests/VolumetricCloudD3D12VisualSmoke.cpp)
target_compile_options(AevryndVolumetricCloudD3D12VisualTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndVolumetricCloudD3D12VisualTest PRIVATE AEVRYND_TEST_ARTIFACT_DIR=L"${CMAKE_BINARY_DIR}/test-artifacts/volumetric-clouds")
target_link_libraries(AevryndVolumetricCloudD3D12VisualTest PRIVATE AevryndEngine)
add_test(NAME AevryndVolumetricCloudD3D12VisualTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndVolumetricCloudD3D12VisualTest>'")

add_executable(AevryndCloudShadowD3D12VisualTest tests/CloudShadowD3D12VisualSmoke.cpp)
target_compile_options(AevryndCloudShadowD3D12VisualTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndCloudShadowD3D12VisualTest PRIVATE AEVRYND_TEST_ARTIFACT_DIR=L"${CMAKE_BINARY_DIR}/test-artifacts/cloud-shadows")
target_link_libraries(AevryndCloudShadowD3D12VisualTest PRIVATE AevryndEngine)
add_test(NAME AevryndCloudShadowD3D12VisualTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndCloudShadowD3D12VisualTest>'")

add_executable(AevryndDirectionalLightShaft3DTest tests/DirectionalLightShaft3DSmoke.cpp)
target_compile_options(AevryndDirectionalLightShaft3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndDirectionalLightShaft3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndDirectionalLightShaft3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndDirectionalLightShaft3DTest>'")

add_executable(AevryndDirectionalLightShaftD3D12VisualTest tests/DirectionalLightShaftD3D12VisualSmoke.cpp)
target_compile_options(AevryndDirectionalLightShaftD3D12VisualTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndDirectionalLightShaftD3D12VisualTest PRIVATE AEVRYND_TEST_ARTIFACT_DIR=L"${CMAKE_BINARY_DIR}/test-artifacts/directional-light-shafts")
target_link_libraries(AevryndDirectionalLightShaftD3D12VisualTest PRIVATE AevryndEngine)
add_test(NAME AevryndDirectionalLightShaftD3D12VisualTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndDirectionalLightShaftD3D12VisualTest>'")

add_executable(AevryndSkinSubsurface3DTest tests/SkinSubsurface3DSmoke.cpp)
target_compile_options(AevryndSkinSubsurface3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndSkinSubsurface3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndSkinSubsurface3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinSubsurface3DTest>'")

add_executable(AevryndSkinSubsurfaceD3D12VisualTest tests/SkinSubsurfaceD3D12VisualSmoke.cpp)
target_compile_options(AevryndSkinSubsurfaceD3D12VisualTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndSkinSubsurfaceD3D12VisualTest PRIVATE AEVRYND_TEST_ARTIFACT_DIR=L"${CMAKE_BINARY_DIR}/test-artifacts/skin-subsurface")
target_link_libraries(AevryndSkinSubsurfaceD3D12VisualTest PRIVATE AevryndEngine)
add_test(NAME AevryndSkinSubsurfaceD3D12VisualTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndSkinSubsurfaceD3D12VisualTest>'")

add_executable(AevryndBloom3DTest tests/Bloom3DSmoke.cpp)
target_compile_options(AevryndBloom3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndBloom3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndBloom3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndBloom3DTest>'")
add_executable(AevryndBloom3DBenchmark tests/Bloom3DBenchmark.cpp)
target_compile_options(AevryndBloom3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndBloom3DBenchmark PRIVATE AevryndEngine)

add_executable(AevryndWaterOptics3DTest tests/WaterOptics3DSmoke.cpp)
target_compile_options(AevryndWaterOptics3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndWaterOptics3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndWaterOptics3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndWaterOptics3DTest>'")
add_executable(AevryndWaterRefraction3DBenchmark tests/WaterRefraction3DBenchmark.cpp)
target_compile_options(AevryndWaterRefraction3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndWaterRefraction3DBenchmark PRIVATE AevryndEngine)

add_executable(AevryndTemporalMotion3DTest tests/TemporalMotion3DSmoke.cpp)
target_compile_options(AevryndTemporalMotion3DTest PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndTemporalMotion3DTest PRIVATE AevryndEngine)
add_test(NAME AevryndTemporalMotion3DTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndTemporalMotion3DTest>'")

add_executable(AevryndRendererBenchmark tests/RendererBenchmark.cpp)
target_compile_options(AevryndRendererBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndRendererBenchmark PRIVATE AevryndEngine)

add_executable(AevryndScene3DBenchmark tests/Scene3DBenchmark.cpp)
target_compile_options(AevryndScene3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndScene3DBenchmark PRIVATE AevryndEngine)

add_executable(AevryndScene3DBvhBenchmark tests/Scene3DBvhBenchmark.cpp)
target_compile_options(AevryndScene3DBvhBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndScene3DBvhBenchmark PRIVATE AevryndEngine)

add_executable(AevryndMeshRaycastBvhBenchmark tests/MeshRaycastBvhBenchmark.cpp)
target_compile_options(AevryndMeshRaycastBvhBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndMeshRaycastBvhBenchmark PRIVATE AevryndEngine)

add_executable(AevryndEnvironmentLighting3DBenchmark tests/EnvironmentLighting3DBenchmark.cpp)
target_compile_options(AevryndEnvironmentLighting3DBenchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEnvironmentLighting3DBenchmark PRIVATE AevryndEngine)

add_executable(AevryndEnvironmentLightingD3D12Benchmark tests/EnvironmentLightingD3D12Benchmark.cpp)
target_compile_options(AevryndEnvironmentLightingD3D12Benchmark PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEnvironmentLightingD3D12Benchmark PRIVATE AevryndEngine)

add_executable(AevryndEnvironmentLightingD3D12VisualTest tests/EnvironmentLightingD3D12VisualSmoke.cpp)
target_compile_options(AevryndEnvironmentLightingD3D12VisualTest PRIVATE /W4 /permissive- /utf-8)
target_compile_definitions(AevryndEnvironmentLightingD3D12VisualTest PRIVATE
  AEVRYND_SOURCE_DIR=L"${CMAKE_SOURCE_DIR}"
  AEVRYND_TEST_ARTIFACT_DIR=L"${CMAKE_BINARY_DIR}/test-artifacts/environment-lighting")
target_link_libraries(AevryndEnvironmentLightingD3D12VisualTest PRIVATE AevryndEngine)
add_test(NAME AevryndEnvironmentLightingD3D12VisualTest COMMAND powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "& '$<TARGET_FILE:AevryndEnvironmentLightingD3D12VisualTest>'")
