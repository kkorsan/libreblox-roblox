set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# --- SMART SYSROOT DETECTION ---
# 1. Check for Ubuntu/Debian Multiarch path
if(EXISTS "/usr/lib/aarch64-linux-gnu")
    set(ARM_SYSROOT "/")
    set(ARM_LIB_DIR "/usr/lib/aarch64-linux-gnu")
# 2. Fallback to your Arch Sysroot
elseif(EXISTS "$ENV{HOME}/vcpkg_sysroot")
    set(ARM_SYSROOT "$ENV{HOME}/vcpkg_sysroot")
    set(ARM_LIB_DIR "${ARM_SYSROOT}/usr/lib")
else()
    message(FATAL_ERROR "Could not find aarch64 libraries in /usr/lib/aarch64-linux-gnu or $HOME/vcpkg_sysroot")
endif()

message(STATUS "Using aarch64 Sysroot: ${ARM_SYSROOT}")

# Toolchain settings
set(CMAKE_SYSROOT "${ARM_SYSROOT}")
set(CMAKE_FIND_ROOT_PATH "${ARM_SYSROOT}" "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

# Ensure we only look in the sysroot, not the host x64 paths
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Pkg-config setup
set(ENV{PKG_CONFIG_SYSROOT_DIR} "${ARM_SYSROOT}")
if("${ARM_SYSROOT}" STREQUAL "/")
    set(ENV{PKG_CONFIG_LIBDIR} "/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
else()
    set(ENV{PKG_CONFIG_LIBDIR} "${ARM_SYSROOT}/usr/lib/pkgconfig:${ARM_SYSROOT}/usr/share/pkgconfig")
endif()

set(CMAKE_EXE_LINKER_FLAGS "-static-libstdc++ -static-libgcc")
