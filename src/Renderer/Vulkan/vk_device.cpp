#include "vk_device.hpp"
#include <algorithm>
#include <vector>
#include <stdio.h>
#include <unordered_map>
#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <optional>
#include <set>
#include <fstream>
#include <cstring>

namespace
{
    struct PhysicalDeviceEvaluation
    {
        VkPhysicalDevice device = VK_NULL_HANDLE;
        VkPhysicalDeviceProperties properties{};
        VkPhysicalDeviceFeatures features{};
        Faye::QueueFamilyIndices queueFamilies{};
        Faye::SwapChainSupportDetails swapChainSupport{};
        VkDeviceSize deviceLocalMemoryBytes = 0;
        bool extensionsSupported = false;
        bool swapChainAdequate = false;
        bool samplerAnisotropy = false;
        bool suitable = false;
        uint64_t score = 0;
        std::string priorityLabel{"Unsupported"};
        std::string rejectionReason{};
    };

    const char *physicalDeviceTypeToString(VkPhysicalDeviceType deviceType)
    {
        switch (deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "Other";
        }
    }

    uint64_t scoreDeviceType(VkPhysicalDeviceType deviceType)
    {
        switch (deviceType)
        {
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return 1'000'000;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return 250'000;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return 125'000;
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return 10'000;
        default:
            return 50'000;
        }
    }

    std::string classifyPriority(uint64_t score, VkPhysicalDeviceType deviceType)
    {
        if (deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        {
            return score >= 1'500'000 ? "Highest" : "High";
        }

        if (deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        {
            return score >= 500'000 ? "Medium" : "Low";
        }

        if (deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU)
        {
            return "Low";
        }

        if (deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU)
        {
            return "Fallback";
        }

        return score >= 400'000 ? "Medium" : "Low";
    }

    double bytesToGiB(VkDeviceSize bytes)
    {
        constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
        return static_cast<double>(bytes) / kGiB;
    }

    VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void *userData)
    {
        auto *logger = static_cast<quill::Logger *>(userData);

        if (logger == nullptr)
        {
            return VK_FALSE;
        }

        if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            LOG_ERROR(logger, "Vulkan validation [{}]: {}", messageType, callbackData->pMessage);
        }
        else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            LOG_WARNING(logger, "Vulkan validation [{}]: {}", messageType, callbackData->pMessage);
        }
        else
        {
            LOG_INFO(logger, "Vulkan validation [{}]: {}", messageType, callbackData->pMessage);
        }

        return VK_FALSE;
    }

    VkResult createDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT *createInfo,
        const VkAllocationCallbacks *allocator,
        VkDebugUtilsMessengerEXT *debugMessenger)
    {
        auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

        if (func == nullptr)
        {
            return VK_ERROR_EXTENSION_NOT_PRESENT;
        }

        return func(instance, createInfo, allocator, debugMessenger);
    }

    void destroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks *allocator)
    {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

        if (func != nullptr)
        {
            func(instance, debugMessenger, allocator);
        }
    }
} // namespace

using namespace Faye;

#ifdef NDEBUG
const bool preferValidationLayers = false;
#else
const bool preferValidationLayers = true;
#endif

VulkanDevice::VulkanDevice(Window &window) : window{window}
{
    validationLayersEnabled = shouldEnableValidationLayers();

    LOG_INFO(Logger::getInstance(), "Creating Instance...");
    createInstance();
    setupDebugMessenger();
    LOG_INFO(Logger::getInstance(), "Creating Surface...");
    createSurface();
    LOG_INFO(Logger::getInstance(), "Creating Devices...");
    createPhysicalDevice();
    createLogicalDevice();
    createAllocator();
    LOG_INFO(Logger::getInstance(), "Creating Command Pools...");
    createCommandPools();
}

void VulkanDevice::createAllocator()
{
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.vulkanApiVersion = VK_VERSION;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;

    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

bool VulkanDevice::shouldEnableValidationLayers()
{
    const char *validationOverride = std::getenv("FAYE_VK_VALIDATION");
    bool requestedValidationLayers = preferValidationLayers;

    if (validationOverride != nullptr)
    {
        std::string value = validationOverride;
        if (value == "0" || value == "false" || value == "FALSE" || value == "off" || value == "OFF")
        {
            requestedValidationLayers = false;
        }
        else if (value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON")
        {
            requestedValidationLayers = true;
        }
    }

    if (!requestedValidationLayers)
    {
        return false;
    }

    if (checkValidationLayerSupport())
    {
        return true;
    }

    LOG_WARNING(
        Logger::getInstance(),
        "Validation layer {} was requested but is not available in this environment. Continuing without validation layers.",
        validationLayers.front());
    return false;
}

VulkanDevice::~VulkanDevice()
{
    vmaDestroyAllocator(allocator);
    vkDestroyCommandPool(device, commandPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    destroyDebugMessenger();
    vkDestroyInstance(instance, nullptr);
}

void VulkanDevice::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) const
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = Logger::getInstance();
}

void VulkanDevice::setupDebugMessenger()
{
    if (!validationLayersEnabled)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    if (createDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to set up Vulkan debug messenger");
    }
}

void VulkanDevice::destroyDebugMessenger()
{
    if (debugMessenger != VK_NULL_HANDLE)
    {
        destroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        debugMessenger = VK_NULL_HANDLE;
    }
}

bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device)
{
    QueueFamilyIndices indices = findQueueFamilies(device);

    bool extensionsSupported = checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported)
    {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
        LOG_INFO(Logger::getInstance(), "SwapChainSupportDetails: {} {}", swapChainSupport.formats.size(), swapChainSupport.presentModes.size());
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

bool VulkanDevice::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto &extension : availableExtensions)
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device)
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport)
        {
            indices.presentFamily = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }
    return indices;
}

