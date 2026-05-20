-- rotator.lua: Example Lua script that spins an entity around the Y axis.
local speed = 90.0  -- degrees per second
local angle = 0.0

function onStart(entity)
    print("Lua rotator started for entity " .. tostring(entity:getId()))
end

function onUpdate(entity, dt)
    angle = angle + speed * dt
    -- Uncomment once setRotationY is exposed via bindEngineAPI:
    -- entity:setRotationY(angle)
end

function onDestroy(entity)
    print("Lua rotator destroyed for entity " .. tostring(entity:getId()))
end
