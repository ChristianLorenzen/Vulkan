cmake_minimum_required(VERSION 3.21)

project(FayeVulkan VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(FAYE_BUILD_IMGUI_DEMO "Build Dear ImGui demo sources" OFF)
option(FAYE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FAYE_COPY_BIN_TO_REPO_BIN "Write executable output to the repository bin/ directory" ON)

# Precompiled headers for some of the stable third party libraries
option(FAYE_USE_PCH "Precompile stable third-party headers" ON)

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