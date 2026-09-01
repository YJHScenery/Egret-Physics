/**
 * @file        collision.h
 * @brief       碰撞检测聚合头文件
 * @details     聚合 GJK、EPA 与 CCD 三个碰撞检测头文件，替代原先的 collision 模块。
 *              使用方可通过包含本头文件一次性获得全部碰撞检测类型。
 *              C++20 模块系统从 Visual Studio 2022 移植到 Visual Studio 2026 后无法正常工作，故回退为头文件版本。
 *
 * @author      jehor
 * @date        2026-08-31
 * @version     1.0.0
 *
 * @copyright   Copyright © 2025 公司名. All rights reserved.
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 */

#ifndef EGRET_PHYSICS_COLLISION_H
#define EGRET_PHYSICS_COLLISION_H

#include "gilbert_johnson_keerthi.h"
#include "expanding_polytope_algorithm.h"
#include "continuous_collision_detector.h"

#endif // EGRET_PHYSICS_COLLISION_H