SwapChainSupportDetails VulkanDevice::querySwapChainSupport(VkPhysicalDevice device)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0)
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    LOG_ERROR(Logger::getInstance(), "Failed to find suitable memory type");
    throw std::runtime_error("Failed to find suitable memory type...");
}

VkSampleCountFlagBits VulkanDevice::getMaxUsableSampleCount()
{
    VkPhysicalDeviceProperties physicalDeviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

    VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT)
    {
        return VK_SAMPLE_COUNT_64_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_32_BIT)
    {
        return VK_SAMPLE_COUNT_32_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_16_BIT)
    {
        return VK_SAMPLE_COUNT_16_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_8_BIT)
    {
        return VK_SAMPLE_COUNT_8_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_4_BIT)
    {
        return VK_SAMPLE_COUNT_4_BIT;
    }
    if (counts & VK_SAMPLE_COUNT_2_BIT)
    {
        return VK_SAMPLE_COUNT_2_BIT;
    }

    return VK_SAMPLE_COUNT_1_BIT;
}

void VulkanDevice::createInstance()
{
    VkApplicationInfo appInfo = {};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 0, 0, 1);
    appInfo.apiVersion = VK_VERSION;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if (validationLayersEnabled)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    std::vector<const char *> extensions = getRequiredExtensions();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (validationLayersEnabled)
    {
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }
    else
    {
        createInfo.pNext = nullptr;
    }

#ifdef __APPLE__
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create instance!");
    }
}

void VulkanDevice::createSurface()
{
    window.createWindowSurface(instance, &surface);
}

