#pragma once

#include "MainArmyPlay.h"
#include "Squads/EarlyGameDefendMainBaseSquad.h"
#include "MyWorker.h"

class DefendMyMain : public MainArmyPlay
{
public:
    explicit DefendMyMain();

    [[nodiscard]] bool isDefensive() const override { return true; }

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

    void removeUnit(const OwnedUnit &gameUnit) override;

    void disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
                 const std::function<void(const OwnedUnit)> &movableUnitCallback) override;

    [[nodiscard]] bool canTransitionToAttack() const;

private:
    BWAPI::UnitType emergencyProduction;
    std::shared_ptr<EarlyGameDefendMainBaseSquad> squad;
    int lastRegroupFrame;
    OwnedUnit reservedGasStealAttacker;
    std::vector<OwnedWorker> reservedWorkerGasStealAttackers;
};
