#include "StrategyEngines/PvZ.h"

#include "Map.h"
#include "Plays/MainArmy/DefendMyMain.h"

std::map<PvZ::OurStrategy, std::string> PvZ::OurStrategyNames = {
        {OurStrategy::EarlyGameDefense, "EarlyGameDefense"},
        {OurStrategy::AntiAllIn,        "AntiAllIn"},
        {OurStrategy::FastExpansion,    "FastExpansion"},
        {OurStrategy::Defensive,        "Defensive"},
        {OurStrategy::Normal,           "Normal"},
        {OurStrategy::MidGame,          "MidGame"}
};

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;
}

PvZ::OurStrategy PvZ::chooseOurStrategy(PvZ::ZergStrategy newEnemyStrategy, std::vector<std::shared_ptr<Play>> &plays)
{
    int enemyStrategyStableFor = 0;
    if (newEnemyStrategy == enemyStrategy) enemyStrategyStableFor = BWAPI::Broodwar->getFrameCount() - enemyStrategyChanged;

    auto strategy = ourStrategy;
    while (true)
    {
        switch (strategy)
        {
            case PvZ::OurStrategy::EarlyGameDefense:
            {
                // Transition appropriately as soon as we have an idea of what the enemy is doing
                switch (newEnemyStrategy)
                {
                    case ZergStrategy::Unknown:
                    case ZergStrategy::GasSteal:
                        return strategy;
                    case ZergStrategy::ZerglingRush:
                    case ZergStrategy::ZerglingAllIn:
                    {
                        strategy = OurStrategy::AntiAllIn;
                        continue;
                    }
                    case ZergStrategy::PoolBeforeHatchery:
                    {
                        strategy = OurStrategy::Defensive;
                        continue;
                    }
                    case ZergStrategy::HatcheryBeforePool:
                    {
                        strategy = OurStrategy::Normal;
                        continue;
                    }
                    case ZergStrategy::Turtle:
                    {
                        strategy = OurStrategy::FastExpansion;
                        continue;
                    }
                    case ZergStrategy::Lair:
                    {
                        strategy = OurStrategy::MidGame;
                        continue;
                    }
                }

                break;
            }

            case PvZ::OurStrategy::AntiAllIn:
            {
                // Transition to normal when we consider it safe to do so

                // Require dragoon range
                if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0) break;

                auto mainArmyPlay = getMainArmyPlay(plays);
                auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
                auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

                int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                                completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

                // Transition when we have 15 units if we still have the enemy strategy recognized as an all-in, 10 otherwise
                if (unitCount >= 15 || (unitCount >= 10 && enemyStrategyStableFor > 480 && newEnemyStrategy != ZergStrategy::ZerglingRush
                                        && newEnemyStrategy != ZergStrategy::ZerglingAllIn))
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }
            case PvZ::OurStrategy::FastExpansion:
            {
                // Transition to normal when the expansion is taken
                auto natural = Map::getMyNatural();
                if (!natural || natural->ownedSince != -1)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }
            case PvZ::OurStrategy::Defensive:
            {
                if (newEnemyStrategy == ZergStrategy::ZerglingRush || newEnemyStrategy == ZergStrategy::ZerglingAllIn)
                {
                    strategy = OurStrategy::AntiAllIn;
                    continue;
                }

                if (newEnemyStrategy == ZergStrategy::Turtle || newEnemyStrategy == ZergStrategy::Lair)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                // Transition to normal when we either detect another Zerg opening or when there are six units in the vanguard cluster
                auto mainArmyPlay = getMainArmyPlay(plays);
                if (mainArmyPlay && typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                    if (vanguard && vanguard->units.size() >= 6)
                    {
                        strategy = OurStrategy::Normal;
                        continue;
                    }
                }

            }
            case PvZ::OurStrategy::Normal:
            {
                // Transition to mid-game when the enemy has lair tech
                // TODO: Also transition in other cases
                if (newEnemyStrategy == ZergStrategy::Lair)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvZ::OurStrategy::MidGame:
                break;
        }

        return strategy;
    }
}
