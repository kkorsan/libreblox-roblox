
enable_testing()

# Common test definitions
add_compile_definitions(RBX_TEST_BUILD)

# Common test dependencies and includes
set(TEST_COMMON_INCLUDES
    ${CMAKE_SOURCE_DIR}/engine/base/include
    ${CMAKE_SOURCE_DIR}/engine/app/include
    ${CMAKE_SOURCE_DIR}/engine/network/include
    ${CMAKE_SOURCE_DIR}/engine/network/include/raknet
    ${CMAKE_SOURCE_DIR}/third-party/bullet-physics
    ${CMAKE_SOURCE_DIR}/engine/client/shared
    ${CMAKE_SOURCE_DIR}/engine/rendering/g3d/include
    ${CMAKE_SOURCE_DIR}/engine/rendering/gfx/base/include
    ${CMAKE_SOURCE_DIR}/engine/rendering/gfx/render/include
    ${CMAKE_SOURCE_DIR}/engine/rendering/gfx/core/include
    ${CMAKE_SOURCE_DIR}/engine/rendering/app-draw/include
)

set(TEST_COMMON_OBJECTS
    $<TARGET_OBJECTS:Base_TEST>
    $<TARGET_OBJECTS:App_TEST>
    $<TARGET_OBJECTS:Network_TEST>
    $<TARGET_OBJECTS:G3D>
    $<TARGET_OBJECTS:GfxBase>
    $<TARGET_OBJECTS:GfxRender>
    $<TARGET_OBJECTS:AppDraw>
    $<TARGET_OBJECTS:GfxCore_TEST>
    $<TARGET_OBJECTS:BulletPhysics>
)

add_subdirectory(Base.UnitTest.Lib)
add_subdirectory(App.UnitTest.Lib)
add_subdirectory(Base.UnitTest)
add_subdirectory(App.UnitTest)
add_subdirectory(RobloxTest)
