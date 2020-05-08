#include "Squad.h"
#include "PathFinding.h"
#include "Players.h"

#if INSTRUMENTATION_ENABLED
#define DEBUG_CLUSTER_MEMBERSHIP true // Also in UnitCluster.cpp
#endif

namespace
{
    // Units are added to a cluster if they are within this distance of the cluster center
    const int ADD_THRESHOLD = 320;

    // Clusters are combined if their centers are within this distance of each other
    const int COMBINE_THRESHOLD = 320;

    // Units are removed from a cluster if they are further than this distance from the cluster center
    // This is deliberately large to try to avoid too much reshuffling of clusters
    const int REMOVE_THRESHOLD = COMBINE_THRESHOLD + ADD_THRESHOLD;

    // Determines whether we need detection to effectively fight the given unit.
    bool unitNeedsDetection(const Unit &unit)
    {
        if (unit->type == BWAPI::UnitTypes::Zerg_Lurker || unit->type == BWAPI::UnitTypes::Zerg_Lurker_Egg) return true;
        if (unit->type.hasPermanentCloak()) return true;
        if (unit->type.isCloakable() && Players::hasResearched(unit->player, unit->type.cloakingTech())) return true;
        if (unit->type.isBurrowable() && Players::hasResearched(unit->player, BWAPI::TechTypes::Burrowing)) return true;

        return false;
    }
}

void Squad::addUnit(const MyUnit &unit)
{
    CherryVis::log(unit->id) << "Added to squad: " << label;

    if (unit->type.isDetector())
    {
        detectors.insert(unit);
    }
    else
    {
        addUnitToBestCluster(unit);
    }
}

void Squad::addUnitToBestCluster(const MyUnit &unit)
{
    // Look for a suitable cluster to add this unit to
    std::shared_ptr<UnitCluster> best = nullptr;
    int bestDist = INT_MAX;
    for (const auto &cluster : clusters)
    {
        int dist = unit->getDistance(cluster->center);
        if (dist <= ADD_THRESHOLD && dist < bestDist)
        {
            bestDist = dist;
            best = cluster;
        }
    }

    if (best)
    {
        best->addUnit(unit);
        unitToCluster[unit] = best;
        return;
    }

    auto newCluster = createCluster(unit);
    clusters.insert(newCluster);
    unitToCluster[unit] = newCluster;
}

void Squad::removeUnit(const MyUnit &unit)
{
    if (unit->type.isDetector())
    {
        auto detectorIt = detectors.find(unit);
        if (detectorIt == detectors.end()) return;

        CherryVis::log(unit->id) << "Removed from squad: " << label;

        detectors.erase(detectorIt);
        return;
    }

    auto clusterIt = unitToCluster.find(unit);
    if (clusterIt == unitToCluster.end()) return;

    CherryVis::log(unit->id) << "Removed from squad: " << label;

    auto cluster = clusterIt->second;
    unitToCluster.erase(clusterIt);

    cluster->removeUnit(unit);
    if (cluster->units.empty())
    {
        clusters.erase(cluster);
    }
}

