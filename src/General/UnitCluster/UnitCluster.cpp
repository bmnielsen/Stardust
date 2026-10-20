#include "UnitCluster.h"
#include "PathFinding.h"
#include "UnitUtil.h"
#include "Map.h"

#include "DebugFlag_CombatSim.h"

#if INSTRUMENTATION_ENABLED
#define DEBUG_CLUSTER_MEMBERSHIP true // Also in Squad.cpp
#endif

namespace
{
    const double pi = 3.14159265358979323846;

    std::map<UnitCluster::Activity, std::string> ActivityNames = {
            {UnitCluster::Activity::Moving,     "Moving"},
            {UnitCluster::Activity::Attacking,  "Attacking"},
            {UnitCluster::Activity::Regrouping, "Regrouping"}
    };
    std::map<UnitCluster::SubActivity, std::string> SubActivityNames = {
            {UnitCluster::SubActivity::None,                 "None"},
            {UnitCluster::SubActivity::ContainStaticDefense, "ContainStaticDefense"},
            {UnitCluster::SubActivity::ContainChoke,         "ContainChoke"},
            {UnitCluster::SubActivity::StandGround,          "StandGround"},
            {UnitCluster::SubActivity::Flee,                 "Flee"}
    };
}

UnitCluster::UnitCluster(const OwnedUnit &gameUnit)
        : midpoint(gameUnit->prevPos)
        , vanguard(gameUnit)
        , vanguardDistToTarget(-1)
        , vanguardDistToMain(0)
        , percentageToEnemyMain(0.0)
        , ballRadius(16)
        , lineRadius(16)
        , enemyAoeRadius(0)
        , currentActivity(Activity::Moving)
        , currentSubActivity(SubActivity::None)
        , lastActivityChange(0)
        , isVanguardCluster(false)
        , area(gameUnit->type.width() * gameUnit->type.height())
{
    units.insert(gameUnit);

#if DEBUG_CLUSTER_MEMBERSHIP
    Telemetry::log(gameUnit->ident) << "Added to new cluster @ " << BWAPI::WalkPosition(midpoint);
#endif
}

void UnitCluster::absorbCluster(const std::shared_ptr<UnitCluster> &other, BWAPI::Position targetPosition)
{
    units.insert(other->units.begin(), other->units.end());
    area += other->area;
    ballRadius = (int) sqrt((double) area / pi);
    lineRadius = 16 * (int)units.size();

    /* Recompute the center */
    if (units.empty()) return; // should never happen, but guard against divide-by-zero
    size_t sumX = 0;
    size_t sumY = 0;
    for (auto &gameUnit : units)
    {
        sumX += gameUnit->prevPos.x;
        sumY += gameUnit->prevPos.y;
    }
    midpoint = BWAPI::Position((int)(sumX / units.size()), (int)(sumY / units.size()));
}

void UnitCluster::addUnit(const OwnedUnit &gameUnit)
{
    if (units.contains(gameUnit)) return;

    units.insert(gameUnit);

    midpoint = BWAPI::Position(
            ((midpoint.x * ((int)units.size() - 1)) + gameUnit->prevPos.x) / (int)units.size(),
            ((midpoint.y * ((int)units.size() - 1)) + gameUnit->prevPos.y) / (int)units.size());

    area += gameUnit->type.width() * gameUnit->type.height();

#if DEBUG_CLUSTER_MEMBERSHIP
    Telemetry::log(gameUnit->ident) << "Added to cluster @ " << BWAPI::WalkPosition(midpoint);
#endif
}

std::set<OwnedUnit>::iterator UnitCluster::removeUnit(std::set<OwnedUnit>::iterator unitIt, BWAPI::Position targetPosition)
{
    auto gameUnit = *unitIt; // NOLINT(performance-unnecessary-copy-initialization) - copied on purpose as it is deleted in the next line

    auto newUnitIt = units.erase(unitIt);
    area -= gameUnit->type.width() * gameUnit->type.height();

    /* Guard against divide-by-zero, but this shouldn't happen as we don't remove the last unit from a cluster in this way */
    if (units.empty()) return newUnitIt;

    if (vanguard == gameUnit)
    {
        updatePositions(targetPosition);
    }
    else
    {
        midpoint = BWAPI::Position(
                ((midpoint.x * ((int)units.size() + 1)) - gameUnit->prevPos.x) / (int)units.size(),
                ((midpoint.y * ((int)units.size() + 1)) - gameUnit->prevPos.y) / (int)units.size());
    }

    return newUnitIt;
}

