#pragma once

#include "MainArmyPlay.h"
#include "Squads/DefendWallSquad.h"

class ForgeFastExpand : public MainArmyPlay
{
public:
    explicit ForgeFastExpand();

    [[nodiscard]] bool isDefensive() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    std::shared_ptr<DefendWallSquad> squad;
};
