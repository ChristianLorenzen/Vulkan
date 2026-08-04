# 3.30 minimum, NOT 3.28: the cxx_std_26 compile feature was documented in
# CMake 3.25 but only actually IMPLEMENTED in 3.30. On 3.28 every
# target_compile_features(... cxx_std_26) fails with "not known to CXX
# compiler" no matter how new GCC is. 3.30 also makes CMP0169 (the deprecated
# single-argument FetchContent_Populate) an error — already handled for Boost
# in cmake/FayeDependencies.cmake.
#
# Ubuntu 24.04 ships CMake 3.28. To upgrade, use Kitware's APT repo:
#   https://apt.kitware.com   (or: pip install --upgrade cmake)
cmake_minimum_required(VERSION 3.30)

project(FayeVulkan VERSION 0.1.1 LANGUAGES CXX)

# C++26 for P2996 static reflection. Requires GCC 16+ (16.1 released, or a
# trunk build); upstream Clang has not merged P2996 as of 2026-08.
# NOTE: this is a DEFAULT, not a mandate — FetchContent subprojects that set
# their own CXX_STANDARD (glm, assimp: 17) keep theirs. That is precisely why
# -freflection must be per-target; see faye_enable_reflection() below.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Faye uses no C++20 modules — every TU is headers + .cpp. Leave this ON and
# CMP0155 (NEW since the cmake_minimum_required 3.21 -> 3.30 bump above) makes
# CMake scan every C++20-or-later source for module dependencies, which appends
# -fmodules-ts -fmodule-mapper=... -fdeps-format=p1689r5 to every compile
# command. That costs a scanning pass per TU for nothing, and the flags are
# GCC-only, so they land in compile_commands.json and make clangd report
# "Unknown argument" on every file. Turn it back ON if real modules ever land.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Fail once, clearly, instead of four times per target with
# "the compiler feature cxx_std_26 is not known to CXX compiler".
if(NOT "cxx_std_26" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
    message(FATAL_ERROR
        "No C++26 support available. Faye needs it for P2996 static reflection.\n"
        "\n"
        "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\n"
        "            ${CMAKE_CXX_COMPILER}\n"
        "  CMake:    ${CMAKE_VERSION}\n"
        "\n"
        "Two independent causes — check the compiler line above first:\n"
        "\n"
        "1. COMPILER TOO OLD. Requires GCC 16+ (upstream Clang has not merged\n"
        "   P2996; MSVC cannot build this tree yet).\n"
        "       sudo add-apt-repository -y ppa:ubuntu-toolchain-r/test\n"
        "       sudo apt update && sudo apt install -y g++-16\n"
        "   The compiler is cached per build directory and cannot be changed in\n"
        "   place, so remove the build dir:  rm -rf build && ./buildandrun.sh\n"
        "\n"
        "2. CMAKE TOO OLD. cxx_std_26 was only implemented in CMake 3.30 —\n"
        "   Ubuntu 24.04 ships 3.28, where this fails even with GCC 16.\n"
        "   Kitware's APT repo (note: 'pip install cmake' is blocked by PEP 668\n"
        "   on Ubuntu 24.04):\n"
        "       wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \\\n"
        "         | gpg --dearmor - \\\n"
        "         | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null\n"
        "       echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg]'\\\n"
        "            ' https://apt.kitware.com/ubuntu/ noble main' \\\n"
        "         | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null\n"
        "       sudo apt update && sudo apt install -y cmake\n"
        "   No-sudo alternative:  sudo apt install pipx && pipx install cmake\n"
        "\n"
        "To configure by hand:\n"
        "    cmake -S . -B build -G Ninja \\\n"
        "          -DCMAKE_C_COMPILER=gcc-16 -DCMAKE_CXX_COMPILER=g++-16\n"
        "\n"
        "See docs/IMPLEMENTATION_PLAN.md Stage 0.")
endif()

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(FAYE_BUILD_IMGUI_DEMO "Build Dear ImGui demo sources" OFF)
option(FAYE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FAYE_COPY_BIN_TO_REPO_BIN "Write executable output to the repository bin/ directory" ON)

# Precompiled headers for some of the stable third party libraries
option(FAYE_USE_PCH "Precompile stable third-party headers" ON)

# P2996 static reflection. Off-switch exists because -freflection is still
# experimental in GCC 16 — if it miscompiles, flip this rather than reverting
# the standard, and guard reflection code with #if FAYE_HAS_REFLECTION.
option(FAYE_ENABLE_REFLECTION "Enable C++26 static reflection (-freflection)" ON)

# ---------------------------------------------------------------------------
# faye_enable_reflection(<target>)
#
# -freflection MUST be applied per-target and NEVER globally. Two independent
# failure modes, both confirmed on this tree (docs/IMPLEMENTATION_PLAN.md,
# probe runs 2 and 3):
#
#   * In CMAKE_CXX_FLAGS it reaches CMake's own project() compiler check, which
#     runs at the compiler's DEFAULT standard (gnu++23) -- configure fails
#     before a single source file is seen.
#   * Even with -std=c++26 alongside it, dependencies that pin a lower
#     CXX_STANDARD (glm, assimp: 17) get the flag too, and CMake appends the
#     target's -std AFTER CMAKE_CXX_FLAGS -- so those TUs compile at c++17 with
#     -freflection attached, which cc1plus rejects outright.
#
# Both surface as: "'-freflection' only supported with '-std=c++26'".
# ---------------------------------------------------------------------------
function(faye_enable_reflection target)
    if(NOT FAYE_ENABLE_REFLECTION)
        return()
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
       CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 16)
        target_compile_options(${target} PRIVATE -freflection)
        target_compile_definitions(${target} PRIVATE FAYE_HAS_REFLECTION=1)
    else()
        message(WARNING
            "FAYE_ENABLE_REFLECTION is ON but ${CMAKE_CXX_COMPILER_ID} "
            "${CMAKE_CXX_COMPILER_VERSION} has no -freflection; "
            "building '${target}' without reflection.")
    endif()
endfunction()

# Concurrency gates (see docs/ECS_AND_JOBS_IMPLEMENTATION_PLAN.md Phases 1 & 7).
option(FAYE_TSAN "Build faye_tests with ThreadSanitizer (GCC/Clang)" OFF)
option(FAYE_JOBS_SINGLE_THREADED "Run all jobs inline on the calling thread (serial debugging)" OFF)

set(QUILL_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(QUILL_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(QUILL_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
set(QUILL_BUILD_FUZZING OFF CACHE BOOL "" FORCE)
set(QUILL_DOCS_GEN OFF CACHE BOOL "" FORCE)
set(QUILL_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)

include(FayeDependencies)
include(FayeShaderCompilation)

faye_configure_dependencies()

enable_testing()

add_subdirectory(src)