#include "StrategyEngines/PvZ.h"

#include "Map.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Units.h"

std::map<PvZ::OurStrategy, std::string> PvZ::OurStrategyNames = {
        {OurStrategy::EarlyGameDefense, "EarlyGameDefense"},
        {OurStrategy::AntiAllIn,        "AntiAllIn"},
        {OurStrategy::AntiSunkenContain,"AntiSunkenContain"},
        {OurStrategy::SairSpeedlot,     "SairSpeedlot"},
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
    if (newEnemyStrategy == enemyStrategy) enemyStrategyStableFor = currentFrame - enemyStrategyChanged;

    auto canTransitionFromAntiAllIn = [&]()
    {
        bool strategyIsAllIn = newEnemyStrategy == ZergStrategy::WorkerRush
                               || newEnemyStrategy == ZergStrategy::ZerglingRush
                               || newEnemyStrategy == ZergStrategy::ZerglingAllIn;

        // Count our total combat units
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
        auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;
        int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                        completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

        // Estimate how many combat units we need
        int requiredUnits;
        if (enemyStrategyStableFor < 240 || strategyIsAllIn)
        {
            requiredUnits = (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0) ? 20 : 15;
        }
        else
        {
            requiredUnits = 2 + Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) / 3;
        }

        return unitCount >= requiredUnits;
    };

    auto strategy = ourStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case PvZ::OurStrategy::EarlyGameDefense:
            {
                // Transition appropriately as soon as we have an idea of what the enemy is doing
                switch (newEnemyStrategy)
                {
                    case ZergStrategy::Unknown:
                        return strategy;
                    case ZergStrategy::SunkenContain:
                    {
                        strategy = OurStrategy::AntiSunkenContain;
                        continue;
                    }
                    case ZergStrategy::WorkerRush:
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
                if (newEnemyStrategy == ZergStrategy::SunkenContain)
                {
                    strategy = OurStrategy::AntiSunkenContain;
                    continue;
                }

                // Transition to normal when we consider it safe to do so
                if (canTransitionFromAntiAllIn())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }
            case PvZ::OurStrategy::AntiSunkenContain:
            {
                if (newEnemyStrategy != ZergStrategy::SunkenContain)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }
            case PvZ::OurStrategy::SairSpeedlot:
            {
                auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
                if (!mainArmyPlay) break;

                // If the enemy is doing a rush and our main army play has transitioned to defending the main, switch to anti all-in strategy
                if (enemyStrategy == ZergStrategy::ZerglingRush && typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    strategy = OurStrategy::AntiAllIn;
                    continue;
                }

                // Transition to mid game when we have gone on the attack, indicating the opening is done
                if (typeid(*mainArmyPlay) == typeid(AttackEnemyBase))
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvZ::OurStrategy::FastExpansion:
            {
                if (newEnemyStrategy == ZergStrategy::SunkenContain)
                {
                    strategy = OurStrategy::AntiSunkenContain;
                    continue;
                }

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
                if (newEnemyStrategy == ZergStrategy::SunkenContain)
                {
                    strategy = OurStrategy::AntiSunkenContain;
                    continue;
                }

                if (newEnemyStrategy == ZergStrategy::WorkerRush ||
                    newEnemyStrategy == ZergStrategy::ZerglingRush ||
                    newEnemyStrategy == ZergStrategy::ZerglingAllIn)
                {
                    strategy = OurStrategy::AntiAllIn;
                    continue;
                }

                // Transition to normal when we either detect another Zerg opening or when there are six units in the vanguard cluster

                if (newEnemyStrategy == ZergStrategy::Turtle || newEnemyStrategy == ZergStrategy::Lair)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
                if (mainArmyPlay && typeid(*mainArmyPlay) == typeid(AttackEnemyBase))
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
                if (newEnemyStrategy == ZergStrategy::SunkenContain)
                {
                    strategy = OurStrategy::AntiSunkenContain;
                    continue;
                }

                if ((newEnemyStrategy == ZergStrategy::WorkerRush ||
                     newEnemyStrategy == ZergStrategy::ZerglingRush ||
                     newEnemyStrategy == ZergStrategy::ZerglingAllIn) &&
                    !canTransitionFromAntiAllIn())
                {
                    strategy = OurStrategy::AntiAllIn;
                    continue;
                }

                // Transition to mid-game when the enemy has lair tech or we are on two bases
                // TODO: Also transition to mid-game in other cases
                if (newEnemyStrategy == ZergStrategy::Lair || Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1)
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

    Log::Get() << "ERROR: Loop in strategy selection, ended on " << OurStrategyNames[strategy];
    return strategy;
}
