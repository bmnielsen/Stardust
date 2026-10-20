#include "CarrierHarass.h"

#include "General.h"
#include "Map.h"
#include "Units.h"

namespace
{
    Base *getTargetBase(Base *current)
    {
        /* Only switch bases when the current target is dead */
        if (current && current->owner == BWAPI::Broodwar->enemy()) return current;

        /* By default target the newest expansion beyond the enemy main/natural */
        Base *best = nullptr;
        int oldest = INT_MIN;
        for (auto &baseLoc : Map::getEnemyBases())
        {
            if (baseLoc == Map::getEnemyStartingMain() || baseLoc == Map::getEnemyStartingNatural()) continue;
            if (baseLoc->ownedSince > oldest)
            {
                best = baseLoc;
                oldest = baseLoc->ownedSince;
            }
        }

        /* Otherwise take the main, then the natural */
        auto maybeSetBase = [&best](Base *baseLoc)
        {
            if (best) return;
            if (!baseLoc) return;
            if (baseLoc->owner != BWAPI::Broodwar->enemy()) return;
            best = baseLoc;
        };
        maybeSetBase(Map::getEnemyStartingMain());
        maybeSetBase(Map::getEnemyStartingNatural());

        return best;
    }
}

void CarrierHarass::update()
{
    /* Set the target base */
    auto newTargetBase = attacking ? getTargetBase(targetBase) : Map::getMyMain();
    if (attacking && newTargetBase == nullptr)
    {
        attacking = false;
        newTargetBase = Map::getMyMain();
    }
    if (newTargetBase != targetBase)
    {
        /* Remove the current squad */
        if (squad)
        {
            status.removedUnits = squad->getUnits();
            Marshal::removeSquad(squad);
            squad = nullptr;
        }

        /* Add the new squad */
        if (newTargetBase)
        {
            squad = std::make_shared<AttackBaseSquad>(newTargetBase, "Carriers");
            Marshal::addSquad(squad);
            for (auto &gameUnit : status.removedUnits)
            {
                squad->addUnit(gameUnit);
            }
            status.removedUnits.clear();
        }

        targetBase = newTargetBase;
    }

    /* When we don't have a target base, the play stays but doesn't do anything */
    if (!targetBase) return;

    /* Update detection - release observers when no longer needed, request observers when needed */
    bool needDetection = squad->needsDetection();
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

    int carrierCount = squad->combatUnitCount();
    if (carrierCount < 4)
    {
        status.unitRequirements.emplace_back(4 - carrierCount, BWAPI::UnitTypes::Protoss_Carrier, targetBase->getPosition());
    }

    int totalInterceptors = 0;
    for (auto &gameUnit : squad->getUnits())
    {
        if (gameUnit->type != BWAPI::UnitTypes::Protoss_Carrier) continue;
        totalInterceptors += gameUnit->rawUnit->getInterceptorCount();
    }

    /* Switch to attacking when we have enough carriers and interceptors */
    if (!attacking && (carrierCount == 4 || (carrierCount == 3 && BWAPI::Broodwar->self()->supplyUsed() > 388))
        && (totalInterceptors / carrierCount) >= 6)
    {
        attacking = true;
    }

    /* Switch to not attacking if we don't have enough carriers or interceptors */
    if (attacking && (carrierCount < 2 || (totalInterceptors / carrierCount) < 2))
    {
        attacking = false;
    }
}

void CarrierHarass::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    for (auto &unitRequirement : status.unitRequirements)
    {
        if (unitRequirement.count < 1) continue;

        prioProdGoals[PRIORITY_SPECIALTEAMS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       label,
                                                                       unitRequirement.type,
                                                                       unitRequirement.count,
                                                                       unitRequirement.type == BWAPI::UnitTypes::Protoss_Carrier ? 2 : 1);
    }

    int weaponLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons);
    if (weaponLevel < 3 && !UnitMgr::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Air_Weapons))
    {
        prioProdGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                 label,
                                                                 BWAPI::UpgradeTypes::Protoss_Air_Weapons,
                                                                 weaponLevel + 1,
                                                                 1);
    }
}
