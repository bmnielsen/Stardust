#pragma once

#include "Play.h"

#include "Squads/DefendBaseSquad.h"

// Ensures we have zealots handy in the early game to protect against rushes or cheese.
class EarlyGameProtection : public Play
{
public:
    EarlyGameProtection();

    const char *label() const override { return "EarlyGameProtection"; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    std::shared_ptr<DefendBaseSquad> squad;
};
