#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class MainArmyPlay : public Play
{
public:
    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;
};
