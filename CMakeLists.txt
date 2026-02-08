# Require a modern CMake version.
cmake_minimum_required(VERSION 3.16)

project(roblox LANGUAGES C CXX ASM)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Configuration & Setup --------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Security hardening flags
if(MSVC)
    add_compile_options(/GS /sdl /guard:cf)
    add_link_options(/NXCOMPAT /DYNAMICBASE /HIGHENTROPYVA)
else()
    add_compile_options(-fstack-protector-strong -fPIC)
    if(NOT APPLE AND NOT MINGW)
        add_link_options(-Wl,-z,relro,-z,now)
    endif()
endif()

# Enable C++ exceptions globally
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions")

# Add our custom modules directory to CMake's search path.
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Modules")

# --- Build Targets Definition ----------------------------------------------
option(BUILD_SDLCLIENT           "Build the SDL Client"       ON)
option(BUILD_ROBLOXSTUDIO        "Build Roblox Studio"        ON)
option(BUILD_RCCSERVICE          "Build the RCC Service"      ON)
option(BUILD_ANDROID             "Build the Android Library"  OFF)
option(BUILD_CORESCRIPTCONVERTER "Build the CoreScriptConverter" ON)

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build" FORCE)
endif()

# Configure build type specific settings
if(CMAKE_BUILD_TYPE MATCHES "Debug")
    set(RBX_BUILD_TYPE "debug")
    add_definitions(-D_DEBUG -DDEBUG -DZLIB_DEBUG)
else()
    set(RBX_BUILD_TYPE "release")
    add_definitions(-DNDEBUG -D_FORTIFY_SOURCE=2)
    add_compile_options(-O3)
endif()

if(MINGW OR CYGWIN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wa,-mbig-obj -O2")
endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# --- Include Custom CMake Modules -------------------------------------------
include(ListFilter)

# -- Find Dependencies --
find_package(Boost REQUIRED COMPONENTS system filesystem thread iostreams program_options chrono date_time atomic unit_test_framework)
find_package(OpenSSL REQUIRED)
find_package(SDL3 CONFIG REQUIRED)
find_package(ZLIB REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)
find_package(Freetype REQUIRED)
find_package(GLEW REQUIRED)
find_package(PNG REQUIRED)
find_package(lz4 CONFIG REQUIRED)
find_package(JPEG REQUIRED)
find_package(draco CONFIG REQUIRED)
include_directories(${JPEG_INCLUDE_DIRS})

add_compile_definitions(HAVE_OPENSSL)

# Special and proprietary
find_package(FMOD REQUIRED)
if(ANDROID)
    find_package(unofficial-breakpad CONFIG REQUIRED)
endif()

# --- Core Module Definition ---
set(ROBLOX_CORE_MODULES
    third-party/bullet-physics
    engine/base
    engine/rendering
)

foreach(module ${ROBLOX_CORE_MODULES})
    add_subdirectory(${CMAKE_SOURCE_DIR}/${module})
endforeach()

# --- Shared Directories ---
set(CLIENT_SHARED_DIR "${CMAKE_SOURCE_DIR}/engine/client/shared")
set(CLIENT_BASE_DIR "${CMAKE_SOURCE_DIR}/engine/client/base")

# --- Conditional Build Targets ---

if(BUILD_RCCSERVICE)
    set(RBX_CONFIG RCC)
    add_subdirectory(engine/app ${CMAKE_BINARY_DIR}/App_RCC)
    add_subdirectory(engine/network ${CMAKE_BINARY_DIR}/Network_RCC)
    add_subdirectory(engine/rendering/gfx/core ${CMAKE_BINARY_DIR}/Rendering/GfxCore_RCC)
    target_compile_definitions(App_RCC PRIVATE RBX_RCC_SECURITY)
    target_compile_definitions(Network_RCC PRIVATE RBX_RCC_SECURITY)
    target_compile_definitions(GfxCore_RCC PRIVATE RBX_RCC_SECURITY)
    add_subdirectory(applications/rcc-service)
endif()

if(BUILD_ROBLOXSTUDIO)
    set(RBX_CONFIG STUDIO)
    add_subdirectory(engine/app ${CMAKE_BINARY_DIR}/App_STUDIO)
    add_subdirectory(engine/network ${CMAKE_BINARY_DIR}/Network_STUDIO)
    add_subdirectory(engine/rendering/gfx/core ${CMAKE_BINARY_DIR}/Rendering/GfxCore_STUDIO)
    target_compile_definitions(App_STUDIO PRIVATE RBX_STUDIO_BUILD)
    target_compile_definitions(Network_STUDIO PRIVATE RBX_STUDIO_BUILD)
    target_compile_definitions(GfxCore_STUDIO PRIVATE RBX_STUDIO_BUILD)
    add_subdirectory(applications/studio)
endif()

# Handle all client-like builds (SDL, Android) in one block
if(BUILD_SDLCLIENT OR BUILD_CORESCRIPTCONVERTER OR BUILD_ANDROID)
    set(RBX_CONFIG CLIENT)
    add_subdirectory(engine/app ${CMAKE_BINARY_DIR}/App_CLIENT)
    add_subdirectory(engine/network ${CMAKE_BINARY_DIR}/Network_CLIENT)
    add_subdirectory(engine/rendering/gfx/core ${CMAKE_BINARY_DIR}/Rendering/GfxCore_CLIENT)

    if(BUILD_CORESCRIPTCONVERTER)
        add_subdirectory(tools/core-script-converter)
    endif()

    if(BUILD_SDLCLIENT)
        add_subdirectory(applications/sdl-client)
    endif()

    if(BUILD_ANDROID)
        add_subdirectory(applications/android)
    endif()
endif()

option(BUILD_TESTING "Build unit tests" ON)

if(BUILD_TESTING)
    set(RBX_CONFIG TEST)
    add_subdirectory(engine/base ${CMAKE_BINARY_DIR}/Base_TEST)
    add_subdirectory(engine/app ${CMAKE_BINARY_DIR}/App_TEST)
    add_subdirectory(engine/network ${CMAKE_BINARY_DIR}/Network_TEST)
    add_subdirectory(engine/rendering/gfx/core ${CMAKE_BINARY_DIR}/Rendering/GfxCore_TEST)
    target_compile_definitions(Base_TEST PRIVATE RBX_TEST_BUILD)
    target_compile_definitions(App_TEST PRIVATE RBX_TEST_BUILD)
    target_compile_definitions(Network_TEST PRIVATE RBX_TEST_BUILD)
    target_compile_definitions(GfxCore_TEST PRIVATE RBX_TEST_BUILD)
    add_subdirectory(tests)
endif()
