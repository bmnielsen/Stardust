#include "UnitCluster.h"
#include "PathFinding.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Geo.h"

#define DEBUG_CLUSTER_MEMBERSHIP true

void UnitCluster::addUnit(BWAPI::Unit unit)
{
    if (units.find(unit) != units.end()) return;

    units.insert(unit);

    center = BWAPI::Position(
            ((center.x * (units.size() - 1)) + unit->getPosition().x) / units.size(),
            ((center.y * (units.size() - 1)) + unit->getPosition().y) / units.size());

    area += unit->getType().width() * unit->getType().height();

#if DEBUG_CLUSTER_MEMBERSHIP
    CherryVis::log(unit) << "Added to cluster @ " << BWAPI::WalkPosition(center);
#endif
}

void UnitCluster::removeUnit(BWAPI::Unit unit)
{
    auto unitIt = units.find(unit);
    if (unitIt == units.end()) return;

    removeUnit(unitIt);

#if DEBUG_CLUSTER_MEMBERSHIP
    CherryVis::log(unit) << "Removed from cluster @ " << BWAPI::WalkPosition(center);
#endif
}

std::set<BWAPI::Unit>::iterator UnitCluster::removeUnit(std::set<BWAPI::Unit>::iterator unitIt)
{
    auto unit = *unitIt;

    auto newUnitIt = units.erase(unitIt);
    if (units.empty()) return newUnitIt;

    center = BWAPI::Position(
            ((center.x * (units.size() + 1)) - unit->getPosition().x) / units.size(),
            ((center.y * (units.size() + 1)) - unit->getPosition().y) / units.size());

    area -= unit->getType().width() * unit->getType().height();

    return newUnitIt;
}

void UnitCluster::updatePositions(BWAPI::Position targetPosition)
{
    int sumX = 0;
    int sumY = 0;
    BWAPI::Unit closestToTarget = nullptr;
    int closestToTargetDist = INT_MAX;
    for (auto unitIt = units.begin(); unitIt != units.end();)
    {
        auto unit = *unitIt;

        if (!unit->exists())
        {
            unitIt = units.erase(unitIt);
            continue;
        }

        sumX += unit->getPosition().x;
        sumY += unit->getPosition().y;

        int dist = PathFinding::GetGroundDistance(targetPosition, unit->getPosition(), unit->getType());
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

std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>>
UnitCluster::selectTargets(std::set<std::shared_ptr<Unit>> &targets, BWAPI::Position targetPosition)
{
    std::vector<std::pair<BWAPI::Unit, std::shared_ptr<Unit>>> result;

    for (auto unit : units)
    {
        result.emplace_back(std::make_pair(unit, UnitUtil::IsRangedUnit(unit->getType())
                                                 ? ChooseRangedTarget(unit, targets, targetPosition)
                                                 : ChooseMeleeTarget(unit, targets, targetPosition)));
    }

    return result;
}
