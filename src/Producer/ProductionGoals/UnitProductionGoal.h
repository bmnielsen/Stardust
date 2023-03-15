#pragma once

#include <utility>
#include "Common.h"
#include "BuildingPlacement.h"
#include "ProductionLocation.h"

class UnitProductionGoal
{
public:
    // Constructor for normal units or buildings
    explicit UnitProductionGoal(std::string requester,
                                BWAPI::UnitType type,
                                int count = -1,
                                int producerLimit = -1,
                                ProductionLocation location = std::monostate())
            : requester(std::move(requester))
            , type(type)
            , count(count)
            , producerLimit(producerLimit)
            , location(std::move(location))
            , reservedBuilder(nullptr) {}

    // Constructor for buildings at a specific build location
    UnitProductionGoal(std::string requester,
                       BWAPI::UnitType type,
                       BuildingPlacement::BuildLocation location,
                       MyUnit reservedBuilder = nullptr)
            : requester(std::move(requester))
            , type(type)
            , count(1)
            , producerLimit(1)
            , location(location)
            , reservedBuilder(std::move(reservedBuilder)) {}

    std::string requester;

    // The unit type
    [[nodiscard]] BWAPI::UnitType unitType() const { return type; }

    // Maximum cap of how many producers of the item we should create
    // May be -1 if we do not want to limit it
    [[nodiscard]] int getProducerLimit() const { return producerLimit; }

    // The number of items that should be produced
    // May be -1 if we want constant production
    [[nodiscard]] int countToProduce() const { return count; };

    // The location to produce the item
    // Will either be a neighbourhood for the building placer, or, if the item is a building, potentially a specific tile position
    [[nodiscard]] ProductionLocation getLocation() const { return location; };

    // For buildings with a fixed tile location, gets a builder we already reserved to build it.
    // This is useful for plays that "own" a worker and want to build something with it.
    [[nodiscard]] MyUnit getReservedBuilder() const { return reservedBuilder; };

    // Sets the number of items that should be produced
    // Used occasionally if something needs to reprioritize the queue
    void setCountToProduce(int newCount) { count = newCount; }

    friend std::ostream &operator<<(std::ostream &os, const UnitProductionGoal &goal)
    {
        os << goal.count << "x" << goal.type << " (" << goal.requester << ")";
        return os;
    }

private:
    BWAPI::UnitType type;
    int count;
    int producerLimit;
    ProductionLocation location;
    MyUnit reservedBuilder;
};
