//
// Created by jehor on 2026/4/25.
//

#include "connecting_line.h"

namespace egret
{
    ConnectingLine::ConnectingLine(): ConstraintsBase(generateID(ConstraintType::ConnectingLine))
    {
    }

    ConnectingLine::ConnectingLine(double length, PhysicalEntity*, PhysicalEntity*)
    {
    }
} // egret