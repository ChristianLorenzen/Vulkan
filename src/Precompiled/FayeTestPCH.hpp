#pragma once

// Precompiled header for faye_tests.
//
// Separate from FayePCH.hpp because faye_tests links neither Vulkan, GLFW nor
// ImGui — it only needs doctest plus the shared logging/math dependencies. Same
// rule applies: third-party and standard-library headers only.
//
// Tests/TestMain.cpp is excluded from the PCH (SKIP_PRECOMPILE_HEADERS): it
// defines DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN before including doctest, which
// the PCH has already parsed without it.

// --- C++ standard library ---
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

// --- doctest ---
#include <doctest/doctest.h>

// --- quill ---
// LogMacros/Logger only, for the same reason as FayePCH.hpp: quill's Backend
// and Frontend headers are the expensive ones and almost nothing needs them.
#include "quill/LogMacros.h"
#include "quill/Logger.h"

// --- glm ---
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
