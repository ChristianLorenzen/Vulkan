set(FAYE_ENGINE_SOURCES
    Assets/ModelRegistry.cpp
    Core/HotReload/HotReloadManager.cpp
    Core/IO/FileSystem.cpp
    Editor/ImGui/EditorPanels.cpp
    Editor/ImGui/ImGuiCustomStyle.cpp
    Editor/ImGui/ImGuiRenderer.cpp
    Platform/GLFW/VKGLFW.cpp
    Platform/Window/Window.cpp
    Renderer/Material/MaterialRegistry.cpp
    Renderer/Material/TextureLoader.cpp
    Renderer/Material/TextureRegistry.cpp
    Renderer/PostProcess/PostProcessEffectLibrary.cpp
    Renderer/Resources/Model.cpp
    Renderer/Scene/Extractors/CameraRenderExtractor.cpp
    Renderer/Scene/Extractors/MeshRenderExtractor.cpp
    Renderer/Scene/Extractors/PointLightRenderExtractor.cpp
    Renderer/Scene/RenderExtractionManager.cpp
    Renderer/Vulkan/vk_shader_manager.cpp
    Renderer/Vulkan/ShaderReflection.cpp
    Renderer/Material/MaterialTemplate.cpp
    Renderer/Vulkan/vk_descriptors.cpp
    Renderer/Vulkan/vk_device.cpp
    Renderer/Vulkan/vk_pipeline.cpp
    Renderer/Vulkan/vk_render_system.cpp
    Renderer/Vulkan/point_light_render_system.cpp
    Renderer/Vulkan/vk_post_process.cpp
    Renderer/Vulkan/vk_renderer.cpp
    Renderer/Vulkan/vk_swapchain.cpp
    Renderer/Vulkan/Vulkan.cpp
    Renderer/Vulkan/VulkanBuffer.cpp
    Renderer/Vulkan/VkImageResource.cpp
    Renderer/Vulkan/vma_implementation.cpp
    Renderer/Vulkan/MaterialCache.cpp
    Renderer/Vulkan/TextureCache.cpp
    Renderer/Vulkan/VkSamplerResource.cpp
    Renderer/Vulkan/VkTextureResource.cpp
    Runtime/main.cpp
    Scripting/ScriptSystem.cpp
    Scripting/LuaScriptSystem.cpp
    Scene/Camera/Camera.cpp
    Scene/Entities/Components.cpp
    Scene/Entities/Entity.cpp
    Scene/Entities/EntityManager.cpp
    Scene/Scene.cpp
    Scene/SceneQueries.cpp
    Scene/SceneManager.cpp
)

include(FetchContent)
FetchContent_GetProperties(imgui SOURCE_DIR _imgui_src)

set(FAYE_IMGUI_SOURCES
    ${_imgui_src}/imgui.cpp
    ${_imgui_src}/imgui_draw.cpp
    ${_imgui_src}/backends/imgui_impl_glfw.cpp
    ${_imgui_src}/backends/imgui_impl_vulkan.cpp
    ${_imgui_src}/imgui_tables.cpp
    ${_imgui_src}/imgui_widgets.cpp)

if(FAYE_BUILD_IMGUI_DEMO)
    list(APPEND FAYE_IMGUI_SOURCES ${_imgui_src}/imgui_demo.cpp)
endif()

set(FAYE_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/point_light.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/point_light.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process_tint.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process_exposure.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process_contrast.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process_vignette.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process_depth_fog.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process_edge_detect.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process_depth_debug.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/custom_material.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/custom_material.frag
)

add_library(faye_imgui STATIC ${FAYE_IMGUI_SOURCES})
add_library(Faye::ImGui ALIAS faye_imgui)

target_include_directories(faye_imgui
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${_imgui_src}
    ${_imgui_src}/backends)

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
    assimp::assimp
    quill::quill
    spirv-reflect-static
    lua_static
    sol2::sol2)

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
    target_link_options(faye_app PRIVATE -rdynamic)
endif()

# --- RotatorScript example plugin ---
FetchContent_GetProperties(assimp SOURCE_DIR _assimp_script_src BINARY_DIR _assimp_script_bin)

add_library(faye_rotator_script SHARED
    Scripting/ExampleScripts/RotatorScript.cpp)

target_include_directories(faye_rotator_script
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${_assimp_script_src}/include
    ${_assimp_script_bin}/include)

target_compile_features(faye_rotator_script PRIVATE cxx_std_20)

if(MSVC)
    target_compile_options(faye_rotator_script PRIVATE /W4)
else()
    target_compile_options(faye_rotator_script PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Clear the debug postfix assimp injects globally so the .so name stays predictable.
set_target_properties(faye_rotator_script PROPERTIES DEBUG_POSTFIX "")

if(FAYE_COPY_BIN_TO_REPO_BIN)
    set_target_properties(faye_rotator_script PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endif()

faye_add_shader_target(
    faye_shaders
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compiled
    SHADERS ${FAYE_SHADER_SOURCES})
add_dependencies(faye_app faye_shaders)

if(FAYE_COPY_BIN_TO_REPO_BIN)
    set_target_properties(faye_app PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
endif()