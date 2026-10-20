#pragma once

#include "Play.h"
#include "MyWorker.h"

class AntiCannonRush : public PlayItem
{
public:
    /* Whether the enemy's current strategy is considered "safe" from a cannon rush perspective */
    bool safeEnemyStrategyDetermined;

    AntiCannonRush();

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

protected:
    OwnedWorker scout;
    std::vector<BWAPI::TilePosition> tilesToScout;

    bool builtPylon;
    bool builtCannon;

    std::set<OwnedWorker> workerAttackers;
    std::map<Unit, std::set<OwnedWorker>> cannonsAndAttackers;

    BWAPI::TilePosition getNextScoutTile();
};
