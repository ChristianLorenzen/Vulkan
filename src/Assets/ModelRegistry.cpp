#include "ModelRegistry.hpp"

#include <stdexcept>

using namespace Faye;

Faye::ModelRegistry::ModelRegistry(VulkanDevice &device)
    : device(device) {}

Faye::ModelHandle Faye::ModelRegistry::createPrimitive(PrimitiveType primitiveType, uint32_t subdivisions)
{
    return registerModel(Model::createPrimitive(device, primitiveType, subdivisions));
}

std::unique_ptr<Faye::Model> Faye::ModelRegistry::makeModelFromFile(const std::string &modelPath)
{
    return Model::createModelFromFile(device, modelPath);
}

Faye::ModelHandle Faye::ModelRegistry::registerModel(std::unique_ptr<Model> model)
{
    if (model == nullptr)
    {
        throw std::runtime_error("Cannot register a null model in ModelRegistry");
    }

    const ModelHandle handle{nextHandleValue++};
    models.emplace(handle.value, std::move(model));
    return handle;
}

Faye::Model *Faye::ModelRegistry::getModel(ModelHandle handle)
{
    auto iterator = models.find(handle.value);
    return iterator != models.end() ? iterator->second.get() : nullptr;
}

const Faye::Model *Faye::ModelRegistry::getModel(ModelHandle handle) const
{
    auto iterator = models.find(handle.value);
    return iterator != models.end() ? iterator->second.get() : nullptr;
}