#pragma once

#include "Common.h"

class UnitProductionGoal
{
public:
    UnitProductionGoal(BWAPI::UnitType type, int count = -1, int producerLimit = -1)
        : type(type)
        , count(count)
        , producerLimit(producerLimit) {}

    // The unit type
    BWAPI::UnitType unitType() { return type; }

    // Maximum cap of how many producers of the item we should create
    // May be -1 if we do not want to limit it
    int getProducerLimit() { return producerLimit; }

    // The number of items that should be produced
    // May be -1 if we want constant production
    int countToProduce() { return count; };

    friend std::ostream& operator<<(std::ostream& os, const UnitProductionGoal& goal)
    {
        os << goal.count << "x" << goal.type;
        return os;
    }

private:
    BWAPI::UnitType type;
    int             count;
    int             producerLimit;
};
