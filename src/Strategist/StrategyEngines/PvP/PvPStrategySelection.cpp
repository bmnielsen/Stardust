#include "StrategyEngines/PvP.h"

#include "Map.h"
#include "Plays/Macro/HiddenBase.h"
#include "Units.h"

std::map<PvP::OurStrategy, std::string> PvP::OurStrategyNames = {
        {OurStrategy::EarlyGameDefense,    "EarlyGameDefense"},
        {OurStrategy::AntiZealotRush,      "AntiZealotRush"},
        {OurStrategy::AntiDarkTemplarRush, "AntiDarkTemplarRush"},
        {OurStrategy::FastExpansion,       "FastExpansion"},
        {OurStrategy::Defensive,           "Defensive"},
        {OurStrategy::Normal,              "Normal"},
        {OurStrategy::ThreeGateRobo,       "3GateRobo"},
        {OurStrategy::DTExpand,            "DTExpand"},
        {OurStrategy::MidGame,             "MidGame"}
};

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;
}

PvP::OurStrategy PvP::chooseOurStrategy(PvP::ProtossStrategy newEnemyStrategy, std::vector <std::shared_ptr<Play>> &plays)
{
    auto canTransitionFromAntiZealotRush = [&]()
    {
        // Count total combat units
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
        auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;
        int unitCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot] +
                        completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

        // Transition immediately if we've misdetected a proxy rush
        if (enemyStrategy == ProtossStrategy::ProxyRush &&
            newEnemyStrategy != ProtossStrategy::WorkerRush &&
            newEnemyStrategy != ProtossStrategy::ProxyRush &&
            newEnemyStrategy != ProtossStrategy::ZealotRush &&
            newEnemyStrategy != ProtossStrategy::ZealotAllIn)
        {
            return true;
        }

        // Transition immediately if we're past frame 4500 and haven't seen an enemy zealot yet
        if (currentFrame > 4500 && !Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Zealot))
        {
            return true;
        }

        // Transition immediately if we've discovered a different enemy strategy and have at least three completed dragoons
        if (newEnemyStrategy != ProtossStrategy::BlockScouting &&
            newEnemyStrategy != ProtossStrategy::WorkerRush &&
            newEnemyStrategy != ProtossStrategy::ProxyRush &&
            newEnemyStrategy != ProtossStrategy::ZealotRush &&
            newEnemyStrategy != ProtossStrategy::ZealotAllIn &&
            completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] > 2)
        {
            if (Units::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) <= unitCount) return true;
        }

        // Transition immediately if we have at least two dragoons and the enemy has expanded
        if (Units::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) > 1 &&
            completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] > 1)
        {
            return true;
        }

        // Require Dragoon Range to be started
        // TODO: This is probably much too conservative
        if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Singularity_Charge) == 0 &&
            !Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Singularity_Charge))
        {
            return false;
        }

        // Transition when we have at least 10 units
        return unitCount >= 10;
    };

    auto canTransitionFromAntiDarkTemplarRush = [&]()
    {
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        if (!mainArmyPlay) return false;

        return mainArmyPlay->getSquad()->hasDetection();
    };

    auto isDTExpandFeasible = [&]()
    {
        if (enemyStrategy == ProtossStrategy::DarkTemplarRush) return false;

        auto frameCutoff = getPlay<HiddenBase>(plays) == nullptr ? 9000 : 10500;
        if (ourStrategy != OurStrategy::DTExpand && currentFrame > frameCutoff) return false;

        // Make sure our main choke is easily defensible
        auto choke = Map::getMyMainChoke();
        if (!choke || !choke->isNarrowChoke || !choke->isRamp) return false;
        if (!Map::mapSpecificOverride()->hasBackdoorNatural() && Map::getMyMain() && Map::getMyNatural() &&
            BWAPI::Broodwar->getGroundHeight(Map::getMyMain()->getTilePosition())
            <= BWAPI::Broodwar->getGroundHeight(Map::getMyNatural()->getTilePosition()))
        {
            return false;
        }

        return !(Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Forge) ||
                 Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Photon_Cannon) ||
                 Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Robotics_Facility) ||
                 Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observatory) ||
                 Units::hasEnemyBuilt(BWAPI::UnitTypes::Protoss_Observer));
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
                // Transition to normal when we consider it safe to do so
                if (canTransitionFromAntiZealotRush())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                break;
            }

            case PvP::OurStrategy::AntiDarkTemplarRush:
            {
                // Transition to normal when we consider it safe to do so
                if (canTransitionFromAntiDarkTemplarRush())
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
                    strategy = isDTExpandFeasible() ? OurStrategy::DTExpand : OurStrategy::Normal;;
                    continue;
                }

                // Transition to normal when we either detect another opening or when there are six units in the vanguard cluster

                if (newEnemyStrategy == ProtossStrategy::Turtle || newEnemyStrategy == ProtossStrategy::MidGame)
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

                auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
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

                // Transition to mid-game when the enemy has done so or we are on two bases
                // TODO: This is very vaguely defined
                if (newEnemyStrategy == ProtossStrategy::MidGame || Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                // TODO: Define conditions to go 3-gate robo based on opponent model
//                strategy = OurStrategy::ThreeGateRobo;
//                continue;

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

                // Transition to mid-game when the enemy has done so or we are on two bases
                // TODO: This is very vaguely defined
                if (newEnemyStrategy == ProtossStrategy::MidGame || Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1)
                {
                    strategy = OurStrategy::MidGame;
                    continue;
                }

                break;
            }
            case PvP::OurStrategy::DTExpand:
            {
                // Transition to normal if a DT expand is no longer feasible
                if (!isDTExpandFeasible())
                {
                    strategy = OurStrategy::Normal;
                    continue;
                }

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

    Log::Get() << "ERROR: Loop in strategy selection, ended on " << OurStrategyNames[strategy];
    return strategy;
}
