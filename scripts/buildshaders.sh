#!/bin/bash
(set -o igncr) 2>/dev/null && set -o igncr; # comment is needed on Windows to ignore this lines trailing \r

FOLDER=$(dirname $0)
python3 "$FOLDER/buildshaders.py" "$FOLDER/../engine/rendering/shaders"