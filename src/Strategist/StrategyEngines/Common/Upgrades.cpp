#include "StrategyEngine.h"

#include "Units.h"

void StrategyEngine::upgradeAtCount(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                                    UpgradeOrTechType upgradeOrTechType,
                                    BWAPI::UnitType unitType,
                                    int unitCount,
                                    int level)
{
    /* First bail out if the upgrade is already done or queued */
    if (upgradeOrTechType.currentLevel() >= level) return;
    if (UnitMgr::isBeingUpgradedOrResearched(upgradeOrTechType)) return;

    /* Now loop through all of the prioritized production goals, keeping track of how many of the desired unit we have */
    int units = UnitMgr::countCompleted(unitType) + UnitMgr::countIncomplete(unitType);
    for (auto &priorityAndProductionGoals : prioProdGoals)
    {
        for (auto iterRef = priorityAndProductionGoals.second.begin(); iterRef != priorityAndProductionGoals.second.end(); iterRef++)
        {
            /* Bail out if this upgrade is already queued */
            auto upgradeProductionGoal = std::get_if<UpgradeProductionGoal>(&*iterRef);
            if (upgradeProductionGoal && upgradeProductionGoal->upgradeType() == upgradeOrTechType) return;

            auto unitProductionGoal = std::get_if<UnitProductionGoal>(&*iterRef);
            if (!unitProductionGoal) continue;

            /* If we are producing an unlimited number of a different unit type first, or at emergency priority, bail out */
            if (unitProductionGoal->countToProduce() == -1 &&
                (unitProductionGoal->unitType() != unitType || priorityAndProductionGoals.first < PRIORITY_MAINARMYBASEPRODUCTION))
            {
                return;
            }

            /* Skip other unit types */
            if (unitProductionGoal->unitType() != unitType) continue;

            /* If we already have enough units, insert the upgrade now */
            if (units >= unitCount)
            {
                priorityAndProductionGoals.second.emplace(iterRef, std::in_place_type<UpgradeProductionGoal>, "SE", upgradeOrTechType);
                return;
            }

            /* If producing an unlimited number, split and insert here */
            if (unitProductionGoal->countToProduce() == -1)
            {
                auto producerLimit = unitProductionGoal->getProducerLimit();
                auto location = unitProductionGoal->getLocation();

                /* Copy the requester since we are about to manipulate the vector */
                auto requester = unitProductionGoal->requester;

                /* Insert the upgrade here */
                iterRef = priorityAndProductionGoals.second.emplace(iterRef, std::in_place_type<UpgradeProductionGoal>, "SE", upgradeOrTechType);

                /* Insert remaining units beforehand */
                if (units < unitCount)
                {
                    priorityAndProductionGoals.second.emplace(iterRef,
                                                              std::in_place_type<UnitProductionGoal>,
                                                              requester,
                                                              unitType,
                                                              unitCount - units,
                                                              producerLimit,
                                                              location);
                }

                return;
            }

            /* If the unit count is reached, split and insert here */
            if ((units + unitProductionGoal->countToProduce()) >= unitCount)
            {
                auto producerLimit = unitProductionGoal->getProducerLimit();
                auto location = unitProductionGoal->getLocation();
                auto remainingCount = (units + unitProductionGoal->countToProduce()) - unitCount;

                /* Copy the requester since we are about to manipulate the vector */
                auto requester = unitProductionGoal->requester;

                /* Remove the current item */
                iterRef = priorityAndProductionGoals.second.erase(iterRef);

                /* Add the remainder after the upgrade */
                if (remainingCount > 0)
                {
                    iterRef = priorityAndProductionGoals.second.emplace(iterRef,
                                                                   std::in_place_type<UnitProductionGoal>,
                                                                   requester,
                                                                   unitType,
                                                                   remainingCount,
                                                                   producerLimit,
                                                                   location);
                }

                /* Add the upgrade */
                iterRef = priorityAndProductionGoals.second.emplace(iterRef, std::in_place_type<UpgradeProductionGoal>, "SE", upgradeOrTechType);

                /* Add the count before the upgrade */
                priorityAndProductionGoals.second.emplace(iterRef,
                                                          std::in_place_type<UnitProductionGoal>,
                                                          requester,
                                                          unitType,
                                                          unitCount - units,
                                                          producerLimit,
                                                          location);

                return;
            }

            /* Otherwise add the units and continue */
            units += unitProductionGoal->countToProduce();
        }
    }
}

