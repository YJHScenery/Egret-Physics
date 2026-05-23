//
// Created by jehor on 2026/4/25.
//

#ifndef EGRET_PHYSICS_CONNECTING_LINE_H
#define EGRET_PHYSICS_CONNECTING_LINE_H
#include "constraints_base.h"
#include <array>
#include "Eigen/Dense"

namespace egret {

class ConnectingLine: public ConstraintsBase {
public:
    ConnectingLine();

    ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd);

    ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd, const std::initializer_list<Eigen::Vector3d>& turningPositions);

    ~ConnectingLine() override = default;

    void addPathTurningPoint(size_t index, const Eigen::Vector3d& pos);

    void removePathTurningPoint(size_t index);

    void changePathTurningPoint(size_t index, const Eigen::Vector3d& newPos);

    [[nodiscard]] size_t getPathPosSize() const;

    [[nodiscard]] ConstraintType getType() const override;

    [[nodiscard]] std::vector<PhysicalEntity*> getConstrainedEntities() override;

    [[nodiscard]] const std::vector<PhysicalEntity*>& getConstrainedEntities() const override;

    void applyVelocityConstraint(double dt) override;

    void applyPositionConstraint(double dt) override;

    [[nodiscard]] double computeConstraintError() const override;

private:
    std::vector<PhysicalEntity*> m_physicalEntities{};

    // 始末会由 Physical Entity 的 Position 给出，此处不会储存此二者。
    std::vector<Eigen::Vector3d> m_pathPositions;

    double m_length{};
};

    // 单摆：将第一个 Physical Entity 定义为质量为 0 的质点。这样它不会受到任何力的作用。
    class SimplePendulum: public ConstraintsBase
    {
    public:
        SimplePendulum();
    };
} // egret

#endif //EGRET_PHYSICS_CONNECTING_LINE_H
