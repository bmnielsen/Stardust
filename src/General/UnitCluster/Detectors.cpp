#include "Squad.h"

#include "Players.h"
#include "Map.h"
#include "Geo.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

/*
 * For now detectors just try to keep detection on the closest unit to them requiring detection. If there are no units requiring detection,
 * they try to stay on top of the squad vanguard unit (cluster vanguard unit closest to the target position).
 *
 * In both cases, they try to avoid being in areas where the enemy has both detection and an anti-air threat.
 */

namespace
{
    const int HALT_DISTANCE = UnitHelper::HaltDistance(BWAPI::UnitTypes::Protoss_Observer) + 16;

    BWAPI::Position scaledPosition(BWAPI::Position currentPosition, BWAPI::Position vector, int length)
    {
        auto scaledVector = Geom::ScaleVector(vector, length);
        if (scaledVector == BWAPI::Positions::Invalid) return BWAPI::Positions::Invalid;

        return currentPosition + scaledVector;
    }

    void moveAwayFrom(OwnedUnit &detector, BWAPI::Position target)
    {
        /* Move in the opposite direction */
        auto behind = scaledPosition(detector->prevPos, detector->prevPos - target, HALT_DISTANCE);
        if (behind.isValid())
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(detector->ident) << "Moving away from target @ " << BWAPI::WalkPosition(target);
#endif
            detector->moveTo(behind);
            return;
        }

#if DEBUG_UNIT_ORDERS
        Telemetry::log(detector->ident) << "Cannot move away from target @ " << BWAPI::WalkPosition(target) << "; default to main";
#endif

        /* Default to main base location when we don't have anywhere better to go */
        detector->moveTo(Map::getMyMain()->getPosition());
    }

    void moveTowards(OwnedUnit &detector, BWAPI::Position target, BWAPI::Position threatDirection)
    {
        /* Check for threats one-and-a-half tiles ahead */
        auto ahead = scaledPosition(detector->prevPos, target - detector->prevPos, 48);
#if DEBUG_UNIT_ORDERS
        Telemetry::log(detector->ident) << "Me " << detector->prevPos << "; target " << target
                                     << "; diff " << (target - detector->prevPos) << "; scaled " << ahead;
#endif
        if (ahead.isValid())
        {
            auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
            if (grid.airThreat(ahead) > 0 && grid.detection(ahead) > 0)
            {
#if DEBUG_UNIT_ORDERS
                Telemetry::log(detector->ident) << "Threat detected, moving away from threat direction @ " << BWAPI::WalkPosition(threatDirection);
#endif
                moveAwayFrom(detector, threatDirection);
                return;
            }
        }

#if DEBUG_UNIT_ORDERS
        Telemetry::log(detector->ident) << "Moving towards target @ " << BWAPI::WalkPosition(target);
#endif
        /* Scale to our halt distance */
        auto scaledVector = Geom::ScaleVector(target - detector->prevPos, HALT_DISTANCE);
        auto scaledTarget = (scaledVector == BWAPI::Positions::Invalid) ? target : detector->prevPos + scaledVector;
        if (!scaledTarget.isValid()) scaledTarget = target;
        detector->moveTo(scaledTarget);
    }
}

void UnitSquad::executeDetectors()
{
    for (auto detector : detectors)
    {
        /* Try to find the nearest enemy requiring detection */
        Unit closest = nullptr;
        int closestDist = INT_MAX;
        for (auto &enemy : enemiesNeedingDetection)
        {
            int span = detector->getDistance(enemy);
            if (span < closestDist)
            {
                closest = enemy;
                closestDist = span;
            }
        }

        /* If we found one, either move towards it if it isn't in range, or away from it if we're too close */
        if (closest)
        {
            if (closestDist > 64)
            {
                moveTowards(detector, closest->prevPos, closest->prevPos);
            }
            else
            {
                moveAwayFrom(detector, closest->prevPos);
            }

            continue;
        }

        /* There is no enemy unit we need to detect, so try to stay with the vanguard cluster */
        if (currentVanguardCluster && currentVanguardCluster->vanguard)
        {
            auto centerToVanguardVector = currentVanguardCluster->vanguard->prevPos - currentVanguardCluster->midpoint;
            moveTowards(
                detector,
                currentVanguardCluster->vanguard->prevPos,
                currentVanguardCluster->midpoint + centerToVanguardVector + centerToVanguardVector);
            continue;
        }

        /* This is the default case where there are no units requiring detection and no friendly units, so just hang out at our base */
        /* The detector should get assigned to a different play by the strategist */
        moveTowards(detector, Map::getMyMain()->getPosition(), targetPosition);
    }
}