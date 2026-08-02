#pragma once

#include <cstdint>

namespace Faye
{
    // Standalone so component headers can name a material without pulling in
    // the registry (and its renderer includes). See MaterialRegistry.hpp for
    // the owner.
    struct MaterialHandle
    {
        uint32_t value = 0;

        bool isValid() const { return value != 0; }

        friend bool operator==(const MaterialHandle &left, const MaterialHandle &right) = default;
    };
}