void StrategyEngine::upgradeWhenUnitCreated(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                                            UpgradeOrTechType upgradeOrTechType,
                                            BWAPI::UnitType unitType,
                                            bool requireCompletedUnit,
                                            bool requireProducer,
                                            int priority)
{
    /* First bail out if the upgrade is already done or queued */
    if (upgradeOrTechType.currentLevel() >= upgradeOrTechType.maxLevel()) return;
    if (UnitMgr::isBeingUpgradedOrResearched(upgradeOrTechType)) return;

    /* Now check if we have at least one of the unit */
    if (UnitMgr::countCompleted(unitType) == 0 && (requireCompletedUnit || UnitMgr::countIncomplete(unitType) == 0)) return;

    /* Now check if we have the required producer, if specified */
    if (requireProducer &&
        UnitMgr::countIncomplete(upgradeOrTechType.whatUpgradesOrResearches()) == 0 &&
        UnitMgr::countCompleted(upgradeOrTechType.whatUpgradesOrResearches()) == 0)
    {
        return;
    }

    prioProdGoals[priority].emplace_back(std::in_place_type<UpgradeProductionGoal>, "SE", upgradeOrTechType);
}

void StrategyEngine::defaultGroundUpgrades(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /* Start when we have completed our second gas and have an army */
    if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 2 &&
        (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon)) > 10)
    {
        int weaponLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
        int armorLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);

        /* Upgrade past 1 and potentially on two forges when we have completed our third nexus */
        int maxLevel, forgeCount;
        if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) >= 3)
        {
            maxLevel = 3;
            if (armorLevel >= 1 || UnitMgr::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Armor))
            {
                forgeCount = 2;
            }
        }
        else
        {
            maxLevel = 1;
            forgeCount = 1;
        }

        /* Weapons -> 1, Armor -> 1, Weapons -> 3, Armor -> 3 */
        if ((weaponLevel == 0 || armorLevel >= 1) && weaponLevel < maxLevel &&
            !UnitMgr::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
        {
            prioProdGoals[PRIORITY_HIGHPRIORITYUPGRADES].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                                   "SE",
                                                                                   BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                                   weaponLevel + 1,
                                                                                   forgeCount);
        }
        if (!UnitMgr::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && armorLevel < maxLevel)
        {
            prioProdGoals[PRIORITY_HIGHPRIORITYUPGRADES].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                                   "SE",
                                                                                   BWAPI::UpgradeTypes::Protoss_Ground_Armor,
                                                                                   armorLevel + 1,
                                                                                   forgeCount);
        }
        if (weaponLevel > 0 && armorLevel == 0 && weaponLevel < maxLevel &&
            !UnitMgr::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
        {
            prioProdGoals[PRIORITY_HIGHPRIORITYUPGRADES].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                                   "SE",
                                                                                   BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                                   weaponLevel + 1,
                                                                                   forgeCount);
        }

        /* Upgrade shields when we are maxed */
        if (BWAPI::Broodwar->self()->supplyUsed() > 300 &&
            BWAPI::Broodwar->self()->minerals() > 1500 &&
            BWAPI::Broodwar->self()->gas() > 1000)
        {
            if (weaponLevel < 3 && armorLevel < 3) forgeCount = 3;
            prioProdGoals[PRIORITY_NORMAL]
                    .emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                  "SE",
                                  BWAPI::UpgradeTypes::Protoss_Plasma_Shields,
                                  BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields) + 1,
                                  forgeCount);
        }
    }
}

void StrategyEngine::upgrade(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                             UpgradeOrTechType upgradeOrTechType,
                             int level,
                             int priority)
{
    /* First bail out if the upgrade is already done or queued */
    if (upgradeOrTechType.currentLevel() >= level) return;
    if (UnitMgr::isBeingUpgradedOrResearched(upgradeOrTechType)) return;

    /* Queue the upgrade */
    prioProdGoals[priority].emplace_back(std::in_place_type<UpgradeProductionGoal>, "SE", upgradeOrTechType, level);
}
