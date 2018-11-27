#pragma once

#include "Common.h"

class ProductionGoal
{
public:
    // The type of unit to produce for this production goal
    virtual BWAPI::UnitType unitType() = 0;

    // Whether or not the production goal is currently fulfilled
    virtual bool isFulfilled() = 0;

    // The number of items that should be produced
    // May be -1 if we want constant production
    virtual int countToProduce() = 0;

    // Maximum cap of how many producers of the item we should create
    // May be -1 if we do not want to limit it
    virtual int producerLimit() = 0;
};
