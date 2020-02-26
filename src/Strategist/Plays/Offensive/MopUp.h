#pragma once

#include "Play.h"
#include "Squads/MopUpSquad.h"

class MopUp : public Play
{
public:
    MopUp();

    [[nodiscard]] const char *label() const override { return "MopUp"; }

    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    std::shared_ptr<MopUpSquad> squad;
};
