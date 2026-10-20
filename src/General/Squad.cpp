#include "Squad.h"
#include "Config.h"
#include "Players.h"

#include <iomanip>

#if INSTRUMENTATION_ENABLED
#define DEBUG_CLUSTER_MEMBERSHIP true // Also in UnitCluster.cpp
#endif

namespace
{
    /* Units are added to a cluster if they are within this distance of the cluster vanguard */
    const int ADD_THRESHOLD = 480;

    /* Clusters are combined if their centers are within this distance of each other, adjusted for cluster size */
    const int COMBINE_THRESHOLD = 480;

    /* Units are removed from a cluster if they are further than this distance from the cluster vanguard, adjusted for cluster size */
    const int REMOVE_THRESHOLD = 600;
}

void UnitSquad::addUnit(const OwnedUnit &gameUnit)
{
    Telemetry::log(gameUnit->ident) << "Added to squad: " << label;

    if (gameUnit->type.isDetector() && !gameUnit->type.isBuilding())
    {
        detectors.insert(gameUnit);
    }
    else if (gameUnit->type == BWAPI::UnitTypes::Protoss_Arbiter)
    {
        arbiters.insert(gameUnit);
    }
    else if (Config::stormCastEnabled() && gameUnit->type == BWAPI::UnitTypes::Protoss_High_Templar)
    {
        templars.insert(gameUnit);
    }
    else
    {
        addUnitToBestCluster(gameUnit);
    }
}

bool UnitSquad::canAddUnitToCluster(const OwnedUnit &gameUnit, const std::shared_ptr<UnitCluster> &clump, int span) const
{
    return span - clump->ballRadius <= ADD_THRESHOLD;
}

bool UnitSquad::shouldCombineClusters(const std::shared_ptr<UnitCluster> &first, const std::shared_ptr<UnitCluster> &second) const
{
    int span = first->midpoint.getApproxDistance(second->midpoint);

    /* Combine if the clusters are close enough together, adjusting for their "ball radius" */
    if (span - first->ballRadius - second->ballRadius <= COMBINE_THRESHOLD) return true;

    /* Also combine if the clusters are further apart but fit together in an arc formation */
    return span - first->lineRadius - second->lineRadius <= COMBINE_THRESHOLD &&
           abs(first->percentageToEnemyMain - second->percentageToEnemyMain) < 0.05;
}

bool UnitSquad::shouldRemoveFromCluster(const OwnedUnit &gameUnit, const std::shared_ptr<UnitCluster> &clump) const
{
    int span = gameUnit->prevPos.getApproxDistance(clump->midpoint);
    return span - clump->ballRadius > REMOVE_THRESHOLD &&
           (gameUnit->distToTargetPosition == -1 || clump->vanguard->distToTargetPosition == -1 ||
            span - clump->lineRadius > REMOVE_THRESHOLD ||
            abs(gameUnit->distToTargetPosition - clump->vanguard->distToTargetPosition) > 480);
}

void UnitSquad::addUnitToBestCluster(const OwnedUnit &gameUnit)
{
    /* Look for a suitable cluster to add this unit to */
    std::shared_ptr<UnitCluster> best = nullptr;
    int bestDist = INT_MAX;
    for (const auto &clump : clusters)
    {
        int span = gameUnit->prevPos.getApproxDistance(clump->vanguard->prevPos);
        if (span < bestDist && canAddUnitToCluster(gameUnit, clump, span))
        {
            bestDist = span;
            best = clump;
        }
    }

    if (best)
    {
        best->addUnit(gameUnit);
        unitToCluster[gameUnit] = best;
        return;
    }

    auto newCluster = createCluster(gameUnit);
    clusters.insert(newCluster);
    unitToCluster[gameUnit] = newCluster;
}

