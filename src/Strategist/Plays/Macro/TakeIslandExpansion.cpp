#include "TakeIslandExpansion.h"

#include "Builder.h"
#include "Workers.h"
#include "Units.h"
#include "Geo.h"
#include "Map.h"

TakeIslandExpansion::TakeIslandExpansion(BWAPI::TilePosition depotPosition)
        : TakeExpansion(depotPosition)
        , shuttle(nullptr) {}

void TakeIslandExpansion::update()
{
    if (!shuttle)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Shuttle, Map::getMyMain()->getPosition());
        return;
    }

    if (!builder)
    {
        builder = Workers::getClosestReassignableWorker(shuttle->lastPosition, false);
        if (!builder) return;

        builder->moveTo(shuttle->lastPosition);
        shuttle->load(builder->bwapiUnit);
    }

    auto targetPos = BWAPI::Position(depotPosition) + BWAPI::Position(64, 48);

    if (builder->bwapiUnit->isLoaded())
    {
        if (shuttle->getDistance(targetPos) < 64)
        {
            if (shuttle->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Unload_All) return;
            shuttle->unloadAll();
        }
        else
        {
            shuttle->moveTo(targetPos);
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
            if (builder->bwapiUnit->getLastCommand().getType() == BWAPI::UnitCommandTypes::Gather &&
                builder->bwapiUnit->getLastCommand().getTarget() == blockingMineral)
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

void TakeIslandExpansion::cancel()
{
    // TODO: Avoid orphaning the worker
    Builder::cancel(depotPosition);
    status.complete = true;
}

void TakeIslandExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}

void TakeIslandExpansion::addUnit(MyUnit unit)
{
    if (unit->type == BWAPI::UnitTypes::Protoss_Shuttle) shuttle = unit;
}

void TakeIslandExpansion::removeUnit(MyUnit unit)
{
    if (shuttle == unit) shuttle = nullptr;
}
