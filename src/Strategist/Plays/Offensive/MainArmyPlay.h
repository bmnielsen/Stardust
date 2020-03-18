#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class MainArmyPlay : public Play
{
public:
    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    virtual void update() override;

    virtual void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;
};
