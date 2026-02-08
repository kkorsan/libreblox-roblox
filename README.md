# Libreblox

The free and open source roblox client, featuring latest libraries,
MacOS, Linux and Windows compatibility and a more organized codebase.

# Information & Requirements
## Features Checklist
- [x] Msvc and Clang compatible C++17 standart code
- [ ] Windows compatible gameservers
- [ ] Properly functioning roblox studio
- [ ] Android and ios clients

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
- Clang
- Ninja
- Git

# Building

TO-DO
