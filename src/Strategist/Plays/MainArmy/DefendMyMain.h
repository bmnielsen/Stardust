#pragma once

#include "MainArmyPlay.h"
#include "Squads/DefendBaseSquad.h"

class DefendMyMain : public MainArmyPlay
{
public:
    BWAPI::UnitType emergencyProduction;

    explicit DefendMyMain();

    [[nodiscard]] const char *label() const override { return "DefendMyMain"; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    std::shared_ptr<DefendBaseSquad> squad;
    int lastRegroupFrame;
    std::vector<MyUnit> reservedWorkers;

    void mineralLineWorkerDefense(std::set<Unit> &enemiesInBase);
};
