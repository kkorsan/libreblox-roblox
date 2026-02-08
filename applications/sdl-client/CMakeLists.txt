include(App)
include(Network)
include(Rendering)
include(ClientShared)
include(ClientBase)

find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
find_package(Boost REQUIRED COMPONENTS program_options filesystem thread system iostreams)
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(SDL3 REQUIRED)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLX REQUIRED glx)
endif()

include_directories(include)

#############################################
################## SOURCES ##################
#############################################

list(APPEND SOURCES
    src/Application.cpp
    src/Crypt.cpp
    src/Document.cpp
    src/FunctionMarshaller.cpp
    src/GameVerbs.cpp
    src/main.cpp
    src/RenderJob.cpp
    src/Teleporter.cpp
    src/UserInput.cpp
    src/View.cpp
    src/stdafx.cpp

    ${CLIENT_BASE_DIR}/RenderSettingsItem.cpp
    ${CLIENT_BASE_DIR}/MachineConfiguration.cpp
    ${CLIENT_BASE_DIR}/ReflectionMetadata.cpp

    ${CLIENT_SHARED_DIR}/UserInputUtil.cpp
    ${CLIENT_SHARED_DIR}/DumpErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/ErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/LogManager.cpp
    ${CLIENT_SHARED_DIR}/VersionInfo.cpp
    ${CLIENT_SHARED_DIR}/CountersClient.cpp
    ${CLIENT_SHARED_DIR}/SDLGameController.cpp
    ${CLIENT_SHARED_DIR}/SharedLauncher.cpp
    ${CLIENT_SHARED_DIR}/AuthenticationMarshallar.cpp

    ${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaVMClient.cpp
)

#############################################
################## HEADERS ##################
#############################################

list(APPEND HEADERS
    include/Application.h
    include/Crypt.h
    include/Document.h
    include/FunctionMarshaller.h
    include/GameVerbs.h
    include/InitializationError.h
    include/RenderJob.h
    include/Teleporter.h
    include/UserInput.h
    include/View.h
    include/stdafx.h

    ${CLIENT_SHARED_DIR}/SharedLauncher.h
    ${CLIENT_SHARED_DIR}/UserInputUtil.h
    ${CLIENT_SHARED_DIR}/SDLGameController.h
    ${CLIENT_SHARED_DIR}/AuthenticationMarshallar.h

    ${CLIENT_BASE_DIR}/RenderSettingsItem.h
    ${CLIENT_BASE_DIR}/MachineConfiguration.h
    ${CLIENT_BASE_DIR}/ReflectionMetadata.h
)

# create the executable
add_executable(RobloxPlayer ${SOURCES} ${HEADERS} ${PLATFORM_SPECIFIC})

if(TARGET GenerateCoreScripts)
    add_dependencies(RobloxPlayer GenerateCoreScripts)
endif()

target_include_directories(RobloxPlayer PRIVATE ${FREETYPE_INCLUDE_DIRS})

target_link_libraries(RobloxPlayer PRIVATE
    $<TARGET_OBJECTS:Base>
    $<TARGET_OBJECTS:App_CLIENT>
    $<TARGET_OBJECTS:Network_CLIENT>
    $<TARGET_OBJECTS:BulletPhysics>
    $<TARGET_OBJECTS:G3D>
    $<TARGET_OBJECTS:AppDraw>
    $<TARGET_OBJECTS:GfxBase>
    $<TARGET_OBJECTS:GfxCore_CLIENT>
    $<TARGET_OBJECTS:GfxRender>

    Boost::system
    Boost::thread
    Boost::filesystem
    Boost::program_options
    Boost::iostreams
    OpenSSL::SSL
    OpenSSL::Crypto
    SDL3::SDL3
    ZLIB::ZLIB
    CURL::libcurl
    GLEW::GLEW
    Threads::Threads
    Freetype::Freetype
    lz4::lz4
    PNG::PNG
    JPEG::JPEG
    "$<$<CONFIG:Release>:FMOD::fmod>"
    "$<$<NOT:$<CONFIG:Release>>:FMOD::fmodL>"
    draco::draco
)

if(WIN32)
    target_link_libraries(RobloxPlayer PRIVATE
        wintrust
        crypt32
        ws2_32
        iphlpapi
        shlwapi
        wininet
        winmm
        pdh
        dbghelp
    )
    # disable console
    target_link_options(RobloxPlayer PRIVATE "-mwindows")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(RobloxPlayer PRIVATE
        m
        rt
        GL
        X11
        dl
     )
    set_target_properties(RobloxPlayer PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN:$ORIGIN/../lib:${CMAKE_SOURCE_DIR}/third-party/fmod/linux/lib/${FMOD_LINUX_ARCH_DIR}"
    )
endif()

# Installation
install(TARGETS RobloxPlayer RUNTIME DESTINATION .)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/AppSettings.xml" DESTINATION .)

# TODO: cleanup
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