bool UnitCluster::hasUnitType(BWAPI::UnitType type) const
{
    return std::any_of(units.begin(), units.end(), [&type](const auto &gameUnit)
    {
        return gameUnit->type == type;
    });
}

void UnitCluster::updatePositions(BWAPI::Position targetPosition)
{
    size_t sumX = 0;
    size_t sumY = 0;

    /* Start by pruning dead units and recomputing unit distances and the cluster center position */
    int minGroundDistance = INT_MAX;
    int minAirDistance = INT_MAX;
    for (auto unitIt = units.begin(); unitIt != units.end();)
    {
        auto gameUnit = *unitIt;

        if (!gameUnit->exists())
        {
            area -= gameUnit->type.width() * gameUnit->type.height();
            unitIt = units.erase(unitIt);
            continue;
        }

        sumX += gameUnit->prevPos.x;
        sumY += gameUnit->prevPos.y;

        if (gameUnit->isFlying)
        {
            gameUnit->distToTargetPosition = gameUnit->prevPos.getApproxDistance(targetPosition);
            if (gameUnit->distToTargetPosition < minAirDistance)
            {
                minAirDistance = gameUnit->distToTargetPosition;
            }

            unitIt++;
            continue;
        }

        gameUnit->distToTargetPosition = Pathfinder::GetGroundDistance(targetPosition,
                                                                    gameUnit->prevPos,
                                                                    gameUnit->type,
                                                                    Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
        if (gameUnit->distToTargetPosition != -1 && gameUnit->distToTargetPosition < minGroundDistance)
        {
            minGroundDistance = gameUnit->distToTargetPosition;
        }

        unitIt++;
    }

    ballRadius = (int) sqrt((double) area / pi);
    lineRadius = 16 * (int)units.size();

    if (units.empty()) return;

    midpoint = BWAPI::Position((int)(sumX / units.size()), (int)(sumY / units.size()));

    /* Now determine which unit is our vanguard unit */
    /* This is generally the unit closest to the target position, but if more than one unit is at approximately the */
    /* same distance, we take the one closest to the cluster center */
    vanguard = nullptr;
    int minCenterDist = INT_MAX;
    for (auto &gameUnit : units)
    {
        if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;
        if (gameUnit->distToTargetPosition == -1) continue;
        if (gameUnit->isFlying != (minGroundDistance == INT_MAX)) continue;
        if (gameUnit->distToTargetPosition - (gameUnit->isFlying ? minAirDistance : minGroundDistance) > 32) continue;

        int centerDist = gameUnit->prevPos.getApproxDistance(midpoint);
        if (centerDist < minCenterDist)
        {
            vanguard = gameUnit;
            minCenterDist = centerDist;
        }
    }

    /* If the vanguard isn't set, it means we have no units with a valid distance to the goal */
    /* So fall back to air distances for all units */
    if (!vanguard)
    {
        minAirDistance = INT_MAX;
        for (auto &gameUnit : units)
        {
            if (gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;
            gameUnit->distToTargetPosition = gameUnit->prevPos.getApproxDistance(targetPosition);
            if (gameUnit->distToTargetPosition < minAirDistance)
            {
                minAirDistance = gameUnit->distToTargetPosition;
                vanguard = gameUnit;
            }
        }
    }

    if (!vanguard)
    {
        vanguard = *units.begin();
    }

    vanguardDistToTarget = vanguard->distToTargetPosition;

    auto vanguardDistTo = [&](BWAPI::Position pt)
    {
        if (vanguard->isFlying)
        {
            return vanguard->prevPos.getApproxDistance(pt);
        }

        auto span = Pathfinder::GetGroundDistance(pt,
                                                   vanguard->prevPos,
                                                   vanguard->type,
                                                   Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea);
        if (span != -1) return span;

        return vanguard->prevPos.getApproxDistance(pt);
    };

    vanguardDistToMain = vanguardDistTo(Map::getMyMain()->getPosition());

    auto enemyMain = Map::getEnemyMain();
    int vanguardDistToEnemyMain = vanguardDistTo(enemyMain ? enemyMain->getPosition() : targetPosition);
    if (vanguardDistToEnemyMain > 0 || vanguardDistToMain > 0)
    {
        percentageToEnemyMain = (double) vanguardDistToMain / (double) (vanguardDistToMain + vanguardDistToEnemyMain);
    }
    else
    {
        percentageToEnemyMain = 0.5;
    }
}

void UnitCluster::setActivity(UnitCluster::Activity newActivity, SubActivity newSubActivity)
{
    if (currentActivity == newActivity) return;

#if DEBUG_COMBATSIM
    Telemetry::log() << BWAPI::WalkPosition(midpoint) << ": Changed activity from " << ActivityNames[currentActivity] << " to "
                     << ActivityNames[newActivity];
#endif

    currentActivity = newActivity;
    currentSubActivity = newSubActivity;
    lastActivityChange = frameNow;
}

void UnitCluster::setSubActivity(SubActivity newSubActivity)
{
    if (currentSubActivity == newSubActivity) return;

#if DEBUG_COMBATSIM
    Telemetry::log() << BWAPI::WalkPosition(midpoint) << ": Changed sub-activity from " << SubActivityNames[currentSubActivity] << " to "
                     << SubActivityNames[newSubActivity];
#endif

    currentSubActivity = newSubActivity;
}

std::string UnitCluster::getCurrentActivity() const
{
    return ActivityNames[currentActivity];
}

std::string UnitCluster::getCurrentSubActivity() const
{
    return SubActivityNames[currentSubActivity];
}

void UnitCluster::addInstrumentation(nlohmann::json &clusterArray) const
{
    auto simResultToJson = [](const CombatSimResult& simResult, bool decision)
    {
        nlohmann::json outcome({
                                      {"decision",                decision},
                                      {"myUnitCount",             simResult.myUnitCount},
                                      {"enemyUnitCount",          simResult.enemyUnitCount},
                                      {"initialMine",             simResult.initialMine},
                                      {"initialEnemy",            simResult.initialEnemy},
                                      {"finalMine",               simResult.finalMine},
                                      {"finalEnemy",              simResult.finalEnemy},
                                      {"enemyHasUndetectedUnits", simResult.enemyHasUndetectedUnits},
                                      {"distanceFactor",          simResult.distanceFactor},
                                      {"aggression",              simResult.aggression},
                                      {"closestReinforcements",   simResult.closestReinforcements},
                                      {"reinforcementPercentage", simResult.reinforcementPercentage}
                              });
        if (simResult.narrowChoke)
        {
            outcome["choke_x"] = simResult.narrowChoke->midpoint.x;
            outcome["choke_y"] = simResult.narrowChoke->midpoint.y;
        }
        else
        {
            outcome["choke_x"] = nlohmann::json();
            outcome["choke_y"] = nlohmann::json();
        }
#if DEBUG_COMBATSIM_CVIS
        if (!simResult.unitLog.empty())
        {
            outcome["unitLog"] = simResult.unitLog;
        }
#endif
        return outcome;
    };

    nlohmann::json simResult;
    nlohmann::json regroupSimResult;
    if (!recentSimResults.empty() && recentSimResults.rbegin()->first.tick == frameNow)
    {
        simResult = simResultToJson(recentSimResults.rbegin()->first, recentSimResults.rbegin()->second);
    }
    if (!recentRegroupSimResults.empty() && recentRegroupSimResults.rbegin()->first.tick == frameNow)
    {
        regroupSimResult = simResultToJson(recentRegroupSimResults.rbegin()->first, recentRegroupSimResults.rbegin()->second);
    }

    clusterArray.push_back({
                                   {"center_x", midpoint.x},
                                   {"center_y", midpoint.y},
                                   {"unit_count", units.size()},
                                   {"is_vanguard", isVanguardCluster},
                                   {"percent_distance_to_target", percentageToEnemyMain},
                                   {"activity", ActivityNames[currentActivity]},
                                   {"subactivity", SubActivityNames[currentSubActivity]},
                                   {"sim_result", simResult},
                                   {"regroup_sim_result", regroupSimResult}
                           });
}
