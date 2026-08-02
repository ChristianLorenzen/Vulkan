#pragma once

#include "Core/Serialization/Uuid.hpp"

namespace Faye
{
    // Stable cross-session identity for an asset; scene files persist these.
    // Deterministic (name-based) for file-sourced assets and built-ins, random
    // v4 for runtime-created ones. See AssetDatabase for the id schemes.
    using AssetId = Uuid;
}
