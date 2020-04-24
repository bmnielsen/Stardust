#include "StrategyEngine.h"

#include "Units.h"

void StrategyEngine::upgradeAtCount(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                    BWAPI::UpgradeType upgradeType,
                                    BWAPI::UnitType unitType,
                                    int unitCount)
{
    // First bail out if the upgrade is already done or queued
    if (BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) > 0) return;
    if (Units::isBeingUpgraded(upgradeType)) return;

    // Now loop through all of the prioritized production goals, keeping track of how many of the desired unit we have
    int units = Units::countCompleted(unitType);
    for (auto &priorityAndProductionGoals : prioritizedProductionGoals)
    {
        for (auto it = priorityAndProductionGoals.second.begin(); it != priorityAndProductionGoals.second.end(); it++)
        {
            // Bail out if this upgrade is already queued
            auto upgradeProductionGoal = std::get_if<UpgradeProductionGoal>(&*it);
            if (upgradeProductionGoal && upgradeProductionGoal->upgradeType() == upgradeType) return;

            auto unitProductionGoal = std::get_if<UnitProductionGoal>(&*it);
            if (!unitProductionGoal) continue;

            if (unitProductionGoal->countToProduce() == -1)
            {
                // If we are producing an unlimited number of a different unit type first, bail out
                if (unitProductionGoal->unitType() != unitType) return;

                // If this isn't the main army production, bail out - we don't want to upgrade in emergencies
                if (priorityAndProductionGoals.first != PRIORITY_MAINARMY) return;

                // Insert the upgrade here
                it = priorityAndProductionGoals.second.emplace(it, UpgradeProductionGoal(upgradeType));

                // Insert remaining units beforehand
                if (units < unitCount)
                {
                    priorityAndProductionGoals.second.emplace(it,
                                                              std::in_place_type<UnitProductionGoal>,
                                                              unitType,
                                                              unitCount - units,
                                                              unitProductionGoal->getProducerLimit(),
                                                              unitProductionGoal->getLocation());
                }

                return;
            }

            if (unitProductionGoal->unitType() == unitType) units += unitProductionGoal->countToProduce();
        }
    }
}

void StrategyEngine::upgradeWhenUnitStarted(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                            BWAPI::UpgradeType upgradeType,
                                            BWAPI::UnitType unitType,
                                            bool requireProducer)
{
    // First bail out if the upgrade is already done or queued
    if (BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) > 0) return;
    if (Units::isBeingUpgraded(upgradeType)) return;

    // Now check if we have at least one of the unit
    if (Units::countIncomplete(unitType) == 0 && Units::countCompleted(unitType) == 0) return;

    // Now check if we have the required producer, if specified
    if (requireProducer &&
        Units::countIncomplete(upgradeType.whatUpgrades()) == 0 &&
        Units::countCompleted(upgradeType.whatUpgrades()) == 0)
    {
        return;
    }

    prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(UpgradeProductionGoal(upgradeType));
}

void StrategyEngine::defaultGroundUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Start when we have completed our second nexus
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) >= 2)
    {
        // Upgrade past 1 and on two forges when we have completed our third gas
        int maxLevel, forgeCount;
        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 3)
        {
            maxLevel = 3;
            forgeCount = 2;
        }
        else
        {
            maxLevel = 1;
            forgeCount = 1;
        }

        int weaponLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
        int armorLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);

        // Weapons -> 1, Armor -> 1, Weapons -> 3, Armor -> 3
        if ((weaponLevel == 0 || armorLevel >= 1) && weaponLevel <= maxLevel &&
            !Units::isBeingUpgraded(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
        {
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                     BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                     weaponLevel + 1,
                                                                     forgeCount);
        }
        if (!Units::isBeingUpgraded(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && armorLevel <= maxLevel)
        {
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                     BWAPI::UpgradeTypes::Protoss_Ground_Armor,
                                                                     armorLevel + 1,
                                                                     forgeCount);
        }
        if (weaponLevel > 0 && armorLevel == 0 && weaponLevel <= maxLevel &&
            !Units::isBeingUpgraded(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
        {
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                     BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                     weaponLevel + 1,
                                                                     forgeCount);
        }

        // TODO: Upgrade shields when maxed
    }
}
