#pragma once

// Precompiled header for faye_app.
//
// Rule for this file: third-party and standard-library headers ONLY. Nothing
// under src/ belongs here — engine headers change constantly, and a PCH is a
// build-wide dependency, so adding one would rebuild all ~70 translation units
// on every edit to it.
//
// Configuration macros the third-party headers below react to (GLM_FORCE_*,
// GLFW_INCLUDE_VULKAN, IMGUI_DEFINE_MATH_OPERATORS) are set as compile
// definitions in src/CMakeLists.txt rather than being #defined here: a PCH is
// force-included before the first line of every TU, so a TU that defines them
// itself would otherwise get a differently-configured copy of the header.
//
// vma_implementation.cpp is excluded from the PCH (SKIP_PRECOMPILE_HEADERS)
// because it defines VMA_IMPLEMENTATION before including vk_mem_alloc.h, which
// the PCH has already parsed without it.

// --- C++ standard library ---
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

// --- Vulkan / VMA / GLFW ---
#include <vulkan/vulkan.h>

// Declarations only — the implementation is switched on by VMA_IMPLEMENTATION
// in vma_implementation.cpp, which opts out of this PCH.
#include "Renderer/Vulkan/vk_mem_alloc.h" // vendored third-party, not engine code

#include <GLFW/glfw3.h>

// --- Dear ImGui ---
#include <imgui.h>

// --- quill ---
// Only the logging front door. quill/Backend.h and quill/Frontend.h alone cost
// ~2.7s to parse and are used by the four files that set logging up, so they
// stay out: a PCH is paid for by every TU, whether it needs the header or not.
#include "quill/LogMacros.h"
#include "quill/Logger.h"

// sol2 is deliberately absent: ~1s to parse and it doubles the PCH size, but
// only the Lua scripting TUs need it (measured +30s of total build CPU).

// --- glm ---
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/hash.hpp>
#include <glm/gtx/quaternion.hpp>
