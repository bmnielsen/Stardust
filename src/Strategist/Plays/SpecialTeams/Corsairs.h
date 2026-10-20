#pragma once

#include "Play.h"
#include "General.h"
#include "Squads/CorsairSquad.h"

class Corsairs : public PlayItem
{
public:
    explicit Corsairs();

    std::shared_ptr<UnitSquad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals) override;

protected:
    std::shared_ptr<CorsairSquad> squad;
};
