#include "StrategyEngines/PvT.h"

#include "Map.h"
#include "Plays/MainArmy/DefendMyMain.h"

std::map<PvT::OurStrategy, std::string> PvT::OurStrategyNames = {
        {OurStrategy::EarlyGameDefense, "EarlyGameDefense"},
        {OurStrategy::AntiMarineRush,   "AntiMarineRush"},
        {OurStrategy::FastExpansion,    "FastExpansion"},
        {OurStrategy::Defensive,        "Defensive"},
        {OurStrategy::Normal,           "Normal"},
        {OurStrategy::MidGame,          "MidGame"}
};

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;
}

PvT::OurStrategy PvT::chooseOurStrategy(PvT::TerranStrategy newEnemyStrategy, std::vector<std::shared_ptr<Play>> &plays)
{
    int enemyStrategyStableFor = 0;
    if (newEnemyStrategy == enemyStrategy) enemyStrategyStableFor = BWAPI::Broodwar->getFrameCount() - enemyStrategyChanged;

    auto canTransitionFromAntiMarineRush = [&]()
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

        // Transition when we have at least 6 units
        return unitCount >= 6;
    };

    auto strategy = ourStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case PvT::OurStrategy::EarlyGameDefense:
            {
                // Transition appropriately as soon as we have an idea of what the enemy is doing
                switch (newEnemyStrategy)
                {
                    case TerranStrategy::Unknown:
                    case TerranStrategy::GasSteal:
                        return strategy;
                    case TerranStrategy::ProxyRush:
                    case TerranStrategy::MarineRush:
                        strategy = OurStrategy::AntiMarineRush;
                        continue;
                    case TerranStrategy::TwoFactory:
                        strategy = OurStrategy::Defensive;
                        continue;
                    case TerranStrategy::WallIn:
                    case TerranStrategy::FastExpansion:
                        strategy = OurStrategy::FastExpansion;
                        continue;
                    case TerranStrategy::Normal:
                        strategy = OurStrategy::Normal;
                        continue;
                    case TerranStrategy::MidGame:
                        strategy = OurStrategy::MidGame;
                        continue;
                }

                break;
            }

            case PvT::OurStrategy::AntiMarineRush:
            {
                // Transition to normal when we consider it safe to do so
                if (canTransitionFromAntiMarineRush())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }
            case PvT::OurStrategy::FastExpansion:
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
            case PvT::OurStrategy::Defensive:
            {
                if (newEnemyStrategy == TerranStrategy::ProxyRush ||
                    newEnemyStrategy == TerranStrategy::MarineRush)
                {
                    strategy = OurStrategy::AntiMarineRush;
                    continue;
                }

                // Transition to normal when we either detect another opening or when there are six units in the vanguard cluster

                if (newEnemyStrategy == TerranStrategy::MidGame)
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
            case PvT::OurStrategy::Normal:
            {
                if ((newEnemyStrategy == TerranStrategy::ProxyRush ||
                     newEnemyStrategy == TerranStrategy::MarineRush) &&
                    !canTransitionFromAntiMarineRush())
                {
                    strategy = OurStrategy::AntiMarineRush;
                    continue;
                }

                // Transition to mid-game when the enemy has done so
                // TODO: This is very vaguely defined
                if (newEnemyStrategy == TerranStrategy::MidGame)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvT::OurStrategy::MidGame:
                break;
        }

        return strategy;
    }

    Log::Get() << "ERROR: Loop in strategy selection, ended on " << OurStrategyNames[strategy];
    return strategy;
}
