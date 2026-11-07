/** @file egret_runtime.c @brief 可信 SDK 实现；仅操作固定 Wasm 内存，不导入宿主能力。 */
#include "../control_abi.h"
#include "egret_control.h"
static EgretExchange exchange;
static EgretU64 nextIdentity = 1;
/** @brief 具名持久状态固定槽。 */
typedef struct
{
    char key[64];
    double number;
    Entity entity;
} State;
static State states[256];
static unsigned stateCount;
/** @brief 固定对象池中的序列。 */
typedef struct
{
    unsigned count, used;
    EgretU64 generation;
    double values[256];
} List;
static List lists[32];
static unsigned iteratorCount, iteratorPositions[32];
static EgretU64 iteratorGeneration;
/** @brief 读取有界字符串相等性。 */
static bool equal(const char *a, const char *b)
{
    for (unsigned i = 0; i < 64; i++)
    {
        if (a[i] != b[i])
            return false;
        if (!a[i])
            return true;
    }
    return false;
}
/** @brief 将错误变为确定性 Wasm trap。 */
static void fail(void)
{
    __builtin_trap();
}
/** @brief 查找或登记具名状态。 */
static State *state(const char *key)
{
    for (unsigned i = 0; i < stateCount; i++)
        if (equal(states[i].key, key))
            return &states[i];
    if (stateCount == 256)
        fail();
    State *s = &states[stateCount++];
    unsigned i = 0;
    for (; i < 63 && key[i]; i++)
        s->key[i] = key[i];
    if (key[i])
        fail();
    s->key[i] = 0;
    return s;
}
/** @brief 查找绑定。 */
static EgretU64 binding(const char *name)
{
    for (unsigned i = 0; i < exchange.bindingCount; i++)
        if (equal(exchange.bindings[i].name, name))
            return exchange.bindings[i].id;
    fail();
    return 0;
}
/** @brief 读取快照或本轮待创建初值，拒绝已销毁实体。 */
static EgretBody body(Entity e)
{
    EgretBody result = {0};
    bool found = false, pending = false;
    for (unsigned i = 0; i < exchange.bodyCount; i++)
        if (exchange.bodies[i].id == e._value)
        {
            result = exchange.bodies[i];
            found = true;
            break;
        }
    for (unsigned i = 0; i < exchange.commandCount; i++)
    {
        EgretCommand c = exchange.commands[i];
        if (c.target != e._value)
            continue;
        if (c.kind == 7)
        {
            for (unsigned j = 0; j < exchange.templateCount; j++)
                if (exchange.templates[j].id == c.source)
                    result = exchange.templates[j];
            result.id = e._value;
            result.position = c.value;
            result.active = 1;
            found = true;
            pending = true;
        }
        if (c.kind == 8)
            found = false;
        if (pending)
        {
            if (c.kind == 0)
                result.position = c.value;
            if (c.kind == 1)
                result.velocity = c.value;
            if (c.kind == 2)
                result.active = c.enabled;
            if (c.kind == 4)
                result.angular = c.value;
            if (c.kind == 5)
                result.mass = c.value.x;
            if (c.kind == 9)
                result.rotation = (EgretRotation){c.value.x, c.value.y, c.value.z, c.scalar};
            if (c.kind == 11)
                result.material = c.value;
        }
    }
    if (!found)
        result.id = 0;
    return result;
}
/** @brief 追加有界动作。 */
static void command(unsigned kind, Entity e, Vector3 v, bool enabled, EgretU64 source)
{
    if(!__builtin_isfinite(v.x)||!__builtin_isfinite(v.y)||!__builtin_isfinite(v.z))
        fail();
    if (kind != 7 && !body(e).id)
        fail();
    if (exchange.commandCount == EGRET_COMMAND_CAPACITY)
        fail();
    exchange.commands[exchange.commandCount++] = (EgretCommand){kind, enabled, e._value, source, {v.x, v.y, v.z}, 0};
}
/** @brief 构造向量。 */
Vector3 vector3(double x, double y, double z)
{
    return (Vector3){x, y, z};
}
/** @brief 构造姿态值。 */
Rotation rotation(double x, double y, double z, double w)
{
    return (Rotation){x, y, z, w};
}
/** @brief 构造材质值。 */
Material material(double s, double d, double r)
{
    return (Material){s, d, r};
}
/** @brief 查找实体。 */
Entity scene_entity(const char *name)
{
    return (Entity){binding(name)};
}
/** @brief 查找模板。 */
EntityTemplate scene_template(const char *name)
{
    return (EntityTemplate){binding(name)};
}
/** @brief 开始集合遍历。 */
EntityCollection scene_entities(void)
{
    if (iteratorCount == 32)
        fail();
    iteratorPositions[iteratorCount] = 0;
    return (EntityCollection){iteratorGeneration * 32 + iteratorCount++};
}
/** @brief 推进集合。 */
Entity collection_next(EntityCollection c)
{
    unsigned index = c._cursor % 32;
    if (c._cursor / 32 != iteratorGeneration || index >= iteratorCount)
        fail();
    unsigned *position = &iteratorPositions[index];
    return (Entity){*position < exchange.bodyCount ? exchange.bodies[(*position)++].id : 0};
}
/** @brief 检查实体有效性。 */
bool entity_exists(Entity e)
{
    return body(e).id != 0;
}
/** @brief 拒绝无效读取。 */
static EgretBody require(Entity e)
{
    EgretBody b = body(e);
    if (!b.id)
        fail();
    return b;
}
/** @brief 读取位置。 */
Vector3 entity_position(Entity e)
{
    EgretVector v = require(e).position;
    return vector3(v.x, v.y, v.z);
}
/** @brief 读取速度。 */
Vector3 entity_velocity(Entity e)
{
    EgretVector v = require(e).velocity;
    return vector3(v.x, v.y, v.z);
}
/** @brief 读取角速度。 */
Vector3 entity_angular_velocity(Entity e)
{
    EgretVector v = require(e).angular;
    return vector3(v.x, v.y, v.z);
}
/** @brief 读取质量。 */
double entity_mass(Entity e)
{
    return require(e).mass;
}
/** @brief 查询转动自由度。 */
bool entity_rotatable(Entity e)
{
    return require(e).rotatable != 0;
}
/** @brief 查询姿态。 */
Rotation entity_rotation(Entity e)
{
    EgretRotation q = require(e).rotation;
    return rotation(q.x, q.y, q.z, q.w);
}
/** @brief 查询材质。 */
Material entity_material(Entity e)
{
    EgretVector v = require(e).material;
    return material(v.x, v.y, v.z);
}
/** @brief 读取活动性。 */
bool entity_active(Entity e)
{
    return require(e).active != 0;
}
/** @brief 查询累计接触。 */
bool entities_touching(Entity a, Entity b)
{
    for (unsigned i = 0; i < exchange.contactCount; i++)
    {
        EgretContact c = exchange.contacts[i];
        if ((c.first == a._value && c.second == b._value) || (c.second == a._value && c.first == b._value))
            return true;
    }
    return false;
}
/** @brief 设置位置。 */
void entity_set_position(Entity e, Vector3 v)
{
    command(0, e, v, true, 0);
}
/** @brief 设置速度。 */
void entity_set_velocity(Entity e, Vector3 v)
{
    command(1, e, v, true, 0);
}
/** @brief 设置活动性。 */
void entity_set_active(Entity e, bool v)
{
    command(2, e, vector3(0, 0, 0), v, 0);
}
/** @brief 设置持久力。 */
void entity_set_force(Entity e, Vector3 v)
{
    command(3, e, v, true, 0);
}
/** @brief 设置角速度。 */
void entity_set_angular_velocity(Entity e, Vector3 v)
{
    if(!require(e).rotatable)fail();
    command(4, e, v, true, 0);
}
/** @brief 设置质量。 */
void entity_set_mass(Entity e, double v)
{
    if(!__builtin_isfinite(v)||v<0||v>1e15)fail();
    command(5, e, vector3(v, 0, 0), true, 0);
}
/** @brief 设置姿态的四个分量。 */
void entity_set_rotation(Entity e, Rotation q)
{
    double norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
    if(!__builtin_isfinite(norm)||norm<1e-24)fail();
    command(9, e, vector3(q.x, q.y, q.z), true, 0);
    exchange.commands[exchange.commandCount - 1].scalar = q.w;
}
/** @brief 设置持久控制力矩。 */
void entity_set_torque(Entity e, Vector3 v)
{
    if(!require(e).rotatable)fail();
    command(10, e, v, true, 0);
}
/** @brief 设置材质系数。 */
void entity_set_material(Entity e, Material v)
{
    if(v.staticFriction<0||v.dynamicFriction<0||v.restitution<0||v.restitution>1)fail();
    command(11, e, vector3(v.staticFriction, v.dynamicFriction, v.restitution), true, 0);
}
/** @brief 登记临时实体并返回不透明身份。 */
Entity scene_spawn(EntityTemplate t, Vector3 p)
{
    if (nextIdentity >= 0x100000000ULL)
        fail();
    Entity e = {exchange.identityPrefix | nextIdentity++};
    command(7, e, p, true, t._value);
    return e;
}
/** @brief 登记永久销毁。 */
void entity_destroy(Entity e)
{
    command(8, e, vector3(0, 0, 0), false, 0);
}
/** @brief 仿真时间。 */
double simulation_time(void)
{
    return exchange.time;
}
/** @brief 物理步长。 */
double simulation_step(void)
{
    return exchange.step;
}
/** @brief 控制调用间隔。 */
double control_delta(void)
{
    return exchange.delta;
}
/** @brief 参数调用必须已由可信准备阶段降级为数值常量。 */
double parameter_number(const char *key)
{
    (void)key;
    fail();
    return 0;
}
/** @brief 物理步序号。 */
unsigned long long simulation_step_index(void)
{
    return exchange.stepIndex;
}
/** @brief 读取持久数值。 */
double state_number(const char *key)
{
    return state(key)->number;
}
/** @brief 写入持久数值。 */
void state_set_number(const char *key, double v)
{
    state(key)->number = v;
}
/** @brief 读取持久布尔值。 */
bool state_bool(const char *key)
{
    return state_number(key) != 0;
}
/** @brief 写入持久布尔值。 */
void state_set_bool(const char *key, bool v)
{
    state_set_number(key, v);
}
/** @brief 读取持久实体值。 */
Entity state_entity(const char *key)
{
    return state(key)->entity;
}
/** @brief 写入持久实体值。 */
void state_set_entity(const char *key, Entity v)
{
    state(key)->entity = v;
}
/** @brief 可信降级后的状态槽查询，不在每步查字符串。 */
double egret_abi_state_number(unsigned slot)
{
    if (slot >= 256)
        fail();
    return states[slot].number;
}
/** @brief 可信降级后的状态槽写入。 */
void egret_abi_state_set_number(unsigned slot, double value)
{
    if (slot >= 256)
        fail();
    states[slot].number = value;
}
/** @brief 可信降级后的布尔读取。 */
bool egret_abi_state_bool(unsigned slot)
{
    return egret_abi_state_number(slot) != 0;
}
/** @brief 可信降级后的布尔写入。 */
void egret_abi_state_set_bool(unsigned slot, bool value)
{
    egret_abi_state_set_number(slot, value);
}
/** @brief 可信降级后的实体读取。 */
Entity egret_abi_state_entity(unsigned slot)
{
    if (slot >= 256)
        fail();
    return states[slot].entity;
}
/** @brief 可信降级后的实体写入。 */
void egret_abi_state_set_entity(unsigned slot, Entity value)
{
    if (slot >= 256)
        fail();
    states[slot].entity = value;
}
/** @brief 校验序列代次。 */
static List *list(NumberList h)
{
    unsigned index = h._value % 32;
    List *l = &lists[index];
    if (!l->used || l->generation != h._value / 32)
        fail();
    return l;
}
/** @brief 申请序列槽。 */
NumberList numbers_create(void)
{
    for (unsigned i = 0; i < 32; i++)
        if (!lists[i].used)
        {
            List *l = &lists[i];
            l->used = 1;
            l->count = 0;
            if (++l->generation >= 0x800000000000000ULL)
                fail();
            return (NumberList){l->generation * 32 + i};
        }
    fail();
    return (NumberList){0};
}
/** @brief 释放序列槽。 */
void numbers_destroy(NumberList h)
{
    list(h)->used = 0;
}
/** @brief 追加元素。 */
void numbers_push(NumberList h, double v)
{
    List *l = list(h);
    if (l->count == 256)
        fail();
    l->values[l->count++] = v;
}
/** @brief 弹出元素。 */
double numbers_pop(NumberList h)
{
    List *l = list(h);
    if (!l->count)
        fail();
    return l->values[--l->count];
}
/** @brief 读取元素数量。 */
unsigned numbers_count(NumberList h)
{
    return list(h)->count;
}
/** @brief 平方根。 */
double math_sqrt(double v)
{
    if (v < 0)
        fail();
    return __builtin_sqrt(v);
}
/** @brief 绝对值。 */
double math_abs(double v)
{
    return __builtin_fabs(v);
}
/** @brief 导出交换区偏移，用户不可调用。 */
unsigned egret_exchange(void)
{
    return (unsigned)&exchange;
}
/** @brief 版本校验后调用用户入口。 */
void egret_step(void)
{
    if (exchange.version != EGRET_ABI_VERSION || exchange.bytes != sizeof(exchange))
        fail();
    iteratorCount = 0;
    if (++iteratorGeneration >= 0x800000000000000ULL)
        fail();
    on_step();
}
/** @brief 支持编译器生成的内存复制，不导入系统库。 */
void *memcpy(void *dst, const void *src, unsigned n)
{
    char *d = dst;
    const char *s = src;
    for (unsigned i = 0; i < n; i++)
        d[i] = s[i];
    return dst;
}
/** @brief 支持编译器生成的清零，不导入系统库。 */
void *memset(void *dst, int v, unsigned n)
{
    char *d = dst;
    for (unsigned i = 0; i < n; i++)
        d[i] = (char)v;
    return dst;
}
