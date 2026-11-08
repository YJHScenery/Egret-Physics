--[[
    @file scene_control.lua
    @brief 协同编队控制基准场景的受限 Lua 控制程序。

    与 scene_control.c 逐行同构：三角波轨迹跟踪、自适应 PD 主控、四卫星编队力合成、
    全场实体枚举与速度限幅、数值序列环形滤波、接触统计、动态实体生命周期管理。
    常量从场景参数读取一次并保存在闭包中；浮点表达式只使用四则运算、math_sqrt 与
    math_abs，保证与受限 C 后端逐位一致。
]]

local KP = parameter_number("kp")
local KD = parameter_number("kd")
local MAXF = parameter_number("maxf")
local RADIUS = parameter_number("radius")
local GAIN = parameter_number("gain")
local SAT_KP = parameter_number("satkp")
local SAT_KD = parameter_number("satkd")
local SAT_MAX = parameter_number("satmax")
local SPEED_CAP = parameter_number("speedcap")
local ITERATIONS = parameter_number("iters")
local TORQUE_GAIN = parameter_number("torque")

-- 跨步持久状态。
local g_react_x = 0.0
local g_react_y = 0.0
local g_react_z = 0.0
local g_average = 0.0
local g_filter = 0.0
local g_hits = 0.0
local g_history = nil

-- @brief 把数值限制在 [-limit, limit] 内。
local function clamp_abs(value, limit)
    if value > limit then return limit end
    if value < -limit then return -limit end
    return value
end

-- @brief 平滑饱和：x / (1 + |x|)，返回值域 (-1, 1)。
local function smooth_sat(value)
    local magnitude = math_abs(value)
    return value / (1.0 + magnitude)
end

-- @brief 保号平方根：sqrt(|value|) 再带上原符号。
local function signed_sqrt(value)
    local root = math_sqrt(math_abs(value))
    if value < 0.0 then return -root end
    return root
end

-- @brief 三维向量长度。
local function vector_length(x, y, z)
    return math_sqrt(x * x + y * y + z * z)
end

-- @brief 累加编队反作用力。
local function add_reaction(x, y, z)
    g_react_x = g_react_x + x
    g_react_y = g_react_y + y
    g_react_z = g_react_z + z
end

-- @brief 单颗卫星的编队控制：径向弹簧、速度阻尼与切向涡旋，返回距离。
local function satellite_step(satellite, hub_x, hub_y, hub_z, phase)
    local p = entity_position(satellite)
    local v = entity_velocity(satellite)
    local rx = p.x - hub_x
    local ry = p.y - hub_y
    local rz = p.z - hub_z
    local d = vector_length(rx, ry, rz)
    if d < 0.0001 then d = 0.0001 end
    local ux = rx / d
    local uy = ry / d
    local uz = rz / d
    local radial = clamp_abs(-SAT_KP * (d - RADIUS), SAT_MAX)
    local swirl = GAIN * (1.0 + smooth_sat(phase))
    local fx = radial * ux - SAT_KD * v.x - uy * swirl
    local fy = radial * uy - SAT_KD * v.y + ux * swirl
    local fz = radial * uz - SAT_KD * v.z + 0.25 * smooth_sat(phase) * uz
    entity_set_force(satellite, vector3(fx, fy, fz))
    add_reaction(-0.35 * fx, -0.35 * fy, -0.35 * fz)
    return d
end

