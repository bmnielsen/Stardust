#pragma once

#include "Play.h"
#include "Squads/DefendBaseSquad.h"

class DefendBase : public PlayItem
{
public:
    Base *baseLoc;
    int enemyValue;

    explicit DefendBase(Base *baseLoc, int enemyValue);

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    static int enemyAirThreatCannons(Base *baseToDefend);

protected:
    std::shared_ptr<DefendBaseSquad> squad;

    BWAPI::TilePosition pylonLocation;
    std::deque<BWAPI::TilePosition> cannonLocations;

    OwnedUnit pylon;
    std::vector<OwnedUnit> cannons;

    int desiredCannons();
};
