#include "Squad.h"
#include "PathFinding.h"

const int MAX_CLUSTER_RADIUS = 320;

void Squad::addUnit(BWAPI::Unit unit)
{
    CherryVis::log(unit) << "Added: " << label;

    // Look for a suitable cluster to add this unit to
    std::shared_ptr<UnitCluster> best = nullptr;
    int bestDist = INT_MAX;
    for (auto cluster : clusters)
    {
        int dist = unit->getDistance(cluster->center);
        if (dist <= MAX_CLUSTER_RADIUS && dist < bestDist)
        {
            bestDist = dist;
            best = cluster;
        }
    }

    if (best)
    {
        best->addUnit(unit);
        return;
    }

    auto newCluster = std::make_shared<UnitCluster>(unit);
    clusters.insert(newCluster);
    unitToCluster[unit] = newCluster;
}

void Squad::removeUnit(BWAPI::Unit unit)
{
    auto clusterIt = unitToCluster.find(unit);
    if (clusterIt == unitToCluster.end()) return;
    
    CherryVis::log(unit) << "Removed: " << label;

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
        (*clusterIt)->update(targetPosition);

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
            if (dist > MAX_CLUSTER_RADIUS)
            {
                secondIt++;
                continue;
            }

            (*firstIt)->units.insert((*secondIt)->units.begin(), (*secondIt)->units.end());
            (*firstIt)->update(targetPosition);
            secondIt = clusters.erase(secondIt);
        }
    }
}

void Squad::execute()
{
    for (auto cluster : clusters)
    {
        execute(*cluster);
    }
}
