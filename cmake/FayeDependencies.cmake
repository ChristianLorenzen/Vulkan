include_guard(GLOBAL)
include(FetchContent)

function(faye_configure_dependencies)
    # CMake 4.0 REMOVED (not merely deprecated) compatibility with
    # cmake_minimum_required(VERSION < 3.5). Several pinned dependencies predate
    # that and now hard-error at configure time — yaml-cpp 0.8.0 declares 2.x,
    # and there is no newer yaml-cpp release to move to.
    #
    # Scoped deliberately to this function: it relaxes the floor for the
    # FetchContent subprojects only, never for Faye's own CMake. Drop it per
    # dependency as each one raises its minimum upstream.
    #
    # The alternative is to stay on CMake 3.31.x — the last 3.x, which still has
    # cxx_std_26 (3.30+) but none of the 4.0 removals. See
    # docs/IMPLEMENTATION_PLAN.md Stage 0.
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)

    find_package(Vulkan REQUIRED)

    if(TARGET Vulkan::Vulkan)
        add_library(Faye::Vulkan ALIAS Vulkan::Vulkan)
    endif()

    set(_glfw_resolved FALSE)

    find_package(glfw3 CONFIG QUIET)

    if(TARGET glfw)
        add_library(Faye::GLFW ALIAS glfw)
        set(_glfw_resolved TRUE)
    elseif(TARGET glfw3)
        add_library(Faye::GLFW ALIAS glfw3)
        set(_glfw_resolved TRUE)
    elseif(TARGET glfw::glfw)
        add_library(Faye::GLFW ALIAS glfw::glfw)
        set(_glfw_resolved TRUE)
    endif()

    if(NOT _glfw_resolved)
        find_package(PkgConfig QUIET)

        if(PkgConfig_FOUND)
            pkg_check_modules(GLFW3 QUIET glfw3)
        endif()

        if(GLFW3_FOUND)
            add_library(faye_glfw INTERFACE)
            target_include_directories(faye_glfw INTERFACE ${GLFW3_INCLUDE_DIRS})
            target_link_directories(faye_glfw INTERFACE ${GLFW3_LIBRARY_DIRS})
            target_link_libraries(faye_glfw INTERFACE ${GLFW3_LINK_LIBRARIES})
            add_library(Faye::GLFW ALIAS faye_glfw)
            set(_glfw_resolved TRUE)
        endif()
    endif()

    if(NOT _glfw_resolved)
        message(STATUS "GLFW not found via find_package/pkg-config — fetching from source.")
        FetchContent_Declare(
            glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
            GIT_SHALLOW TRUE
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
        add_library(Faye::GLFW ALIAS glfw)
        set(_glfw_resolved TRUE)
    endif()

    # GLM — header-only; available as a system package on Linux but not on Windows.
    # Always fetch so the build is self-contained across platforms.
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(glm)

    # =========================================================================
    # UNPINNED DEPENDENCIES — imgui, quill, assimp, spirv_reflect, lua, sol2
    #
    # These six track moving branches, so a fresh build dir re-clones them at
    # whatever HEAD happens to be. Two consequences worth caring about now that
    # the tree is on a bleeding-edge toolchain:
    # * a build failure may be an unrelated upstream regression, not yours;
    # * a green build does not reproduce next week, or on another machine.
    #
    # Run  ./tools/pin_deps.sh <build-dir>  to print exact GIT_TAG lines for the
    # commits currently checked out and known-good, then paste them in below.
    # Deliberately not guessed here: a wrong tag breaks the build outright.
    # =========================================================================
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking # TODO(pin): see tools/pin_deps.sh
        GIT_SHALLOW TRUE
    )

    FetchContent_Declare(
        quill
        GIT_REPOSITORY https://github.com/odygrd/quill.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )

    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(imgui quill assimp)

    FetchContent_Declare(
        spirv_reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )

    set(SPIRV_REFLECT_STATIC_LIB ON CACHE BOOL "Build SPIRV-Reflect as a static library" FORCE)
    set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Build the spirv-reflect CLI tool" FORCE)
    set(SPIRV_REFLECT_EXAMPLES OFF CACHE BOOL "Build SPIRV-Reflect examples" FORCE)
    FetchContent_MakeAvailable(spirv_reflect)

    # Lua 5.4 — used by LuaScriptSystem
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/walterschell/Lua.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    set(LUA_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)

    # sol2 — header-only C++/Lua binding layer
    FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG main
        GIT_SHALLOW TRUE
    )

    FetchContent_MakeAvailable(lua sol2)

    # doctest — unit-test framework for faye_tests
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.12
        GIT_SHALLOW TRUE
    )
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)

    # yaml-cpp — scene/asset serialization format
    FetchContent_Declare(
        yaml-cpp
        GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
        GIT_TAG 0.8.0
        GIT_SHALLOW TRUE
    )
    set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
    set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(yaml-cpp)

    # GCC 15 stopped supplying <cstdint> transitively through other libstdc++
    # headers. yaml-cpp 0.8.0 (src/emitterutils.cpp:221, 'uint16_t') predates
    # that change, and there is NO fixed release to move to — 0.8.0 is still the
    # latest tag; the fix exists only on master. Forcing the include keeps a real
    # release pin instead of pinning to an unreleased commit.
    # Remove this when a yaml-cpp > 0.8.0 lands.
    if(TARGET yaml-cpp)
        if(MSVC)
            target_compile_options(yaml-cpp PRIVATE /FIcstdint)
        else()
            target_compile_options(yaml-cpp PRIVATE -include cstdint)
        endif()
    endif()

    # Boost — header-only surface (boost.uuid and its transitive headers),
    # pinned to a release tag. Consumed as include paths only: we do NOT run
    # Boost's own CMake, which expects the full superproject layout.
    #
    # GIT_SUBMODULES is mandatory here. boostorg/boost is the superproject: it
    # holds no code, only ~155 submodules, and FetchContent inits all of them
    # when the list is omitted (GIT_SHALLOW caps history depth, not breadth) —
    # 2.1 GB of checkout for a library we use four headers from. The list below
    # is the full `#include <boost/...>` closure of boost.uuid at 1.87, which
    # Boost 1.87 deliberately pared back to these few modules. ~2.5 MB total.
    # Re-derive it (not just the build.jam deps, which miss static_assert) if
    # the pin moves.
    FetchContent_Declare(
        boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG boost-1.87.0
        GIT_SHALLOW TRUE
        GIT_SUBMODULES libs/uuid libs/assert libs/config
        libs/throw_exception libs/type_traits libs/static_assert
        SOURCE_SUBDIR faye-do-not-configure
        EXCLUDE_FROM_ALL
    )

    # Populate without configuring: SOURCE_SUBDIR names a directory that does
    # not exist, so MakeAvailable downloads and sets boost_SOURCE_DIR but never
    # runs add_subdirectory. This replaces the single-argument
    # FetchContent_Populate(boost), which is deprecated from CMake 3.30 and an
    # error under CMP0169 — harmless at 3.28, but it would break the moment the
    # minimum is raised again.
    FetchContent_MakeAvailable(boost)

    add_library(faye_boost_headers INTERFACE)
    file(GLOB _faye_boost_module_includes "${boost_SOURCE_DIR}/libs/*/include")
    target_include_directories(faye_boost_headers INTERFACE
        "${boost_SOURCE_DIR}/boost"
        ${_faye_boost_module_includes})
    add_library(Faye::BoostHeaders ALIAS faye_boost_headers)
endfunction()