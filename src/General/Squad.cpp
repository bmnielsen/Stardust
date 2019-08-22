#include "Squad.h"
#include "PathFinding.h"

#define DEBUG_CLUSTER_MEMBERSHIP false

namespace {
    // Units are added to a cluster if they are within this distance of the cluster center
    const int ADD_THRESHOLD = 320;

    // Clusters are combined if their centers are within this distance of each other
    const int COMBINE_THRESHOLD = 320;

    // Units are removed from a cluster if they are further than this distance from the cluster center
    const int REMOVE_THRESHOLD = COMBINE_THRESHOLD + ADD_THRESHOLD*2;
}

void Squad::addUnit(BWAPI::Unit unit)
{
    CherryVis::log(unit) << "Added to squad: " << label;

    addUnitToBestCluster(unit);
}

void Squad::addUnitToBestCluster(BWAPI::Unit unit)
{
    // Look for a suitable cluster to add this unit to
    std::shared_ptr<UnitCluster> best = nullptr;
    int bestDist = INT_MAX;
    for (const auto& cluster : clusters)
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
#if DEBUG_CLUSTER_MEMBERSHIP
        CherryVis::log(unit) << "Added to cluster " << BWAPI::WalkPosition(best->center);
#endif
        return;
    }

    auto newCluster = createCluster(unit);
    clusters.insert(newCluster);
    unitToCluster[unit] = newCluster;
#if DEBUG_CLUSTER_MEMBERSHIP
    CherryVis::log(unit) << "Added to new cluster " << BWAPI::WalkPosition(newCluster->center);
#endif
}

void Squad::removeUnit(BWAPI::Unit unit)
{
    auto clusterIt = unitToCluster.find(unit);
    if (clusterIt == unitToCluster.end()) return;
    
    CherryVis::log(unit) << "Removed from squad: " << label;

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
    // Update the clusters: remove dead units, recompute position data
    for (auto clusterIt = clusters.begin(); clusterIt != clusters.end(); )
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
    // Clusters decide themselves if they should be split while doing their micro
    for (auto firstIt = clusters.begin(); firstIt != clusters.end(); firstIt++)
    {
        auto secondIt = firstIt;
        for (secondIt++; secondIt != clusters.end(); )
        {
            int dist = PathFinding::GetGroundDistance((*firstIt)->center, (*secondIt)->center, BWAPI::UnitTypes::Protoss_Dragoon);
            if (dist > COMBINE_THRESHOLD)
            {
                secondIt++;
                continue;
            }

            (*firstIt)->units.insert((*secondIt)->units.begin(), (*secondIt)->units.end());
            (*firstIt)->updatePositions(targetPosition);
            secondIt = clusters.erase(secondIt);
        }
    }

    // Find units that should be removed from their cluster
    for (auto &cluster : clusters)
    {
        for (auto unitIt = cluster->units.begin(); unitIt != cluster->units.end(); )
        {
            if ((*unitIt)->getDistance(cluster->center) <= REMOVE_THRESHOLD)
            {
                unitIt++;
                continue;
            }

            auto unit = *unitIt;
            unitIt = cluster->removeUnit(unitIt);
#if DEBUG_CLUSTER_MEMBERSHIP
            CherryVis::log(unit) << "Removed from cluster " << BWAPI::WalkPosition(cluster->center);
#endif
            addUnitToBestCluster(unit);
        }
    }
}

void Squad::execute()
{
    for (const auto& cluster : clusters)
    {
        execute(*cluster);
    }
}

std::vector<BWAPI::Unit> Squad::getUnits()
{
    std::vector<BWAPI::Unit> result;

    for (auto & unitAndCluster : unitToCluster)
    {
        result.push_back(unitAndCluster.first);
    }

    return result;
}
