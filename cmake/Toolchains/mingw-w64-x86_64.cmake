# MinGW-w64 toolchain file for cross-compiling to Windows x86_64
# Usage:
#   mkdir build-windows && cd build-windows
#   cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/mingw-w64-x86_64.cmake -DCMAKE_PREFIX_PATH=/path/to/winlibs -DCMAKE_BUILD_TYPE=Release ..
#   cmake --build . --config Release -- -j$(nproc)

# Target system
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10)

# Compilers
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)

# Default location(s) where Windows deps built for mingw should be installed (user override via -DCMAKE_PREFIX_PATH)
# Example: -DCMAKE_PREFIX_PATH=/opt/winlibs;/usr/local/mingw
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/third_party/win64" CACHE STRING "Paths to find Windows dependencies (SDL3, OpenSSL, curl, etc.)")
endif()

# Tell CMake to search for libs/includes in the cross sysroot
# The user-provided CMAKE_PREFIX_PATH should contain the windows builds of required libs.
set(CMAKE_FIND_ROOT_PATH ${CMAKE_PREFIX_PATH} ${CMAKE_FIND_ROOT_PATH})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Optional: make executables use the static libstdc++/libgcc to reduce DLL dependency noise
set(CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")

# Optional: minimize RPATH usage
set(CMAKE_SKIP_RPATH TRUE)

# Some projects use pkg-config; try to prefer the cross-pkg-config variant if present
# You can set PKG_CONFIG_PATH to a directory with .pc files for your Windows libraries.
# E.g. export PKG_CONFIG_PATH=/opt/winlibs/lib/pkgconfig
