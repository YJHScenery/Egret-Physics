/** @file scene_control.c
 *  @brief 协同编队控制基准场景的受限 C 控制程序。
 *
 *  该程序在每个物理步执行：三角波轨迹跟踪、自适应 PD 主控、四卫星编队力合成、
 *  全场实体枚举与速度限幅、数值序列环形滤波、接触统计、动态实体生命周期管理。
 *  全部常量来自场景参数，在准备阶段被注入为编译期字面量；所有浮点表达式只使用
 *  四则运算、math_sqrt 与 math_abs，保证与 Lua 后端逐位一致。
 */

/** @brief 把数值限制在 [-limit, limit] 内。 */
double clamp_abs(double value, double limit)
{
    if (value > limit) { return limit; }
    if (value < -limit) { return -limit; }
    return value;
}

/** @brief 平滑饱和：x / (1 + |x|)，返回值域 (-1, 1)。 */
double smooth_sat(double value)
{
    double magnitude = math_abs(value);
    return value / (1.0 + magnitude);
}

/** @brief 保号平方根：sqrt(|value|) 再带上原符号。 */
double signed_sqrt(double value)
{
    double root = math_sqrt(math_abs(value));
    if (value < 0.0) { return -root; }
    return root;
}

/** @brief 三维向量长度。 */
double vector_length(double x, double y, double z)
{
    return math_sqrt(x * x + y * y + z * z);
}

/* 跨步持久状态。 */
double g_react_x = 0.0;
double g_react_y = 0.0;
double g_react_z = 0.0;
double g_average = 0.0;
double g_filter = 0.0;
double g_hits = 0.0;
NumberList g_history;

/** @brief 累加编队反作用力。 */
void add_reaction(double x, double y, double z)
{
    g_react_x = g_react_x + x;
    g_react_y = g_react_y + y;
    g_react_z = g_react_z + z;
}

/** @brief 单颗卫星的编队控制：径向弹簧、速度阻尼与切向涡旋，返回距离。 */
double satellite_step(Entity satellite, double hub_x, double hub_y, double hub_z, double phase,
                      double sat_kp, double sat_kd, double sat_max, double radius, double gain)
{
    Vector3 p = entity_position(satellite);
    Vector3 v = entity_velocity(satellite);
    double rx = p.x - hub_x;
    double ry = p.y - hub_y;
    double rz = p.z - hub_z;
    double d = vector_length(rx, ry, rz);
    if (d < 0.0001) { d = 0.0001; }
    double ux = rx / d;
    double uy = ry / d;
    double uz = rz / d;
    double radial = clamp_abs(-sat_kp * (d - radius), sat_max);
    double swirl = gain * (1.0 + smooth_sat(phase));
    double fx = radial * ux - sat_kd * v.x - uy * swirl;
    double fy = radial * uy - sat_kd * v.y + ux * swirl;
    double fz = radial * uz - sat_kd * v.z + 0.25 * smooth_sat(phase) * uz;
    entity_set_force(satellite, vector3(fx, fy, fz));
    add_reaction(-0.35 * fx, -0.35 * fy, -0.35 * fz);
    return d;
}

