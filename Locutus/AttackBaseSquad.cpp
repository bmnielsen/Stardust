#include "AttackBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"
#include "Players.h"
#include "Geo.h"

void AttackBaseSquad::execute(UnitCluster & cluster)
{
    // Look for enemies near this cluster
    auto enemyUnits = Units::getInRadius(BWAPI::Broodwar->enemy(), cluster.center, 480);

    // If there are none, move to the base
    if (enemyUnits.empty())
    {
        for (auto unit : cluster.units)
        {
            Units::getMine(unit).move(targetPosition);
        }

        return;
    }

    // TODO: Run combat simulation

    // Micro each unit
    for (auto unit : cluster.units)
    {
        auto & myUnit = Units::getMine(unit);

        // If the unit is stuck, unstick it
        if (myUnit.isStuck())
        {
            myUnit.stop();
            continue;
        }

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!myUnit.isReady()) continue;

        // Pick a target
        BWAPI::Unit bestTarget = nullptr;
        int bestScore = 0;
        for (auto enemyUnit : enemyUnits)
        {
            // Ignore units that are about to die
            if (enemyUnit->doomed) continue;

            BWAPI::Unit target = enemyUnit->unit;

            // Ignore anything we can't attack
            if (!target || 
                !target->exists() ||
                !target->isVisible() ||
                !target->isDetected() ||
                target->isStasised() ||
                !UnitUtil::CanAttack(unit, target))
            {
                continue;
            }

            // Get our weapon range
            int range = Players::weaponRange(BWAPI::Broodwar->self(), target->isFlying() ? unit->getType().airWeapon() : unit->getType().groundWeapon());

            // Ranged units should ignore anything under dark swarm
            if (target->isUnderDarkSwarm() && range > 32) continue;

            // Distance to target
            int dist = unit->getDistance(target);

            int score = 480 - dist;
            if (UnitUtil::CanAttack(target, unit))
            {
                score *= 2;
            }

            if (score > bestScore)
            {
                bestScore = score;
                bestTarget = target;
            }
        }

        if (bestTarget)
        {
            int cooldown = bestTarget->isFlying() ? unit->getAirWeaponCooldown() : unit->getGroundWeaponCooldown();
            int range = Players::weaponRange(BWAPI::Broodwar->self(), bestTarget->isFlying() ? unit->getType().airWeapon() : unit->getType().groundWeapon());
            int dist = unit->getDistance(bestTarget);

            // TODO: Port over proper kiting
            if (unit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
            {
                int framesToFiringRange = std::max(0, dist - range) / unit->getType().topSpeed();
                if ((cooldown - BWAPI::Broodwar->getRemainingLatencyFrames() - 2) > framesToFiringRange)
                {
                    myUnit.fleeFrom(bestTarget->getPosition());
                    continue;
                }
            }

            if (cooldown <= BWAPI::Broodwar->getRemainingLatencyFrames())
            {
                // Attack if we expect to be in range after latency frames
                auto myPosition = UnitUtil::PredictPosition(unit, BWAPI::Broodwar->getRemainingLatencyFrames());
                auto targetPosition = UnitUtil::PredictPosition(bestTarget, BWAPI::Broodwar->getRemainingLatencyFrames());
                auto predictedDist = Geo::EdgeToEdgeDistance(unit->getType(), myPosition, bestTarget->getType(), targetPosition);

                if (predictedDist <= range)
                {
                    myUnit.attack(bestTarget);
                    continue;
                }
            }

            // Plot an intercept course
            auto intercept = Geo::FindInterceptPoint(unit, bestTarget);
            if (!intercept.isValid()) intercept = UnitUtil::PredictPosition(bestTarget, 5);
            myUnit.move(intercept);
        }
    }
}