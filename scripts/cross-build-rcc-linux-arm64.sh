#!/bin/bash
set -e

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-linux-arm64}"
VCPKG_ROOT="${VCPKG_ROOT:-$HOME/vcpkg}"

# 1. Environment Validation
if ! command -v ninja &> /dev/null; then
    echo "Error: ninja not found. Install it with 'sudo apt install ninja-build' or 'sudo pacman -S ninja'"
    exit 1
fi

if ! command -v aarch64-linux-gnu-g++ &> /dev/null; then
    echo "Error: aarch64-linux-gnu-g++ not found."
    echo "If using Distrobox: sudo apt install g++-aarch64-linux-gnu libxmu-dev:arm64 libxi-dev:arm64 libgl-dev:arm64"
    exit 1
fi

# 2. Path Detection (Multiarch vs Sysroot)
if [ -d "/usr/lib/aarch64-linux-gnu" ]; then
    SYSROOT="/"
    LIBDIR="/usr/lib/aarch64-linux-gnu"
    echo "--- Environment: Debian/Ubuntu Multiarch Detected ---"
else
    SYSROOT="$HOME/vcpkg_sysroot"
    LIBDIR="$SYSROOT/usr/lib"
    echo "--- Environment: Arch Linux Sysroot Detected ---"
fi

# 3. Configure with Ninja
# Note: We pass the -D hints to prevent GLEW from leaking host /usr/lib paths
cmake -G Ninja -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$REPO_ROOT/cmake/Toolchains/linux-aarch64.cmake" \
    -DVCPKG_TARGET_TRIPLET=arm64-linux-cross \
    -DVCPKG_OVERLAY_TRIPLETS="$REPO_ROOT/cmake/Triplets" \
    -DCMAKE_BUILD_TYPE=Release \
    -DOPENGL_opengl_LIBRARY="$LIBDIR/libGL.so" \
    -DOPENGL_glx_LIBRARY="$LIBDIR/libGL.so" \
    -DOPENGL_INCLUDE_DIR="$SYSROOT/usr/include" \
    -DX11_X11_LIB="$LIBDIR/libX11.so" \
    -DX11_Xext_LIB="$LIBDIR/libXext.so" \
    -DX11_X11_INCLUDE_PATH="$SYSROOT/usr/include" \
    -DVCPKG_COMMON_DEFINITIONS="-DDBUS_SESSION_SOCKET_DIR=/tmp" \
    -DBUILD_ROBLOXSTUDIO=OFF \
    -DBUILD_RCCSERVICE=ON \
    -DBUILD_SDLCLIENT=OFF \
    -DBUILD_CORESCRIPTCONVERTER=OFF \
    -DBUILD_TESTING=OFF

# 4. Execute Build
echo "--- Starting Ninja Build ---"
cmake --build "$BUILD_DIR" -j $(nproc)

echo "Success! Output is in $BUILD_DIR"
