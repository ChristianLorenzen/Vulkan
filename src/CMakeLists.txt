set(FAYE_ENGINE_SOURCES
    Assets/ModelRegistry.cpp
    Assets/AssetDatabase.cpp
    Core/HotReload/HotReloadManager.cpp
    Core/HotReload/HotReloadSystem.cpp
    Core/IO/FileSystem.cpp
    Core/Jobs/JobSystem.cpp
    Core/ECS/SystemSchedule.cpp
    Core/Logging/Logger.cpp
    Core/Path/Paths.cpp
    Core/Serialization/Uuid.cpp
    Core/Time/Timer.cpp
    Editor/Editor.cpp
    Editor/ImGuiIntegration/EditorRenderLayer.cpp
    Editor/ImGuiIntegration/ImGuiCustomStyle.cpp
    Editor/ImGuiIntegration/ImGuiRenderer.cpp
    Editor/Panels/EditorPanels.cpp
    Editor/Panels/AssetExplorer/AssetExplorerPanel.cpp
    Editor/Panels/Console/ConsolePanel.cpp
    Editor/Panels/EditorView/EditorViewPanel.cpp
    Editor/Panels/FrameStats/FrameStatsPanel.cpp
    Editor/Panels/Hierarchy/HierarchyPanel.cpp
    Editor/Panels/Inspector/ComponentDrawers.cpp
    Editor/Panels/Inspector/FieldDrawers.cpp
    Editor/Panels/Inspector/InspectorPanel.cpp
    Editor/Panels/Inspector/MaterialEditor.cpp
    Editor/Panels/RuntimeView/RuntimeViewPanel.cpp
    Editor/Panels/SceneSettings/SceneSettingsPanel.cpp
    Editor/Panels/WaterDebug/WaterDebugPanel.cpp
    Editor/Utility/FieldDrawRegistry.cpp
    Editor/Widgets/AssetBrowser.cpp
    Editor/Widgets/EditorWidgets.cpp
    Editor/Widgets/FilePickerDialog.cpp
    Platform/GLFW/VKGLFW.cpp
    Platform/Window/Window.cpp
    Renderer/Material/MaterialRegistry.cpp
    Renderer/Material/TextureLoader.cpp
    Renderer/Material/TextureRegistry.cpp
    Renderer/PostProcess/PostProcessEffectLibrary.cpp
    Renderer/Resources/Model.cpp
    Renderer/Environment/environment_map.cpp
    Renderer/Scene/Extractors/MeshRenderExtractor.cpp
    Renderer/Scene/Extractors/PointLightRenderExtractor.cpp
    Renderer/Scene/Extractors/DirectionalLightRenderExtractor.cpp
    Renderer/Scene/LightingUniforms.cpp
    Renderer/Scene/RenderExtractionManager.cpp
    Renderer/Vulkan/vk_shader_manager.cpp
    Renderer/Vulkan/ShaderReflection.cpp
    Renderer/Material/MaterialTemplate.cpp
    Renderer/Vulkan/vk_descriptors.cpp
    Renderer/Vulkan/vk_device.cpp
    Renderer/Vulkan/vk_pipeline.cpp
    Renderer/Vulkan/vk_compute_pipeline.cpp
    Renderer/Vulkan/vk_render_system.cpp
    Renderer/Vulkan/point_light_render_system.cpp
    Renderer/Vulkan/editor_grid_render_system.cpp
    Renderer/Vulkan/skybox_render_system.cpp
    Renderer/Vulkan/vk_post_process.cpp
    Renderer/Vulkan/vk_renderer.cpp
    Renderer/Vulkan/vk_swapchain.cpp
    Renderer/Vulkan/Vulkan.cpp
    Renderer/Vulkan/profiler/vk_profiler.cpp
    Renderer/Vulkan/VulkanBuffer.cpp
    Renderer/Vulkan/VkImageResource.cpp
    Renderer/Vulkan/vma_implementation.cpp
    Renderer/Vulkan/MaterialCache.cpp
    Renderer/Vulkan/TextureCache.cpp
    Renderer/Vulkan/VkSamplerResource.cpp
    Renderer/Vulkan/VkTextureResource.cpp
    Runtime/main.cpp
    Runtime/Engine.cpp
    Scripting/ScriptSystem.cpp
    Scripting/LuaScriptSystem.cpp
    Scripting/ScriptingSystem.cpp
    Scene/Camera/Camera.cpp
    Core/ECS/Reflection/SemanticRegistry.cpp
    Scene/Entities/Components.cpp
    Scene/Entities/Entity.cpp
    Scene/Entities/RegisterComponents.cpp
    Scene/Scene.cpp
    Scene/SceneQueries.cpp
    Scene/SceneManager.cpp
    Scene/SceneBuilder.cpp
    Scene/Serialization/Serializer.cpp
    Scene/Serialization/Deserializer.cpp
    Scene/Serialization/ComponentSerializers.cpp
    Scene/Serialization/ReflectedSerializers.cpp
    Scene/Serialization/MaterialEnumNames.cpp
    Scene/Serialization/SceneFileWriter.cpp
    Scene/Serialization/SceneFileReader.cpp
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

file(GLOB_RECURSE FAYE_SHADER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.tesc
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.tese
)

add_library(faye_imgui STATIC ${FAYE_IMGUI_SOURCES})
add_library(Faye::ImGui ALIAS faye_imgui)

# Explicit rather than silently inheriting CMAKE_CXX_STANDARD. ImGui's own code
# needs nothing newer than C++11, but it is built at 26 here because that is
# what the C++26 migration probe actually verified. Lowering it is safe if
# compile time ever matters — just make it a decision, not an accident.
target_compile_features(faye_imgui PRIVATE cxx_std_26)

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

# PUBLIC so faye_imgui and everything including imgui_internal.h agree on the
# operator set — a mismatch is an ODR violation, and the precompiled header
# (which parses imgui.h once for the whole target) makes a per-file #define
# unreliable anyway.
target_compile_definitions(faye_imgui PUBLIC IMGUI_DEFINE_MATH_OPERATORS)

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
    glm::glm
    assimp::assimp
    quill::quill
    spirv-reflect-static
    lua_static
    sol2::sol2
    Faye::BoostHeaders
    yaml-cpp)

