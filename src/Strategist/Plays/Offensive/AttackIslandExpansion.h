#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackIslandExpansion : public PlayItem
{
public:
    Base *baseLoc;

    explicit AttackIslandExpansion(Base *baseLoc);

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

protected:
    std::shared_ptr<AttackBaseSquad> squad;
};
