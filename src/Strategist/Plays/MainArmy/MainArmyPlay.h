#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class MainArmyPlay : public PlayItem
{
public:
    explicit MainArmyPlay(std::string label) : PlayItem(std::move(label)) {};

    [[nodiscard]] virtual bool isDefensive() const = 0;

    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;
};
