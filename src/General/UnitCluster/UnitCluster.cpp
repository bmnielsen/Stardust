#include "UnitCluster.h"
#include "PathFinding.h"
#include "Units.h"
#include "UnitUtil.h"

#define DEBUG_CLUSTER_MEMBERSHIP true

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

void UnitCluster::removeUnit(const MyUnit &unit)
{
    auto unitIt = units.find(unit);
    if (unitIt == units.end()) return;

    removeUnit(unitIt);

#if DEBUG_CLUSTER_MEMBERSHIP
    CherryVis::log(unit->id) << "Removed from cluster @ " << BWAPI::WalkPosition(center);
#endif
}

std::set<MyUnit>::iterator UnitCluster::removeUnit(std::set<MyUnit>::iterator unitIt)
{
    auto unit = *unitIt;

    auto newUnitIt = units.erase(unitIt);
    if (units.empty()) return newUnitIt;

    center = BWAPI::Position(
            ((center.x * (units.size() + 1)) - unit->lastPosition.x) / units.size(),
            ((center.y * (units.size() + 1)) - unit->lastPosition.y) / units.size());

    area -= unit->type.width() * unit->type.height();

    return newUnitIt;
}

void UnitCluster::updatePositions(BWAPI::Position targetPosition)
{
    int sumX = 0;
    int sumY = 0;
    MyUnit closestToTarget = nullptr;
    int closestToTargetDist = INT_MAX;
    for (auto unitIt = units.begin(); unitIt != units.end();)
    {
        auto unit = *unitIt;

        if (!unit->exists())
        {
            unitIt = units.erase(unitIt);
            continue;
        }

        sumX += unit->lastPosition.x;
        sumY += unit->lastPosition.y;

        int dist = PathFinding::GetGroundDistance(targetPosition, unit->lastPosition, unit->type);
        if (dist != -1 && dist < closestToTargetDist)
        {
            closestToTargetDist = dist;
            closestToTarget = unit;
        }

        unitIt++;
    }

    if (units.empty()) return;

    center = BWAPI::Position(sumX / units.size(), sumY / units.size());
    vanguard = closestToTarget;
}

void UnitCluster::setActivity(UnitCluster::Activity newActivity)
{
    if (currentActivity == newActivity) return;

    currentActivity = newActivity;
    lastActivityChange = BWAPI::Broodwar->getFrameCount();
}

std::vector<std::pair<MyUnit, Unit>>
UnitCluster::selectTargets(std::set<Unit> &targets, BWAPI::Position targetPosition)
{
    std::vector<std::pair<MyUnit, Unit>> result;

    for (auto &unit : units)
    {
        result.emplace_back(std::make_pair(unit, UnitUtil::IsRangedUnit(unit->type)
                                                 ? ChooseRangedTarget(unit, targets, targetPosition)
                                                 : ChooseMeleeTarget(unit, targets, targetPosition)));
    }

    return result;
}
