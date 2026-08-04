#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>

#include "Scene/Camera/Camera.hpp"

namespace Faye { class VulkanBuffer; }

namespace Faye
{
    // Light-count caps. Both light arrays live in SceneLightingUBO (set 0,
    // binding 2) and the GLSL mirror in FayeLighting.glsl must use the same
    // values. Directional lights use a small array so a future fill/rim light
    // needs no UBO/shader change.
#define MAX_POINT_LIGHTS 10
#define MAX_DIRECTIONAL_LIGHTS 4

    // GPU-layout (std140) mirror structs. These are NOT the ECS components:
    // the extractor + packSceneLighting() translate components into these.
    struct GpuPointLight
    {
        glm::vec4 position{}; // xyz = world position, w unused (=1)
        glm::vec4 color{};    // rgb = colour, w = intensity
    };

    // Directional light. `direction`/`color` are kept LAST so a future
    // `glm::mat4 lightViewProj` (and/or a cascade array) for shadow mapping can
    // be appended without disturbing the std140 offsets shaders already read.
    struct GpuDirectionalLight
    {
        glm::vec4 direction{}; // xyz = normalised world direction the light travels toward
        glm::vec4 color{};     // rgb = colour, w = intensity
    };

    // Camera / per-frame data only. All lighting lives in SceneLightingUBO.
    // Manually std140-padded (GLM uses default 4-byte alignment); inverseProjection
    // sits on a 16-byte boundary and stays at the end.
    struct GlobalUBO
    {
        glm::mat4 projection{1.f};          // offset   0
        glm::mat4 view{1.f};                // offset  64
        glm::mat4 inverseView{1.f};         // offset 128
        glm::mat4 priorViewProjection{1.f}; // offset 192
        float time = 0.0f;                  // offset 256
        float deltaTime = 0.0f;             // offset 260
        // offscreen scene render extent in pixels, not the swapchain extent
        glm::vec2 resolution{0.f};          // offset 264
        glm::mat4 inverseProjection{1.f};   // offset 272
    };

    // All scene lighting: ambient + directional + point. Bound at set 0,
    // binding 2 (fragment stage). std140 layout mirrored by FayeLighting.glsl.
    // Trailing ints padded to a 16-byte block boundary (480 bytes) so the bound
    // buffer range matches the SPIRV block size.
    struct SceneLightingUBO
    {
        glm::vec4 ambientColor{1.0f, 1.0f, 1.0f, 0.15f};              // rgb + intensity
        GpuDirectionalLight directionalLights[MAX_DIRECTIONAL_LIGHTS]; // offset 16
        GpuPointLight pointLights[MAX_POINT_LIGHTS];                   // offset 144
        int numDirectionalLights = 0;                                 // offset 464
        int numPointLights = 0;                                       // offset 468
        int _pad0 = 0;                                                // offset 472
        int _pad1 = 0;                                                // offset 476 -> block size 480
        glm::vec4 skyParams{0.0f, 1.0f, 1.0f, 0.0f}; // offset 480
    };

    // Lock the std140 layouts so any accidental drift from the GLSL mirrors
    // (FayeGlobal.glsl / FayeLighting.glsl) is caught at compile time.
    static_assert(offsetof(GlobalUBO, inverseProjection) == 272, "GlobalUBO std140 layout drift");
    static_assert(sizeof(GlobalUBO) == 336, "GlobalUBO std140 size drift");
    static_assert(offsetof(SceneLightingUBO, directionalLights) == 16, "SceneLightingUBO layout drift");
    static_assert(offsetof(SceneLightingUBO, pointLights) == 144, "SceneLightingUBO layout drift");
    static_assert(offsetof(SceneLightingUBO, numDirectionalLights) == 464, "SceneLightingUBO layout drift");
    static_assert(offsetof(SceneLightingUBO, skyParams) == 480, "SceneLightingUBO layout drift");
    static_assert(sizeof(SceneLightingUBO) == 496, "SceneLightingUBO std140 size drift");

    struct FrameContext
    {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        const Camera &camera;
        // Push-descriptor data for set 0 (global UBO + prepass depth + lighting).
        // Render systems push these directly into the command buffer rather than
        // binding a pre-allocated VkDescriptorSet.
        VulkanBuffer *globalBuffer{nullptr};
        VulkanBuffer *lightingBuffer{nullptr};
        VkDescriptorImageInfo prepassDepthInfo{};
        VkDescriptorImageInfo waterFieldInfo{};
        VkDescriptorImageInfo skyCubeInfo{};
        VkDescriptorImageInfo irradianceInfo{};
        VkDescriptorImageInfo prefilteredInfo{};
        uint32_t prefilteredMipCount = 1;
    };

    using FrameInfo = FrameContext;
}