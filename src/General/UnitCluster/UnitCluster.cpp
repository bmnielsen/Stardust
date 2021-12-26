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

UnitCluster::UnitCluster(const MyUnit &unit)
        : center(unit->lastPosition)
        , vanguard(unit)
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
        , area(unit->type.width() * unit->type.height())
{
    units.insert(unit);

#if DEBUG_CLUSTER_MEMBERSHIP
    CherryVis::log(unit->id) << "Added to new cluster @ " << BWAPI::WalkPosition(center);
#endif
}

void UnitCluster::absorbCluster(const std::shared_ptr<UnitCluster> &other, BWAPI::Position targetPosition)
{
    units.insert(other->units.begin(), other->units.end());
    area += other->area;
    ballRadius = (int) sqrt((double) area / pi);
    lineRadius = 16 * units.size();

    // Recompute the center
    if (units.empty()) return; // should never happen, but guard against divide-by-zero
    int sumX = 0;
    int sumY = 0;
    for (auto &unit : units)
    {
        sumX += unit->lastPosition.x;
        sumY += unit->lastPosition.y;
    }
    center = BWAPI::Position(sumX / units.size(), sumY / units.size());
}

void UnitCluster::addUnit(const MyUnit &unit)
{
    if (units.find(unit) != units.end()) return;

    units.insert(unit);

    center = BWAPI::Position(
            ((center.x * (units.size() - 1)) + unit->lastPosition.x) / units.size(),
            ((center.y * (units.size() - 1)) + unit->lastPosition.y) / units.size());

    area += unit->type.width() * unit->type.height();

#if DEBUG_CLUSTER_MEMBERSHIP
    CherryVis::log(unit->id) << "Added to cluster @ " << BWAPI::WalkPosition(center);
#endif
}

std::set<MyUnit>::iterator UnitCluster::removeUnit(std::set<MyUnit>::iterator unitIt, BWAPI::Position targetPosition)
{
    auto unit = *unitIt;

    auto newUnitIt = units.erase(unitIt);
    area -= unit->type.width() * unit->type.height();

    // Guard against divide-by-zero, but this shouldn't happen as we don't remove the last unit from a cluster in this way
    if (units.empty()) return newUnitIt;

    if (vanguard == unit)
    {
        updatePositions(targetPosition);
    }
    else
    {
        center = BWAPI::Position(
                ((center.x * (units.size() + 1)) - unit->lastPosition.x) / units.size(),
                ((center.y * (units.size() + 1)) - unit->lastPosition.y) / units.size());
    }

    return newUnitIt;
}

void UnitCluster::updatePositions(BWAPI::Position targetPosition)
{
    int sumX = 0;
    int sumY = 0;

    // Start by pruning dead units and recomputing unit distances and the cluster center position
    int minGroundDistance = INT_MAX;
    int minAirDistance = INT_MAX;
    for (auto unitIt = units.begin(); unitIt != units.end();)
    {
        auto unit = *unitIt;

        if (!unit->exists())
        {
            area -= unit->type.width() * unit->type.height();
            unitIt = units.erase(unitIt);
            continue;
        }

        sumX += unit->lastPosition.x;
        sumY += unit->lastPosition.y;

        if (unit->isFlying)
        {
            unit->distToTargetPosition = unit->lastPosition.getApproxDistance(targetPosition);
            if (unit->distToTargetPosition < minAirDistance)
            {
                minAirDistance = unit->distToTargetPosition;
            }

            unitIt++;
            continue;
        }

        unit->distToTargetPosition = PathFinding::GetGroundDistance(targetPosition,
                                                                    unit->lastPosition,
                                                                    unit->type,
                                                                    PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
        if (unit->distToTargetPosition != -1 && unit->distToTargetPosition < minGroundDistance)
        {
            minGroundDistance = unit->distToTargetPosition;
        }

        unitIt++;
    }

    ballRadius = (int) sqrt((double) area / pi);
    lineRadius = 16 * units.size();

    if (units.empty()) return;

    center = BWAPI::Position(sumX / units.size(), sumY / units.size());

    // Now determine which unit is our vanguard unit
    // This is generally the unit closest to the target position, but if more than one unit is at approximately the
    // same distance, we take the one closest to the cluster center
    vanguard = nullptr;
    int minCenterDist = INT_MAX;
    for (auto &unit : units)
    {
        if (unit->distToTargetPosition == -1) continue;
        if (unit->isFlying != (minGroundDistance == INT_MAX)) continue;
        if (unit->distToTargetPosition - (unit->isFlying ? minAirDistance : minGroundDistance) > 32) continue;

        int centerDist = unit->lastPosition.getApproxDistance(center);
        if (centerDist < minCenterDist)
        {
            vanguard = unit;
            minCenterDist = centerDist;
        }
    }

    // If the vanguard isn't set, it means we have no units with a valid distance to the goal
    // So fall back to air distances for all units
    if (!vanguard)
    {
        minAirDistance = INT_MAX;
        for (auto &unit : units)
        {
            unit->distToTargetPosition = unit->lastPosition.getApproxDistance(targetPosition);
            if (unit->distToTargetPosition < minAirDistance)
            {
                minAirDistance = unit->distToTargetPosition;
                vanguard = unit;
            }
        }
    }

    vanguardDistToTarget = vanguard->distToTargetPosition;

    auto vanguardDistTo = [&](BWAPI::Position pos)
    {
        if (vanguard->isFlying)
        {
            return vanguard->lastPosition.getApproxDistance(pos);
        }

        auto dist = PathFinding::GetGroundDistance(pos,
                                                   vanguard->lastPosition,
                                                   vanguard->type,
                                                   PathFinding::PathFindingOptions::UseNeighbouringBWEMArea);
        if (dist != -1) return dist;

        return vanguard->lastPosition.getApproxDistance(pos);
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
    CherryVis::log() << BWAPI::WalkPosition(center) << ": Changed activity from " << ActivityNames[currentActivity] << " to "
                     << ActivityNames[newActivity];
#endif

    currentActivity = newActivity;
    currentSubActivity = newSubActivity;
    lastActivityChange = currentFrame;
}

void UnitCluster::setSubActivity(SubActivity newSubActivity)
{
    if (currentSubActivity == newSubActivity) return;

#if DEBUG_COMBATSIM
    CherryVis::log() << BWAPI::WalkPosition(center) << ": Changed sub-activity from " << SubActivityNames[currentSubActivity] << " to "
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
