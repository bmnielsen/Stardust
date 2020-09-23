#pragma once

#include "Play.h"

class DarkTemplarHarass : public Play
{
public:
    DarkTemplarHarass(): Play("DarkTemplarHarass") {}

    void update() override;

    void disband(const std::function<void(const MyUnit&)> &removedUnitCallback,
                 const std::function<void(const MyUnit&)> &movableUnitCallback) override
    {
        for (const auto &unit : units)
        {
            movableUnitCallback(unit);
        }
    }

    void addUnit(MyUnit unit) override
    {
        units.insert(unit);
    }

    void removeUnit(MyUnit unit) override
    {
        units.erase(unit);
    }

protected:
    std::set<MyUnit> units;
};
