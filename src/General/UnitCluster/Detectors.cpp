#include "Squad.h"

#include "Players.h"
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
    void moveAwayFrom(MyUnit &detector, BWAPI::Position target)
    {
        // Move towards two tiles in the opposite direction
        auto behind = Geo::ScaleVector(detector->lastPosition - target, 64);
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

    void moveTowards(MyUnit &detector, BWAPI::Position target)
    {
        // Check for threats one-and-a-half tiles ahead
        auto ahead = Geo::ScaleVector(target - detector->lastPosition, 48);
        if (ahead.isValid())
        {
            auto grid = Players::grid(BWAPI::Broodwar->enemy());
            if (grid.airThreat(ahead) > 0 && grid.detection(ahead) > 0)
            {
#if DEBUG_UNIT_ORDERS
                CherryVis::log(detector->id) << "Threat detected, moving away from target @ " << BWAPI::WalkPosition(target);
#endif
                moveAwayFrom(detector, target);
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
    Unit vanguard = nullptr;

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
                moveTowards(detector, closest->lastPosition);
            }
            else
            {
                moveAwayFrom(detector, closest->lastPosition);
            }

            continue;
        }

        // There is no enemy unit we need to detect, so try to stay at the front
        if (!vanguard)
        {
            closestDist = INT_MAX;
            for (auto &cluster : clusters)
            {
                int dist = cluster->vanguard->getDistance(targetPosition);
                if (dist < closestDist)
                {
                    vanguard = cluster->vanguard;
                    closestDist = dist;
                }
            }
        }
        if (vanguard)
        {
            moveTowards(detector, vanguard->lastPosition);
            continue;
        }

        // This is the default case where there are no units requiring detection and no friendly units, so just hang out at our base
        // The detector should get assigned to a different play by the strategist
        moveTowards(detector, Map::getMyMain()->getPosition());
    }
}