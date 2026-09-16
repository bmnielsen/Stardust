#pragma once

#include "Play.h"
#include "Squads/AttackBaseSquad.h"

class AttackIslandExpansion : public Play
{
public:
    Base *base;

    explicit AttackIslandExpansion(Base *base);

    [[nodiscard]] std::shared_ptr<Squad> getSquad() const override { return squad; }

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

protected:
    std::shared_ptr<AttackBaseSquad> squad;
};
