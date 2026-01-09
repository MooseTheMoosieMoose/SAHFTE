#!/bin/bash
set -e
cmake -S . -B build -G Ninja
echo "Building project..."
cmake --build build
echo "Build complete."