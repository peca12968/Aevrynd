cmake_minimum_required(VERSION 3.24)
project(AevryndEnvironmentCollisionStreaming3DTest LANGUAGES CXX)

add_executable(AevryndEnvironmentCollisionStreaming3DTest
  ../EnvironmentCollisionStreaming3DSmoke.cpp
  ../../src/CollisionWorld3D.cpp
  ../../src/PhysicsWorld3D.cpp
  ../../src/EnvironmentCollisionStreaming3D.cpp
)
target_include_directories(AevryndEnvironmentCollisionStreaming3DTest PRIVATE
  ../../include
)
target_compile_features(AevryndEnvironmentCollisionStreaming3DTest PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(AevryndEnvironmentCollisionStreaming3DTest PRIVATE
    /W4 /WX /permissive- /utf-8 /Z7
  )
endif()

enable_testing()
add_test(NAME AevryndEnvironmentCollisionStreaming3DTest
  COMMAND AevryndEnvironmentCollisionStreaming3DTest)
