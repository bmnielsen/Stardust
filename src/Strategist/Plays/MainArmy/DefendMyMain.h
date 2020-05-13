#pragma once

#include "MainArmyPlay.h"
#include "Squads/EarlyGameDefendMainBaseSquad.h"

class DefendMyMain : public MainArmyPlay
{
public:
    BWAPI::UnitType emergencyProduction;

    explicit DefendMyMain();

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void removeUnit(MyUnit unit) override;

    void disband(const std::function<void(const MyUnit&)> &removedUnitCallback,
                 const std::function<void(const MyUnit&)> &movableUnitCallback) override;

private:
    std::shared_ptr<EarlyGameDefendMainBaseSquad> squad;
    int lastRegroupFrame;
    std::vector<MyUnit> reservedWorkers;
    MyUnit reservedGasStealAttacker;

    void mineralLineWorkerDefense(std::set<Unit> &enemiesInBase);
};
