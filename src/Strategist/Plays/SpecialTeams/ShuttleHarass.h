#pragma once

#include "Play.h"

class ShuttleHarass : public Play
{
public:
    ShuttleHarass() : Play("ShuttleHarass") {}

    void update() override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override
    {
        for (const auto &shuttle : shuttles)
        {
            movableUnitCallback(shuttle);
        }
        for (const auto &unit : cargo)
        {
            movableUnitCallback(unit);
        }
    }

    void addUnit(const MyUnit &unit) override
    {
        if (unit->type == BWAPI::UnitTypes::Protoss_Shuttle)
        {
            shuttles.insert(unit);
        }
        else
        {
            cargo.insert(unit);
        }
        Play::addUnit(unit);
    }

    void removeUnit(const MyUnit &unit) override
    {
        if (unit->type == BWAPI::UnitTypes::Protoss_Shuttle)
        {
            shuttles.erase(unit);
        }
        else
        {
            cargo.erase(unit);
        }
        Play::removeUnit(unit);
    }

protected:
    std::set<MyUnit> shuttles;
    std::set<MyUnit> cargo;
};
