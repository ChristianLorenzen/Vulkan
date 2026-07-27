#include "MaterialRegistry.hpp"

#include <algorithm>
#include <stdexcept>

using namespace Faye;

Faye::MaterialRegistry::MaterialRegistry()
{
    MaterialData fallback{};
    fallback.name = "Default Material";
    fallback.color = {1.0f, 1.0f, 1.0f};
    fallback.baseColorFactor = {1.0f, 1.0f, 1.0f, 1.0f};
    fallback.metallicFactor = 0.0f;
    fallback.roughnessFactor = 1.0f;
    defaultHandle = registerMaterial(std::move(fallback));
}

Faye::MaterialHandle Faye::MaterialRegistry::registerMaterial(MaterialData materialData, MaterialPipelineConfig pipelineConfig)
{
    return registerMaterial(std::make_unique<Material>(std::move(materialData), std::move(pipelineConfig)));
}

Faye::MaterialHandle Faye::MaterialRegistry::registerMaterial(std::unique_ptr<Faye::Material> material)
{
    if (material == nullptr)
    {
        throw std::runtime_error("Cannot register a null material in MaterialRegistry");
    }

    const MaterialHandle handle{nextHandleValue++};
    materials.emplace(handle.value, std::move(material));
    return handle;
}

Faye::Material *Faye::MaterialRegistry::getMaterial(MaterialHandle handle)
{
    auto iterator = materials.find(handle.value);
    return iterator != materials.end() ? iterator->second.get() : nullptr;
}

const Faye::Material *Faye::MaterialRegistry::getMaterial(MaterialHandle handle) const
{
    auto iterator = materials.find(handle.value);
    return iterator != materials.end() ? iterator->second.get() : nullptr;
}

std::vector<Faye::MaterialHandle> Faye::MaterialRegistry::getAllHandles() const
{
    std::vector<MaterialHandle> handles;
    handles.reserve(materials.size());
    for (const auto &[value, material] : materials)
    {
        handles.push_back(MaterialHandle{value});
    }

    std::sort(handles.begin(), handles.end(), [](MaterialHandle a, MaterialHandle b) {
        return a.value < b.value;
    });
    return handles;
}

Faye::Material *Faye::MaterialRegistry::getMaterialOrDefault(MaterialHandle handle)
{
    if (Material *material = getMaterial(handle))
    {
        return material;
    }
    return getMaterial(defaultHandle);
}

const Faye::Material *Faye::MaterialRegistry::getMaterialOrDefault(MaterialHandle handle) const
{
    if (const Material *material = getMaterial(handle))
    {
        return material;
    }
    return getMaterial(defaultHandle);
}
