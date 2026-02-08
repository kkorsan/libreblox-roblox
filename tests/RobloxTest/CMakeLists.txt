include(App)
include(Base)
include(Network)
include(Rendering)
include(ClientShared)
include(Tests)

add_executable(RobloxTest
    main.cpp
    Tests.cpp

    ${CMAKE_SOURCE_DIR}/engine/app/src/script/LuaVMDummy.cpp
)


find_package(X11 REQUIRED)

target_link_libraries(RobloxTest PRIVATE
    AppTestLib
    BaseTestLib

    ${TEST_COMMON_OBJECTS}

    # Boost::unit_test_framework
    Boost::filesystem
    Boost::system
    Boost::thread
    Boost::iostreams
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
    X11::X11
    EGL
    "$<$<CONFIG:Release>:FMOD::fmod>"
    "$<$<NOT:$<CONFIG:Release>>:FMOD::fmodL>"
)
