#!/bin/bash
# 
# Exit if any command fails
set -e

# Go through the preset so the generator (Ninja) stays in one place — a bare
# "cmake -S . -B build" silently falls back to Makefiles on a fresh build dir.
cmake --preset linux-debug
cmake --build --preset linux-debug

./bin/faye_app