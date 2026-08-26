add_library(AevryndEngine STATIC
  src/Engine.cpp
  src/D3D12Renderer.cpp
  src/StreamlineDLSS.cpp
)
set(STREAMLINE_ROOT "${CMAKE_SOURCE_DIR}/third_party/streamline")
target_include_directories(AevryndEngine PUBLIC include PRIVATE "${STREAMLINE_ROOT}/include")
target_compile_definitions(AevryndEngine PUBLIC UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(AevryndEngine PRIVATE /W4 /permissive- /utf-8)
target_link_libraries(AevryndEngine PUBLIC "${STREAMLINE_ROOT}/lib/x64/sl.interposer.lib" d3dcompiler user32 gdi32 xinput9_1_0)
