#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"
#include "Base.h"

class CarrierHarass : public PlayItem
{
public:
    CarrierHarass() : PlayItem("CarrierHarass"), targetBase(nullptr), attacking(false) {}

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

protected:
    Base *targetBase;
    std::shared_ptr<AttackBaseSquad> squad;
    bool attacking;
};
