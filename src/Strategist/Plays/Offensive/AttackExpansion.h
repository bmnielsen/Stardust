#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackExpansion : public Play
{
public:
    Base *base;
    int enemyDefenseValue;

    explicit AttackExpansion(Base *base, int enemyDefenseValue);

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

protected:
    std::shared_ptr<AttackBaseSquad> squad;
};
