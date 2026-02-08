#!/bin/bash

echo "Running CoreScriptConverter"
DIR="${1:-.}"
cd "$DIR"

# Auto-detect CoreScriptConverter executable. Prefer exact names, fallback to any
# executable matching CoreScriptConverter* in the directory.
EXEC=""
if [[ -x "./CoreScriptConverter" ]]; then
    EXEC="./CoreScriptConverter"
elif [[ -x "./CoreScriptConverter2" ]]; then
    EXEC="./CoreScriptConverter2"
else
    for f in CoreScriptConverter*; do
        if [[ -f "$f" && -x "$f" ]]; then
            EXEC="./$f"
            break
        fi
    done
fi

if [[ -z "$EXEC" ]]; then
    echo "ERROR: CoreScriptConverter binary not found in $DIR"
    exit 1
fi

"$EXEC" --rscloc "./rsc.config" --verbose

cd "$DIR/../../../engine/app/src/script/"

if [[ -e "LuaGenCS.inl" ]];
then
    file1=$(md5sum LuaGenCSNew.inl | cut -d' ' -f1)
    file2=$(md5sum LuaGenCS.inl | cut -d' ' -f1)

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
