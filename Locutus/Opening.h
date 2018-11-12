#pragma once

#include "Common.h"
#include "ProductionGoal.h"

class Opening
{
public:
    // The prioritized goals of the opening
    virtual std::vector<ProductionGoal> & goals() = 0;
};
