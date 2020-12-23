#include "UnitCluster.h"
#include "PathFinding.h"
#include "Units.h"
#include "UnitUtil.h"
#include "Map.h"

#include "DebugFlag_CombatSim.h"

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
            {UnitCluster::SubActivity::StandGround,          "StandGround"},
            {UnitCluster::SubActivity::Flee,                 "Flee"}
    };
}

UnitCluster::UnitCluster(const MyUnit &unit)
        : center(unit->lastPosition)
        , vanguard(unit)
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

std::set<MyUnit>::iterator UnitCluster::removeUnit(std::set<MyUnit>::iterator unitIt, BWAPI::Position targetPosition)
{
    auto unit = *unitIt;

    auto newUnitIt = units.erase(unitIt);

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

        area -= unit->type.width() * unit->type.height();
        if (area < 0)
        {
            Log::Get() << "ERROR: Cluster area negative after removing " << *unit;
        }
    }

    return newUnitIt;
}

void UnitCluster::updatePositions(BWAPI::Position targetPosition)
{
    int sumX = 0;
    int sumY = 0;
    auto mainPosition = Map::getMyMain()->getPosition();
    vanguard = nullptr;
    vanguardDistToMain = -1;
    vanguardDistToTarget = INT_MAX;
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
        if (dist > vanguardDistToTarget)
        {
            unitIt++;
            continue;
        }

        int mainDist = PathFinding::GetGroundDistance(mainPosition, unit->lastPosition, unit->type);
        if (dist != -1 || (mainDist != -1 && vanguardDistToTarget == INT_MAX && mainDist > vanguardDistToMain))
        {
            vanguard = unit;
            if (dist != -1)
            {
                vanguardDistToTarget = dist;
            }
            if (mainDist != -1)
            {
                vanguardDistToMain = mainDist;
            }
        }

        unitIt++;
    }

    if (units.empty()) return;

    center = BWAPI::Position(sumX / units.size(), sumY / units.size());

    // Ensure the vanguard and vanguard distances are set if we for whatever reason couldn't get any proper distances above
    if (!vanguard) vanguard = *units.begin();
    if (vanguardDistToTarget == INT_MAX) vanguardDistToTarget = vanguard->getDistance(targetPosition);
    if (vanguardDistToMain == -1) vanguardDistToMain = vanguard->getDistance(mainPosition);

    if (vanguardDistToTarget > 0 || vanguardDistToMain > 0)
    {
        percentageToEnemyMain = (double)vanguardDistToMain / (double)(vanguardDistToMain + vanguardDistToTarget);
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

std::string UnitCluster::getCurrentActivity() const
{
    return ActivityNames[currentActivity];
}

std::string UnitCluster::getCurrentSubActivity() const
{
    return SubActivityNames[currentSubActivity];
}