void UnitSquad::removeUnit(const OwnedUnit &gameUnit)
{
    if (gameUnit->type.isDetector())
    {
        auto detectorIt = detectors.find(gameUnit);
        if (detectorIt == detectors.end()) return;

        Telemetry::log(gameUnit->ident) << "Removed from squad: " << label;

        detectors.erase(detectorIt);
        return;
    }

    if (gameUnit->type == BWAPI::UnitTypes::Protoss_Arbiter)
    {
        auto arbiterIt = arbiters.find(gameUnit);
        if (arbiterIt == arbiters.end()) return;

        Telemetry::log(gameUnit->ident) << "Removed from squad: " << label;

        arbiters.erase(arbiterIt);
        return;
    }

    if (Config::stormCastEnabled() && gameUnit->type == BWAPI::UnitTypes::Protoss_High_Templar)
    {
        auto templarIt = templars.find(gameUnit);
        if (templarIt == templars.end()) return;

        Telemetry::log(gameUnit->ident) << "Removed from squad: " << label;

        templars.erase(templarIt);
        return;
    }

    auto clusterIt = unitToCluster.find(gameUnit);
    if (clusterIt == unitToCluster.end()) return;

    Telemetry::log(gameUnit->ident) << "Removed from squad: " << label;

    auto clump = clusterIt->second;
    unitToCluster.erase(clusterIt);

    auto unitIt = clump->units.find(gameUnit);
    if (unitIt != clump->units.end())
    {
        clump->removeUnit(unitIt, targetPosition);

#if DEBUG_CLUSTER_MEMBERSHIP
        Telemetry::log(gameUnit->ident) << "Removed from cluster @ " << BWAPI::WalkPosition(clump->midpoint);
#endif

        if (clump->units.empty())
        {
            clusters.erase(clump);
        }
    }
}

void UnitSquad::updateClusters()
{
    /* If we have no target position, skip this */
    if (!targetPosition.isValid())
    {
        AppLog::Get() << "ERROR: Trying to update clusters of a squad with no target position. Squad: " << label;
        return;
    }

    /* Remove dead detectors */
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

    /* Remove dead arbiters */
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

    /* Remove dead templars */
    for (auto templarIt = templars.begin(); templarIt != templars.end();)
    {
        if (!(*templarIt)->exists())
        {
            templarIt = templars.erase(templarIt);
        }
        else
        {
            templarIt++;
        }
    }

    /* Update the clusters: remove dead units, recompute position data */
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

    /* Find clusters that should be combined */
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

            /* Combine into the cluster closest to the target */
            auto &combineInto = ((*firstIt)->vanguardDistToTarget < (*secondIt)->vanguardDistToTarget) ? *firstIt : *secondIt;
            auto &combineFrom = ((*firstIt)->vanguardDistToTarget < (*secondIt)->vanguardDistToTarget) ? *secondIt : *firstIt;

            combineInto->absorbCluster(combineFrom, targetPosition);

            /* Update cluster of the moved units */
            for (const auto &movedUnit : combineFrom->units)
            {
                unitToCluster[movedUnit] = combineInto;

#if DEBUG_CLUSTER_MEMBERSHIP
                Telemetry::log(movedUnit->ident) << "Combined into cluster @ " << BWAPI::WalkPosition(combineInto->midpoint);
#endif
            }

            /* Delete the appropriate cluster */
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

    /* Find units that should be removed from their cluster */
    for (auto &clump : clusters)
    {
        for (auto unitIt = clump->units.begin(); unitIt != clump->units.end();)
        {
            if (!shouldRemoveFromCluster(*unitIt, clump))
            {
                unitIt++;
                continue;
            }

            auto gameUnit = *unitIt;
            unitIt = clump->removeUnit(unitIt, targetPosition);
            addUnitToBestCluster(gameUnit);
        }
    }

    /* Find the vanguard cluster */
    currentVanguardCluster = nullptr;
    vanguardClusterDistToTargetPosition = INT_MAX;
    for (const auto &clump : clusters)
    {
        clump->isVanguardCluster = false;

        if (clump->vanguardDistToTarget < vanguardClusterDistToTargetPosition)
        {
            vanguardClusterDistToTargetPosition = clump->vanguardDistToTarget;
            currentVanguardCluster = clump;
        }
    }
    if (currentVanguardCluster) currentVanguardCluster->isVanguardCluster = true;

#if INSTRUMENTATION_ENABLED_VERBOSE
    for (const auto &clump : clusters)
    {
        Telemetry::drawCircle(clump->midpoint.x, clump->midpoint.y, clump->ballRadius, Telemetry::DrawColor::Teal);
        Telemetry::drawCircle(clump->midpoint.x, clump->midpoint.y, clump->lineRadius, Telemetry::DrawColor::Blue);
        Telemetry::drawCircle(clump->vanguard->prevPos.x, clump->vanguard->prevPos.y, 32, Telemetry::DrawColor::Grey);
    }
#endif
}

