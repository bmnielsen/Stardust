#pragma once

#include <utility>
#include "Common.h"
#include "BuildingPlacement.h"
#include "ProductionLocation.h"

class UnitProductionGoal
{
public:
    // Constructor for normal units or buildings
    explicit UnitProductionGoal(BWAPI::UnitType type,
                                int count = -1,
                                int producerLimit = -1,
                                ProductionLocation location = std::monostate())
            : type(type)
            , count(count)
            , producerLimit(producerLimit)
            , location(std::move(location)) {}

    // Constructor for buildings at a specific build location
    UnitProductionGoal(BWAPI::UnitType type, BuildingPlacement::BuildLocation location)
            : type(type)
            , count(1)
            , producerLimit(1)
            , location(location) {}

    // The unit type
    BWAPI::UnitType unitType() { return type; }

    // Maximum cap of how many producers of the item we should create
    // May be -1 if we do not want to limit it
    int getProducerLimit() { return producerLimit; }

    // The number of items that should be produced
    // May be -1 if we want constant production
    int countToProduce() { return count; };

    // The location to produce the item
    // Will either be a neighbourhood for the building placer, or, if the item is a building, potentially a specific tile position
    ProductionLocation getLocation() { return location; };

    friend std::ostream &operator<<(std::ostream &os, const UnitProductionGoal &goal)
    {
        os << goal.count << "x" << goal.type;
        return os;
    }

private:
    BWAPI::UnitType type;
    int count;
    int producerLimit;
    ProductionLocation location;
};
