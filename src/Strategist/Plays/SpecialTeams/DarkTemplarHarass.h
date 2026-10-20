#pragma once

#include "Play.h"

class DarkTemplarHarass : public PlayItem
{
public:
    DarkTemplarHarass() : PlayItem("DarkTemplarHarass") {}

    void update() override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override
    {
        for (const auto &gameUnit : units)
        {
            movableUnitCallback(gameUnit);
        }
    }

    void addUnit(const OwnedUnit &gameUnit) override
    {
        units.insert(gameUnit);
        PlayItem::addUnit(gameUnit);
    }

    void removeUnit(const OwnedUnit &gameUnit) override
    {
        units.erase(gameUnit);
        PlayItem::removeUnit(gameUnit);
    }

protected:
    std::set<OwnedUnit> units;
};
