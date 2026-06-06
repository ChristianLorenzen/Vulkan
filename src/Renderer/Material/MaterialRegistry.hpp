#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Material.hpp"

namespace Faye
{
    struct MaterialHandle
    {
        uint32_t value = 0;

        bool isValid() const { return value != 0; }

        friend bool operator==(const MaterialHandle &left, const MaterialHandle &right) = default;
    };

    class MaterialRegistry
    {
    public:
        static constexpr MaterialHandle invalidHandle{};

        MaterialRegistry();

        MaterialHandle registerMaterial(MaterialData materialData, MaterialPipelineConfig pipelineConfig = {});
        MaterialHandle registerMaterial(std::unique_ptr<Material> material);

        Material *getMaterial(MaterialHandle handle);
        const Material *getMaterial(MaterialHandle handle) const;

        // Returns the handle of the built-in white fallback material.
        MaterialHandle getDefaultHandle() const { return defaultHandle; }

        // Returns the material for handle, or the fallback material if the
        // handle is invalid or not registered.
        Material *getMaterialOrDefault(MaterialHandle handle);
        const Material *getMaterialOrDefault(MaterialHandle handle) const;

    private:
        uint32_t nextHandleValue = 1;
        std::unordered_map<uint32_t, std::unique_ptr<Material>> materials;
        MaterialHandle defaultHandle{};
    };
}