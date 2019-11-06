#include "TakeIslandExpansion.h"

#include "Builder.h"
#include "Workers.h"
#include "Units.h"
#include "Geo.h"
#include "PathFinding.h"
#include "Map.h"

TakeIslandExpansion::TakeIslandExpansion(BWAPI::TilePosition
                                         depotPosition) :
        depotPosition(depotPosition), shuttle(nullptr), builder(nullptr) {}

void TakeIslandExpansion::update()
{
    if (!shuttle)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Shuttle, Map::getMyMain()->getPosition());
        return;
    }

    if (!builder)
    {
        builder = Workers::getClosestReassignableWorker(shuttle->getPosition(), false);
        if (!builder) return;

        Units::getMine(builder).moveTo(shuttle->getPosition());
        shuttle->load(builder);
    }

    auto targetPos = BWAPI::Position(depotPosition) + BWAPI::Position(64, 48);

    if (builder->isLoaded())
    {
        if (shuttle->getDistance(targetPos) < 64)
        {
            if (shuttle->getLastCommand().getType() == BWAPI::UnitCommandTypes::Unload_All) return;
            shuttle->unloadAll();
        }
        else
        {
            Units::getMine(shuttle).moveTo(targetPos);
        }

        return;
    }

    if (builder->getDistance(targetPos) < 128)
    {
        if (shuttle)
        {
            status.removedUnits.push_back(shuttle);
        }

        BWAPI::Unit blockingMineral = nullptr;
        for (auto unit : BWAPI::Broodwar->getNeutralUnits())
        {
            if (!unit->exists()) continue;
            if (unit->getTilePosition() == BWAPI::TilePosition(116, 11))
            {
                blockingMineral = unit;
                break;
            }
        }

        if (blockingMineral)
        {
            if (builder->getLastCommand().getType() == BWAPI::UnitCommandTypes::Gather &&
                builder->getLastCommand().getTarget() == blockingMineral)
                return;
            builder->gather(blockingMineral);
            return;
        }

        Builder::build(BWAPI::UnitTypes::Protoss_Nexus, depotPosition, builder);
    }

    // Treat the play as complete when the nexus has been created
    auto pendingBuilding = Builder::pendingHere(depotPosition);
    if (pendingBuilding && pendingBuilding->isConstructionStarted())
    {
        status.complete = true;
    }
}

void TakeIslandExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[20].emplace_back(std::in_place_type<UnitProductionGoal>, unitRequirement.type, unitRequirement.count, 1);
    }
}

void TakeIslandExpansion::addUnit(BWAPI::Unit unit)
{
    if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle) shuttle = unit;
}

void TakeIslandExpansion::removeUnit(BWAPI::Unit unit)
{
    if (shuttle == unit) shuttle = nullptr;
}
