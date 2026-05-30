/**
* @file        connecting_rod.h
 * @brief       连接杆约束器头文件
 * @details     定义ConnectingRod类，提供刚性杆约束功能。
 *
 * @author      作者姓名 <作者邮箱>
 * @date        2026-04-26
 * @version     1.0.0
 *
 * @copyright   版权信息 (如 Copyright © 2025 公司名. All rights reserved.)
 * @license     GPL v3.0
 *
 * @ingroup     Physics
 * @defgroup    组名 (如果文件定义了一个模块组)
 */
#ifndef EGRET_PHYSICS_CONNECTING_ROD_H
#define EGRET_PHYSICS_CONNECTING_ROD_H

namespace egret {

/**
 * @brief       连接杆约束器类，实现刚性杆约束。
 * @details     ConnectingRod 继承自 ConstraintsBase，提供两个物理实体之间的
 *              刚性距离约束功能，保持两点间的距离恒定。
 *              连接杆约束器用于模拟刚性连接，如机械连杆、刚性支架等。
 *              与连接线不同，连接杆是刚性约束，既限制最大距离也限制最小距离。
 * 
 * @invariant   约束长度恒定
 * @invariant   m_physicalEntities 包含两个物理实体
 * @remark      连接杆是刚性约束，保持距离恒定，不允许拉伸或压缩
 * @see         ConstraintsBase, ConnectingLine, PhysicalEntity
 */
class ConnectingRod {

};

} // egret

#endif //EGRET_PHYSICS_CONNECTING_ROD_H
