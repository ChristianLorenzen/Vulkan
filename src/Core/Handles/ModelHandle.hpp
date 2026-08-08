#pragma once

#include <cstdint>

namespace Faye
{
    // Standalone so component headers can name a model without pulling in the
    // registry (and its renderer includes). See ModelRegistry.hpp for the owner.
    struct ModelHandle
    {
        uint32_t value = 0;

        bool isValid() const { return value != 0; }

        friend bool operator==(const ModelHandle &left, const ModelHandle &right) = default;
    };
}
