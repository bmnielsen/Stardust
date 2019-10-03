#include "TakeNaturalExpansion.h"

#include "Map.h"
#include "Builder.h"

TakeNaturalExpansion::TakeNaturalExpansion() : depotPosition(Map::getMyNatural()->getTilePosition()) {}

void TakeNaturalExpansion::update()
{
    // No longer required when the building has been queued for building
    if (Builder::isPendingHere(depotPosition))
    {
        status.complete = true;
    }
}

void TakeNaturalExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    auto buildLocation = BuildingPlacement::BuildLocation(depotPosition,
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           depotPosition,
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0);
    prioritizedProductionGoals[15].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Nexus, buildLocation);
}