void Squad::updateClusters()
{
    // If we have no target position, skip this
    if (!targetPosition.isValid())
    {
        Log::Get() << "ERROR: Trying to update clusters of a squad with no target position. Squad: " << label;
        return;
    }

    // Remove dead detectors
    for (auto detectorIt = detectors.begin(); detectorIt != detectors.end();)
    {
        if (!(*detectorIt)->exists())
        {
            detectorIt = detectors.erase(detectorIt);
        }
        else
        {
            detectorIt++;
        }
    }

    // Update the clusters: remove dead units, recompute position data
    for (auto clusterIt = clusters.begin(); clusterIt != clusters.end();)
    {
        (*clusterIt)->updatePositions(targetPosition);

        if ((*clusterIt)->units.empty())
        {
            clusterIt = clusters.erase(clusterIt);
        }
        else
        {
            clusterIt++;
        }
    }

    // Find clusters that should be combined
    for (auto firstIt = clusters.begin(); firstIt != clusters.end(); )
    {
        auto secondIt = firstIt;
        for (secondIt++; secondIt != clusters.end();)
        {
            int dist = PathFinding::GetGroundDistance((*firstIt)->center, (*secondIt)->center, BWAPI::UnitTypes::Protoss_Dragoon);
            if (dist > COMBINE_THRESHOLD)
            {
                secondIt++;
                continue;
            }

            // Combine into the cluster with the most units
            auto &combineInto = ((*firstIt)->units.size() >= (*secondIt)->units.size()) ? *firstIt : *secondIt;
            auto &combineFrom = ((*firstIt)->units.size() >= (*secondIt)->units.size()) ? *secondIt : *firstIt;

            combineInto->units.insert(combineFrom->units.begin(), combineFrom->units.end());
            combineInto->updatePositions(targetPosition);

            // Update cluster of the moved units
            for (const auto &movedUnit : combineFrom->units)
            {
                unitToCluster[movedUnit] = combineInto;

#if DEBUG_CLUSTER_MEMBERSHIP
                CherryVis::log(movedUnit->id) << "Combined into cluster @ " << BWAPI::WalkPosition(combineInto->center);
#endif
            }

            // Delete the appropriate cluster
            if (combineInto == *firstIt)
            {
                secondIt = clusters.erase(secondIt);
            }
            else
            {
                firstIt = clusters.erase(firstIt);
                goto continueOuterLoop;
            }
        }
        firstIt++;
        continueOuterLoop:;
    }

    // Find units that should be removed from their cluster
    for (auto &cluster : clusters)
    {
        for (auto unitIt = cluster->units.begin(); unitIt != cluster->units.end();)
        {
            if ((*unitIt)->getDistance(cluster->center) <= REMOVE_THRESHOLD)
            {
                unitIt++;
                continue;
            }

            auto unit = *unitIt;
            unitIt = cluster->removeUnit(unitIt);
            addUnitToBestCluster(unit);
        }
    }
}

void Squad::execute()
{
    enemiesNeedingDetection.clear();

    for (const auto &cluster : clusters)
    {
        execute(*cluster);
    }

    executeDetectors();
}

std::vector<MyUnit> Squad::getUnits() const
{
    std::vector<MyUnit> result(detectors.begin(), detectors.end());

    for (auto &unitAndCluster : unitToCluster)
    {
        result.push_back(unitAndCluster.first);
    }

    return result;
}

std::map<BWAPI::UnitType, int> Squad::getUnitCountByType() const
{
    std::map<BWAPI::UnitType, int> result;

    for (const auto &unitAndCluster : unitToCluster)
    {
        result[unitAndCluster.first->type]++;
    }
    for (const auto &detector : detectors)
    {
        result[detector->type]++;
    }

    return result;
}


bool Squad::hasClusterWithActivity(UnitCluster::Activity activity) const
{
    for (const auto &cluster : clusters)
    {
        if (cluster->currentActivity == activity) return true;
    }

    return false;
}

std::shared_ptr<UnitCluster> Squad::vanguardCluster(int *distToTargetPosition)
{
    int minDist = INT_MAX;
    std::shared_ptr<UnitCluster> vanguard = nullptr;
    for (const auto &cluster : clusters)
    {
        int dist = PathFinding::GetGroundDistance(cluster->vanguard->lastPosition, targetPosition);
        if (dist < minDist)
        {
            minDist = dist;
            vanguard = cluster;
        }
    }

    if (distToTargetPosition)
    {
        *distToTargetPosition = minDist;
    }

    return vanguard;
}

void Squad::updateDetectionNeeds(std::set<Unit> &enemyUnits)
{
    for (const auto &unit : enemyUnits)
    {
        if (!unit->canAttackAir() && !unit->canAttackGround()) continue;

        if (unitNeedsDetection(unit))
        {
            enemiesNeedingDetection.insert(unit);
        }
    }
}
