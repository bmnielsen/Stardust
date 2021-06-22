#include "Squad.h"
#include "Players.h"

#include "DebugFlag_CombatSim.h"

#include <iomanip>

#if INSTRUMENTATION_ENABLED
#define DEBUG_CLUSTER_MEMBERSHIP true // Also in UnitCluster.cpp
#endif

namespace
{
    // Units are added to a cluster if they are within this distance of the cluster vanguard
    const int ADD_THRESHOLD = 480;

    // Clusters are combined if their centers are within this distance of each other, adjusted for cluster size
    const int COMBINE_THRESHOLD = 480;

    // Units are removed from a cluster if they are further than this distance from the cluster vanguard, adjusted for cluster size
    const int REMOVE_THRESHOLD = 600;
}

void Squad::addUnit(const MyUnit &unit)
{
    CherryVis::log(unit->id) << "Added to squad: " << label;

    if (unit->type.isDetector())
    {
        detectors.insert(unit);
    }
    else if (unit->type == BWAPI::UnitTypes::Protoss_Arbiter)
    {
        arbiters.insert(unit);
    }
    else
    {
        addUnitToBestCluster(unit);
    }
}

bool Squad::canAddUnitToCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster, int dist) const
{
    return dist - cluster->ballRadius <= ADD_THRESHOLD;
}

bool Squad::shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const
{
    int dist = first->center.getApproxDistance(second->center);

    // Combine if the clusters are close enough together, adjusting for their "ball radius"
    if (dist - first->ballRadius - second->ballRadius <= COMBINE_THRESHOLD) return true;

    // Also combine if the clusters are further apart but fit together in an arc formation
    return dist - first->lineRadius - second->lineRadius <= COMBINE_THRESHOLD &&
           abs(first->percentageToEnemyMain - second->percentageToEnemyMain) < 0.05;
}

bool Squad::shouldRemoveFromCluster(const MyUnit &unit, const std::shared_ptr<UnitCluster> &cluster) const
{
    int dist = unit->lastPosition.getApproxDistance(cluster->center);
    return dist - cluster->ballRadius > REMOVE_THRESHOLD &&
           (unit->distToTargetPosition == -1 || cluster->vanguard->distToTargetPosition == -1 ||
            dist - cluster->lineRadius > REMOVE_THRESHOLD ||
            abs(unit->distToTargetPosition - cluster->vanguard->distToTargetPosition) > 480);
}

void Squad::addUnitToBestCluster(const MyUnit &unit)
{
    // Look for a suitable cluster to add this unit to
    std::shared_ptr<UnitCluster> best = nullptr;
    int bestDist = INT_MAX;
    for (const auto &cluster : clusters)
    {
        int dist = unit->lastPosition.getApproxDistance(cluster->vanguard->lastPosition);
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

    if (unit->type == BWAPI::UnitTypes::Protoss_Arbiter)
    {
        auto arbiterIt = arbiters.find(unit);
        if (arbiterIt == arbiters.end()) return;

        CherryVis::log(unit->id) << "Removed from squad: " << label;

        arbiters.erase(arbiterIt);
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

    // Remove dead arbiters
    for (auto arbiterIt = arbiters.begin(); arbiterIt != arbiters.end();)
    {
        if (!(*arbiterIt)->exists())
        {
            arbiterIt = arbiters.erase(arbiterIt);
        }
        else
        {
            arbiterIt++;
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

            // Combine into the cluster closest to the target
            auto &combineInto = ((*firstIt)->vanguardDistToMain < (*secondIt)->vanguardDistToMain) ? *firstIt : *secondIt;
            auto &combineFrom = ((*firstIt)->vanguardDistToMain < (*secondIt)->vanguardDistToMain) ? *secondIt : *firstIt;

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
           << "\ntarget: " << BWAPI::WalkPosition(targetPosition)
           << "\n%dist: " << std::setprecision(2) << cluster->percentageToEnemyMain;
        if (cluster == currentVanguardCluster)
        {
            os << "\n*Vanguard*";
        }

        values.push_back(os.str());

        CherryVis::drawCircle(cluster->center.x, cluster->center.y, cluster->ballRadius, CherryVis::DrawColor::Teal);
        CherryVis::drawCircle(cluster->center.x, cluster->center.y, cluster->lineRadius, CherryVis::DrawColor::Blue);
        CherryVis::drawCircle(cluster->vanguard->lastPosition.x, cluster->vanguard->lastPosition.y, 32, CherryVis::DrawColor::Grey);
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
                << std::setprecision(2);
            if (simResult.first.distanceFactor > -0.1)
            {
                dbg << "; d=" << simResult.first.distanceFactor;
            }
            if (simResult.first.aggression > -0.1)
            {
                dbg << "; a=" << simResult.first.aggression;
            }
            if (simResult.first.closestReinforcements > -0.1)
            {
                dbg << "; cr=" << simResult.first.closestReinforcements;
            }
            if (simResult.first.reinforcementPercentage > -0.1)
            {
                dbg << "; r%=" << simResult.first.reinforcementPercentage;
            }
            dbg << "; %l=" << simResult.first.myPercentLost()
                << "; vg=" << simResult.first.valueGain()
                << "; %g=" << simResult.first.percentGain()
                << "; %t=" << simResult.first.myPercentageOfTotal();
            if (simResult.second) dbg << "; ATCK";
        };

        if (!cluster->recentSimResults.empty()) addSimResult(*cluster->recentSimResults.rbegin());
        if (!cluster->recentRegroupSimResults.empty()) addSimResult(*cluster->recentRegroupSimResults.rbegin());

        CherryVis::drawText(cluster->center.x, cluster->center.y - 24, dbg.str());
#endif
    }

    executeDetectors();
    executeArbiters();
}

std::vector<MyUnit> Squad::getUnits() const
{
    std::vector<MyUnit> result(detectors.begin(), detectors.end());
    result.insert(result.end(), arbiters.begin(), arbiters.end());

    for (auto &unitAndCluster : unitToCluster)
    {
        result.push_back(unitAndCluster.first);
    }

    return result;
}

bool Squad::empty() const
{
    return detectors.empty() && arbiters.empty() && unitToCluster.empty();
}

int Squad::combatUnitCount() const
{
    return unitToCluster.size();
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
    for (const auto &arbiter : arbiters)
    {
        result[arbiter->type]++;
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

bool Squad::canReassignFromVanguardCluster(MyUnit &unit) const
{
    // Allow if the unit isn't in the vanguard cluster
    auto clusterIt = unitToCluster.find(unit);
    if (clusterIt == unitToCluster.end()) return true;
    if (clusterIt->second != currentVanguardCluster) return true;

    // Allow if the vanguard cluster has lots of units
    if (currentVanguardCluster->units.size() > 20) return true;

    // Allow if the vanguard cluster is not attacking or fleeing
    return currentVanguardCluster->currentActivity != UnitCluster::Activity::Attacking &&
           (currentVanguardCluster->currentActivity != UnitCluster::Activity::Regrouping ||
            currentVanguardCluster->currentSubActivity != UnitCluster::SubActivity::Flee);
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
