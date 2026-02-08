#!/usr/bin/env bash
#
# cross-build-sdlclient-windows.sh (Industry Standard Version with DLL Deployment)
set -euo pipefail

# --- Configuration & Paths ---
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Defaults
PREFIX="${PREFIX:-$REPO_ROOT/third_party/win64}"
BUILD_DIR="${BUILD_DIR:-$REPO_ROOT/build-windows-x64}"
CONFIG="Release"
DIST_DIR="$REPO_ROOT/dist/windows-x64"
JOBS="$(getconf _NPROCESSORS_ONLN || echo 4)"
CLEAN=0
EXTRA_CMAKE_ARGS=()

usage() {
    echo "Usage: $0 [-p prefix] [-b build_dir] [-c config] [--clean] [-- extra-cmake-args]"
    exit 1
}

# Simple arg parsing
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--prefix) PREFIX="$2"; shift 2 ;;
        -b|--build-dir) BUILD_DIR="$2"; shift 2 ;;
        -c|--config) CONFIG="$2"; shift 2 ;;
        --clean) CLEAN=1; shift ;;
        --) shift; EXTRA_CMAKE_ARGS=("$@"); break ;;
        -h|--help) usage ;;
        *) shift ;;
    esac
done

# Set the install prefix (where the final bundle will be gathered)
PKG_NAME="SDLClient-win64-$(date +%Y%m%d)"
INSTALL_TARGET_DIR="$DIST_DIR/$PKG_NAME"

# Clean/Prep
if [[ $CLEAN -eq 1 ]]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
mkdir -p "$DIST_DIR"

# 1. Configure
echo "Configuring with CMake..."
cmake -S "$REPO_ROOT" -B "$BUILD_DIR" \
    -DVCPKG_CHAINLOAD_TOOLCHAIN_FILE="$REPO_ROOT/cmake/Toolchains/mingw-w64-x86_64.cmake" \
    -DVCPKG_TARGET_TRIPLET=x64-mingw-static \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DCMAKE_BUILD_TYPE="$CONFIG" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_TARGET_DIR" \
    -DBUILD_ROBLOXSTUDIO=OFF \
    -DBUILD_RCCSERVICE=OFF \
    -DBUILD_SDLCLIENT=ON \
    -DBUILD_CORESCRIPTCONVERTER=OFF \
    -DBUILD_TESTING=OFF \
    -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
    "${EXTRA_CMAKE_ARGS[@]}"

# 2. Build
echo "Building RobloxPlayer..."
cmake --build "$BUILD_DIR" --target RobloxPlayer -j "$JOBS"

# 3. Install (The Industry-Standard Way)
echo "Installing and bundling DLLs to $INSTALL_TARGET_DIR..."
rm -rf "$INSTALL_TARGET_DIR" # Ensure a clean bundle
cmake --install "$BUILD_DIR"

# 4. Automatic DLL Deployment Fallback (Critical for MinGW runtime)
echo "Verifying and deploying required runtime DLLs..."
REQUIRED_DLLS=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
    "fmod.dll"
)

# Common MinGW binary paths (search in order)
MINGW_BIN_PATHS=(
    "$(dirname "$(which x86_64-w64-mingw32-gcc 2>/dev/null)")/../x86_64-w64-mingw32/bin"
    "$(dirname "$(which x86_64-w64-mingw32-gcc 2>/dev/null)")/../bin"
    "/usr/x86_64-w64-mingw32/bin"
    "/mingw64/bin"
    "$REPO_ROOT/third_party/fmod/windows/lib/x64"
)

# Verify and deploy missing DLLs
for dll in "${REQUIRED_DLLS[@]}"; do
    if [ -f "$INSTALL_TARGET_DIR/$dll" ]; then
        echo "✓ $dll already deployed"
        continue
    fi

    deployed=0
    for path in "${MINGW_BIN_PATHS[@]}"; do
        src_dll="$path/$dll"
        if [ -f "$src_dll" ]; then
            echo "→ Deploying $dll from $path"
            cp "$src_dll" "$INSTALL_TARGET_DIR/"
            deployed=1
            break
        fi
    done

    if [ $deployed -eq 0 ]; then
        echo "⚠️ WARNING: Could not find $dll in any search path!"
        echo "   Search paths checked:"
        for path in "${MINGW_BIN_PATHS[@]}"; do
            echo "   - $path"
        done
        echo "   Please ensure MinGW-W64 toolchain is properly installed"
        sleep 2  # Give user time to see warning
    else
        echo "✓ $dll successfully deployed"
    fi
done

# Final verification
echo "Verifying critical dependencies..."
missing_dlls=()
for dll in "${REQUIRED_DLLS[@]}"; do
    if [ ! -f "$INSTALL_TARGET_DIR/$dll" ]; then
        missing_dlls+=("$dll")
    fi
done

if [ ${#missing_dlls[@]} -gt 0 ]; then
    echo "❌ CRITICAL ERROR: Missing required DLLs:"
    for dll in "${missing_dlls[@]}"; do
        echo "   - $dll"
    done
    echo "Deployment failed. Cannot create package."
    exit 1
fi
echo "✓ All critical DLLs present"

# 5. Strip symbols to reduce size
if command -v x86_64-w64-mingw32-strip >/dev/null 2>&1; then
    echo "Stripping binaries in $INSTALL_TARGET_DIR..."
    find "$INSTALL_TARGET_DIR" -maxdepth 1 -type f \( -name "*.exe" -o -name "*.dll" \) -exec x86_64-w64-mingw32-strip --strip-all {} +
fi

# 6. Packaging
echo "Creating Zip..."
ZIP_NAME="${PKG_NAME}.zip"
(cd "$INSTALL_TARGET_DIR" && zip -r9 -q "../$ZIP_NAME" .)

echo "--------------------------------------------------"
echo "✅ SUCCESS! Portable package created at:"
echo "   $DIST_DIR/$ZIP_NAME"
echo ""
echo "📦 Package contents:"
du -h "$DIST_DIR/$ZIP_NAME"
echo ""
echo "🔍 Verify deployment integrity with:"
echo "   unzip -l $DIST_DIR/$ZIP_NAME"
