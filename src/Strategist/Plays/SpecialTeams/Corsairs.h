#pragma once

#include "Play.h"
#include "General.h"
#include "Squads/CorsairSquad.h"

class Corsairs : public Play
{
public:
    explicit Corsairs();

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

protected:
    std::shared_ptr<CorsairSquad> squad;
};
