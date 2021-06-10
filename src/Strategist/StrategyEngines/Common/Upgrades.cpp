#include "StrategyEngine.h"

#include "Units.h"

void StrategyEngine::upgradeAtCount(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                    UpgradeOrTechType upgradeOrTechType,
                                    BWAPI::UnitType unitType,
                                    int unitCount)
{
    // First bail out if the upgrade is already done or queued
    if (upgradeOrTechType.currentLevel() > 0) return;
    if (Units::isBeingUpgradedOrResearched(upgradeOrTechType)) return;

    // Now loop through all of the prioritized production goals, keeping track of how many of the desired unit we have
    int units = Units::countCompleted(unitType) + Units::countIncomplete(unitType);
    for (auto &priorityAndProductionGoals : prioritizedProductionGoals)
    {
        for (auto it = priorityAndProductionGoals.second.begin(); it != priorityAndProductionGoals.second.end(); it++)
        {
            // Bail out if this upgrade is already queued
            auto upgradeProductionGoal = std::get_if<UpgradeProductionGoal>(&*it);
            if (upgradeProductionGoal && upgradeProductionGoal->upgradeType() == upgradeOrTechType) return;

            auto unitProductionGoal = std::get_if<UnitProductionGoal>(&*it);
            if (!unitProductionGoal) continue;

            // If we are producing an unlimited number of a different unit type first, or at emergency priority, bail out
            if (unitProductionGoal->countToProduce() == -1 &&
                (unitProductionGoal->unitType() != unitType || priorityAndProductionGoals.first < PRIORITY_MAINARMYBASEPRODUCTION))
            {
                return;
            }

            // Skip other unit types
            if (unitProductionGoal->unitType() != unitType) continue;

            // If we already have enough units, insert the upgrade now
            if (units >= unitCount)
            {
                priorityAndProductionGoals.second.emplace(it, UpgradeProductionGoal(upgradeOrTechType));
                return;
            }

            // If producing an unlimited number, split and insert here
            if (unitProductionGoal->countToProduce() == -1)
            {
                auto producerLimit = unitProductionGoal->getProducerLimit();
                auto location = unitProductionGoal->getLocation();

                // Insert the upgrade here
                it = priorityAndProductionGoals.second.emplace(it, UpgradeProductionGoal(upgradeOrTechType));

                // Insert remaining units beforehand
                if (units < unitCount)
                {
                    priorityAndProductionGoals.second.emplace(it,
                                                              std::in_place_type<UnitProductionGoal>,
                                                              unitType,
                                                              unitCount - units,
                                                              producerLimit,
                                                              location);
                }

                return;
            }

            // If the unit count is reached, split and insert here
            if ((units + unitProductionGoal->countToProduce()) >= unitCount)
            {
                auto producerLimit = unitProductionGoal->getProducerLimit();
                auto location = unitProductionGoal->getLocation();

                // Remove the current item
                it = priorityAndProductionGoals.second.erase(it);

                // Add the remainder after the upgrade
                if ((units + unitProductionGoal->countToProduce()) > unitCount)
                {
                    it = priorityAndProductionGoals.second.emplace(it,
                                                                   std::in_place_type<UnitProductionGoal>,
                                                                   unitType,
                                                                   (units + unitProductionGoal->countToProduce()) - unitCount,
                                                                   producerLimit,
                                                                   location);
                }

                // Add the upgrade
                it = priorityAndProductionGoals.second.emplace(it, UpgradeProductionGoal(upgradeOrTechType));

                // Add the count before the upgrade
                priorityAndProductionGoals.second.emplace(it,
                                                          std::in_place_type<UnitProductionGoal>,
                                                          unitType,
                                                          unitCount - units,
                                                          producerLimit,
                                                          location);

                return;
            }

            // Otherwise add the units and continue
            units += unitProductionGoal->countToProduce();
        }
    }
}

void StrategyEngine::upgradeWhenUnitCreated(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                            UpgradeOrTechType upgradeOrTechType,
                                            BWAPI::UnitType unitType,
                                            bool requireCompletedUnit,
                                            bool requireProducer,
                                            int priority)
{
    // First bail out if the upgrade is already done or queued
    if (upgradeOrTechType.currentLevel() > 0) return;
    if (Units::isBeingUpgradedOrResearched(upgradeOrTechType)) return;

    // Now check if we have at least one of the unit
    if (Units::countCompleted(unitType) == 0 && (requireCompletedUnit || Units::countIncomplete(unitType) == 0)) return;

    // Now check if we have the required producer, if specified
    if (requireProducer &&
        Units::countIncomplete(upgradeOrTechType.whatUpgradesOrResearches()) == 0 &&
        Units::countCompleted(upgradeOrTechType.whatUpgradesOrResearches()) == 0)
    {
        return;
    }

    prioritizedProductionGoals[priority].emplace_back(UpgradeProductionGoal(upgradeOrTechType));
}

void StrategyEngine::defaultGroundUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Start when we have completed our second gas and have an army
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 2 &&
        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon)) > 10)
    {
        int weaponLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
        int armorLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);

        // Upgrade past 1 and potentially on two forges when we have completed our third nexus
        int maxLevel, forgeCount;
        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) >= 3)
        {
            maxLevel = 3;
            if (armorLevel >= 1 || Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Armor))
            {
                forgeCount = 2;
            }
        }
        else
        {
            maxLevel = 1;
            forgeCount = 1;
        }

        // Weapons -> 1, Armor -> 1, Weapons -> 3, Armor -> 3
        if ((weaponLevel == 0 || armorLevel >= 1) && weaponLevel < maxLevel &&
            !Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
        {
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                     BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                     weaponLevel + 1,
                                                                     forgeCount);
        }
        if (!Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && armorLevel < maxLevel)
        {
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                     BWAPI::UpgradeTypes::Protoss_Ground_Armor,
                                                                     armorLevel + 1,
                                                                     forgeCount);
        }
        if (weaponLevel > 0 && armorLevel == 0 && weaponLevel < maxLevel &&
            !Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
        {
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                     BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                     weaponLevel + 1,
                                                                     forgeCount);
        }

        // Upgrade shields when we are maxed
        if (BWAPI::Broodwar->self()->supplyUsed() > 300 &&
            BWAPI::Broodwar->self()->minerals() > 1500 &&
            BWAPI::Broodwar->self()->gas() > 1000)
        {
            if (weaponLevel < 3 && armorLevel < 3) forgeCount = 3;
            prioritizedProductionGoals[PRIORITY_NORMAL]
                    .emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                  BWAPI::UpgradeTypes::Protoss_Plasma_Shields,
                                  BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Plasma_Shields) + 1,
                                  forgeCount);
        }
    }
}