void VulkanDevice::createPhysicalDevice()
{
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    auto evaluatePhysicalDevice = [this](VkPhysicalDevice candidate)
    {
        PhysicalDeviceEvaluation evaluation{};
        evaluation.device = candidate;

        vkGetPhysicalDeviceProperties(candidate, &evaluation.properties);
        vkGetPhysicalDeviceFeatures(candidate, &evaluation.features);
        evaluation.queueFamilies = findQueueFamilies(candidate);
        evaluation.extensionsSupported = checkDeviceExtensionSupport(candidate);

        if (evaluation.extensionsSupported)
        {
            evaluation.swapChainSupport = querySwapChainSupport(candidate);
            evaluation.swapChainAdequate = !evaluation.swapChainSupport.formats.empty() &&
                                           !evaluation.swapChainSupport.presentModes.empty();
        }

        evaluation.samplerAnisotropy = evaluation.features.samplerAnisotropy == VK_TRUE;
        evaluation.suitable = evaluation.queueFamilies.isComplete() &&
                              evaluation.extensionsSupported &&
                              evaluation.swapChainAdequate &&
                              evaluation.samplerAnisotropy;

        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(candidate, &memoryProperties);
        for (uint32_t heapIndex = 0; heapIndex < memoryProperties.memoryHeapCount; ++heapIndex)
        {
            const auto &heap = memoryProperties.memoryHeaps[heapIndex];
            if ((heap.flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) != 0)
            {
                evaluation.deviceLocalMemoryBytes += heap.size;
            }
        }

        if (!evaluation.queueFamilies.isComplete())
        {
            evaluation.rejectionReason = "missing graphics/present queue support";
        }
        else if (!evaluation.extensionsSupported)
        {
            evaluation.rejectionReason = "missing required device extensions";
        }
        else if (!evaluation.swapChainAdequate)
        {
            evaluation.rejectionReason = "swapchain support is incomplete";
        }
        else if (!evaluation.samplerAnisotropy)
        {
            evaluation.rejectionReason = "sampler anisotropy is unavailable";
        }

        if (evaluation.suitable)
        {
            evaluation.score += scoreDeviceType(evaluation.properties.deviceType);
            evaluation.score += static_cast<uint64_t>(bytesToGiB(evaluation.deviceLocalMemoryBytes) * 100'000.0);
            evaluation.score += static_cast<uint64_t>(evaluation.properties.limits.maxImageDimension2D) * 10;
            evaluation.score += static_cast<uint64_t>(evaluation.properties.limits.maxPerStageDescriptorSampledImages) * 2;
            evaluation.score += static_cast<uint64_t>(VK_VERSION_MAJOR(evaluation.properties.apiVersion)) * 10'000;
            evaluation.score += static_cast<uint64_t>(VK_VERSION_MINOR(evaluation.properties.apiVersion)) * 1'000;
            evaluation.priorityLabel = classifyPriority(evaluation.score, evaluation.properties.deviceType);
        }

        return evaluation;
    };

    std::vector<PhysicalDeviceEvaluation> evaluations;
    evaluations.reserve(devices.size());

    for (const auto &device : devices)
    {
        evaluations.push_back(evaluatePhysicalDevice(device));
    }

    std::sort(evaluations.begin(), evaluations.end(), [](const PhysicalDeviceEvaluation &lhs, const PhysicalDeviceEvaluation &rhs)
              {
                  if (lhs.suitable != rhs.suitable)
                  {
                      return lhs.suitable > rhs.suitable;
                  }

                  return lhs.score > rhs.score; });

    for (const auto &evaluation : evaluations)
    {
        if (evaluation.suitable)
        {
            LOG_INFO(
                Logger::getInstance(),
                "GPU candidate: {} | type={} | suitable=yes | priority={} | score={} | device_local_memory={:.2f} GiB | max2D={} | swapchain_formats={} | present_modes={}",
                evaluation.properties.deviceName,
                physicalDeviceTypeToString(evaluation.properties.deviceType),
                evaluation.priorityLabel,
                evaluation.score,
                bytesToGiB(evaluation.deviceLocalMemoryBytes),
                evaluation.properties.limits.maxImageDimension2D,
                evaluation.swapChainSupport.formats.size(),
                evaluation.swapChainSupport.presentModes.size());
        }
        else
        {
            LOG_WARNING(
                Logger::getInstance(),
                "GPU candidate: {} | type={} | suitable=no | reason={}",
                evaluation.properties.deviceName,
                physicalDeviceTypeToString(evaluation.properties.deviceType),
                evaluation.rejectionReason);
        }
    }

    auto selected = std::find_if(evaluations.begin(), evaluations.end(), [](const PhysicalDeviceEvaluation &evaluation)
                                 { return evaluation.suitable; });

    if (selected != evaluations.end())
    {
        physicalDevice = selected->device;
        properties = selected->properties;
    }

    if (physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    LOG_INFO(
        Logger::getInstance(),
        "Selected physical device: {} | type={} | priority={} | score={}",
        properties.deviceName,
        physicalDeviceTypeToString(properties.deviceType),
        selected->priorityLabel,
        selected->score);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
}

void VulkanDevice::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
    graphicsQueueFamilyIndex = indices.graphicsFamily.value();

    // Specifying the queue to use
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};
    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // Setup device features. Empty for now, but should fill out once we start using Vulkan features.
    // VkPhysicalDeviceFeatures deviceFeatures = {};
    // deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkPhysicalDeviceVulkan13Features featuresVK13 = {};
    featuresVK13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;

    VkPhysicalDeviceVulkan12Features featuresVK12 = {};
    featuresVK12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    featuresVK12.pNext = &featuresVK13;

    VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
    deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    deviceFeatures2.pNext = &featuresVK12;

    vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);

    // Create logical device
    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    // We have the &deviceFeatures struct, but setting to null since now using pNext chaining.
    createInfo.pEnabledFeatures = nullptr;
    createInfo.pNext = &deviceFeatures2;

    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    // Back-compat with older implementations of vulkan. Now, enabledLayerCount and ppEnabledLayerNames are ignored.
    if (validationLayersEnabled)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create logical device");
    }

    vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);
}

void VulkanDevice::createCommandPools()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create command pool");
    }
}

VkFormat VulkanDevice::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates)
    {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find supported format");
}

bool VulkanDevice::checkValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char *layerName : validationLayers)
    {
        bool layerFound = false;
        for (const auto &layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

std::vector<const char *> VulkanDevice::getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifdef __APPLE__
    extensions.push_back("VK_KHR_portability_enumeration");
    extensions.push_back("VK_KHR_get_physical_device_properties2");
#endif

    if (validationLayersEnabled)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void VulkanDevice::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo &allocInfo, VkBuffer &buffer, VmaAllocation &allocation)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create buffer via VMA");
    }
}

void VulkanDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferCopy copyRegion = {};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    endSingleTimeCommands(commandBuffer);
}

void VulkanDevice::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {
        width,
        height,
        1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

VkCommandBuffer VulkanDevice::beginSingleTimeCommands()
{
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void VulkanDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer)
{
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

/// @brief Creates the vkImage, and then allocates and binds the necessary memory.
/// @param imageInfo
/// @param properties
/// @param image
/// @param imageMemory
void VulkanDevice::createImageWithInfo(
    const VkImageCreateInfo &imageInfo,
    const VmaAllocationCreateInfo &allocInfo,
    VkImage &image,
    VmaAllocation &allocation)
{
    if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create image via VMA");
    }
}
