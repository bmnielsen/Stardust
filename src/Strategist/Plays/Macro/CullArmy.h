#pragma once

#include "Play.h"

/* Culls some of our army if we need supply room for something else */
class CullArmy : public PlayItem
{
public:
    int supplyNeeded;

    explicit CullArmy(int supplyNeeded) : PlayItem("CullArmy"), supplyNeeded(supplyNeeded) {}

    void update() override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    void addUnit(const OwnedUnit &gameUnit) override;

    void removeUnit(const OwnedUnit &gameUnit) override;

private:
    std::vector<OwnedUnit> units;
};
