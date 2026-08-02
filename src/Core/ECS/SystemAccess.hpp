#pragma once

#include <cstdint>
#include <vector>

#include "Core/ECS/ComponentPool.hpp"

namespace Faye::Ecs
{
    // What a system declares it touches. Built fluently:
    //   SystemAccess{}.read<Transform>().write<Velocity>()
    // access() is a PROMISE: the scheduler trusts it to decide what may run
    // concurrently, and AccessCheck.hpp makes a lie a deterministic abort.
    struct SystemAccess
    {
        std::vector<ComponentId> reads;    // components read but never written
        std::vector<ComponentId> writes;   // components mutated in place
        bool needsMainThread = false;      // touches Vulkan/GLFW/ImGui/Lua -> thread 0
        bool isExclusive = false;          // needs the whole World; conflicts with everything

        template <class T>
        SystemAccess &read()
        {
            reads.push_back(componentId<T>());
            return *this;
        }
        template <class T>
        SystemAccess &write()
        {
            writes.push_back(componentId<T>());
            return *this;
        }
        SystemAccess &mainThread()
        {
            needsMainThread = true;
            return *this;
        }
        SystemAccess &exclusive()
        {
            isExclusive = true;
            return *this;
        }
    };

    // The mask form the scheduler actually uses, compiled once at registration.
    // ComponentId doubles as a bit position (same idea as the pre-ECS
    // ComponentMask), so a whole access set is a uint64_t and a conflict test
    // is a handful of AND instructions.
    struct CompiledAccess
    {
        uint64_t readMask = 0;
        uint64_t writeMask = 0;
        bool needsMainThread = false;
        bool isExclusive = false;
    };

    inline CompiledAccess compileAccess(const SystemAccess &access)
    {
        CompiledAccess compiled;
        for (const ComponentId id : access.reads)
            compiled.readMask |= uint64_t(1) << id;
        for (const ComponentId id : access.writes)
            compiled.writeMask |= uint64_t(1) << id;
        compiled.needsMainThread = access.needsMainThread;
        compiled.isExclusive = access.isExclusive;
        return compiled;
    }

    // Two systems may run concurrently iff no write overlaps anything the other
    // touches. Readers never conflict with readers — that is where parallelism
    // comes from: a frame typically has many readers of Transform and one writer.
    inline bool conflicts(const CompiledAccess &a, const CompiledAccess &b)
    {
        if (a.isExclusive || b.isExclusive)
            return true;
        if ((a.writeMask & b.writeMask) != 0)
            return true;   // writer vs writer
        if ((a.writeMask & b.readMask) != 0)
            return true;   // writer vs reader
        if ((a.readMask & b.writeMask) != 0)
            return true;   // reader vs writer
        return false;
    }
}
