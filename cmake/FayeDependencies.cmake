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
        message(FATAL_ERROR
            "GLFW was not found. Install glfw3 and make it discoverable via CMAKE_PREFIX_PATH, a package manager integration, or pkg-config.")
    endif()

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
endfunction()