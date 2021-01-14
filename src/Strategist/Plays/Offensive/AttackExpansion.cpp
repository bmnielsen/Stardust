#include "AttackExpansion.h"

#include "General.h"
#include "Players.h"
#include "Units.h"
#include "UnitUtil.h"
#include "PathFinding.h"

AttackExpansion::AttackExpansion(Base *base, int enemyDefenseValue)
        : Play((std::ostringstream() << "Attack expansion @ " << base->getTilePosition()).str())
        , base(base)
        , enemyDefenseValue(enemyDefenseValue)
        , squad(std::make_shared<AttackBaseSquad>(base))
{
    General::addSquad(squad);
}

void AttackExpansion::update()
{
    // Complete the play when the base is no longer owned by the enemy
    if (base->owner != BWAPI::Broodwar->enemy())
    {
        status.complete = true;
        return;
    }

    // Gather enemy threats at the base
    int enemyValue = 0;
    bool requireDragoons = false;
    for (const auto &unit : Units::enemyAtBase(base))
    {
        enemyValue += CombatSim::unitValue(unit);

        // Require dragoons against certain enemy types
        requireDragoons = requireDragoons ||
                          (unit->isFlying ||
                           unit->type == BWAPI::UnitTypes::Protoss_Dragoon ||
                           unit->type == BWAPI::UnitTypes::Terran_Vulture);
    }

    // Update detection - release observers when no longer needed, request observers when needed
    bool needDetection = squad->needsDetection() || Units::hasEnemyBuilt(BWAPI::UnitTypes::Terran_Vulture_Spider_Mine);
    auto &detectors = squad->getDetectors();
    if (!needDetection && !detectors.empty())
    {
        status.removedUnits.insert(status.removedUnits.end(), detectors.begin(), detectors.end());
    }
    else if (needDetection && detectors.empty())
    {
        status.unitRequirements.emplace_back(1, BWAPI::UnitTypes::Protoss_Observer, squad->getTargetPosition());

        // Release the squad units until we get the detector
        status.removedUnits = squad->getUnits();
        return;
    }

    // Reserve enough units to attack the base
    int ourValue = 0;
    for (auto &unit : squad->getUnits())
    {
        ourValue += CombatSim::unitValue(unit);
    }

    int requestedUnits = 0;
    while (ourValue < enemyValue * 2)
    {
        requestedUnits++;
        ourValue += CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon);
    }

    // Ensure we have at least three units whenever we attack a base
    requestedUnits = std::max(requestedUnits, 3 - (int)squad->getUnits().size());

    // TODO: Request zealot or dragoon when we have that capability
    if (requestedUnits > 0)
    {
        // Only reserve units that have a safe path to the base
        auto gridNodePredicate = [](const NavigationGrid::GridNode &gridNode)
        {
            return gridNode.cost < 300 || Players::grid(BWAPI::Broodwar->enemy()).groundThreat(gridNode.center()) == 0;
        };

        status.unitRequirements.emplace_back(requestedUnits, BWAPI::UnitTypes::Protoss_Dragoon, base->getPosition(), gridNodePredicate, false);
    }
}

void AttackExpansion::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Build an observer if we need one
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.type != BWAPI::UnitTypes::Protoss_Observer) continue;
        if (unitRequirement.count < 1) continue;

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 unitRequirement.type,
                                                                 unitRequirement.count,
                                                                 1);
    }
}
