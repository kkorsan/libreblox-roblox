#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys

# Configuration
SHADER_DIR = os.path.join(os.path.dirname(__file__), "../assets/shaders")
SOURCE_DIR = os.path.join(SHADER_DIR, "source")
SHADERS_JSON = os.path.join(SHADER_DIR, "shaders.json")

# Tools (assumed to be in PATH)
# on arch linux: sudo pacman -S glslang spirv-cross
# on ubuntu/debian: sudo apt install glslang-tools spirv-cross
# on fedora: sudo dnf install glslang spirv-cross
# on macOS: brew install glslang spirv-cross
# on windows: download from respective github releases and add to PATH OR try with winget
GLSLANG = "glslangValidator"
SPIRV_CROSS = "spirv-cross"


def get_md5(data):
    return hashlib.md5(data).digest()


def preprocess_source(source_path, include_dirs, processed_files=None):
    if processed_files is None:
        processed_files = set()

    # Avoid cycles (simple check)
    abs_path = os.path.abspath(source_path)
    if abs_path in processed_files:
        return ""
    processed_files.add(abs_path)

    try:
        with open(source_path, "r") as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: Include file not found: {source_path}")
        return ""

    output = []
    # Add #line directive for better error reporting (optional, maybe confuses glslang if in HLSL mode?)
    # output.append(f'#line 1 "{os.path.basename(source_path)}"\n')

    for line in lines:
        if line.strip().startswith("#include"):
            # Parse include
            # Formats: #include "file" or #include <file>
            import re

            match = re.search(r'#include\s+["<](.+)[">]', line)
            if match:
                inc_file = match.group(1)
                # Find file in include dirs
                found = False
                for d in include_dirs:
                    p = os.path.join(d, inc_file)
                    if os.path.exists(p):
                        output.append(
                            preprocess_source(p, include_dirs, processed_files)
                        )
                        # Restore line number for parent file?
                        # output.append(f'#line {i+2} "{os.path.basename(source_path)}"\n')
                        found = True
                        break
                if not found:
                    print(f"Warning: Include not found: {inc_file}")
                    output.append(line)  # Leave it for compiler to fail
            else:
                output.append(line)
        else:
            output.append(line)

    return "".join(output)


