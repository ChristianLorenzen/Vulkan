#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "Assets/ModelHandle.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/Resources/PrimitiveType.hpp"

namespace Faye
{
    class ModelRegistry
    {
    public:
        static constexpr ModelHandle invalidHandle{};

        explicit ModelRegistry(VulkanDevice &device);

        ModelHandle createPrimitive(PrimitiveType primitiveType, uint32_t subdivisions = 64);

        std::unique_ptr<Model> makeModelFromFile(const std::string &modelPath);

        ModelHandle registerModel(std::unique_ptr<Model> model);

        Model *getModel(ModelHandle handle);
        const Model *getModel(ModelHandle handle) const;

    private:
        VulkanDevice &device;
        uint32_t nextHandleValue = 1;
        std::unordered_map<uint32_t, std::unique_ptr<Model>> models;
    };
}
