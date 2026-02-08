project(sgCore)

# Collect all source files recursively
file(GLOB_RECURSE SGCORE_SOURCES *.cpp)
file(GLOB_RECURSE SGCORE_HEADERS *.h)

# Define the library
add_library(sgCore STATIC ${SGCORE_SOURCES} ${SGCORE_HEADERS})

# Set platform-specific definitions
if(WIN32)
    target_compile_definitions(sgCore PRIVATE SG_CURRENT_PLATFORM=0)  # SG_PLATFORM_WINDOWS
elseif(APPLE)
    target_compile_definitions(sgCore PRIVATE SG_CURRENT_PLATFORM=1)  # SG_PLATFORM_MAC
else()
    target_compile_definitions(sgCore PRIVATE SG_CURRENT_PLATFORM=3)  # SG_PLATFORM_LINUX for Linux/Unix
endif()

# Include directories
target_include_directories(sgCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
