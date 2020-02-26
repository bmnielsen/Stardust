#pragma once

#include "Play.h"
#include "Squads/DefendBaseSquad.h"

class RallyArmy : public Play
{
public:
    RallyArmy();

    [[nodiscard]] const char *label() const override { return "RallyArmy"; }

    [[nodiscard]] bool receivesUnassignedUnits() const override { return true; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    std::shared_ptr<DefendBaseSquad> squad;
};
