cmake_minimum_required(VERSION 3.10)
project(CSG)

add_subdirectory(${CMAKE_SOURCE_DIR}/third-party/sg-core sgCore)
add_library(CSG OBJECT
    src/CSGKernel.cpp
    include/CSGKernel.h
)

target_include_directories(CSG PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(CSG PUBLIC ${CMAKE_SOURCE_DIR}/third-party/sg-core)
target_include_directories(CSG PUBLIC ${CMAKE_SOURCE_DIR}/engine/app/include)
target_include_directories(CSG PUBLIC ${CMAKE_SOURCE_DIR}/engine/base/include)
target_include_directories(CSG PUBLIC ${CMAKE_SOURCE_DIR}/engine/rendering/g3d/include)
target_include_directories(CSG PUBLIC ${CMAKE_SOURCE_DIR}/engine/log/include)

# Set platform-specific definitions
if(WIN32)
    target_compile_definitions(CSG PRIVATE SG_CURRENT_PLATFORM=0)  # SG_PLATFORM_WINDOWS
elseif(APPLE)
    target_compile_definitions(CSG PRIVATE SG_CURRENT_PLATFORM=1)  # SG_PLATFORM_MAC
else()
    target_compile_definitions(CSG PRIVATE SG_CURRENT_PLATFORM=3)  # SG_PLATFORM_LINUX for Linux/Unix
endif()

add_dependencies(CSG sgCore)
