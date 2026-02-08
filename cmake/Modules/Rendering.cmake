include(App)

if(ANDROID)
    add_definitions(-DGLEW_NO_GLU)
endif(ANDROID)

include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/gfx/render/include)
include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/g3d/include)
include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/gfx/base/include)
include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/app-draw/include)
include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/gfx/core/GL)
include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/gfx/core/include)