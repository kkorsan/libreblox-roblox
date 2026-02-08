#!/bin/bash

echo "Running CoreScriptConverter"
DIR="${1:-.}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

# Choose rsc.config: prefer local rsc.config in build dir, fallback to repo one
if [[ -f "rsc.config" ]]; then
    RSCL="./rsc.config"
elif [[ -f "../rsc.config" ]]; then
    RSCL="../rsc.config"
elif [[ -f "${SCRIPT_DIR}/../rsc.config" ]]; then
    RSCL="${SCRIPT_DIR}/../rsc.config"
else
    echo "ERROR: rsc.config not found in $DIR or in tool path (${SCRIPT_DIR}/../rsc.config)"
    exit 1
fi

# auto-detect executable (prefer CoreScriptConverter, then CoreScriptConverter2, then any matching file)
if [[ -x "./CoreScriptConverter" ]]; then
    EXE="./CoreScriptConverter"
elif [[ -x "./CoreScriptConverter2" ]]; then
    EXE="./CoreScriptConverter2"
else
    EXE=""
    for f in CoreScriptConverter*; do
        if [[ -f "$f" && -x "$f" ]]; then
            EXE="./$f"
            break
        fi
    done
fi

if [[ -z "$EXE" ]]; then
    echo "ERROR: CoreScriptConverter binary not found in $DIR"
    exit 1
fi

echo "Using binary: $EXE"
"$EXE" --rscloc "${RSCL}" --verbose

cd "$DIR/../../../engine/app/src/script/"

if [[ -e "LuaGenCS.inl" ]];
then
    file1=$(md5 -q LuaGenCSNew.inl)
    file2=$(md5 -q LuaGenCS.inl)

    echo $file1
    echo $file2

    if [ $file1 == $file2 ]
    then
        echo "No CoreScript changes detected"
        rm "LuaGenCSNew.inl"
    else
        echo "CoreScript changes detected, will require linking"
        rm "LuaGenCS.inl"
        mv "LuaGenCSNew.inl" "LuaGenCS.inl"
    fi
else
    echo "CoreScript changes detected, will require linking"
    mv "LuaGenCSNew.inl" "LuaGenCS.inl"
fi
