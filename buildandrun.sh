#!/bin/bash
#
# Exit if any command fails
set -e

# The configured compiler is baked into CMakeCache.txt and cannot be changed in
# place. build/ directories created before the C++26 migration hold the system
# default (GCC 13 on Ubuntu 24.04), so reconfiguring against the preset's
# g++-16 fails with a wall of "cxx_std_26 is not known to CXX compiler".
# Detect the mismatch and wipe rather than making the user decode that.
_cache="build/CMakeCache.txt"
_want_raw="$(command -v g++-16 2>/dev/null || true)"
if [ -f "$_cache" ] && [ -n "$_want_raw" ]; then
    _have_raw="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$_cache" | head -1)"

    # Resolve BOTH sides before comparing. The cache holds whatever the preset
    # wrote -- a bare "g++-16" -- while `command -v` yields "/usr/bin/g++-16".
    # Comparing those as strings never matches, so the guard fires on every
    # run and deletes a perfectly good build directory (full rebuild each time).
    _resolve() {
        local p="$1"
        [ -n "$p" ] || return 1
        command -v "$p" >/dev/null 2>&1 && p="$(command -v "$p")"
        readlink -f "$p" 2>/dev/null || printf '%s' "$p"
    }
    _want="$(_resolve "$_want_raw")"
    _have="$(_resolve "$_have_raw")"

    # Only act when both resolved and genuinely differ — never wipe on ambiguity.
    if [ -n "$_have" ] && [ -n "$_want" ] && [ "$_have" != "$_want" ]; then
        echo "build/ was configured with ${_have_raw} (${_have}),"
        echo "but this tree needs ${_want_raw} (${_want}) for C++26."
        echo "Removing build/ so it can be reconfigured..."
        rm -rf build
    fi
fi

# Go through the preset so the generator (Ninja) and the GCC 16 toolchain stay
# in one place — a bare "cmake -S . -B build" silently falls back to Makefiles
# on a fresh build dir AND picks up the too-old default compiler.
cmake --preset linux-debug
cmake --build --preset linux-debug

./bin/faye_app
