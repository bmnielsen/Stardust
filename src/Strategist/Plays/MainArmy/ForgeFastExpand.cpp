#include "ForgeFastExpand.h"

#include "Geo.h"
#include "General.h"

ForgeFastExpand::ForgeFastExpand()
        : MainArmyPlay("ForgeFastExpand")
        , squad(std::make_shared<DefendWallSquad>())
{
    General::addSquad(squad);
}

void ForgeFastExpand::update()
{

}

void ForgeFastExpand::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{

}
