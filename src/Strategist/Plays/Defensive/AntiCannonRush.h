#pragma once

#include "Play.h"

class AntiCannonRush : public Play
{
public:
    AntiCannonRush();

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

protected:
    MyUnit scout;
    std::vector<BWAPI::TilePosition> tilesToScout;

    bool builtPylon;
    bool builtCannon;

    std::set<MyUnit> workerAttackers;
    std::map<Unit, std::set<MyUnit>> cannonsAndAttackers;

    BWAPI::TilePosition getNextScoutTile();
};
