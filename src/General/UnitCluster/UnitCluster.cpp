#include "UnitCluster.h"
#include "PathFinding.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define DEBUG_CLUSTER_MEMBERSHIP true // Also in Squad.cpp
#endif

namespace
{
    std::map<UnitCluster::Activity, std::string> ActivityNames = {
            {UnitCluster::Activity::Moving,     "Moving"},
            {UnitCluster::Activity::Attacking,  "Attacking"},
            {UnitCluster::Activity::Regrouping, "Regrouping"}
    };
    std::map<UnitCluster::SubActivity, std::string> SubActivityNames = {
            {UnitCluster::SubActivity::None,                 "None"},
            {UnitCluster::SubActivity::ContainStaticDefense, "ContainStaticDefense"},
            {UnitCluster::SubActivity::ContainChoke,         "ContainChoke"},
            {UnitCluster::SubActivity::Flee,                 "Flee"}
    };
}

UnitCluster::UnitCluster(const MyUnit &unit)
        : center(unit->lastPosition)
        , vanguard(unit)
        , currentActivity(Activity::Moving)
        , currentSubActivity(SubActivity::None)
        , lastActivityChange(0)
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
    updatePositions(targetPosition);
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
    if (area < 0)
    {
        Log::Get() << "ERROR: Cluster area negative after removing " << *unit;
    }

    return newUnitIt;
}

void UnitCluster::updatePositions(BWAPI::Position targetPosition)
{
    int sumX = 0;
    int sumY = 0;
    MyUnit closestToTarget = nullptr;
    int closestToTargetDist = INT_MAX;
    MyUnit furthestFromMain = nullptr;
    int furthestFromMainDist = 0;
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

        int dist = PathFinding::GetGroundDistance(targetPosition, unit->lastPosition, unit->type);
        if (dist != -1 && dist < closestToTargetDist)
        {
            closestToTargetDist = dist;
            closestToTarget = unit;
        }

        if (!closestToTarget)
        {
            int mainDist = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), unit->lastPosition, unit->type);
            if (mainDist != -1 && mainDist > furthestFromMainDist)
            {
                furthestFromMainDist = mainDist;
                furthestFromMain = unit;
            }
        }

        unitIt++;
    }

    if (units.empty()) return;

    center = BWAPI::Position(sumX / units.size(), sumY / units.size());
    vanguard = closestToTarget ? closestToTarget : furthestFromMain;
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
    lastActivityChange = BWAPI::Broodwar->getFrameCount();
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

std::vector<std::pair<MyUnit, Unit>>
UnitCluster::selectTargets(std::set<Unit> &targets, BWAPI::Position targetPosition)
{
    // For our targeting we want to know if we are attacking a reachable enemy base
    // Criteria:
    // - Target is near a base
    // - The base is owned by the enemy
    // - The base has a resource depot or hasn't been scouted yet
    // - The base has a navigation grid path from our main choke (i.e. hasn't been walled-off)
    auto targetBase = Map::baseNear(targetPosition);
    bool targetIsReachableEnemyBase = targetBase
                                      && targetBase->owner == BWAPI::Broodwar->enemy()
                                      && (targetBase->lastScouted == -1 || (targetBase->resourceDepot && targetBase->resourceDepot->exists()));
    if (targetIsReachableEnemyBase && Map::getMyMainChoke())
    {
        auto grid = PathFinding::getNavigationGrid(targetBase->getTilePosition());
        if (grid)
        {
            auto node = (*grid)[Map::getMyMainChoke()->center];
            targetIsReachableEnemyBase = node.nextNode != nullptr;
        }
    }

    std::vector<std::pair<MyUnit, Unit>> result;

    for (auto &unit : units)
    {
        result.emplace_back(std::make_pair(unit, UnitUtil::IsRangedUnit(unit->type)
                                                 ? ChooseRangedTarget(unit, targets, targetPosition, targetIsReachableEnemyBase)
                                                 : ChooseMeleeTarget(unit, targets, targetPosition, targetIsReachableEnemyBase)));
    }

    return result;
}
