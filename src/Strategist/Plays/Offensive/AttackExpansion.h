#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackExpansion : public PlayItem
{
public:
    Base *baseLoc;
    int enemyDefenseValue;

    explicit AttackExpansion(Base *baseLoc, int enemyDefenseValue);

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

protected:
    std::shared_ptr<AttackBaseSquad> squad;
};
