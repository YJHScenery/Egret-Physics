/** @file control_abi.h @brief 宿主与 Wasm32 共享的定长无指针 ABI，版本一。 */
#pragma once
#ifdef __cplusplus
#include <cstdint>
using EgretU64 = std::uint64_t;
using EgretU32 = std::uint32_t;
#else
typedef unsigned long long EgretU64;
typedef unsigned EgretU32;
#endif
#define EGRET_ABI_VERSION 1
#define EGRET_BODY_CAPACITY 4096
#define EGRET_COMMAND_CAPACITY 1024
#define EGRET_BINDING_CAPACITY 256
#define EGRET_CONTACT_CAPACITY 8192
/** @brief 固定宽度向量。 */
typedef struct
{
    double x, y, z;
} EgretVector;
/** @brief 以 x、y、z、w 顺序编码的姿态四元数。 */
typedef struct
{
    double x, y, z, w;
} EgretRotation;
/** @brief 权威快照副本；有效性仍由宿主重新检查。 */
typedef struct
{
    EgretU64 id;
    EgretVector position, velocity, angular;
    double mass;
    EgretU32 active, rotatable;
    EgretRotation rotation;
    EgretVector material;
} EgretBody;
/** @brief 具名稳定身份绑定。 */
typedef struct
{
    EgretU64 id;
    char name[64];
} EgretBinding;
/** @brief 接触对。 */
typedef struct
{
    EgretU64 first, second;
} EgretContact;
/** @brief 输出动作，source 仅用于模板创建。 */
typedef struct
{
    EgretU32 kind, enabled;
    EgretU64 target, source;
    EgretVector value;
    double scalar;
} EgretCommand;
/** @brief 单次回调交换区，宿主每次刷新全部输入并清空输出数量。 */
typedef struct
{
    EgretU32 version, bytes, bodyCount, bindingCount, contactCount, commandCount, error, templateCount;
    double time, step, delta;
    EgretU64 stepIndex, identityPrefix;
    EgretBinding bindings[EGRET_BINDING_CAPACITY];
    EgretBody bodies[EGRET_BODY_CAPACITY];
    EgretContact contacts[EGRET_CONTACT_CAPACITY];
    EgretCommand commands[EGRET_COMMAND_CAPACITY];
    EgretBody templates[EGRET_BODY_CAPACITY];
} EgretExchange;
#ifdef __cplusplus
static_assert(sizeof(EgretBody) == 152 && sizeof(EgretCommand) == 56);
static_assert(sizeof(EgretExchange) == 1452104);
#endif
