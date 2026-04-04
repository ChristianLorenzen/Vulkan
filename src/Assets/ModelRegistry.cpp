#include "ModelRegistry.hpp"

#include <stdexcept>

using namespace Faye;

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