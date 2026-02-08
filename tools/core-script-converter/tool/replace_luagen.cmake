# replace_luagen.cmake
#
# Usage:
#   cmake -DGEN_FILE=/abs/path/LuaGenCSNew.inl -DOUT_FILE=/abs/path/LuaGenCS.inl -P replace_luagen.cmake
#
# This script compares the generated LuaGenCSNew.inl (GEN_FILE) with the existing
# LuaGenCS.inl (OUT_FILE) and:
#  - if OUT_FILE doesn't exist -> renames GEN_FILE -> OUT_FILE
#  - if both exist and are identical -> removes GEN_FILE
#  - if both exist and differ -> replaces OUT_FILE with GEN_FILE
#
# It's intentionally simple and cross-platform (uses CMake file operations).
#

if(NOT DEFINED GEN_FILE)
    message(FATAL_ERROR "GEN_FILE not defined. Example usage:\n  cmake -DGEN_FILE=/abs/path/LuaGenCSNew.inl -DOUT_FILE=/abs/path/LuaGenCS.inl -P replace_luagen.cmake")
endif()

if(NOT DEFINED OUT_FILE)
    message(FATAL_ERROR "OUT_FILE not defined. Example usage:\n  cmake -DGEN_FILE=/abs/path/LuaGenCSNew.inl -DOUT_FILE=/abs/path/LuaGenCS.inl -P replace_luagen.cmake")
endif()

message(STATUS "replace_luagen: checking generated core script: '${GEN_FILE}'")

if(NOT EXISTS "${GEN_FILE}")
    message(STATUS "replace_luagen: no generated file '${GEN_FILE}' found; nothing to do.")
else()
    if(NOT EXISTS "${OUT_FILE}")
        # No existing file, move generated -> output
        file(RENAME "${GEN_FILE}" "${OUT_FILE}")
        message(STATUS "replace_luagen: installed '${OUT_FILE}' (was not present).")
    else()
        # Both exist, compare contents
        file(READ "${GEN_FILE}" _gen_contents)
        file(READ "${OUT_FILE}" _out_contents)

        if("${_gen_contents}" STREQUAL "${_out_contents}")
            # Identical: remove the generated one, nothing changed
            file(REMOVE "${GEN_FILE}")
            message(STATUS "replace_luagen: no CoreScript changes detected; removed '${GEN_FILE}'.")
        else()
            # Different: replace the old file with the generated one
            file(REMOVE "${OUT_FILE}")
            file(RENAME "${GEN_FILE}" "${OUT_FILE}")
            message(STATUS "replace_luagen: CoreScript changes detected; updated '${OUT_FILE}'.")
        endif()
    endif()
endif()

message(STATUS "replace_luagen: done.")
