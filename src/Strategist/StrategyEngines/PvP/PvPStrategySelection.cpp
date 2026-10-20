#include "StrategyEngines/PvP.h"

#include "Map.h"
#include "Plays/Macro/HiddenBase.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/Scouting/EjectEnemyScout.h"
#include "Units.h"

std::map<PvP::OurStrategy, std::string> PvP::OurStrategyNames = {
        {OurStrategy::ForgeExpandDT,       "ForgeExpandDT"},
        {OurStrategy::TwoGateDT,           "TwoGateDT"},
        {OurStrategy::EarlyGameDefense,    "EarlyGameDefense"},
        {OurStrategy::AntiZealotRush,      "AntiZealotRush"},
        {OurStrategy::AntiDarkTemplarRush, "AntiDarkTemplarRush"},
        {OurStrategy::FastExpansion,       "FastExpansion"},
        {OurStrategy::Defensive,           "Defensive"},
        {OurStrategy::Normal,              "Normal"},
        {OurStrategy::OneGateCoreNoZealot, "OneGateCoreNoZealot"},
        {OurStrategy::ThreeGateRobo,       "3GateRobo"},
        {OurStrategy::DTExpand,            "DTExpand"},
        {OurStrategy::MidGame,             "MidGame"}
};

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;
}

PvP::OurStrategy PvP::chooseOurStrategy(PvP::ProtossStrategy newEnemyStrategy, std::vector <std::shared_ptr<PlayItem>> &playList)
{
    auto canTransitionFromAntiZealotRush = [&]()
    {
        /* Count total combat units */
        auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
        auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
        auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;
        int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                        completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

        /* Transition immediately if we've misdetected a proxy rush */
        if (enemyStrategy == ProtossStrategy::ProxyRush &&
            newEnemyStrategy != ProtossStrategy::WorkerRush &&
            newEnemyStrategy != ProtossStrategy::ProxyRush &&
            newEnemyStrategy != ProtossStrategy::ZealotRush &&
            newEnemyStrategy != ProtossStrategy::ZealotAllIn)
        {
            return true;
        }

        /* Transition immediately if we're past frame 4500 and haven't seen an enemy zealot yet */
        if (frameNow > 4500 && !UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Zealot))
        {
            return true;
        }

        /* Transition immediately if we've discovered a different enemy strategy and have at least three completed dragoons */
        if (newEnemyStrategy != ProtossStrategy::BlockScouting &&
            newEnemyStrategy != ProtossStrategy::WorkerRush &&
            newEnemyStrategy != ProtossStrategy::ProxyRush &&
            newEnemyStrategy != ProtossStrategy::ZealotRush &&
            newEnemyStrategy != ProtossStrategy::ZealotAllIn &&
            completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] > 2)
        {
            if (UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) <= unitCount) return true;
        }

        /* Transition immediately if we have at least two dragoons and the enemy has expanded */
        if (UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) > 1 &&
            completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] > 1)
        {
            return true;
        }

        /* Require Dragoon Range to be started */
        /* TODO: This is probably much too conservative */
        if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0 &&
            !UnitMgr::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Singularity_Charge))
        {
            return false;
        }

        /* Transition when we have at least 10 units */
        return unitCount >= 10;
    };

    auto canTransitionFromAntiDarkTemplarRush = [&]()
    {
        auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
        if (!mainArmyPlay) return false;

        return mainArmyPlay->getSquad()->hasDetection();
    };

    auto isDTExpandFeasible = [&]()
    {
        if (enemyStrategy == ProtossStrategy::DarkTemplarRush) return false;
        if (Map::mapSpecificOverride()->hasBackdoorNatural()) return false;

        auto frameCutoff = getPlay<HiddenBase>(playList) == nullptr ? 9000 : 10500;
        if (ourStrategy != OurStrategy::DTExpand && frameNow > frameCutoff) return false;

        /* Make sure our main choke is easily defensible */
        auto choke = Map::getMyMainChoke();
        if (!choke || !choke->isNarrowChoke || !choke->isRamp) return false;
        if (!Map::mapSpecificOverride()->hasBackdoorNatural() && Map::getMyMain() && Map::getMyNatural() &&
            BWAPI::Broodwar->getGroundHeight(Map::getMyMain()->getTilePosition())
            <= BWAPI::Broodwar->getGroundHeight(Map::getMyNatural()->getTilePosition()))
        {
            return false;
        }

        return !(UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Forge) ||
                 UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Photon_Cannon) ||
                 UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility) ||
                 UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory) ||
                 UnitMgr::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer));
    };

    auto strategy = ourStrategy;

    /* P1-B (2026-09-20 report v2): one-base all-in lines have a ~14 minute */
    /* decision window. Past the configured frame with one base and no */
    /* army-value lead, rotate into MidGame so the natural expansion and */
    /* mid-game production take over (PvP::allinExitDue). Expansion-owning */
    /* lines (FFE/2GateDT/DTExpand) and an in-progress MidGame are excluded - */
    /* the exit never fights its own transition. */
    if (ourStrategy != OurStrategy::ForgeExpandDT &&
        ourStrategy != OurStrategy::TwoGateDT &&
        ourStrategy != OurStrategy::DTExpand &&
        ourStrategy != OurStrategy::MidGame &&
        allinExitDue())
    {
        strategy = OurStrategy::MidGame;
    }

    for (int i = 0; i < 10; i++)
    {
        switch (strategy)
        {
            case PvP::OurStrategy::ForgeExpandDT:
            {
                auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
                if (!mainArmyPlay) break;

                /* If the enemy is doing a rush and our main army play has transitioned to defending the main, switch to anti all-in strategy */
                if ((enemyStrategy == ProtossStrategy::ZealotRush || enemyStrategy == ProtossStrategy::ProxyRush)
                    && typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    strategy = OurStrategy::AntiZealotRush;
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
            case PvP::OurStrategy::TwoGateDT:
            {
                if ((newEnemyStrategy == ProtossStrategy::WorkerRush ||
                     newEnemyStrategy == ProtossStrategy::ProxyRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotAllIn) &&
                    !canTransitionFromAntiZealotRush())
                {
                    strategy = OurStrategy::AntiZealotRush;
                    continue;
                }

                /* If we failed to eject the enemy scout early enough, abandon DTs and go for a normal strategy */
                if (frameNow > 7500)
                {
                    auto ejectScoutPlay = getPlay<EjectEnemyScout>(playList);
                    if (ejectScoutPlay && !ejectScoutPlay->hasEjectedScout())
                    {
                        strategy = OurStrategy::Normal;
                        continue;
                    }
                }

                /* Transition to DTExpand when our DTs are completed */
                if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Dark_Templar) > 1)
                {
                    strategy = OurStrategy::DTExpand;
                    continue;
                }

                break;
            }
            case PvP::OurStrategy::EarlyGameDefense:
            {
                /* Transition appropriately as soon as we have an idea of what the enemy is doing */
                switch (newEnemyStrategy)
                {
                    case ProtossStrategy::Unknown:
                        return strategy;
                    case ProtossStrategy::WorkerRush:
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
                    case ProtossStrategy::Turtle:
                    {
                        strategy = OurStrategy::FastExpansion;
                        continue;
                    }
                    case ProtossStrategy::DarkTemplarRush:
                    {
                        strategy = OurStrategy::AntiDarkTemplarRush;
                        continue;
                    }
                    case ProtossStrategy::FastExpansion:
                    case ProtossStrategy::EarlyForge:
                    case ProtossStrategy::NoZealotCore:
                    case ProtossStrategy::OneZealotCore:
                    case ProtossStrategy::BlockScouting:
                    case ProtossStrategy::EarlyRobo:
                    {
                        strategy = OurStrategy::Normal;
                        continue;
                    }
                    case ProtossStrategy::DragoonAllIn:
                    {
                        strategy = isDTExpandFeasible() ? OurStrategy::DTExpand : OurStrategy::Normal;
                        continue;
                    }
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
                /* Transition to normal when we consider it safe to do so */
                if (canTransitionFromAntiZealotRush())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }

            case PvP::OurStrategy::AntiDarkTemplarRush:
            {
                /* Transition to normal when we consider it safe to do so */
                if (canTransitionFromAntiDarkTemplarRush())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }

            case PvP::OurStrategy::FastExpansion:
            {
                /* Transition to normal when the expansion is taken */
                auto natBase = Map::getMyNatural();
                if (!natBase || natBase->ownedSince != -1)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }

            case PvP::OurStrategy::Defensive:
            {
                if (newEnemyStrategy == ProtossStrategy::WorkerRush ||
                    newEnemyStrategy == ProtossStrategy::ProxyRush ||
                    newEnemyStrategy == ProtossStrategy::ZealotRush ||
                    newEnemyStrategy == ProtossStrategy::ZealotAllIn)
                {
                    strategy = OurStrategy::AntiZealotRush;
                    continue;
                }

                if (newEnemyStrategy == ProtossStrategy::DarkTemplarRush)
                {
                    strategy = OurStrategy::AntiDarkTemplarRush;
                    continue;
                }

                if (newEnemyStrategy == ProtossStrategy::DragoonAllIn)
                {
                    strategy = isDTExpandFeasible() ? OurStrategy::DTExpand : OurStrategy::Normal;
                    continue;
                }

                /* Transition to normal when we either detect another opening or when there are six units in the vanguard cluster */

                if (newEnemyStrategy == ProtossStrategy::Turtle || newEnemyStrategy == ProtossStrategy::MidGame)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                auto mainArmyPlay = getPlay<MainArmyPlay>(playList);
                if (mainArmyPlay && mainArmyPlay->isDefensive())
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
                if ((newEnemyStrategy == ProtossStrategy::WorkerRush ||
                     newEnemyStrategy == ProtossStrategy::ProxyRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotAllIn) &&
                    !canTransitionFromAntiZealotRush())
                {
                    strategy = OurStrategy::AntiZealotRush;
                    continue;
                }

                if (newEnemyStrategy == ProtossStrategy::DarkTemplarRush &&
                    !canTransitionFromAntiDarkTemplarRush())
                {
                    strategy = OurStrategy::AntiDarkTemplarRush;
                    continue;
                }

                if (newEnemyStrategy == ProtossStrategy::DragoonAllIn && isDTExpandFeasible())
                {
                    strategy = OurStrategy::DTExpand;
                    continue;
                }

                /* Transition to mid-game when the enemy has done so or we are on two bases */
                /* TODO: This is very vaguely defined */
                if (newEnemyStrategy == ProtossStrategy::MidGame || UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                /* TODO: Define conditions to go 3-gate robo based on opponent model */
/* strategy = OurStrategy::ThreeGateRobo; */
/* continue; */

                break;
            }
            case PvP::OurStrategy::ThreeGateRobo:
            {
                if ((newEnemyStrategy == ProtossStrategy::WorkerRush ||
                     newEnemyStrategy == ProtossStrategy::ProxyRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotRush ||
                     newEnemyStrategy == ProtossStrategy::ZealotAllIn) &&
                    !canTransitionFromAntiZealotRush())
                {
                    strategy = OurStrategy::AntiZealotRush;
                    continue;
                }

                if (newEnemyStrategy == ProtossStrategy::DragoonAllIn && isDTExpandFeasible())
                {
                    strategy = OurStrategy::DTExpand;
                    continue;
                }

                /* Transition to mid-game when the enemy has done so or we are on two bases */
                /* TODO: This is very vaguely defined */
                if (newEnemyStrategy == ProtossStrategy::MidGame || UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvP::OurStrategy::DTExpand:
            {
                /* Transition to normal if a DT expand is no longer feasible */
                if (!isDTExpandFeasible())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                /* Transition to mid-game when we have taken our natural */
                auto natBase = Map::getMyNatural();
                if (!natBase || natBase->ownedSince != -1)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvP::OurStrategy::MidGame:
                if (newEnemyStrategy == ProtossStrategy::DarkTemplarRush &&
                    !canTransitionFromAntiDarkTemplarRush())
                {
                    strategy = OurStrategy::AntiDarkTemplarRush;
                    continue;
                }

                break;
        }

        return strategy;
    }

    AppLog::Get() << "ERROR: Loop in strategy selection, ended on " << OurStrategyNames[strategy];
    return strategy;
}
