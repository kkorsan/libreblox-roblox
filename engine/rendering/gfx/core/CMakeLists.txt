include(Rendering)

find_package(GLEW REQUIRED)

include_directories(include)
include_directories(GL)

if(DEFINED RBX_CONFIG)
    set(TARGET_NAME GfxCore_${RBX_CONFIG})
else()
    set(TARGET_NAME GfxCore)
endif()

if(ANDROID)
    list(APPEND SOURCES GL/ContextGLAndroid.cpp)
elseif(WIN32)
    # list(APPEND SOURCES GL/ContextGLWin32.cpp)
    # # D3D9
    # list(APPEND SOURCES
    #     D3D9/DeviceD3D9.cpp
    #     D3D9/DeviceContextD3D9.cpp
    #     D3D9/FramebufferD3D9.cpp
    #     D3D9/GeometryD3D9.cpp
    #     D3D9/ShaderD3D9.cpp
    #     D3D9/TextureD3D9.cpp
    # )
    # # D3D11
    # list(APPEND SOURCES
    #     D3D11/DeviceD3D11.cpp
    #     D3D11/DeviceContextD3D11.cpp
    #     D3D11/FramebufferD3D11.cpp
    #     D3D11/GeometryD3D11.cpp
    #     D3D11/ShaderD3D11.cpp
    #     D3D11/TextureD3D11.cpp
    #     #Win32
    #     D3D11/DeviceD3D11Win32.cpp
    # )
    #
    list(APPEND SOURCES GL/ContextGLSDL.cpp)
elseif(APPLE)
    list(APPEND SOURCES GL/ContextGLEAGL.mm)
    list(APPEND SOURCES GL/ContextGLNSGL.mm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    list(APPEND SOURCES GL/ContextGLX11.cpp)
    list(APPEND SOURCES GL/ContextGLSDL.cpp)
endif()

list(APPEND HEADERS GL/ContextGL.h)
list(APPEND HEADERS GL/DeviceGL.h)
list(APPEND HEADERS GL/FramebufferGL.h)
list(APPEND HEADERS GL/GeometryGL.h)
list(APPEND HEADERS GL/ShaderGL.h)
list(APPEND HEADERS GL/TextureGL.h)
list(APPEND HEADERS include/GfxCore/Device.h)
list(APPEND HEADERS include/GfxCore/Framebuffer.h)
list(APPEND HEADERS include/GfxCore/Geometry.h)
list(APPEND HEADERS include/GfxCore/Resource.h)
list(APPEND HEADERS include/GfxCore/Shader.h)
list(APPEND HEADERS include/GfxCore/States.h)
list(APPEND HEADERS include/GfxCore/Texture.h)
list(APPEND HEADERS include/GfxCore/pix.h)

list(APPEND SOURCES Device.cpp)
list(APPEND SOURCES DeviceCreate.cpp)
list(APPEND SOURCES Framebuffer.cpp)
list(APPEND SOURCES GL/CardboardVRGL.cpp)
list(APPEND SOURCES GL/DeviceContextGL.cpp)
list(APPEND SOURCES GL/DeviceGL.cpp)
list(APPEND SOURCES GL/FramebufferGL.cpp)
list(APPEND SOURCES GL/GeometryGL.cpp)
list(APPEND SOURCES GL/ShaderGL.cpp)
list(APPEND SOURCES GL/TextureGL.cpp)
list(APPEND SOURCES Geometry.cpp)
list(APPEND SOURCES Resource.cpp)
list(APPEND SOURCES Shader.cpp)
list(APPEND SOURCES States.cpp)
list(APPEND SOURCES Texture.cpp)
list(APPEND SOURCES pix.cpp)

add_library(${TARGET_NAME} OBJECT ${SOURCES} ${HEADERS})

target_link_libraries(${TARGET_NAME} PRIVATE
    GLEW::GLEW
)
