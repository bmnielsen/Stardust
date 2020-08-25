#include "ScoutEnemyExpos.h"

#include "Workers.h"
#include "Map.h"
#include "Units.h"

void ScoutEnemyExpos::update()
{
    // Clear the scout if it is dead
    if (scout && !scout->exists()) scout = nullptr;

    // Pick a target base
    // We score them based on three factors:
    // - How soon we expect the enemy to expand to the base (i.e. order of base in vector)
    // - How long it has been since we scouted the base
    // - How close our scout is to the base
    Base *targetBase = nullptr;
    double bestScore = 0.0;
    int scoreFactor = 0;
    for (auto base : Map::getUntakenExpansions(BWAPI::Broodwar->enemy()))
    {
        int framesSinceScouted = BWAPI::Broodwar->getFrameCount() - base->lastScouted;
        scoreFactor++;

        double score = (double) framesSinceScouted / (double) scoreFactor;

        if (scout)
        {
            int dist = scout->getDistance(base->getPosition());
            if (dist == -1) continue;
            score /= ((double) dist / 2000.0);
        }

        if (score > bestScore)
        {
            bestScore = score;
            targetBase = base;
        }
    }

    if (!targetBase)
    {
        if (scout)
        {
            if (scout->type.isWorker())
            {
                Workers::releaseWorker(scout);
                scout = nullptr;
            }
            else
            {
                status.removedUnits.push_back(scout);
            }
        }

#if INSTRUMENTATION_ENABLED
        CherryVis::setBoardValue("ScoutEnemyExpos_target", "(none)");
#endif

        return;
    }

#if INSTRUMENTATION_ENABLED
    CherryVis::setBoardValue("ScoutEnemyExpos_target", (std::ostringstream() << targetBase->getTilePosition()).str());
#endif

    // TODO: Support scouting with a worker or other unit

    // We prefer to scout with an observer, so if we don't currently have one, request one
    // This play is lower-priority than combat plays, so it might get taken by another play if the enemy has cloaked units
    if (!scout || scout->type != BWAPI::UnitTypes::Protoss_Observer)
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, targetBase->getPosition());
    }

    // Move the scout
    if (scout)
    {
        // TODO: Path to avoid threats

        scout->moveTo(targetBase->getPosition());
    }
}

void ScoutEnemyExpos::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Produce an observer if we have a unit requirement for it and have the prerequisites
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observatory) == 0
            || Units::countCompleted(BWAPI::UnitTypes::Protoss_Robotics_Facility) == 0)
        {
            continue;
        }

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Observer, 1, 1);
    }
}

void ScoutEnemyExpos::disband(const std::function<void(const MyUnit &)> &removedUnitCallback,
                              const std::function<void(const MyUnit &)> &movableUnitCallback)
{
    if (scout)
    {
        if (scout->type.isWorker())
        {
            Workers::releaseWorker(scout);
        }
        else
        {
            movableUnitCallback(scout);
        }
    }
}

void ScoutEnemyExpos::addUnit(const MyUnit &unit)
{
    if (scout && scout->type.isWorker())
    {
        Workers::releaseWorker(scout);
    }

    scout = unit;
}

void ScoutEnemyExpos::removeUnit(const MyUnit &unit)
{
    if (scout == unit) scout = nullptr;
}
