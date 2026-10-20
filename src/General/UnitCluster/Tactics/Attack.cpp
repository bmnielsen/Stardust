#include "UnitCluster.h"

#include "Map.h"
#include "Players.h"
#include "Geo.h"
#include "UnitUtil.h"

#include "DebugFlag_UnitOrders.h"

void UnitCluster::attack(std::vector<std::pair<OwnedUnit, Unit>> &unitsAndTargets, BWAPI::Position targetPosition)
{
    /* If this map has chokes that may need to be cleared, check if this cluster needs to do so to reach its target */
    if (Map::mapSpecificOverride()->hasAttackClearableChokes())
    {
        /* We don't bother if any enemy unit is already in range of a unit in the cluster */
        bool anyUnitInRange = false;
        for (const auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.second && unitAndTarget.first->isInOurWeaponRange(unitAndTarget.second))
            {
                anyUnitInRange = true;
                break;
            }
        }

        if (!anyUnitInRange && Map::mapSpecificOverride()->clusterMove(*this, targetPosition))
        {
            return;
        }
    }

    /* Form an arc if none of our units are in danger or in range yet */
    auto &grid = PlayerInfo::grid(BWAPI::Broodwar->enemy());
    Unit vanguardTarget = nullptr;
    bool canFormArc = true;
    for (auto &unitAndTarget : unitsAndTargets)
    {
        if ((unitAndTarget.first->isFlying
             ? grid.airThreat(unitAndTarget.first->prevPos)
             : grid.groundThreat(unitAndTarget.first->prevPos)) > 0 ||
            (unitAndTarget.second && unitAndTarget.first->isInOurWeaponRange(unitAndTarget.second)))
        {
            canFormArc = false;
            break;
        }

        if (unitAndTarget.first == vanguard) vanguardTarget = unitAndTarget.second;
    }
    canFormArc = canFormArc && vanguardTarget && vanguardTarget->canAttack(vanguard);

    /* If it is a ground unit, check that our vanguard unit has a clear path to its target */
    if (canFormArc && !vanguard->isFlying)
    {
        std::vector<BWAPI::TilePosition> tiles;
        Geom::FindTilesBetween(vanguard->getTilePosition(), vanguardTarget->getTilePosition(), tiles);
        for (auto tilePos : tiles)
        {
            if (!Map::isWalkable(tilePos))
            {
                canFormArc = false;
                break;
            }
        }
    }

    if (canFormArc)
    {
        auto pivot = vanguard->prevPos + Geom::ScaleVector(vanguardTarget->simPosition - vanguard->prevPos,
                                                               vanguard->prevPos.getApproxDistance(vanguardTarget->simPosition) + 64);

        /* Determine the desired distance to the pivot */
        /* If our units are on average within one tile of where we want them, shorten the distance by one tile */
        /* Otherwise wait until they have formed up around the vanguard */

        auto effectiveDist = [&pivot](const OwnedUnit &gameUnit)
        {
            return gameUnit->prevPos.getApproxDistance(pivot) + (UnitHelper::IsRangedUnit(gameUnit->type) ? 0 : 32);
        };

        int desiredDistance = effectiveDist(vanguard);

        int accumulator = 0;
        int count = 0;
        int countWithinLimit = 0;
        for (auto &unitAndTarget : unitsAndTargets)
        {
            if (unitAndTarget.first->type == BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;
            if (unitAndTarget.first->isFlying) continue;

            int span = effectiveDist(unitAndTarget.first);
            if (span <= (desiredDistance + 24)) countWithinLimit++;
            if (countWithinLimit >= 8) break;
            accumulator += span;
            count++;
        }

        if (countWithinLimit >= 8 || (count > 0 && (accumulator / count) <= (desiredDistance + 24))) desiredDistance -= 32;

        if (formArc(pivot, desiredDistance)) return;
    }

    /* Micro each unit */
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &myUnit = unitAndTarget.first;

        /* If the unit is stuck, unstick it */
        if (myUnit->unstick()) continue;

        /* If the unit is not ready (i.e. is already in the middle of an attack), don't touch it */
        if (!myUnit->isReady()) continue;

        /* Attack target */
        if (unitAndTarget.second)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(unitAndTarget.first->ident) << "Target: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->simPosition);
#endif
            myUnit->attackUnit(unitAndTarget.second, unitsAndTargets, true, enemyAoeRadius);
        }
        else if (myUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon)
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(unitAndTarget.first->ident) << "No target";
#endif
        }
        else
        {
#if DEBUG_UNIT_ORDERS
            Telemetry::log(unitAndTarget.first->ident) << "No target: move to " << BWAPI::WalkPosition(targetPosition);
#endif
            myUnit->moveTo(targetPosition);
        }
    }
}
