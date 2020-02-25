#include "DefendBaseSquad.h"

#include "Units.h"
#include "UnitUtil.h"

void DefendBaseSquad::setTargetPosition()
{
    // If the base is our main, defend at the single main choke (if there is one)
    if (base == Map::getMyMain())
    {
        // Find a single unblocked choke out of the main
        Choke *choke = nullptr;
        for (auto bwemChoke : base->getArea()->ChokePoints())
        {
            if (bwemChoke->Blocked()) continue;
            if (choke)
            {
                choke = nullptr;
                break;
            }
            choke = Map::choke(bwemChoke);
        }

        if (choke)
        {
            targetPosition = choke->Center();
            return;
        }
    }

    // If this base is our natural, defend at the single natural choke (if there is one)
    if (base == Map::getMyNatural())
    {
        // Find a single unblocked choke out of the natural that doesn't go to our main
        auto mainArea = Map::getMyMain()->getArea();
        Choke *choke = nullptr;
        for (auto bwemChoke : base->getArea()->ChokePoints())
        {
            if (bwemChoke->Blocked()) continue;
            if (bwemChoke->GetAreas().first == mainArea || bwemChoke->GetAreas().second == mainArea) continue;
            if (choke)
            {
                choke = nullptr;
                break;
            }
            choke = Map::choke(bwemChoke);
        }

        if (choke)
        {
            targetPosition = choke->Center();
            return;
        }
    }

    // By default we defend at the center of the mineral line
    targetPosition = base->mineralLineCenter;
}

void DefendBaseSquad::execute(UnitCluster &cluster)
{
    std::set<Unit> enemyUnits;

    // Get enemy combat units in our base
    Units::enemyInArea(enemyUnits, Map::getMyMain()->getArea(), [](const Unit &unit)
    {
        return UnitUtil::IsCombatUnit(unit->type) && UnitUtil::CanAttackGround(unit->type);
    });

    bool enemyInOurBase = !enemyUnits.empty();

    // Get enemy combat units very close to the target position
    Units::enemyInRadius(enemyUnits, targetPosition, 64);

    // Select targets
    auto unitsAndTargets = cluster.selectTargets(enemyUnits, targetPosition);

    // Run combat sim
    auto simResult = cluster.runCombatSim(unitsAndTargets, enemyUnits);

    // TODO: Needs tuning
    bool attack =
            simResult.myPercentLost() <= 0.001 ||
            (simResult.valueGain() > 0 && simResult.percentGain() > -0.05) ||
            simResult.percentGain() > 0.2;

    if (attack)
    {
        // Reset our defensive position when all enemy units are out of our base
        if (!enemyInOurBase) targetPosition = defaultTargetPosition;

        cluster.attack(unitsAndTargets, targetPosition);
        return;
    }

    // Ensure the target position is set to the mineral line center
    // This allows our workers to help with the defense
    targetPosition = base->mineralLineCenter;

    // Move our units in the following way:
    // - If the unit is in the mineral line and close to its target, attack it
    // - If the unit's target is far out of its attack range, move towards it
    //   Rationale: we want to encourage our enemies to get distracted and chase us
    // - Otherwise move towards the mineral line center

    // TODO: This probably doesn't make sense against ranged units

    // For each of our units, move it towards the mineral line center if it is not in the mineral line, otherwise attack
    for (auto &unitAndTarget : unitsAndTargets)
    {
        auto &unit = unitAndTarget.first;
        auto &target = unitAndTarget.second;

        // If the unit is stuck, unstick it
        if (unit->unstick()) continue;

        // If the unit is not ready (i.e. is already in the middle of an attack), don't touch it
        if (!unit->isReady()) continue;

        // If the unit has no target, just move to the target position
        if (!target)
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "No target: move to " << BWAPI::WalkPosition(targetPosition);
#endif
            unit->moveTo(targetPosition);
            continue;
        }

        auto enemyPosition = target->predictPosition(BWAPI::Broodwar->getLatencyFrames());

        // Attack the enemy if we are in the mineral line and in range of the enemy (or the enemy is in range of us)
        if (base->isInMineralLine(BWAPI::TilePosition(unit->tilePositionX, unit->tilePositionY)) &&
            (unit->isInOurWeaponRange(target, enemyPosition) || unit->isInEnemyWeaponRange(target, enemyPosition)))
        {
#if DEBUG_UNIT_ORDERS
            CherryVis::log(unitAndTarget.first->id) << "Target: " << unitAndTarget.second->type << " @ "
                                                    << BWAPI::WalkPosition(unitAndTarget.second->lastPosition);
#endif
            unit->attackUnit(target);
            continue;
        }

#if DEBUG_UNIT_ORDERS
        CherryVis::log(unitAndTarget.first->id) << "Retreating: move to " << BWAPI::WalkPosition(targetPosition);
#endif
        unit->moveTo(targetPosition);
    }
}
