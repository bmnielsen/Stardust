#include "Squad.h"
#include "PathFinding.h"
#include "Players.h"

#include "DebugFlag_CombatSim.h"

#include <iomanip>

#if INSTRUMENTATION_ENABLED
#define DEBUG_CLUSTER_MEMBERSHIP true // Also in UnitCluster.cpp
#endif

namespace
{
    // Units are added to a cluster if they are within this distance of the cluster center
    const int ADD_THRESHOLD = 480;

    // Clusters are combined if their centers are within this distance of each other, adjusted for cluster size
    const int COMBINE_THRESHOLD = 480;

    // Units are removed from a cluster if they are further than this distance from the cluster center, adjusted for cluster size
    const int REMOVE_THRESHOLD = 640;
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

bool Squad::canAddUnitToCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster, int dist) const
{
    return dist <= ADD_THRESHOLD;
}

bool Squad::shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const
{
    return PathFinding::GetGroundDistance(first->center, second->center, BWAPI::UnitTypes::Protoss_Dragoon) <=
           (COMBINE_THRESHOLD + (first->units.size() + second->units.size()) * 32);
}

bool Squad::shouldRemoveFromCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster) const
{
    return unit->getDistance(cluster->center) > (REMOVE_THRESHOLD + cluster->units.size() * 32);
}

void Squad::addUnitToBestCluster(const MyUnit &unit)
{
    // Look for a suitable cluster to add this unit to
    std::shared_ptr<UnitCluster> best = nullptr;
    int bestDist = INT_MAX;
    for (const auto &cluster : clusters)
    {
        int dist = unit->getDistance(cluster->center);
        if (dist < bestDist && canAddUnitToCluster(unit, cluster, dist))
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

    auto unitIt = cluster->units.find(unit);
    if (unitIt != cluster->units.end())
    {
        cluster->removeUnit(unitIt, targetPosition);

#if DEBUG_CLUSTER_MEMBERSHIP
        CherryVis::log(unit->id) << "Removed from cluster @ " << BWAPI::WalkPosition(cluster->center);
#endif

        if (cluster->units.empty())
        {
            clusters.erase(cluster);
        }
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
    for (auto firstIt = clusters.begin(); firstIt != clusters.end();)
    {
        auto secondIt = firstIt;
        for (secondIt++; secondIt != clusters.end();)
        {
            if (!shouldCombineClusters(*firstIt, *secondIt))
            {
                secondIt++;
                continue;
            }

            // Combine into the cluster with the most units
            auto &combineInto = ((*firstIt)->units.size() >= (*secondIt)->units.size()) ? *firstIt : *secondIt;
            auto &combineFrom = ((*firstIt)->units.size() >= (*secondIt)->units.size()) ? *secondIt : *firstIt;

            combineInto->absorbCluster(combineFrom, targetPosition);

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
            if (!shouldRemoveFromCluster(*unitIt, cluster))
            {
                unitIt++;
                continue;
            }

            auto unit = *unitIt;
            unitIt = cluster->removeUnit(unitIt, targetPosition);
            addUnitToBestCluster(unit);
        }
    }

    // Find the vanguard cluster
    currentVanguardCluster = nullptr;
    vanguardClusterDistToTargetPosition = INT_MAX;
    for (const auto &cluster : clusters)
    {
        cluster->isVanguardCluster = false;

        if (cluster->vanguardDistToTarget < vanguardClusterDistToTargetPosition)
        {
            vanguardClusterDistToTargetPosition = cluster->vanguardDistToTarget;
            currentVanguardCluster = cluster;
        }
    }
    if (currentVanguardCluster) currentVanguardCluster->isVanguardCluster = true;

#if INSTRUMENTATION_ENABLED
    std::vector<std::string> values;
    for (const auto &cluster : clusters)
    {
        std::ostringstream os;
        os << "center: " << BWAPI::WalkPosition(cluster->center)
           << "\nactivity: " << cluster->getCurrentActivity()
           << "\nsub-activity: " << cluster->getCurrentSubActivity()
           << "\ntarget: " << BWAPI::WalkPosition(targetPosition);
        if (cluster == currentVanguardCluster)
        {
            os << "\n*Vanguard*";
        }

        values.push_back(os.str());
    }
    CherryVis::setBoardListValue((std::ostringstream() << label << "_clusters").str(), values);
#endif
}

void Squad::execute()
{
    enemiesNeedingDetection.clear();

    for (const auto &cluster : clusters)
    {
        execute(*cluster);

#if DEBUG_COMBATSIM
        std::ostringstream dbg;

        dbg << label;

        dbg << "\n" << cluster->getCurrentActivity();
        if (cluster->currentSubActivity != UnitCluster::SubActivity::None) dbg << "-" << cluster->getCurrentSubActivity();

        auto addSimResult = [&dbg](const std::pair<CombatSimResult, bool> &simResult)
        {
            if (simResult.first.frame != BWAPI::Broodwar->getFrameCount()) return;
            
            dbg << "\n"
                << simResult.first.initialMine << "," << simResult.first.initialEnemy
                << "-" << simResult.first.finalMine << "," << simResult.first.finalEnemy
                << std::setprecision(2)
                << ": %l=" << simResult.first.myPercentLost()
                << "; vg=" << simResult.first.valueGain()
                << "; %g=" << simResult.first.percentGain()
                << (simResult.second ? "; ATTACK" : "; RETREAT");
        };

        if (!cluster->recentSimResults.empty()) addSimResult(*cluster->recentSimResults.rbegin());
        if (!cluster->recentRegroupSimResults.empty()) addSimResult(*cluster->recentRegroupSimResults.rbegin());

        CherryVis::drawText(cluster->center.x, cluster->center.y - 24, dbg.str());
#endif
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

std::shared_ptr<UnitCluster> Squad::vanguardCluster(int *distToTargetPosition) const
{
    if (distToTargetPosition)
    {
        *distToTargetPosition = vanguardClusterDistToTargetPosition;
    }

    return currentVanguardCluster;
}

bool Squad::isInVanguardCluster(MyUnit &unit) const
{
    auto clusterIt = unitToCluster.find(unit);
    if (clusterIt == unitToCluster.end()) return false;

    return clusterIt->second == currentVanguardCluster;
}

void Squad::updateDetectionNeeds(std::set<Unit> &enemyUnits)
{
    for (const auto &unit : enemyUnits)
    {
        if (!unit->canAttackAir() && !unit->canAttackGround()) continue;

        if (unit->needsDetection())
        {
            enemiesNeedingDetection.insert(unit);
        }
    }
}
