//
// Created by jehor on 2026/4/25.
//

#include "connecting_line.h"
#include "physical_entity.h"

namespace egret
{
    // static_assert(sizeof(PhysicalEntity) > 0, "PhysicalEntity must be complete");

    ConnectingLine::ConnectingLine() : ConstraintsBase(generateID(ConstraintType::ConnectingLine))
    {
    }

    ConnectingLine::ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd) :
        m_length(length)
    {
        m_physicalEntities.reserve(2);
        m_physicalEntities.push_back(entityStart);
        m_physicalEntities.push_back(entityEnd);
    }

    ConnectingLine::ConnectingLine(double length, PhysicalEntity* entityStart, PhysicalEntity* entityEnd,
                                   const std::initializer_list<Eigen::Vector3d>& turningPositions) : ConnectingLine(
        length, entityStart, entityEnd)
    {
        m_pathPositions = turningPositions;
    }

    void ConnectingLine::addPathTurningPoint(size_t index, const Eigen::Vector3d& pos)
    {
        if (index >= m_pathPositions.size()) {
            return;
        }
        m_pathPositions.insert(m_pathPositions.begin() + static_cast<long long>(index), pos);
    }

    void ConnectingLine::removePathTurningPoint(size_t index)
    {
        if (index >= m_pathPositions.size()) {
            return;
        }
        m_pathPositions.erase(m_pathPositions.begin() + static_cast<long long>(index));
    }

    void ConnectingLine::changePathTurningPoint(size_t index, const Eigen::Vector3d& newPos)
    {
        if (index >= m_pathPositions.size()) {
            return;
        }
        m_pathPositions[index] = newPos;
    }

    size_t ConnectingLine::getPathPosSize() const
    {
        return m_pathPositions.size();
    }

    ConstraintType ConnectingLine::getType() const
    {
        return ConstraintType::ConnectingLine;
    }

    std::vector<PhysicalEntity*> ConnectingLine::getConstrainedEntities()
    {
        return m_physicalEntities;
    }

    const std::vector<PhysicalEntity*>& ConnectingLine::getConstrainedEntities() const
    {
        return m_physicalEntities;
    }

    void ConnectingLine::applyVelocityConstraint(double dt)
    {
    }

    void ConnectingLine::applyPositionConstraint(double dt)
    {
    }

    double ConnectingLine::computeConstraintError() const
    {
        return 0.0;
    }
} // egret
