/**
* @file        connecting_line.h
 * @brief       连接线约束器头文件
 * @details     定义ConnectingLine类，提供两点间的距离约束功能，
 *              支持设置约束长度和路径点。
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
#ifndef EGRET_PHYSICS_CONNECTING_LINE_H
#define EGRET_PHYSICS_CONNECTING_LINE_H
#include "constraints_base.h"
#include <array>
#include "Eigen/Dense"

namespace egret
{
    /**
     * @brief       连接线约束器类，实现两点间的距离约束。
     * @details     ConnectingLine 继承自 ConstraintsBase，提供两个物理实体之间的
     *              距离约束功能，支持设置约束长度和路径点。
     *              连接线约束器用于模拟绳子、链条等只能被拉伸而不能被压缩的约束。
     *              支持路径点（turningPositions）用于处理绳子遇到阻碍物的情况。
     * 
     * @invariant   m_length > 0，约束长度必须为正值
     * @invariant   m_physicalEntities 包含两个物理实体（起点和终点）
     * @invariant   m_pathPositions 存储路径转折点（不含始末点）
     * @remark      连接线是柔性约束，只能限制最大距离，不能限制最小距离
     * @see         ConstraintsBase, SimplePendulum, PhysicalEntity
     */
    class ConnectingLine : public ConstraintsBase
    {
    public:
        ConnectingLine();

        ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd);

        ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd,
                       const std::initializer_list<Eigen::Vector3d>& turningPositions);

        ~ConnectingLine() override = default;

        void addPathTurningPoint(size_t index, const Eigen::Vector3d& pos);

        void removePathTurningPoint(size_t index);

        void changePathTurningPoint(size_t index, const Eigen::Vector3d& newPos);

        [[nodiscard]] size_t getPathPosSize() const;

        [[nodiscard]] ConstraintType getType() const override;

        // [[nodiscard]] std::vector<PhysicalEntity*> getConstrainedEntities() override;
        //
        // [[nodiscard]] const std::vector<PhysicalEntity*>& getConstrainedEntities() const override;

        void applyVelocityConstraint(double dt) override;

        void applyPositionConstraint(double dt) override;

        [[nodiscard]] double computeConstraintError() const override;

        void applyAngularVelocityConstraint(double dt) override;

    protected:
        explicit ConnectingLine(std::uint64_t id);




        // 始末会由 Physical Entity 的 Position 给出，此处不会储存此二者。
        std::vector<Eigen::Vector3d> m_pathPositions;

        double m_length{};
    };
} // egret

#endif //EGRET_PHYSICS_CONNECTING_LINE_H
