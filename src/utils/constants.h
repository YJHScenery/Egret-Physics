/**
 * @file        constants.h
 * @brief       物理引擎常量定义文件，提供圆周率、万有引力常数等基础物理常量。
 * @details     定义物理引擎中使用的国际单位制常量，包括圆周率、重力加速度、
 *              万有引力常数等，供全系统共享使用。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-23
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Utils
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_CONSTANTS_H
#define EGRET_PHYSICS_CONSTANTS_H
#include <numbers>

namespace egret
{
    /// 圆周率 π
    /// 圆周长与直径之比
    constexpr double PI = std::numbers::pi;

    /// 万有引力常数 G
    /// 单位: m^3·kg^-1·s^-2
    /// 描述: 决定两个物体之间引力大小的基本常数
    constexpr double G = 6.67430e-11; // 精度: ±0.00015e-11

    /// 标准重力加速度 g
    /// 单位: m/s^2
    /// 描述: 地球表面海平面处的平均重力加速度
    constexpr double STANDARD_GRAVITY = 9.80665;

    /// 光速 c (虽然主要属于相对论，但在经典力学某些领域也会使用)
    /// 单位: m/s
    /// 描述: 真空中电磁波的传播速度
    constexpr double SPEED_OF_LIGHT = 299792458.0;

    /// 标准大气压 atm
    /// 单位: Pa (N/m^2)
    /// 描述: 海平面上的标准大气压力
    constexpr double STANDARD_ATMOSPHERE = 101325.0;

    /// 地球质量 M_earth
    /// 单位: kg
    /// 描述: 地球的总质量
    constexpr double EARTH_MASS = 5.9722e24;

    /// 地球平均半径 R_earth
    /// 单位: m
    /// 描述: 地球的平均半径
    constexpr double EARTH_MEAN_RADIUS = 6371000.0;

    /// 月球质量 M_moon
    /// 单位: kg
    /// 描述: 月球的总质量
    constexpr double MOON_MASS = 7.342e22;

    /// 月球平均轨道半径 R_moon_orbit
    /// 单位: m
    /// 描述: 月球绕地球公转的平均距离
    constexpr double MOON_ORBIT_RADIUS = 3.844e8;

    /// 太阳质量 M_sun
    /// 单位: kg
    /// 描述: 太阳的总质量
    constexpr double SUN_MASS = 1.9885e30;

    /// 天文单位 AU
    /// 单位: m
    /// 描述: 地球到太阳的平均距离
    constexpr double ASTRONOMICAL_UNIT = 1.49597870700e11;

    /// 1标准重力加速度下的力 (1g 力)
    /// 单位: N
    /// 描述: 质量为1 kg的物体在标准重力下的重量
    constexpr double G_FORCE_1KG = STANDARD_GRAVITY; // = 9.80665 N
}

#endif // EGRET_PHYSICS_CONSTANTS_H
