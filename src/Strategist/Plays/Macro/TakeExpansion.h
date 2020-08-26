#pragma once

#include "Play.h"

// Takes the next expansion.
class TakeExpansion : public Play
{
public:
    explicit TakeExpansion(Base *base);

    void update() override;

    void addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals) override;

    [[nodiscard]] bool constructionStarted() const;

    void disband(const std::function<void(const MyUnit&)> &removedUnitCallback,
                 const std::function<void(const MyUnit&)> &movableUnitCallback) override;

    BWAPI::TilePosition depotPosition;

protected:
    Base *base;
    MyUnit builder;
    BWAPI::UnitType requiredBlockClearBuilding;
    BWAPI::TilePosition requiredBlockClearBuildingTile;
};
