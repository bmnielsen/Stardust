#include "StrategyEngines/PvT.h"

#include "Map.h"
#include "Units.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"

std::map<PvT::OurStrategy, std::string> PvT::OurStrategyNames = {
        {OurStrategy::ForgeExpandGoons, "ForgeExpandGoons"},
        {OurStrategy::EarlyGameDefense, "EarlyGameDefense"},
        {OurStrategy::AntiMarineRush,   "AntiMarineRush"},
        {OurStrategy::FastExpansion,    "FastExpansion"},
        {OurStrategy::Defensive,        "Defensive"},
        {OurStrategy::NormalOpening,    "Normal"},
        {OurStrategy::MidGame,          "MidGame"},
};

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;
}

PvT::OurStrategy PvT::chooseOurStrategy(PvT::TerranStrategy newEnemyStrategy, std::vector<std::shared_ptr<PlayItem>> &playList)
{
    auto canTransitionFromAntiMarineRush = [&]()
    {
        /* Transition immediately if we've discovered a different enemy strategy */
        if (newEnemyStrategy != TerranStrategy::WorkerRush &&
            newEnemyStrategy != TerranStrategy::ProxyRush &&
            newEnemyStrategy != TerranStrategy::MarineRush &&
            newEnemyStrategy != TerranStrategy::BlockScouting)
        {
            return true;
        }

        /* Require Dragoon Range */
        /* TODO: This is probably much too conservative */
        if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0) return false;

        /* Count total combat units */
        auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
        auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
        auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;
        int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                        completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

        /* Transition when we have at least 6 units */
        return unitCount >= 6;
    };

    auto strategy = ourStrategy;
    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case PvT::OurStrategy::ForgeExpandGoons:
            {
                auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
                if (!mainArmyPlay) break;

                /* If the enemy is doing a rush and our main army play has transitioned to defending the main, switch to anti all-in strategy */
                if ((enemyStrategy == TerranStrategy::MarineRush || enemyStrategy == TerranStrategy::ProxyRush)
                    && typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    strategy = OurStrategy::AntiMarineRush;
                    continue;
                }

                /* Transition to mid game when we have gone on the attack, indicating the opening is done */
                if (typeid(*mainArmyPlay) == typeid(AttackEnemyBase))
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvT::OurStrategy::EarlyGameDefense:
            {
                /* Transition appropriately as soon as we have an idea of what the enemy is doing */
                switch (newEnemyStrategy)
                {
                    case TerranStrategy::Unknown:
                        return strategy;
                    case TerranStrategy::WorkerRush:
                    case TerranStrategy::ProxyRush:
                    case TerranStrategy::MarineRush:
                    case TerranStrategy::BlockScouting:
                        strategy = OurStrategy::AntiMarineRush;
                        continue;
                    case TerranStrategy::TwoFactory:
                        strategy = OurStrategy::Defensive;
                        continue;
                    case TerranStrategy::FastExpansion:
                        strategy = OurStrategy::FastExpansion;
                        continue;
                    case TerranStrategy::WallIn:
                    case TerranStrategy::NormalOpening:
                        strategy = OurStrategy::NormalOpening;
                        continue;
                    case TerranStrategy::MidGameMech:
                    case TerranStrategy::MidGameBio:
                        strategy = OurStrategy::MidGame;
                        continue;
                }

                break;
            }

            case PvT::OurStrategy::AntiMarineRush:
            {
                /* Transition to normal when we consider it safe to do so */
                if (canTransitionFromAntiMarineRush())
                {
                    strategy = OurStrategy::NormalOpening;
                    continue;
                }

                break;
            }
            case PvT::OurStrategy::FastExpansion:
            {
                /* Transition to normal when the expansion is taken */
                auto natBase = Map::getMyNatural();
                if (!natBase || natBase->ownedSince != -1)
                {
                    strategy = OurStrategy::NormalOpening;
                    continue;
                }

                break;
            }
            case PvT::OurStrategy::Defensive:
            {
                if (newEnemyStrategy == TerranStrategy::WorkerRush ||
                    newEnemyStrategy == TerranStrategy::ProxyRush ||
                    newEnemyStrategy == TerranStrategy::MarineRush ||
                    newEnemyStrategy == TerranStrategy::BlockScouting)
                {
                    strategy = OurStrategy::AntiMarineRush;
                    continue;
                }

                /* Transition to normal when we either detect another opening or when there are six units in the vanguard cluster */

                if (newEnemyStrategy == TerranStrategy::MidGameMech ||
                    newEnemyStrategy == TerranStrategy::MidGameBio)
                {
                    strategy = OurStrategy::NormalOpening;
                    continue;
                }

                auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
                if (mainArmyPlay && mainArmyPlay->isDefensive())
                {
                    auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                    if (vanguard && vanguard->units.size() >= 6)
                    {
                        strategy = OurStrategy::NormalOpening;
                        continue;
                    }
                }

            }
            case PvT::OurStrategy::NormalOpening:
            {
                if ((newEnemyStrategy == TerranStrategy::WorkerRush ||
                     newEnemyStrategy == TerranStrategy::ProxyRush ||
                     newEnemyStrategy == TerranStrategy::MarineRush ||
                     newEnemyStrategy == TerranStrategy::BlockScouting) &&
                    !canTransitionFromAntiMarineRush())
                {
                    strategy = OurStrategy::AntiMarineRush;
                    continue;
                }

                /* Transition to mid-game when the enemy has done so or we are on two bases */
                /* TODO: This is very vaguely defined */
                if (newEnemyStrategy == TerranStrategy::MidGameMech ||
                    newEnemyStrategy == TerranStrategy::MidGameBio ||
                    UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1)
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

    AppLog::Get() << "ERROR: Loop in strategy selection, ended on " << OurStrategyNames[strategy];
    return strategy;
}