/** @brief 主控制回调。 */
void on_step(void)
{
    double kp = parameter_number("kp");
    double kd = parameter_number("kd");
    double maxf = parameter_number("maxf");
    double radius = parameter_number("radius");
    double gain = parameter_number("gain");
    double sat_kp = parameter_number("satkp");
    double sat_kd = parameter_number("satkd");
    double sat_max = parameter_number("satmax");
    double speed_cap = parameter_number("speedcap");
    double iterations = parameter_number("iters");
    double torque_gain = parameter_number("torque");

    unsigned long long step = simulation_step_index();
    double now = simulation_time();
    double delta = control_delta();

    if (step == 0ULL)
    {
        g_history = numbers_create();
        g_average = 0.0;
        g_filter = 0.0;
        g_hits = 0.0;
        state_set_bool("spawned", false);
        state_set_number("epoch", now);
    }

    Entity hub = scene_entity("hub");
    Entity sat1 = scene_entity("sat1");
    Entity sat2 = scene_entity("sat2");
    Entity sat3 = scene_entity("sat3");
    Entity sat4 = scene_entity("sat4");
    Entity ball = scene_entity("ball");

    Vector3 hub_pos = entity_position(hub);
    Vector3 hub_vel = entity_velocity(hub);
    Vector3 hub_spin = entity_angular_velocity(hub);
    double hub_mass = entity_mass(hub);
    bool hub_active = entity_active(hub);
    Rotation hub_rotation = entity_rotation(hub);
    Material hub_material = entity_material(hub);
    Vector3 ball_pos = entity_position(ball);

    unsigned long long cycle = step % 1200ULL;
    double phase = cycle / 600.0;
    if (phase > 1.0) { phase = 2.0 - phase; }
    double tilt = vector_length(hub_rotation.x, hub_rotation.y, 0.0);
    double target_z = 3.4 + 0.35 * phase + 0.02 * tilt;

    double err_x = 0.0 - hub_pos.x;
    double err_y = 0.0 - hub_pos.y;
    double err_z = target_z - hub_pos.z;
    double dist = vector_length(err_x, err_y, err_z);

    g_average = g_average * 0.94 + dist * 0.06;
    double filter_value = g_filter;
    double index = 0.0;
    while (index < iterations)
    {
        filter_value = 0.5 * filter_value + 0.25 * math_sqrt(0.25 + math_abs(filter_value) * 0.01 + dist * 0.05);
        index = index + 1.0;
    }
    g_filter = filter_value;

    double adapt = 1.0 + gain * (g_average / (1.0 + g_average)) + 0.05 * filter_value;
    double mass_factor = 1.0 + 0.03 * hub_mass;
    double boost = 1.0 + 0.6 * smooth_sat(signed_sqrt(dist - 0.15) * 3.0) + 0.2 * delta;
    double force_x = clamp_abs(kp * adapt * mass_factor * err_x - kd * hub_vel.x, maxf) * boost;
    double force_y = clamp_abs(kp * adapt * mass_factor * err_y - kd * hub_vel.y, maxf) * boost;
    double force_z = clamp_abs(kp * adapt * mass_factor * err_z - kd * hub_vel.z, maxf) * boost;
    force_x = clamp_abs(force_x, maxf * 2.0);
    force_y = clamp_abs(force_y, maxf * 2.0);
    force_z = clamp_abs(force_z, maxf * 2.0);

    if (entity_rotatable(hub))
    {
        entity_set_torque(hub, vector3(-torque_gain * hub_spin.x + 0.01 * filter_value,
                                       -torque_gain * hub_spin.y,
                                       -torque_gain * hub_spin.z + 0.02 * smooth_sat(phase)));
    }

    g_react_x = 0.0;
    g_react_y = 0.0;
    g_react_z = 0.0;
    double d1 = satellite_step(sat1, hub_pos.x, hub_pos.y, hub_pos.z, phase, sat_kp, sat_kd, sat_max, radius, gain);
    double d2 = satellite_step(sat2, hub_pos.x, hub_pos.y, hub_pos.z, phase + 0.35, sat_kp, sat_kd, sat_max, radius, gain);
    double d3 = satellite_step(sat3, hub_pos.x, hub_pos.y, hub_pos.z, phase + 0.70, sat_kp, sat_kd, sat_max, radius, gain);
    double d4 = satellite_step(sat4, hub_pos.x, hub_pos.y, hub_pos.z, phase + 1.05, sat_kp, sat_kd, sat_max, radius, gain);
    entity_set_force(hub, vector3(clamp_abs(force_x + g_react_x, maxf * 2.0),
                                  clamp_abs(force_y + g_react_y, maxf * 2.0),
                                  clamp_abs(force_z + g_react_z, maxf * 2.0)));

    Material adjusted = material(hub_material.staticFriction, hub_material.dynamicFriction, hub_material.restitution);
    entity_set_material(hub, adjusted);

    EntityCollection members = scene_entities();
    Entity cursor_entity = collection_next(members);
    double count = 0.0;
    double mass_sum = 0.0;
    double max_speed = 0.0;
    Entity fastest = hub;
    while (entity_exists(cursor_entity))
    {
        count = count + 1.0;
        mass_sum = mass_sum + entity_mass(cursor_entity);
        Vector3 velocity_now = entity_velocity(cursor_entity);
        double speed_now = vector_length(velocity_now.x, velocity_now.y, velocity_now.z);
        if (speed_now > max_speed)
        {
            max_speed = speed_now;
            fastest = cursor_entity;
        }
        cursor_entity = collection_next(members);
    }

    if (max_speed > speed_cap)
    {
        Vector3 fast_velocity = entity_velocity(fastest);
        entity_set_force(fastest, vector3(-0.7 * fast_velocity.x, -0.7 * fast_velocity.y, -0.7 * fast_velocity.z));
        state_set_bool("limiting", true);
    }
    else
    {
        state_set_bool("limiting", false);
    }

    bool touching = entities_touching(hub, sat1);
    if (touching) { g_hits = g_hits + 1.0; }
    state_set_bool("contact", touching);
    state_set_number("hits", g_hits);
    state_set_number("dist", dist);
    state_set_number("filter", g_filter);
    state_set_number("maxspeed", max_speed);
    state_set_number("masssum", mass_sum);
    state_set_number("bodies", count);
    state_set_number("formation", d1 + d2 + d3 + d4);
    state_set_number("ballz", ball_pos.z);
    state_set_number("react", vector_length(g_react_x, g_react_y, g_react_z));

    numbers_push(g_history, dist);
    if (numbers_count(g_history) >= 24U)
    {
        double newest = numbers_pop(g_history);
        state_set_number("newest", newest);
        numbers_destroy(g_history);
        g_history = numbers_create();
    }

    if (step == 2000ULL)
    {
        Entity child = scene_spawn(scene_template("sat1"), vector3(hub_pos.x, hub_pos.y, hub_pos.z + 1.6));
        entity_set_velocity(child, vector3(0.6, 0.0, -0.4));
        entity_set_mass(child, 0.75);
        entity_set_material(child, material(0.2, 0.3, 0.5));
        state_set_entity("child", child);
        state_set_bool("spawned", true);
    }
    if (step == 6000ULL)
    {
        Entity child = state_entity("child");
        if (entity_exists(child)) { entity_destroy(child); }
        state_set_bool("spawned", false);
    }

    state_set_bool("hubactive", hub_active);
    state_set_number("hubmass", hub_mass);
    state_set_number("clock", now);
}
