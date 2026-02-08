#!/bin/bash
set -e

# Configuration
BUILD_DIR=${1:-build}
DEPLOY_DIR=${2:-deploy}

# Ensure absolute paths
BUILD_DIR=$(realpath "$BUILD_DIR")
DEPLOY_DIR=$(realpath "$DEPLOY_DIR")

echo "Deploying from '$BUILD_DIR' to '$DEPLOY_DIR'..."

if [ ! -d "$BUILD_DIR" ]; then
    echo "Error: Build directory '$BUILD_DIR' does not exist."
    exit 1
fi

# Clean deploy directory
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR"

# Step 1: Run CMake Install
# This copies the executables and files explicitly marked for install in CMakeLists.txt
echo "Running CMake install..."
cmake --install "$BUILD_DIR" --prefix "$DEPLOY_DIR"

# Step 2: Resolve Shared Libraries
echo "Resolving shared library dependencies..."

# Helper function to copy dependencies
bundle_deps() {
    local binary_path="$1"

    if [ ! -f "$binary_path" ]; then
        echo "Warning: Binary '$binary_path' not found. Skipping dependency resolution."
        return
    fi

    echo "  Scanning $binary_path..."

    # Use ldd to find dependencies
    ldd "$binary_path" | grep "=>" | awk '{print $3}' | while read -r lib; do
        if [[ -z "$lib" || ! -f "$lib" ]]; then continue; fi

        # Filter out system libraries.
        # We assume libraries in /lib, /usr/lib, /lib64, /usr/lib64 are system libraries
        # and should be present on the target system.
        # We mainly want to bundle vcpkg libraries or other custom locations.
        if [[ "$lib" == /lib* || "$lib" == /usr/lib* ]]; then
             continue
        fi

        local lib_name=$(basename "$lib")
        local dest_path="$DEPLOY_DIR/$lib_name"

        if [ ! -f "$dest_path" ]; then
            echo "    Bundling $lib_name"
            cp -L "$lib" "$dest_path"

            # Recursively check dependencies of the copied lib
            bundle_deps "$dest_path"
        fi
    done
}

# Bundle dependencies for the main executables
bundle_deps "$DEPLOY_DIR/RobloxPlayer"
bundle_deps "$DEPLOY_DIR/RCCService"

echo "Deployment complete in '$DEPLOY_DIR'."
echo "You can now package this directory (e.g., tar -czvf roblox-linux.tar.gz -C $(dirname "$DEPLOY_DIR") $(basename "$DEPLOY_DIR"))"
