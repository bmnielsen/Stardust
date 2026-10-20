#include "AttackExpansion.h"

#include "General.h"
#include "Players.h"
#include "Units.h"
#include "UnitUtil.h"
#include "PathFinding.h"

AttackExpansion::AttackExpansion(Base *baseLoc, int enemyDefenseValue)
        : PlayItem((std::ostringstream() << "Attack expansion @ " << baseLoc->getTilePosition()).str())
        , baseLoc(baseLoc)
        , enemyDefenseValue(enemyDefenseValue)
        , squad(std::make_shared<AttackBaseSquad>(baseLoc))
{
    Marshal::addSquad(squad);
}

void AttackExpansion::update()
{
    /* Complete the play when the base is no longer owned by the enemy */
    if (baseLoc->owner != BWAPI::Broodwar->enemy())
    {
        status.complete = true;
        return;
    }

    /* Gather enemy threats at the base */
    int enemyValue = 0;
    bool requireDragoons = false;
    for (const auto &gameUnit : UnitMgr::enemyAtBase(baseLoc))
    {
        enemyValue += CombatSim::unitValue(gameUnit);

        /* Require dragoons against certain enemy types */
        requireDragoons = requireDragoons ||
                          (gameUnit->isFlying ||
                           gameUnit->type == BWAPI::UnitTypes::Protoss_Dragoon ||
                           gameUnit->type == BWAPI::UnitTypes::Terran_Vulture);
    }

    /* Update detection - release observers when no longer needed, request observers when needed */
    bool needDetection = squad->needsDetection() || UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
    auto &detectors = squad->getDetectors();
    if (!needDetection && !detectors.empty())
    {
        status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
    }
    else if (needDetection && detectors.empty())
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());

        /* Release the squad units until we get the detector */
        status.removedUnits = squad->getUnits();
        return;
    }

    /* Reserve enough units to attack the base */
    int ourValue = 0;
    for (auto &gameUnit : squad->getUnits())
    {
        ourValue += CombatSim::unitValue(gameUnit);
    }

    int requestedUnits = 0;
    while (ourValue < enemyValue * 2)
    {
        requestedUnits++;
        ourValue += CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon);
    }

    /* Ensure we have at least three units whenever we attack a base */
    requestedUnits = std::max(requestedUnits, 3 - (int) squad->getUnits().size());

    /* TODO: Request zealot or dragoon when we have that capability */
    if (requestedUnits > 0)
    {
        /* Prefer units that have a safe path to the base */
        auto gridNodePredicate = [](const NavGrid::GridNode &gridNode)
        {
            return gridNode.cost < 1200 || PlayerInfo::grid(BWAPI::Broodwar->enemy()).groundThreat(gridNode.midpoint()) == 0;
        };
        status.unitRequirements.emplace_back(requestedUnits,
                                             BWAPI::UnitTypes::Protoss_Dragoon,
                                             baseLoc->getPosition(),
                                             false,
                                             gridNodePredicate,
                                             true);
    }
}

void AttackExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Build an observer if we need one */
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 label,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}
