// #pragma once

// #include "Renderer.hpp"
// #include "Camera/Camera.hpp"
// #include "vk_device.hpp"
// #include "vk_types.hpp"

// namespace Faye{

//     class VulkanRenderer : public Renderer {
//         public:
//             VulkanRenderer(Window &window) : window(window) {
//                 vk_device = new VulkanDevice(window);
//             }
//             ~VulkanRenderer() {
//                 delete vk_device;
//                 delete camera;
//             };

//             VulkanRenderer(const VulkanRenderer &) = delete;
//             void operator=(const VulkanRenderer &) = delete;

//             void init() override;
//             void draw() override;

//         private:
//             VulkanDevice *vk_device;
//             Camera *camera;
//             Window *window;
//     };
// }