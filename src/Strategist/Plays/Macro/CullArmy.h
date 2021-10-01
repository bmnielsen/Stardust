#pragma once

#include "Play.h"

// Culls some of our army if we need supply room for something else
class CullArmy : public Play
{
public:
    int supplyNeeded;

    explicit CullArmy(int supplyNeeded) : Play("CullArmy"), supplyNeeded(supplyNeeded) {}

    void update() override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

    void addUnit(const MyUnit &unit) override;

    void removeUnit(const MyUnit &unit) override;

private:
    std::vector<MyUnit> units;
};
