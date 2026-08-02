#include "MaterialTemplate.hpp"

namespace Faye
{
    MaterialTemplateRegistry::Handle MaterialTemplateRegistry::registerTemplate(MaterialTemplate tmpl)
    {
        const Handle handle = nextHandle++;
        templates.push_back(std::move(tmpl));
        return handle;
    }

    const MaterialTemplate *MaterialTemplateRegistry::get(Handle handle) const
    {
        if (handle == 0u)
        {
            // Handle 0 is the built-in PBR pipeline; no MaterialTemplate entry.
            return nullptr;
        }

        const std::size_t idx = static_cast<std::size_t>(handle) - 1u;
        if (idx >= templates.size())
        {
            return nullptr;
        }
        return &templates[idx];
    }

} // namespace Faye
