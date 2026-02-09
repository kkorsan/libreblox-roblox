# Libreblox

The free and open source roblox client, featuring latest libraries,
MacOS, Linux and Windows compatibility and a more organized codebase.

# Information & Requirements
## Features Checklist
- [x] Msvc and Gcc compatible C++17 standart code
- [ ] Windows compatible gameservers
- [ ] Properly functioning roblox studio
- [ ] Android and ios clients
- [ ] WebAssembly port

## Compilable Projects Checklist
- [x] SDL Client (`applications/sdl-client`)
- [ ] Roblox Studio (`applications/studio`) 
- [x] RCCService (`applications/rcc-service`) (Linux only for now)
- [x] CoreScriptConverter (`tools/core-script-converter`)
- [x] App
- [x] Base
- [x] Network
- [x] Rendering Stack
  - [x] ShaderCompiler (migrated to python script: `scripts/buildshaders.py`)
- [ ] Unit tests - will require major rework to function with modern Boost.Test

## Prerequisites
- CMake (>= 3.15)
- Vcpkg
- Gcc or Msvc (VS2019+)
- Ninja
- Git

# Building with Gnu Compiler Collection

First off, set up vcpkg via Terminal
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # on Linux/macOS
.\bootstrap-vcpkg.bat # on Windows
```

Qt needs to be installed manually, you can use the
online installer to do so (it isn't a necessity so you can skip here,
only if you don't wanna build Roblox Studio of course)
```bash
https://download.qt.io/official_releases/online_installers/
```

You're halfway there, now build with cd to
your root directory and type in
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DCMAKE_BUILD_TYPE=Release -G "Ninja"
# additional arguments you may need:
# \
#-DBUILD_SDLCLIENT=ON \ - explicitly build the Windows client
#-DBUILD_ROBLOXSTUDIO=ON \ - explicitly build Roblox Studio
#-DBUILD_RCCSERVICE=ON \ - explicitly build RCC service
#-DBUILD_CORESCRIPTCONVERTER=ON \ - explicitly build CoreScriptConverter
```
```
ninja -C build
```

# Building with Microsoft Visual C++

To-Do, Not tested.
