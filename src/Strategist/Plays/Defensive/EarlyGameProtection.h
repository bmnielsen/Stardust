#pragma once

#include "Play.h"

#include "Squads/DefendBaseSquad.h"

// Ensures we have zealots handy in the early game to protect against rushes or cheese.
class EarlyGameProtection : public Play
{
public:
    EarlyGameProtection();

    const char *label() const { return "EarlyGameProtection"; }

    std::shared_ptr<Squad> getSquad() { return squad; }

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals);

private:
    std::shared_ptr<DefendBaseSquad> squad;
};