-- @brief 主控制回调。
function on_step()
    local step = simulation_step_index()
    local now = simulation_time()
    local delta = control_delta()

    if step == 0 then
        g_history = numbers_create()
        g_average = 0.0
        g_filter = 0.0
        g_hits = 0.0
        state_set_bool("spawned", false)
        state_set_number("epoch", now)
    end

    local hub = scene_entity("hub")
    local sat1 = scene_entity("sat1")
    local sat2 = scene_entity("sat2")
    local sat3 = scene_entity("sat3")
    local sat4 = scene_entity("sat4")
    local ball = scene_entity("ball")

    local hub_pos = entity_position(hub)
    local hub_vel = entity_velocity(hub)
    local hub_spin = entity_angular_velocity(hub)
    local hub_mass = entity_mass(hub)
    local hub_active = entity_active(hub)
    local hub_rotation = entity_rotation(hub)
    local hub_material = entity_material(hub)
    local ball_pos = entity_position(ball)

    local cycle = step % 1200
    local phase = cycle / 600.0
    if phase > 1.0 then phase = 2.0 - phase end
    local tilt = vector_length(hub_rotation.x, hub_rotation.y, 0.0)
    local target_z = 3.4 + 0.35 * phase + 0.02 * tilt

    local err_x = 0.0 - hub_pos.x
    local err_y = 0.0 - hub_pos.y
    local err_z = target_z - hub_pos.z
    local dist = vector_length(err_x, err_y, err_z)

    g_average = g_average * 0.94 + dist * 0.06
    local filter_value = g_filter
    local index = 0.0
    while index < ITERATIONS do
        filter_value = 0.5 * filter_value + 0.25 * math_sqrt(0.25 + math_abs(filter_value) * 0.01 + dist * 0.05)
        index = index + 1.0
    end
    g_filter = filter_value

    local adapt = 1.0 + GAIN * (g_average / (1.0 + g_average)) + 0.05 * filter_value
    local mass_factor = 1.0 + 0.03 * hub_mass
    local boost = 1.0 + 0.6 * smooth_sat(signed_sqrt(dist - 0.15) * 3.0) + 0.2 * delta
    local force_x = clamp_abs(KP * adapt * mass_factor * err_x - KD * hub_vel.x, MAXF) * boost
    local force_y = clamp_abs(KP * adapt * mass_factor * err_y - KD * hub_vel.y, MAXF) * boost
    local force_z = clamp_abs(KP * adapt * mass_factor * err_z - KD * hub_vel.z, MAXF) * boost
    force_x = clamp_abs(force_x, MAXF * 2.0)
    force_y = clamp_abs(force_y, MAXF * 2.0)
    force_z = clamp_abs(force_z, MAXF * 2.0)

    if entity_rotatable(hub) then
        entity_set_torque(hub, vector3(-TORQUE_GAIN * hub_spin.x + 0.01 * filter_value,
                                       -TORQUE_GAIN * hub_spin.y,
                                       -TORQUE_GAIN * hub_spin.z + 0.02 * smooth_sat(phase)))
    end

    g_react_x = 0.0
    g_react_y = 0.0
    g_react_z = 0.0
    local d1 = satellite_step(sat1, hub_pos.x, hub_pos.y, hub_pos.z, phase)
    local d2 = satellite_step(sat2, hub_pos.x, hub_pos.y, hub_pos.z, phase + 0.35)
    local d3 = satellite_step(sat3, hub_pos.x, hub_pos.y, hub_pos.z, phase + 0.70)
    local d4 = satellite_step(sat4, hub_pos.x, hub_pos.y, hub_pos.z, phase + 1.05)
    entity_set_force(hub, vector3(clamp_abs(force_x + g_react_x, MAXF * 2.0),
                                  clamp_abs(force_y + g_react_y, MAXF * 2.0),
                                  clamp_abs(force_z + g_react_z, MAXF * 2.0)))

    local adjusted = material(hub_material.staticFriction, hub_material.dynamicFriction, hub_material.restitution)
    entity_set_material(hub, adjusted)

    local members = scene_entities()
    local cursor_entity = collection_next(members)
    local count = 0.0
    local mass_sum = 0.0
    local max_speed = 0.0
    local fastest = hub
    while entity_exists(cursor_entity) do
        count = count + 1.0
        mass_sum = mass_sum + entity_mass(cursor_entity)
        local velocity_now = entity_velocity(cursor_entity)
        local speed_now = vector_length(velocity_now.x, velocity_now.y, velocity_now.z)
        if speed_now > max_speed then
            max_speed = speed_now
            fastest = cursor_entity
        end
        cursor_entity = collection_next(members)
    end

    if max_speed > SPEED_CAP then
        local fast_velocity = entity_velocity(fastest)
        entity_set_force(fastest, vector3(-0.7 * fast_velocity.x, -0.7 * fast_velocity.y, -0.7 * fast_velocity.z))
        state_set_bool("limiting", true)
    else
        state_set_bool("limiting", false)
    end

    local touching = entities_touching(hub, sat1)
    if touching then g_hits = g_hits + 1.0 end
    state_set_bool("contact", touching)
    state_set_number("hits", g_hits)
    state_set_number("dist", dist)
    state_set_number("filter", g_filter)
    state_set_number("maxspeed", max_speed)
    state_set_number("masssum", mass_sum)
    state_set_number("bodies", count)
    state_set_number("formation", d1 + d2 + d3 + d4)
    state_set_number("ballz", ball_pos.z)
    state_set_number("react", vector_length(g_react_x, g_react_y, g_react_z))

    numbers_push(g_history, dist)
    if numbers_count(g_history) >= 24 then
        local newest = numbers_pop(g_history)
        state_set_number("newest", newest)
        numbers_destroy(g_history)
        g_history = numbers_create()
    end

    if step == 2000 then
        local child = scene_spawn(scene_template("sat1"), vector3(hub_pos.x, hub_pos.y, hub_pos.z + 1.6))
        entity_set_velocity(child, vector3(0.6, 0.0, -0.4))
        entity_set_mass(child, 0.75)
        entity_set_material(child, material(0.2, 0.3, 0.5))
        state_set_entity("child", child)
        state_set_bool("spawned", true)
    end
    if step == 6000 then
        local child = state_entity("child")
        if entity_exists(child) then entity_destroy(child) end
        state_set_bool("spawned", false)
    end

    state_set_bool("hubactive", hub_active)
    state_set_number("hubmass", hub_mass)
    state_set_number("clock", now)
end
