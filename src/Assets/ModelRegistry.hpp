#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Renderer/Resources/Model.hpp"

namespace Faye
{
    struct ModelHandle
    {
        uint32_t value = 0;

        bool isValid() const { return value != 0; }

        friend bool operator==(const ModelHandle &left, const ModelHandle &right) = default;
    };

    class ModelRegistry
    {
    public:
        static constexpr ModelHandle invalidHandle{};

        ModelHandle registerModel(std::unique_ptr<Model> model);

        Model *getModel(ModelHandle handle);
        const Model *getModel(ModelHandle handle) const;

    private:
        uint32_t nextHandleValue = 1;
        std::unordered_map<uint32_t, std::unique_ptr<Model>> models;
    };
}