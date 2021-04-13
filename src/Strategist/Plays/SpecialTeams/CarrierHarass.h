#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"
#include "Base.h"

class CarrierHarass : public Play
{
public:
    CarrierHarass() : Play("CarrierHarass"), targetBase(nullptr), attacking(false) {}

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

protected:
    Base *targetBase;
    std::shared_ptr<AttackBaseSquad> squad;
    bool attacking;
};
