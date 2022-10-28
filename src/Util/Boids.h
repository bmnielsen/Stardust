#pragma once

#include "Common.h"

#include "Unit.h"

namespace Boids
{
    BWAPI::Position AvoidNoGoArea(const UnitImpl *unit);

    void AddSeparation(const UnitImpl *unit, const Unit &other, double detectionLimitFactor, double weight, int &separationX, int &separationY);

    void AddSeparation(const UnitImpl *unit, const Unit &other, int detectionLimit, double weight, int &separationX, int &separationY);

    BWAPI::Position ComputePosition(const UnitImpl *unit,
                                    const std::vector<int> &x,
                                    const std::vector<int> &y,
                                    int scale,
                                    int minDist = 16,
                                    bool collision = false);
}