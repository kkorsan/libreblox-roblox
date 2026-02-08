include(App)
include(Network)
include(Rendering)
include(ClientShared)

find_package(Crow CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_compile_definitions(RBX_RCC_SECURITY)

include_directories(./include)

add_executable(RCCService
    src/DummyWindow.cpp
    src/OperationalSecurity.cpp
    src/RCCService.cpp
    src/RCCServiceHttpImpl.cpp
    src/stdafx.cpp
    src/ThumbnailGenerator.cpp

    ${CLIENT_SHARED_DIR}/DumpErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/ErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/LogManager.cpp
    ${CLIENT_SHARED_DIR}/VersionInfo.cpp
    ${CLIENT_SHARED_DIR}/CountersClient.cpp
    ${CLIENT_SHARED_DIR}/SharedLauncher.cpp

    ${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaVMServer.cpp
)

target_link_libraries(RCCService PRIVATE
    $<TARGET_OBJECTS:Base>
    $<TARGET_OBJECTS:App_RCC>
    $<TARGET_OBJECTS:Network_RCC>
    $<TARGET_OBJECTS:BulletPhysics>
    $<TARGET_OBJECTS:G3D>
    $<TARGET_OBJECTS:AppDraw>
    $<TARGET_OBJECTS:GfxBase>
    $<TARGET_OBJECTS:GfxCore_RCC>
    $<TARGET_OBJECTS:GfxRender>

    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::program_options
    Boost::chrono
    Boost::date_time
    Boost::atomic
    Boost::iostreams
    OpenSSL::SSL
    OpenSSL::Crypto
    SDL3::SDL3
    ZLIB::ZLIB
    CURL::libcurl
    GLEW::GLEW
    OpenGL::GL
    Threads::Threads
    Freetype::Freetype
    lz4::lz4
    PNG::PNG
    JPEG::JPEG
    Crow::Crow
    nlohmann_json::nlohmann_json
    "$<$<CONFIG:Release>:FMOD::fmod>"
    "$<$<NOT:$<CONFIG:Release>>:FMOD::fmodL>"

    draco::draco
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(EGL REQUIRED egl)
    pkg_check_modules(WAYLAND_CLIENT wayland-client)
    pkg_check_modules(WAYLAND_EGL wayland-egl)
    
    find_package(X11 REQUIRED)
    
    target_include_directories(RCCService PRIVATE ${EGL_INCLUDE_DIRS} ${WAYLAND_CLIENT_INCLUDE_DIRS} ${WAYLAND_EGL_INCLUDE_DIRS})
    target_link_libraries(RCCService PRIVATE m rt ${X11_LIBRARIES} ${EGL_LIBRARIES} ${WAYLAND_CLIENT_LIBRARIES} ${WAYLAND_EGL_LIBRARIES})
    
    set_target_properties(RCCService PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:${CMAKE_SOURCE_DIR}/third-party/fmod/linux/lib/${FMOD_LINUX_ARCH_DIR}"
    )
endif()

# Installation
install(TARGETS RCCService RUNTIME DESTINATION .)

# TODO: cleanup (Copied from client logic)
if(UNIX AND NOT APPLE)
    if(DEFINED FMOD_LINUX_ARCH_DIR)
        set(FMOD_LIB_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/linux/lib/${FMOD_LINUX_ARCH_DIR}")
    else()
        set(FMOD_LIB_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/linux/lib/x86_64")
    endif()

    file(GLOB FMOD_RELEASE_FILES
        "${FMOD_LIB_DIR}/libfmod.so"
        "${FMOD_LIB_DIR}/libfmod.so.*"
    )

    file(GLOB FMOD_DEBUG_FILES
        "${FMOD_LIB_DIR}/libfmodL.so"
        "${FMOD_LIB_DIR}/libfmodL.so.*"
    )

    if(FMOD_RELEASE_FILES)
        install(FILES ${FMOD_RELEASE_FILES} DESTINATION .)
    endif()

    if(FMOD_DEBUG_FILES)
        install(FILES ${FMOD_DEBUG_FILES} DESTINATION .)
    endif()

elseif(WIN32)
    set(FMOD_LIB_DIR "${CMAKE_SOURCE_DIR}/third-party/fmod/windows/lib/x64")

    file(GLOB FMOD_RELEASE_FILES "${FMOD_LIB_DIR}/fmod.dll")
    file(GLOB FMOD_DEBUG_FILES   "${FMOD_LIB_DIR}/fmodL.dll")

    if(FMOD_RELEASE_FILES)
        install(FILES ${FMOD_RELEASE_FILES} DESTINATION .)
    endif()

    if(FMOD_DEBUG_FILES)
        install(FILES ${FMOD_DEBUG_FILES} DESTINATION .)
    endif()
endif()
