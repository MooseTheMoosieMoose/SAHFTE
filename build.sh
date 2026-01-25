#!/bin/bash
set -e

if [ "$#" -gt 0 ]; then
    #Handle No args
    echo "No build target specified, building CPP test bed only in debug mode"
    export CPP_FE="TRUE"
    export PY_BINDS=""
    export DEBUG_MODE="TRUE"
else
    #Start everything default
    export CPP_FE=""
    export PY_BINDS=""
    export DEBUG_MODE="TRUE"
    #Loop over given args to determine the build mode requested
    for arg in "$@"; do
        if [ arg == "cpp_fe" ]; then
            export CPP_FE="TRUE"
        elif [ arg == "py_binds" ]; then
            export PY_BINDS="TRUE"
        elif [ arg == "release" ]; then
            export DEBUG_MODE="FALSE"
    done
fi


cmake -S . -B build -G Ninja
echo "Building project..."
cmake --build build
echo "Build complete."