target_compile_features(faye_app PRIVATE cxx_std_26)
faye_enable_reflection(faye_app)

# Bake the repo root as an absolute path so asset/shader paths work regardless
# of the working directory (avoids CWD dependency when launching from an IDE).
target_compile_definitions(faye_app PRIVATE
    FAYE_ASSET_ROOT="${CMAKE_SOURCE_DIR}"
)

# Configuration macros for the third-party headers in the PCH. These used to be
# #defined ad hoc at the top of individual sources; the precompiled header is
# force-included before those lines, so they have to be target-wide to stay
# consistent (GLM_FORCE_DEPTH_ZERO_TO_ONE in particular changes what
# glm::perspective produces). The remaining in-file #defines are identical
# redefinitions and stay harmless.
target_compile_definitions(faye_app PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
    GLFW_INCLUDE_VULKAN
)

if(FAYE_JOBS_SINGLE_THREADED)
    target_compile_definitions(faye_app PRIVATE JOBS_SINGLE_THREADED)
endif()

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

# --- Precompiled headers ---
# Precompiles the stable third-party surface (ImGui, quill, glm, Vulkan, VMA,
# GLFW) plus the standard library once per target instead of once per
# translation unit. See src/Precompiled/FayePCH.hpp for what may go in there.
if(FAYE_USE_PCH)
    target_precompile_headers(faye_app PRIVATE Precompiled/FayePCH.hpp)

    # Single-header libraries are compiled here, and the PCH is force-included
    # ahead of the *_IMPLEMENTATION macro that switches them on:
    # vma_implementation.cpp - the PCH already parsed vk_mem_alloc.h without
    # VMA_IMPLEMENTATION, so the definitions would
    # silently vanish.
    # Model.cpp              - stb_image's SSE2 path miscompiles under a PCH
    # at -O0 ("last argument must be an 8-bit
    # immediate" out of emmintrin.h).
    set_source_files_properties(
        Renderer/Vulkan/vma_implementation.cpp
        Renderer/Resources/Model.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

# --- Unit tests ---
# Test files live in a Tests/ directory next to the implementation they cover
# (e.g. Core/Jobs/Tests/); TestMain.cpp provides the shared doctest entry point.
add_executable(faye_tests
    Tests/TestMain.cpp
    Core/HotReload/HotReloadManager.cpp
    Core/Jobs/JobSystem.cpp
    Core/ECS/SystemSchedule.cpp
    Core/Jobs/Tests/JobSystemTests.cpp
    Core/ECS/Tests/EcsTests.cpp
    Core/ECS/Tests/TypeRegistryTests.cpp
    Core/ECS/Tests/ScheduleTests.cpp
    Core/ECS/Reflection/SemanticRegistry.cpp
    Core/ECS/Tests/ReflectionTests.cpp
    Core/Logging/Logger.cpp
    Core/Path/Paths.cpp
    Core/Path/Tests/PathsTests.cpp
    Core/Serialization/Uuid.cpp
    Core/Serialization/Tests/UuidTests.cpp
    Core/IO/FileSystem.cpp
    # PostProcessEffectComponent's definitionId dropdown names its option
    # provider by reflection, so kDescriptor now holds the function's ADDRESS
    # and the descriptor no longer links without it. Vulkan-free -- this file
    # only reads effect definitions off disk -- so the test target stays
    # headless.
    Renderer/PostProcess/PostProcessEffectLibrary.cpp
    Assets/AssetDatabase.cpp
    Assets/Tests/AssetDatabaseTests.cpp
    Scene/Camera/Camera.cpp
    Scene/Entities/Components.cpp
    Scene/Entities/Entity.cpp
    Scene/Entities/RegisterComponents.cpp
    Scene/Scene.cpp
    Scene/Tests/SceneTests.cpp
    Scene/Serialization/Serializer.cpp
    Scene/Serialization/Deserializer.cpp
    Scene/Serialization/ComponentSerializers.cpp
    Scene/Serialization/ReflectedSerializers.cpp
    Scene/Serialization/MaterialEnumNames.cpp
    Scene/Serialization/Tests/SceneSerializationTests.cpp
    Scripting/ScriptSystem.cpp
    Scripting/LuaScriptSystem.cpp
    Scripting/Tests/ScriptSystemTests.cpp
    Scripting/Tests/LuaScriptSystemTests.cpp)

target_include_directories(faye_tests
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(faye_tests
    PRIVATE
    doctest::doctest
    glm::glm
    quill::quill
    sol2::sol2
    lua_static
    assimp::assimp
    Faye::BoostHeaders
    yaml-cpp)

# Core/Path/Paths.cpp (pulled in for the hot-reload watch path) needs the same
# repo-root definition faye_app gets.
target_compile_definitions(faye_tests PRIVATE
    FAYE_ASSET_ROOT="${CMAKE_SOURCE_DIR}"
)

# Match faye_app's glm configuration (see the note there); the tests share
# Camera/Components, so the two targets must agree.
target_compile_definitions(faye_tests PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
)

if(FAYE_JOBS_SINGLE_THREADED)
    target_compile_definitions(faye_tests PRIVATE JOBS_SINGLE_THREADED)
endif()

# ThreadSanitizer gate for the concurrent code (job system, scheduler). Scoped
# to faye_tests: instrumenting the Vulkan app would flag driver/GLFW races that
# aren't ours. Serial (FAYE_JOBS_SINGLE_THREADED) defeats the purpose here.
if(FAYE_TSAN)
    if(MSVC)
        message(WARNING "FAYE_TSAN ignored: ThreadSanitizer requires GCC/Clang.")
    elseif(FAYE_JOBS_SINGLE_THREADED)
        message(WARNING "FAYE_TSAN with FAYE_JOBS_SINGLE_THREADED runs every job inline; no threads to check.")
    else()
        target_compile_options(faye_tests PRIVATE -fsanitize=thread -g)
        target_link_options(faye_tests PRIVATE -fsanitize=thread)
    endif()
endif()

target_compile_features(faye_tests PRIVATE cxx_std_26)
faye_enable_reflection(faye_tests)

if(MSVC)
    target_compile_options(faye_tests PRIVATE /W4)
else()
    target_compile_options(faye_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(faye_tests PRIVATE dl pthread)
endif()

if(FAYE_USE_PCH)
    target_precompile_headers(faye_tests PRIVATE Precompiled/FayeTestPCH.hpp)

    # Defines DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN before including doctest, which
    # the PCH has already parsed without it — without this, main() disappears.
    set_source_files_properties(Tests/TestMain.cpp
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
endif()

add_test(NAME faye_tests COMMAND faye_tests)

# --- RotatorScript example plugin ---
FetchContent_GetProperties(assimp SOURCE_DIR _assimp_script_src BINARY_DIR _assimp_script_bin)

add_library(faye_rotator_script SHARED
    Scripting/ExampleScripts/RotatorScript.cpp)

target_include_directories(faye_rotator_script
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${_assimp_script_src}/include
    ${_assimp_script_bin}/include)

# Must match faye_app's standard and reflection flag: descriptors and engine
# headers cross this .so boundary, so the plugin ABI contract is "same compiler,
# same flags". This target has no PCH and no target_link_libraries, which also
# makes it the missing-include canary when libstdc++ prunes transitive headers.
target_compile_features(faye_rotator_script PRIVATE cxx_std_26)
faye_enable_reflection(faye_rotator_script)

# Same glm configuration as faye_app: the plugin includes engine headers
# (Camera, Components) that no longer set these themselves.
target_compile_definitions(faye_rotator_script PRIVATE
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_ENABLE_EXPERIMENTAL
)

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