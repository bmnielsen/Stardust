#pragma once

#include "Common.h"

#include "Unit.h"

namespace Boids
{
    void AddSeparation(const UnitImpl *unit, const Unit &other, double detectionLimitFactor, double weight, int &separationX, int &separationY);
}