def compile_shader(name, source_file, entrypoint, target, defines):
    # Determine stage
    if target.startswith("vs"):
        stage = "vert"
    elif target.startswith("ps"):
        stage = "frag"
    else:
        return None

    # Determine platform target
    # target string example: "vs_3_0"

    # Use .vert.hlsl or .frag.hlsl to tell glslang both the stage and the language
    ext = ".vert.hlsl" if stage == "vert" else ".frag.hlsl"
    temp_source = f"temp_shader{ext}"

    # Preprocess source manually to handle includes
    full_source = preprocess_source(
        source_file, [os.path.dirname(source_file), SOURCE_DIR]
    )

    # replace tex2Dbias, texCUBEbias, tex3Dbias with tex2D, texCUBE, tex3D
    # since glslangValidator does not support bias variants in HLSL mode
    if "bias" in full_source:
        import re

        full_source = re.sub(
            r"tex2Dbias\s*\(([^,]+),([^)]+)\)", r"tex2D(\1, (\2).xy)", full_source
        )
        full_source = re.sub(
            r"texCUBEbias\s*\(([^,]+),([^)]+)\)", r"texCUBE(\1, (\2).xyz)", full_source
        )
        full_source = re.sub(
            r"tex3Dbias\s*\(([^,]+),([^)]+)\)", r"tex3D(\1, (\2).xyz)", full_source
        )

    with open(temp_source, "w") as f:
        f.write(full_source)

    # Preprocess and compile to SPIR-V
    # We use -V for Vulkan (SPIR-V) semantics
    # Removed -S as the extension should handle it
    cmd = [GLSLANG, "-V", "-e", entrypoint, "-o", "temp.spv", temp_source]

    # Add defines
    if target.startswith("vs"):
        cmd.append("-DSHADER_STAGE_VS")
    elif target.startswith("ps"):
        cmd.append("-DSHADER_STAGE_PS")

    if defines:
        for d in defines.split():
            cmd.append("-D" + d)

    try:
        subprocess.check_output(cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print(f"Error compiling {name}: {e.output.decode('utf-8')}")
        if os.path.exists(temp_source):
            os.remove(temp_source)
        return None
    finally:
        if os.path.exists(temp_source):
            os.remove(temp_source)

    # Cross-compile to GLSL

    # Default to desktop GLSL 1.20 for "glsl" (OpenGL 2.1)
    cross_cmd = [SPIRV_CROSS, "temp.spv", "--no-es"]
    version = "120"

    if "GL3" in defines:
        version = "140"  # or 330
    elif "GLSLES" in defines:
        cross_cmd = [SPIRV_CROSS, "temp.spv", "--es"]
        version = "100"
        if "GL3" in defines:
            version = "300"

    cross_cmd.append("--version")
    cross_cmd.append(version)

    try:
        glsl_source = subprocess.check_output(cross_cmd, stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print(f"Error converting {name}: {e.output.decode('utf-8')}")
        print(f"Command: {' '.join(cross_cmd)}")
        if os.path.exists("temp.spv"):
            os.remove("temp.spv")
        return None

    if os.path.exists("temp.spv"):
        os.remove("temp.spv")

    return glsl_source


def pack_shaders(target_name, glsl_version_defines):
    print(f"Packing shaders for target: {target_name}")

    with open(SHADERS_JSON, "r") as f:
        manifest = json.load(f)

    entries = []
    blob = b""

    succeeded = 0
    failed = 0

    for item in manifest:
        name = item["name"]
        source = item["source"]
        target_profile = item["target"]  # e.g. vs_3_0
        entrypoint = item["entrypoint"]
        item_defines = item.get("defines", "")
        exclude = item.get("exclude", "")

        if target_name in exclude.split():
            continue

        full_defines = f"{item_defines} {glsl_version_defines}".strip()
        source_path = os.path.join(SOURCE_DIR, source)

        # maybe precompute a hash of source + defines to skip recompilation if unchanged?
        # for now, lets always compile

        compiled_data = compile_shader(
            name, source_path, entrypoint, target_profile, full_defines
        )

        if not compiled_data:
            failed += 1
            continue

        succeeded += 1

        # Calculate MD5 (just use content hash)
        m = hashlib.md5()
        m.update(compiled_data)
        data_md5 = m.digest()

        entries.append({"name": name, "md5": data_md5, "data": compiled_data})

    # Write pack
    out_path = os.path.join(SHADER_DIR, f"shaders_{target_name}.pack")

    with open(out_path, "wb") as f:
        # Header: RBXS (4) + Count (4)
        f.write(b"RBXS")
        f.write(struct.pack("I", len(entries)))

        current_offset = 8 + len(entries) * 96  # 96 bytes per entry struct

        # Write Entries
        for e in entries:
            # Name (64 bytes)
            name_bytes = e["name"].encode("utf-8")[:63]
            f.write(name_bytes.ljust(64, b"\0"))

            # MD5 (16 bytes)
            f.write(e["md5"])

            # Offset (4 bytes)
            f.write(struct.pack("I", current_offset))

            # Size (4 bytes)
            size = len(e["data"])
            f.write(struct.pack("I", size))

            # Reserved (8 bytes)
            f.write(b"\0" * 8)

            current_offset += size

        # Write Data
        for e in entries:
            f.write(e["data"])

    print(f"Packed {succeeded} shaders. {failed} failed.")


def main():
    global SHADER_DIR
    global SOURCE_DIR
    global SHADERS_JSON

    parser = argparse.ArgumentParser(description="Compile and pack shaders.")
    parser.add_argument("folder", nargs="?", help="Path to the shaders directory")
    args = parser.parse_args()

    if args.folder:
        SHADER_DIR = args.folder

    SOURCE_DIR = os.path.join(SHADER_DIR, "source")
    SHADERS_JSON = os.path.join(SHADER_DIR, "shaders.json")

    # Check tools
    if not shutil.which(GLSLANG):
        print("Error: glslangValidator not found in PATH.")
        sys.exit(1)
    if not shutil.which(SPIRV_CROSS):
        print("Error: spirv-cross not found in PATH.")
        sys.exit(1)

    # Compile for supported targets
    pack_shaders("glsl", "GLSL")  # OpenGL 2.1
    pack_shaders("glsl3", "GLSL GL3")  # OpenGL 3.3+
    pack_shaders("glsles", "GLSL GLSLES")  # GLES 2.0
    pack_shaders("glsles3", "GLSL GLSLES GL3")  # GLES 3.0


if __name__ == "__main__":
    main()
