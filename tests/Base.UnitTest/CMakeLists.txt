include(App)
include(Base)
include(Network)
include(Rendering)
include(ClientShared)
include(Tests)

file(GLOB BASE_UNITTEST_SOURCES *.cpp)

add_executable(Base.UnitTest
    ${BASE_UNITTEST_SOURCES}
)

target_link_libraries(Base.UnitTest PRIVATE
    BaseTestLib
    Boost::unit_test_framework
    OpenSSL::SSL
    OpenSSL::Crypto
    OpenGL::GL
)

target_sources(Base.UnitTest PRIVATE $<TARGET_OBJECTS:Base>)
