-- rotator.lua: Example Lua script that spins an entity around the Y axis.
--
-- Delta time is available two ways:
--   1. Engine.dt  -- global engine context table, always up-to-date
--   2. dt         -- passed as the second argument to onUpdate
-- Both hold the same value; use whichever reads more naturally.
-- Multiplying speed by dt makes rotation framerate-independent.

local speed = math.pi / 2  -- radians per second (~90 degrees/sec)
local angle = 0.0           -- accumulated angle in radians

function onStart(entity)
    print("Lua rotator started for entity " .. tostring(entity:getId()))
end

function onUpdate(entity, dt)
    -- Engine.dt and the dt parameter are identical; either works.
    angle = angle + speed * Engine.dt
    entity:setRotationY(angle)
end

function onDestroy(entity)
    print("Lua rotator destroyed for entity " .. tostring(entity:getId()))
end
