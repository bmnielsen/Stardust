#pragma once

#include <utility>
#include "Common.h"
#include "BuildingPlacement.h"
#include "ProductionLocation.h"

class UnitProductionGoal
{
public:
    // Constructors for normal units or buildings
    explicit UnitProductionGoal(std::string requester,
                                BWAPI::UnitType type,
                                int count = -1,
                                int producerLimit = -1,
                                ProductionLocation location = std::monostate(),
                                int frame = 0)
            : requester(std::move(requester))
            , type(type)
            , count(count)
            , producerLimit(producerLimit)
            , location(std::move(location))
            , reservedBuilder(nullptr)
            , frame(frame) {}

    explicit UnitProductionGoal(std::string requester,
                                BWAPI::UnitType type,
                                int count,
                                int producerLimit,
                                int frame)
            : requester(std::move(requester))
            , type(type)
            , count(count)
            , producerLimit(producerLimit)
            , location(std::monostate())
            , reservedBuilder(nullptr)
            , frame(frame) {}

    // Constructors for buildings at a specific build location
    UnitProductionGoal(std::string requester,
                       BWAPI::UnitType type,
                       BuildingPlacement::BuildLocation location,
                       MyUnit reservedBuilder = nullptr,
                       int frame = 0)
            : requester(std::move(requester))
            , type(type)
            , count(1)
            , producerLimit(1)
            , location(location)
            , reservedBuilder(std::move(reservedBuilder))
            , frame(frame) {}

    UnitProductionGoal(std::string requester,
                       BWAPI::UnitType type,
                       BuildingPlacement::BuildLocation location,
                       int frame)
            : requester(std::move(requester))
            , type(type)
            , count(1)
            , producerLimit(1)
            , location(location)
            , reservedBuilder(nullptr)
            , frame(frame) {}

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

    // The frame when the first item should be produced
    [[nodiscard]] int getFrame() const { return frame; };

    // Sets the number of items that should be produced
    // Used occasionally if something needs to reprioritize the queue
    void setCountToProduce(int newCount) { count = newCount; }

    friend std::ostream &operator<<(std::ostream &os, const UnitProductionGoal &goal)
    {
        if (goal.type.isBuilding())
        {
            os << goal.type;
            if (auto *buildLocation = std::get_if<BuildingPlacement::BuildLocation>(&goal.location))
            {
                os << "@" << buildLocation->location.tile;
            }
            else
            {
                os << "@UNK";
            }
        }
        else
        {
            os << goal.count << "x" << goal.type;
        }
        if (goal.frame > 0)
        {
            os << "%" << goal.frame;
        }
        os << " (" << goal.requester << ")";
        return os;
    }

private:
    BWAPI::UnitType type;
    int count;
    int producerLimit;
    ProductionLocation location;
    MyUnit reservedBuilder;
    int frame;
};
