#pragma once

#include "Play.h"

class DarkTemplarHarass : public Play
{
public:
    DarkTemplarHarass(): Play("DarkTemplarHarass") {}

    void update() override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override
    {
        for (const auto &unit : units)
        {
            movableUnitCallback(unit);
        }
    }

    void addUnit(const MyUnit &unit) override
    {
        units.insert(unit);
        Play::addUnit(unit);
    }

    void removeUnit(const MyUnit &unit) override
    {
        units.erase(unit);
        Play::removeUnit(unit);
    }

protected:
    std::set<MyUnit> units;
};
