set(FAYE_ENGINE_SOURCES
    Core/IO/FileSystem.cpp
    Platform/GLFW/VKGLFW.cpp
    Platform/Window/Window.cpp
    Renderer/Resources/Model.cpp
    Renderer/Vulkan/vk_descriptors.cpp
    Renderer/Vulkan/vk_device.cpp
    Renderer/Vulkan/vk_pipeline.cpp
    Renderer/Vulkan/vk_render_system.cpp
    Renderer/Vulkan/vk_renderer.cpp
    Renderer/Vulkan/vk_swapchain.cpp
    Renderer/Vulkan/Vulkan.cpp
    Renderer/Vulkan/VulkanBuffer.cpp
    Runtime/main.cpp
    Scene/Camera/Camera.cpp
    Scene/Entities/GameObject.cpp)

set(FAYE_IMGUI_SOURCES
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_impl_glfw.cpp
    imgui/imgui_impl_vulkan.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp)

if(FAYE_BUILD_IMGUI_DEMO)
    list(APPEND FAYE_IMGUI_SOURCES imgui/imgui_demo.cpp)
endif()

set(FAYE_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag)

add_library(faye_imgui STATIC ${FAYE_IMGUI_SOURCES})
add_library(Faye::ImGui ALIAS faye_imgui)

target_include_directories(faye_imgui
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

target_link_libraries(faye_imgui
    PUBLIC
    Faye::GLFW
    Faye::Vulkan)

if(MSVC)
    target_compile_definitions(faye_imgui PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

add_executable(faye_app ${FAYE_ENGINE_SOURCES})
add_executable(Faye::App ALIAS faye_app)

target_include_directories(faye_app
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(faye_app
    PRIVATE
    Faye::ImGui
    Faye::GLFW
    Faye::Vulkan
    quill::quill)

target_compile_features(faye_app PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(faye_app PRIVATE /W4)
    target_compile_definitions(faye_app PRIVATE _CRT_SECURE_NO_WARNINGS)

    if(FAYE_WARNINGS_AS_ERRORS)
        target_compile_options(faye_app PRIVATE /WX)
    endif()
else()
    target_compile_options(faye_app PRIVATE -Wall -Wextra -Wpedantic)

    if(FAYE_WARNINGS_AS_ERRORS)
        target_compile_options(faye_app PRIVATE -Werror)
    endif()
endif()

if(APPLE)
    target_link_libraries(faye_app PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(faye_app PRIVATE dl pthread)
endif()

faye_add_shader_target(faye_shaders SHADERS ${FAYE_SHADER_SOURCES})
add_dependencies(faye_app faye_shaders)

if(FAYE_COPY_BIN_TO_REPO_BIN)
    set_target_properties(faye_app PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endif()