#include "StrategyEngines/PvP.h"

#include "Map.h"
#include "Plays/MainArmy/DefendMyMain.h"

std::map<PvP::OurStrategy, std::string> PvP::OurStrategyNames = {
        {OurStrategy::EarlyGameDefense, "EarlyGameDefense"},
        {OurStrategy::AntiZealotRush,   "AntiZealotRush"},
        {OurStrategy::FastExpansion,    "FastExpansion"},
        {OurStrategy::Defensive,        "Defensive"},
        {OurStrategy::Normal,           "Normal"},
        {OurStrategy::DTExpand,         "DTExpand"},
        {OurStrategy::MidGame,          "MidGame"}
};

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;
}

PvP::OurStrategy PvP::chooseOurStrategy(PvP::ProtossStrategy newEnemyStrategy, std::vector<std::shared_ptr<Play>> &plays)
{
    int enemyStrategyStableFor = 0;
    if (newEnemyStrategy == enemyStrategy) enemyStrategyStableFor = BWAPI::Broodwar->getFrameCount() - enemyStrategyChanged;

    auto canTransitionFromAntiZealotRush = [&]()
    {
        // Require Dragoon Range
        // TODO: This is probably much too conservative
        if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0) return false;

        // Count total combat units
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
        auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;
        int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                        completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

        // Transition when we have at least 10 units
        return unitCount >= 10;
    };

    auto strategy = ourStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case PvP::OurStrategy::EarlyGameDefense:
            {
                // Transition appropriately as soon as we have an idea of what the enemy is doing
                switch (newEnemyStrategy)
                {
                    case ProtossStrategy::Unknown:
                    case ProtossStrategy::GasSteal:
                        return strategy;
                    case ProtossStrategy::ProxyRush:
                    case ProtossStrategy::ZealotRush:
                    case ProtossStrategy::ZealotAllIn:
                    {
                        strategy = OurStrategy::AntiZealotRush;
                        continue;
                    }
                    case ProtossStrategy::TwoGate:
                    {
                        strategy = OurStrategy::Defensive;
                        continue;
                    }
                    case ProtossStrategy::FastExpansion:
                    case ProtossStrategy::Turtle:
                    {
                        strategy = OurStrategy::FastExpansion;
                        continue;
                    }
                    case ProtossStrategy::EarlyForge:
                    case ProtossStrategy::OneGateCore:
                    case ProtossStrategy::BlockScouting:
                    case ProtossStrategy::DragoonAllIn:
                    case ProtossStrategy::DarkTemplarRush:
                    {
                        strategy = OurStrategy::Normal;
                        continue;
                    }
//                    case ProtossStrategy::DragoonAllIn:
//                    {
//                        strategy = OurStrategy::DTExpand;
//                        continue;
//                    }
                    case ProtossStrategy::MidGame:
                    {
                        strategy = OurStrategy::MidGame;
                        continue;
                    }
                }

                break;
            }

            case PvP::OurStrategy::AntiZealotRush:
            {
                // Transition to normal when we consider it safe to do so
                if (canTransitionFromAntiZealotRush())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }
            case PvP::OurStrategy::FastExpansion:
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
            case PvP::OurStrategy::Defensive:
            {
                if (newEnemyStrategy == ProtossStrategy::ProxyRush ||
                    newEnemyStrategy == ProtossStrategy::ZealotRush ||
                    newEnemyStrategy == ProtossStrategy::ZealotAllIn)
                {
                    strategy = OurStrategy::AntiZealotRush;
                    continue;
                }

                if (newEnemyStrategy == ProtossStrategy::DragoonAllIn)
                {
                    strategy = OurStrategy::DTExpand;
                    continue;
                }

                // Transition to normal when we either detect another opening or when there are six units in the vanguard cluster

                if (newEnemyStrategy == ProtossStrategy::Turtle || newEnemyStrategy == ProtossStrategy::MidGame)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
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
            case PvP::OurStrategy::Normal:
            {
                if ((newEnemyStrategy == ProtossStrategy::ProxyRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotAllIn) &&
                    !canTransitionFromAntiZealotRush())
                {
                    strategy = OurStrategy::AntiZealotRush;
                    continue;
                }

                if (newEnemyStrategy == ProtossStrategy::DragoonAllIn)
                {
                    strategy = OurStrategy::DTExpand;
                    continue;
                }

                // Transition to mid-game when the enemy has done so
                // TODO: This is very vaguely defined
                if (newEnemyStrategy == ProtossStrategy::MidGame)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvP::OurStrategy::DTExpand:
            {
                // Transition to mid-game when we have taken our natural
                auto natural = Map::getMyNatural();
                if (!natural || natural->ownedSince != -1)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvP::OurStrategy::MidGame:
                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy selection, ended on " << OurStrategyNames[strategy];
    return strategy;
}
