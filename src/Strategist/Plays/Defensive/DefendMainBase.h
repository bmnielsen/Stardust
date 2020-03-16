#pragma once

#include "DefendBase.h"

class DefendMainBase : public DefendBase
{
public:
    explicit DefendMainBase();

    [[nodiscard]] const char *label() const override { return "DefendMainBase"; }

    std::shared_ptr<Squad> getSquad() override { return squad; }

    void update() override;

    [[nodiscard]] bool receivesUnassignedUnits() const override { return enemyUnitsInBase; }

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

private:
    int lastRegroupFrame;
    bool enemyUnitsInBase;
    BWAPI::UnitType emergencyProduction;
};
