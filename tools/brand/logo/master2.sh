#!/bin/bash
cd "$(dirname "$0")"; mkdir -p master2
"${BLENDER_PYTHON:-python3}" mk.py icon-amp master2/icon-1024.png 128 93.1 2>&1 | grep -E "Error|Traceback|wrote"
"${BLENDER_PYTHON:-python3}" mk.py word-ivory master2/wordmark-clear.png 96 100 2>&1 | grep -E "Error|Traceback|wrote"
"${BLENDER_PYTHON:-python3}" mk.py word-tolex master2/wordmark-tolex.png 96 100 2>&1 | grep -E "Error|Traceback|wrote"
echo ALLDONE
