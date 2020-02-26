#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackMainBase : public Play
{
public:
    explicit AttackMainBase(Base *base);

    [[nodiscard]] const char *label() const override { return "AttackMainBase"; }

    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    static void MainArmyProduction(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

private:
    Base *base;
    std::shared_ptr<AttackBaseSquad> squad;
};
