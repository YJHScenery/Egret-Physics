/** @file egret_control.h @brief 场景控制领域接口；长度单位米，时间单位秒，力单位牛顿。 */
#pragma once
#ifndef __cplusplus
typedef _Bool bool;
#define true 1
#define false 0
#endif
/** @brief 不透明实体值，禁止用户访问内部字段或转换为整数。 */
typedef struct
{
    unsigned long long _value;
} Entity;
/** @brief 初始场景物体的不可变克隆模板。 */
typedef struct
{
    unsigned long long _value;
} EntityTemplate;
/** @brief 三维数值向量，允许访问 x、y、z。 */
typedef struct
{
    double x, y, z;
} Vector3;
/** @brief 姿态四元数，设置时由宿主归一化，禁止零四元数。 */
typedef struct
{
    double x, y, z, w;
} Rotation;
/** @brief 非负摩擦系数与零至一恢复系数。 */
typedef struct
{
    double staticFriction, dynamicFriction, restitution;
} Material;
/** @brief 同轮快照实体迭代器，不包含本轮创建的实体。 */
typedef struct
{
    unsigned long long _cursor;
} EntityCollection;
/** @brief 固定资源内的数值序列值，容量耗尽使本次求解失败。 */
typedef struct
{
    unsigned long long _value;
} NumberList;
/** @brief 构造向量。 */
Vector3 vector3(double x, double y, double z);
/** @brief 构造四元数值。 */
Rotation rotation(double x, double y, double z, double w);
/** @brief 构造材质系数值。 */
Material material(double staticFriction, double dynamicFriction, double restitution);
/** @brief 按持久别名读取实体，实体已销毁时保留失效句柄。 */
Entity scene_entity(const char *alias);
/** @brief 获取初始化时保存的物体克隆模板。 */
EntityTemplate scene_template(const char *alias);
/** @brief 获取同轮实体集合。 */
EntityCollection scene_entities(void);
/** @brief 返回下一个实体，结束时返回不存在的实体值。 */
Entity collection_next(EntityCollection collection);
/** @brief 检查快照实体或本批次创建意图是否存在。 */
bool entity_exists(Entity entity);
/** @brief 读取同轮位置；新实体读取本批次初值。 */
Vector3 entity_position(Entity entity);
/** @brief 读取同轮速度。 */
Vector3 entity_velocity(Entity entity);
/** @brief 读取同轮角速度。 */
Vector3 entity_angular_velocity(Entity entity);
/** @brief 读取同轮质量。 */
double entity_mass(Entity entity);
/** @brief 查询物理转动自由度。 */
bool entity_rotatable(Entity entity);
/** @brief 读取姿态。 */
Rotation entity_rotation(Entity entity);
/** @brief 读取材质系数。 */
Material entity_material(Entity entity);
/** @brief 读取活动状态。 */
bool entity_active(Entity entity);
/** @brief 查询自上次回调后累计的接触对。 */
bool entities_touching(Entity first, Entity second);
/** @brief 设置位置，在整批验证后提交。 */
void entity_set_position(Entity entity, Vector3 value);
/** @brief 设置速度，在整批验证后提交。 */
void entity_set_velocity(Entity entity, Vector3 value);
/** @brief 替换本控制层持久力；零向量取消。 */
void entity_set_force(Entity entity, Vector3 value);
/** @brief 设置可恢复的活动开关。 */
void entity_set_active(Entity entity, bool value);
/** @brief 设置角速度。 */
void entity_set_angular_velocity(Entity entity, Vector3 value);
/** @brief 设置非负质量，并由原生接口维护惯量。 */
void entity_set_mass(Entity entity, double value);
/** @brief 设置姿态，完整批次验证后归一化。 */
void entity_set_rotation(Entity entity, Rotation value);
/** @brief 替换控制层持久力矩，零向量取消；要求物体具有转动自由度。 */
void entity_set_torque(Entity entity, Vector3 value);
/** @brief 设置静摩擦、动摩擦与恢复系数。 */
void entity_set_material(Entity entity, Material value);
/** @brief 从模板创建实体，支持同回调继续配置；下一回调参加枚举。 */
Entity scene_spawn(EntityTemplate model, Vector3 position);
/** @brief 永久销毁；存在关联约束或共享力源时拒绝并终止求解。 */
void entity_destroy(Entity entity);
/** @brief 当前仿真时间。 */
double simulation_time(void);
/** @brief 物理步长。 */
double simulation_step(void);
/** @brief 距离上一次控制调用的仿真时间，第一次为零。 */
double control_delta(void);
/** @brief 读取具名有限数值参数；准备时绑定，参数改变会重新编译。 */
double parameter_number(const char *key);
/** @brief 当前物理步序号。 */
unsigned long long simulation_step_index(void);
/** @brief 读取具名持久数值，初始为零。 */
double state_number(const char *key);
/** @brief 更新具名持久数值。 */
void state_set_number(const char *key, double value);
/** @brief 读取具名布尔状态，初始为假。 */
bool state_bool(const char *key);
/** @brief 更新具名布尔状态。 */
void state_set_bool(const char *key, bool value);
/** @brief 读取具名实体状态，初始为无效句柄。 */
Entity state_entity(const char *key);
/** @brief 保存实体句柄，包含本轮新建实体。 */
void state_set_entity(const char *key, Entity value);
/** @brief 创建有界数值序列，可保存在全局变量跨步使用。 */
NumberList numbers_create(void);
/** @brief 释放数值序列。 */
void numbers_destroy(NumberList list);
/** @brief 向序列尾追加数值。 */
void numbers_push(NumberList list, double value);
/** @brief 从序列尾取出数值，空序列报错。 */
double numbers_pop(NumberList list);
/** @brief 获取序列长度。 */
unsigned numbers_count(NumberList list);
/** @brief 平方根，负输入使求解失败。 */
double math_sqrt(double value);
/** @brief 绝对值。 */
double math_abs(double value);
/** @brief 用户定义的控制回调。 */
void on_step(void);
