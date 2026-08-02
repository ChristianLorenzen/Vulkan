include_guard(GLOBAL)
include(FetchContent)

function(faye_configure_dependencies)
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
            GIT_TAG        3.4
            GIT_SHALLOW    TRUE
        )
        set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)
        add_library(Faye::GLFW ALIAS glfw)
        set(_glfw_resolved TRUE)
    endif()

    # GLM — header-only; available as a system package on Linux but not on Windows.
    # Always fetch so the build is self-contained across platforms.
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glm)

    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG        docking
        GIT_SHALLOW    TRUE
    )

    FetchContent_Declare(
        quill
        GIT_REPOSITORY https://github.com/odygrd/quill.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )

    FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )

    FetchContent_MakeAvailable(imgui quill assimp)

    FetchContent_Declare(
        spirv_reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )

    set(SPIRV_REFLECT_STATIC_LIB ON  CACHE BOOL "Build SPIRV-Reflect as a static library" FORCE)
    set(SPIRV_REFLECT_EXECUTABLE OFF CACHE BOOL "Build the spirv-reflect CLI tool"         FORCE)
    set(SPIRV_REFLECT_EXAMPLES   OFF CACHE BOOL "Build SPIRV-Reflect examples"              FORCE)
    FetchContent_MakeAvailable(spirv_reflect)

    # Lua 5.4 — used by LuaScriptSystem
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/walterschell/Lua.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
    )
    set(LUA_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(LUA_BUILD_COMPILER OFF CACHE BOOL "" FORCE)

    # sol2 — header-only C++/Lua binding layer
    FetchContent_Declare(
        sol2
        GIT_REPOSITORY https://github.com/ThePhD/sol2.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE
    )

    FetchContent_MakeAvailable(lua sol2)

    # doctest — unit-test framework for faye_tests
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG        v2.4.12
        GIT_SHALLOW    TRUE
    )
    set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(DOCTEST_NO_INSTALL ON  CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(doctest)

    # Boost — header-only surface (boost.uuid and its transitive headers),
    # pinned to a release tag. Shallow-cloned WITH all submodules so the
    # header closure is complete without maintaining a fragile per-module
    # list (boostdep can narrow this later if the download size matters).
    # Consumed as include paths only: we do NOT run Boost's own CMake, which
    # expects the full superproject layout.
    FetchContent_Declare(
        boost
        GIT_REPOSITORY https://github.com/boostorg/boost.git
        GIT_TAG        boost-1.87.0
        GIT_SHALLOW    TRUE
        EXCLUDE_FROM_ALL
    )
    FetchContent_GetProperties(boost)
    if(NOT boost_POPULATED)
        FetchContent_Populate(boost)
    endif()
    add_library(faye_boost_headers INTERFACE)
    file(GLOB _faye_boost_module_includes "${boost_SOURCE_DIR}/libs/*/include")
    target_include_directories(faye_boost_headers INTERFACE
        "${boost_SOURCE_DIR}/boost"
        ${_faye_boost_module_includes})
    add_library(Faye::BoostHeaders ALIAS faye_boost_headers)
endfunction()