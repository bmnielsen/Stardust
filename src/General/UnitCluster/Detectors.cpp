#include "Squad.h"

#include "Players.h"
#include "PathFinding.h"
#include "Map.h"
#include "Geo.h"

/*
 * For now detectors just try to keep detection on the closest unit to them requiring detection. If there are no units requiring detection,
 * they try to stay on top of the squad vanguard unit (cluster vanguard unit closest to the target position).
 *
 * In both cases, they try to avoid being in areas where the enemy has both detection and an anti-air threat.
 */

namespace
{
    BWAPI::Position scaledPosition(BWAPI::Position currentPosition, BWAPI::Position vector, int length)
    {
        auto scaledVector = Geo::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void moveAwayFrom(MyUnit &detector, BWAPI::Position target)
    {
        // Move towards two tiles in the opposite direction
        auto behind = scaledPosition(detector->lastPosition, detector->lastPosition - target, 64);
        if (behind.isValid())
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(detector->id) << "Moving away from target @ " << BWAPI::WalkPosition(target);
#endif
            detector->moveTo(behind);
            return;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(detector->id) << "Cannot move away from target @ " << BWAPI::WalkPosition(target) << "; default to main";
#endif

        // Default to main base location when we don't have anywhere better to go
        detector->moveTo(Map::getMyMain()->getPosition());
    }

    void moveTowards(MyUnit &detector, BWAPI::Position target, BWAPI::Position threatDirection)
    {
        // Check for threats one-and-a-half tiles ahead
        auto ahead = scaledPosition(detector->lastPosition, target - detector->lastPosition, 48);
#if DEBUG_UNIT_ORDERS
        CherryVis::log(detector->id) << "Me " << detector->lastPosition << "; target " << target
                                     << "; diff " << (target - detector->lastPosition) << "; scaled " << ahead;
#endif
        if (ahead.isValid())
        {
            auto grid = Players::grid(BWAPI::Broodwar->enemy());
            if (grid.airThreat(ahead) > 0 && grid.detection(ahead) > 0)
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(detector->id) << "Threat detected, moving away from threat direction @ " << BWAPI::WalkPosition(threatDirection);
#endif
                moveAwayFrom(detector, threatDirection);
                return;
            }
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(detector->id) << "Moving towards target @ " << BWAPI::WalkPosition(target);
#endif
        detector->moveTo(target);
    }
}

void Squad::executeDetectors()
{
    std::shared_ptr<UnitCluster> bestCluster = nullptr;

    for (auto detector : detectors)
    {
        // Try to find the nearest enemy requiring detection
        Unit closest = nullptr;
        int closestDist = INT_MAX;
        for (auto &enemy : enemiesNeedingDetection)
        {
            int dist = detector->getDistance(enemy);
            if (dist < closestDist)
            {
                closest = enemy;
                closestDist = dist;
            }
        }

        // If we found one, either move towards it if it isn't in range, or away from it if we're too close
        if (closest)
        {
            int sightRange = Players::unitSightRange(BWAPI::Broodwar->self(), detector->type);
            if (closestDist > sightRange - 32)
            {
                moveTowards(detector, closest->lastPosition, closest->lastPosition);
            }
            else
            {
                moveAwayFrom(detector, closest->lastPosition);
            }

            continue;
        }

        // There is no enemy unit we need to detect, so try to stay with the frontmost cluster
        if (!bestCluster)
        {
            closestDist = INT_MAX;
            for (auto &cluster : clusters)
            {
                int dist = PathFinding::GetGroundDistance(cluster->vanguard->lastPosition, targetPosition);
                if (dist == -1) dist = cluster->center.getApproxDistance(targetPosition);
                if (dist < closestDist)
                {
                    closestDist = dist;
                    bestCluster = cluster;
                }
            }
        }
        if (bestCluster)
        {
            moveTowards(detector, bestCluster->center, bestCluster->vanguard->lastPosition);
            continue;
        }

        // This is the default case where there are no units requiring detection and no friendly units, so just hang out at our base
        // The detector should get assigned to a different play by the strategist
        moveTowards(detector, Map::getMyMain()->getPosition(), targetPosition);
    }
}