include(App)
include(Network)
include(Rendering)
include(ClientShared)

include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/gfx/core)

add_executable(CoreScriptConverter2
    cscmain.cpp

    ${CLIENT_SHARED_DIR}/DumpErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/ErrorUploader.cpp
    ${CLIENT_SHARED_DIR}/LogManager.cpp
    ${CLIENT_SHARED_DIR}/VersionInfo.cpp
    ${CLIENT_SHARED_DIR}/CountersClient.cpp

    ${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaVMServer.cpp
)

target_link_libraries(CoreScriptConverter2 PRIVATE
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
    "$<$<CONFIG:Release>:FMOD::fmod>"
    "$<$<NOT:$<CONFIG:Release>>:FMOD::fmodL>"
    draco::draco
)

# Generate core scripts when requested by other targets (e.g., RobloxPlayer)
# This defines a custom command that runs the built CoreScriptConverter2 and
# then compares/moves the generated LuaGenCSNew.inl into engine/app/src/script/LuaGenCS.inl

# Path to the final generated file and the config used by the converter
set(CORE_SCRIPT_OUTPUT ${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaGenCS.inl)
set(CORESCRIPTS_RSC_CONFIG ${CMAKE_SOURCE_DIR}/tools/core-script-converter/tool/rsc.config)

# Collect script source files so changes trigger regeneration
file(GLOB_RECURSE CORESCRIPT_FILES
    "${CMAKE_SOURCE_DIR}/assets/content/scripts/*.lua"
)

add_custom_command(
    OUTPUT ${CORE_SCRIPT_OUTPUT}
    COMMAND ${CMAKE_COMMAND} -E echo "Running CoreScriptConverter"
    COMMAND $<TARGET_FILE:CoreScriptConverter2> --rscloc ${CORESCRIPTS_RSC_CONFIG} --verbose
    COMMAND ${CMAKE_COMMAND} -DGEN_FILE=${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaGenCSNew.inl -DOUT_FILE=${CORE_SCRIPT_OUTPUT} -P ${CMAKE_SOURCE_DIR}/tools/core-script-converter/tool/replace_luagen.cmake
    DEPENDS CoreScriptConverter2 ${CORESCRIPT_FILES} ${CORESCRIPTS_RSC_CONFIG}
    COMMENT "Generating core scripts (LuaGenCS.inl)"
    VERBATIM
)

add_custom_target(GenerateCoreScripts DEPENDS ${CORE_SCRIPT_OUTPUT})
