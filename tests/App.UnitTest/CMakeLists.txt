include(App)
include(Base)
include(Network)
include(Rendering)
include(ClientShared)
include(Tests)

file(GLOB APP_UNITTEST_SOURCES *.cpp)

add_executable(App.UnitTest
    ${APP_UNITTEST_SOURCES}

    ${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaVMDummy.cpp
)

target_include_directories(App.UnitTest PRIVATE
    ${TEST_COMMON_INCLUDES}
)


find_package(X11 REQUIRED)

target_link_libraries(App.UnitTest PRIVATE
    AppTestLib
    BaseTestLib
    # Boost::unit_test_framework
    Boost::system
    Boost::filesystem
    Boost::iostreams
    Boost::thread
    Boost::chrono
    Boost::date_time
    Boost::atomic
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    GLEW::GLEW
    OpenGL::GL
    PNG::PNG
    JPEG::JPEG
    lz4::lz4
    Freetype::Freetype
    ZLIB::ZLIB
    Threads::Threads
    SDL3::SDL3
    X11::X11
    EGL
    "$<$<CONFIG:Release>:FMOD::fmod>"
    "$<$<NOT:$<CONFIG:Release>>:FMOD::fmodL>"
)

target_sources(App.UnitTest PRIVATE ${TEST_COMMON_OBJECTS})

# Rendering might be needed if App uses it. Root CMake: App_RCC includes engine/rendering/gfx/core (GfxCore_RCC).
# And App likely depends on Rendering (engine/rendering).
# I'll add Rendering objects just in case.
# Note: Root CMake: add_subdirectory(engine/rendering) -> Rendering target.