bool UnitSquad::hasDetection() const
{
    if (detectors.empty()) return false;
    if (enemiesNeedingDetection.empty()) return true;

    /* We consider the squad to have detection if an observer is near the vanguard unit */
    if (currentVanguardCluster && currentVanguardCluster->vanguard && std::any_of(
                detectors.begin(),
                detectors.end(),
                [&](const OwnedUnit &detector)
                {
                    return detector->getDistance(currentVanguardCluster->vanguard) < 120;
                }))
    {
        return true;
    }

    /* Otherwise we consider the squad to have detection if one of the enemies needing detection has been detected */
    auto &grid = PlayerInfo::grid(BWAPI::Broodwar->self());
    return std::any_of(
            enemiesNeedingDetection.begin(),
            enemiesNeedingDetection.end(),
            [&grid](const Unit &enemyNeedingDetection)
            {
                return grid.detection(enemyNeedingDetection->prevPos) > 0;
            });
}

void UnitSquad::execute()
{
    enemiesNeedingDetection.clear();

    for (const auto &clump : clusters)
    {
        execute(*clump);
    }

    executeDetectors();
    executeArbiters();
    executeTemplars();
}

std::vector<OwnedUnit> UnitSquad::getUnits() const
{
    std::vector<OwnedUnit> outcome(detectors.begin(), detectors.end());
    outcome.insert(outcome.end(), arbiters.begin(), arbiters.end());
    outcome.insert(outcome.end(), templars.begin(), templars.end());

    for (auto &unitAndCluster : unitToCluster)
    {
        outcome.push_back(unitAndCluster.first);
    }

    return outcome;
}

bool UnitSquad::empty() const
{
    return detectors.empty() && arbiters.empty() && templars.empty() && unitToCluster.empty();
}

int UnitSquad::combatUnitCount() const
{
    return (int)unitToCluster.size();
}

std::map<BWAPI::UnitType, int> UnitSquad::getUnitCountByType() const
{
    std::map<BWAPI::UnitType, int> outcome;

    for (const auto &unitAndCluster : unitToCluster)
    {
        outcome[unitAndCluster.first->type]++;
    }
    for (const auto &detector : detectors)
    {
        outcome[detector->type]++;
    }
    for (const auto &arbiter : arbiters)
    {
        outcome[arbiter->type]++;
    }
    for (const auto &templar : templars)
    {
        outcome[templar->type]++;
    }

    return outcome;
}


bool UnitSquad::hasClusterWithActivity(UnitCluster::Activity activity) const
{
    return std::any_of(clusters.begin(), clusters.end(), [&activity](const auto &clump)
    {
        return clump->currentActivity == activity;
    });
}

std::shared_ptr<UnitCluster> UnitSquad::vanguardCluster(int *distToTargetPosition) const
{
    if (distToTargetPosition)
    {
        *distToTargetPosition = vanguardClusterDistToTargetPosition;
    }

    return currentVanguardCluster;
}

bool UnitSquad::canReassignFromVanguardCluster(OwnedUnit &gameUnit) const
{
    /* Allow if the unit isn't in the vanguard cluster */
    auto clusterIt = unitToCluster.find(gameUnit);
    if (clusterIt == unitToCluster.end()) return true;
    if (clusterIt->second != currentVanguardCluster) return true;

    /* Allow if the vanguard cluster has lots of units */
    if (currentVanguardCluster->units.size() > 20) return true;

    /* Allow if the vanguard cluster is not attacking or fleeing */
    return currentVanguardCluster->currentActivity != UnitCluster::Activity::Attacking &&
           (currentVanguardCluster->currentActivity != UnitCluster::Activity::Regrouping ||
            currentVanguardCluster->currentSubActivity != UnitCluster::SubActivity::Flee);
}

void UnitSquad::updateDetectionNeeds(std::set<Unit> &enemyUnits)
{
    for (const auto &gameUnit : enemyUnits)
    {
        if (!gameUnit->canAttackAir() && !gameUnit->canAttackGround()) continue;

        if (gameUnit->needsDetection())
        {
            enemiesNeedingDetection.insert(gameUnit);
        }
    }
}

void UnitSquad::addInstrumentation(nlohmann::json &squadArray) const
{
    nlohmann::json clusterArray;
    for (auto &clump : clusters) clump->addInstrumentation(clusterArray);

    squadArray.push_back({
        {"label", label},
        {"target_position_x", targetPosition.x},
        {"target_position_y", targetPosition.y},
        {"count_combatUnits", combatUnitCount()},
        {"count_cannons", getUnitCountByType()[BWAPI::UnitTypes::Protoss_Photon_Cannon]},
        {"count_observers", detectors.size()},
        {"count_arbiters", arbiters.size()},
        {"clusters", clusterArray}
    });
}
