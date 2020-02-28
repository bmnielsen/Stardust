#include "TakeNaturalExpansion.h"

#include "Map.h"
#include "Builder.h"
#include "Units.h"
#include "PathFinding.h"

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
    // Currently we do this as soon as we have at least 5 combat units and one of them is 2000 units of ground distance away from our main
    // TODO: More nuanced approach
    int army = Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon);
    if (army < 5) return;

    bool unitIsAwayFromHome = false;
    for (const auto &unit : Units::allMine())
    {
        if (unit->type != BWAPI::UnitTypes::Protoss_Zealot && unit->type != BWAPI::UnitTypes::Protoss_Dragoon) continue;
        if (!unit->completed) continue;
        if (!unit->exists()) continue;

        int dist = PathFinding::GetGroundDistance(unit->lastPosition, Map::getMyMain()->getPosition(), unit->type);
        if (dist > 2000)
        {
            unitIsAwayFromHome = true;
            break;
        }
    }
    if (!unitIsAwayFromHome) return;

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(depotPosition),
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           depotPosition,
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0);
    prioritizedProductionGoals[15].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Nexus, buildLocation);
}
