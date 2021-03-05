#pragma once

#include "MainArmyPlay.h"
#include "Squads/EarlyGameDefendMainBaseSquad.h"

class DefendMyMain : public MainArmyPlay
{
public:
    explicit DefendMyMain();

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    void removeUnit(const MyUnit &unit) override;

    void disband(const std::function<void(const MyUnit)> &removedUnitCallback,
                 const std::function<void(const MyUnit)> &movableUnitCallback) override;

    [[nodiscard]] bool canTransitionToAttack() const;

private:
    BWAPI::UnitType emergencyProduction;
    std::shared_ptr<EarlyGameDefendMainBaseSquad> squad;
    int lastRegroupFrame;
    MyUnit reservedGasStealAttacker;
    std::vector<MyUnit> reservedWorkerGasStealAttackers;
};
