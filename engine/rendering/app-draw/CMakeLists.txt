include(App)

include_directories(include)
include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/g3d/include)
include_directories(${CMAKE_SOURCE_DIR}/engine/rendering/gfx/base/include)

list(APPEND HEADERS include/appdraw/Draw.h)
list(APPEND HEADERS include/appdraw/HitTest.h)
list(APPEND HEADERS include/appdraw/DrawAdorn.h)
list(APPEND HEADERS include/appdraw/HandleType.h)
list(APPEND HEADERS include/appdraw/DrawPrimitives.h)

list(APPEND SOURCES src/Draw.cpp)
list(APPEND SOURCES src/DrawAdorn.cpp)
list(APPEND SOURCES src/HitTest.cpp)

add_library(AppDraw OBJECT ${SOURCES} ${HEADERS})
