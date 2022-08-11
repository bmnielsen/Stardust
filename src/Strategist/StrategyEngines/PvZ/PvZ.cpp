#include "StrategyEngines/PvZ.h"

#include "Units.h"
#include "Map.h"
#include "Strategist.h"
#include "Players.h"
#include "Workers.h"
#include "Opponent.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"
#include "Plays/Scouting/EjectEnemyScout.h"
#include "Plays/SpecialTeams/Corsairs.h"
#include "Plays/SpecialTeams/Elevator.h"

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;

    template<class T, class ...Args>
    void setMainPlay(MainArmyPlay *current, Args &&...args)
    {
        if (typeid(*current) == typeid(T)) return;
        current->status.transitionTo = std::make_shared<T>(std::forward<Args>(args)...);
    }

    bool enemyHasOurNatural()
    {
        auto natural = Map::getMyNatural();
        return natural && natural->owner == BWAPI::Broodwar->enemy();
    }
}

void PvZ::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<EjectEnemyScout>());
    plays.emplace_back(std::make_shared<Corsairs>());
    plays.emplace_back(std::make_shared<DefendMyMain>());
}

void PvZ::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto newEnemyStrategy = recognizeEnemyStrategy();
    auto newStrategy = chooseOurStrategy(newEnemyStrategy, plays);

    if (enemyStrategy != newEnemyStrategy)
    {
        Log::Get() << "Enemy strategy changed from " << ZergStrategyNames[enemyStrategy] << " to " << ZergStrategyNames[newEnemyStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Enemy strategy changed from " << ZergStrategyNames[enemyStrategy] << " to " << ZergStrategyNames[newEnemyStrategy];
#endif

        enemyStrategy = newEnemyStrategy;
        enemyStrategyChanged = currentFrame;
    }

    if (ourStrategy != newStrategy)
    {
        Log::Get() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#endif

        ourStrategy = newStrategy;
    }

    bool defendOurMain;
    if (hasEnemyStolenOurGas())
    {
        defendOurMain = true;
    }
    else
    {
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        auto canTransitionToAttack = [&](int requiredUnitCount, bool requireDragoon)
        {
            if (!mainArmyPlay) return false;

            auto vanguardCluster = mainArmyPlay->getSquad()->vanguardCluster();
            if (!vanguardCluster || !vanguardCluster->vanguard) return false;

            bool hasDragoon = false;
            int count = 0;
            for (const auto &unit : vanguardCluster->units)
            {
                if (unit->getDistance(vanguardCluster->vanguard) < 200)
                {
                    if (unit->type == BWAPI::UnitTypes::Protoss_Dragoon) hasDragoon = true;
                    count++;
                }
            }

            if (count < requiredUnitCount || (requireDragoon && !hasDragoon)) return false;

            // If the enemy has done a sneak attack recently, don't leave our base until we have built defensive cannons
            if (currentFrame > 10000) return true; // only relevant in early game
            if (enemyHasOurNatural()) return true; // exception if the enemy has our natural
            auto sneakAttack = Opponent::minValueInPreviousGames("sneakAttack", INT_MAX, 20, 0);
            return sneakAttack > 10000 || Units::countCompleted(BWAPI::UnitTypes::Protoss_Photon_Cannon) >= 2;
        };

        switch (ourStrategy)
        {
            case OurStrategy::EarlyGameDefense:
            case OurStrategy::AntiAllIn:
            {
                defendOurMain = true;
                break;
            }
            case OurStrategy::AntiSunkenContain:
            {
                // Attack when we have +1 and speed
                defendOurMain = (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) == 0 ||
                                 BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Leg_Enhancements) == 0);

                // Elevator some units out of our main to attack the enemy main
                {
                    auto elevatorPlay = getPlay<Elevator>(plays);
                    if (!elevatorPlay)
                    {
                        plays.emplace(beforePlayIt<MainArmyPlay>(plays),
                                      std::make_shared<Elevator>(true, BWAPI::UnitTypes::Protoss_Zealot));
                    }
                }

                break;
            }
            default:
            {
                if (!mainArmyPlay)
                {
                    defendOurMain = true;
                    break;
                }

                defendOurMain = false;

                // Transition from defense when appropriate
                if (typeid(*mainArmyPlay) == typeid(DefendMyMain))
                {
                    if (ourStrategy == OurStrategy::FastExpansion)
                    {
                        defendOurMain = !canTransitionToAttack(3, false);
                    }
                    else
                    {
                        defendOurMain = !canTransitionToAttack(4, true);
                    }
                }

                break;
            }
        }
    }

    updateAttackPlays(plays, defendOurMain);

    // Set the worker scout to monitor the enemy choke once the pool is finished
    if (Strategist::getWorkerScoutStatus() == Strategist::WorkerScoutStatus::EnemyBaseScouted)
    {
        auto play = getPlay<EarlyGameWorkerScout>(plays);
        if (play)
        {
            auto pools = Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Spawning_Pool);
            if (!pools.empty() && (*pools.begin())->completed)
            {
                play->monitorEnemyChoke();
            }
        }
    }

    updateDefendBasePlays(plays);
    updateSpecialTeamsPlays(plays);
    defaultExpansions(plays);
    scoutExpos(plays, 10000);
}

void PvZ::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    reserveMineralsForExpansion(mineralReservations);
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(prioritizedProductionGoals);

    if (handleIslandExpansionProduction(plays, prioritizedProductionGoals)) return;

    auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
    auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
    auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

    int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
    int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];
    int dtCount = completedUnits[BWAPI::UnitTypes::Protoss_Dark_Templar] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dark_Templar];
    int corsairCount = Units::countAll(BWAPI::UnitTypes::Protoss_Corsair);

    int inProgressCount = Units::countIncomplete(BWAPI::UnitTypes::Protoss_Zealot)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dragoon)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Dark_Templar)
                          + Units::countIncomplete(BWAPI::UnitTypes::Protoss_Corsair);

    handleGasStealProduction(prioritizedProductionGoals, zealotCount);

    auto buildAntiSneakAttackCannons = [&]()
    {
        // Only relevant in early game
        if (currentFrame > 10000) return;

        // Check if the enemy has done a sneak attack recently
        auto sneakAttack = Opponent::minValueInPreviousGames("sneakAttack", INT_MAX, 20, 0);
        if (sneakAttack > 10000) return;

        // Build three cannons
        buildDefensiveCannons(prioritizedProductionGoals, false, 0, 3);
    };

    auto buildCorsairs = [&]()
    {
        // Limit to 5 + 2 for each enemy mutalisk
        int limit = 5 + (Units::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) / 2);
        if (corsairCount >= limit) return false;

        // Always build if we have DTs to force enemy to defend its overlords
        if (dtCount > 0) return true;

        // Always build if the enemy has air units that can threaten our bases
        if (Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Mutalisk)) return true;

        // Don't build if the enemy has defended its overlords
        auto &grid = Players::grid(BWAPI::Broodwar->enemy());
        int defendedOverlords = 0;
        int undefendedOverlords = 0;
        for (auto &overlord : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Overlord))
        {
            if (!overlord->lastPositionValid) continue;

            long threat = grid.airThreat(overlord->lastPosition);
            ((threat == 0) ? undefendedOverlords : defendedOverlords)++;
        }
        if (defendedOverlords > undefendedOverlords) return false;

        return true;
    };

    // Main army production
    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        {
            // We start with two-gate zealots until we have more scouting information
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       2);
            break;
        }
        case OurStrategy::AntiAllIn:
        {
            // Get at least six zealots before dragoons, more if our choke is hard to defend
            int desiredZealots = 6;
            auto mainChoke = Map::getMyMainChoke();
            if (mainChoke && !mainChoke->isNarrowChoke)
            {
                desiredZealots = 10;
            }

            // Also bump up the number of zealots if the enemy has a lot of lings
            desiredZealots = std::max(desiredZealots, 2 + Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) / 3);

            int zealotsRequired = desiredZealots - zealotCount;

            handleAntiRushProduction(prioritizedProductionGoals, dragoonCount, zealotCount, zealotsRequired);

            // We can build anti-sneak-attack cannons when we have started goon range
            if (Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Singularity_Charge))
            {
                CherryVis::setBoardValue("anti-sneak-attack", "started-goon-range");
                buildAntiSneakAttackCannons();
            }
            else
            {
                CherryVis::setBoardValue("anti-sneak-attack", "not-started-goon-range");
            }

            break;
        }

        case OurStrategy::AntiSunkenContain:
        {
            // Cancel goons, goon range
            cancelTrainingUnits(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon);
            if (Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Singularity_Charge))
            {
                for (const auto &core : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Cybernetics_Core))
                {
                    if (core->bwapiUnit->isUpgrading())
                    {
                        core->bwapiUnit->cancelUpgrade();
                    }
                }
            }

            // Build pure zealots
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE-anticontain",
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            // Get +1 weapons, then zealot speed
            upgrade(prioritizedProductionGoals, BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
            if (Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) ||
                BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons) > 0)
            {
                upgrade(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements);
            }

            // Get a shuttle
            if (Units::countAll(BWAPI::UnitTypes::Protoss_Shuttle) < 1)
            {
                prioritizedProductionGoals[PRIORITY_SPECIALTEAMS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                               "SE",
                                                                               BWAPI::UnitTypes::Protoss_Shuttle,
                                                                               1,
                                                                               1);
            }

            // Take one expansion using a shuttle
            if (Units::countAll(BWAPI::UnitTypes::Protoss_Nexus) < 2) takeExpansionWithShuttle(plays);

            break;
        }

        case OurStrategy::FastExpansion:
        {
            // We've scouted a non-threatening opening, so generally skip zealots and go straight for dragoons
            // Build a couple of zealots though if we have seen zerglings on the way and have nothing to defend with
            if (Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 0)
            {
                int unitCount = zealotCount + dragoonCount;
                if (unitCount < 2)
                {
                    prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                                  "SE-fe",
                                                                                  BWAPI::UnitTypes::Protoss_Zealot,
                                                                                  2 - unitCount,
                                                                                  2);
                }
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE-fe",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE-fe",
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            // Build anti-sneak-attack cannons immediately
            buildAntiSneakAttackCannons();

            break;
        }

        case OurStrategy::Defensive:
        case OurStrategy::Normal:
        {
            // Scale our desired zealots based on enemy ling count
            // Start with three before goons
            int unitCount = zealotCount + dragoonCount;
            int desiredZealots = std::max(3 - unitCount, 1 + (Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) + 2) / 3);
            if (zealotCount < desiredZealots)
            {
                prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                              "SE",
                                                                              BWAPI::UnitTypes::Protoss_Zealot,
                                                                              desiredZealots - zealotCount,
                                                                              2);

                cancelTrainingUnits(prioritizedProductionGoals,
                                    BWAPI::UnitTypes::Protoss_Dragoon,
                                    desiredZealots - zealotCount,
                                    BWAPI::UnitTypes::Protoss_Zealot.buildTime());
            }

            if (buildCorsairs())
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           "SE",
                                                                           BWAPI::UnitTypes::Protoss_Corsair,
                                                                           1,
                                                                           1);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       "SE",
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            // Upgrade goon range at 2 dragoons
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            // Upgrade zealots at varying cutoffs depending on whether we already have a forge
            if (Units::countAll(BWAPI::UnitTypes::Protoss_Forge) > 0)
            {
                upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Protoss_Ground_Weapons, BWAPI::UnitTypes::Protoss_Zealot, 4);
            }
            else
            {
                upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Protoss_Ground_Weapons, BWAPI::UnitTypes::Protoss_Zealot, 6);
            }

            // Build anti-sneak-attack cannons on 4 completed units
            // Exception is if the enemy has taken our natural, in which case we skip this
            if (enemyHasOurNatural())
            {
                CherryVis::setBoardValue("anti-sneak-attack", "enemy-has-our-natural");
            }
            else if ((zealotCount + dragoonCount - inProgressCount) >= 4)
            {
                CherryVis::setBoardValue("anti-sneak-attack", "enough-units");
                buildAntiSneakAttackCannons();
            }
            else
            {
                CherryVis::setBoardValue("anti-sneak-attack", "not-enough-units");
            }

            break;
        }
        case OurStrategy::MidGame:
        {
            // TODO: Higher-tech units

            // Baseline production is one combat unit for every 6 workers (approximately 3 units per mining base)
            int higherPriorityCount = (Workers::mineralWorkers() / 6) - inProgressCount;

            // Keep some zealots in the mix if the opponent has a lot of lings
            int requiredZealots = 0;
            if (Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) > 6)
            {
                int maxZealots = 10;
                if (Units::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) == 0) maxZealots = 20;
                requiredZealots = std::min(maxZealots, Units::countEnemy(BWAPI::UnitTypes::Zerg_Zergling) / 2);
            }

            // Keep zealots in the mix in the later game
            if (dragoonCount > 12)
            {
                requiredZealots = std::max(requiredZealots, (dragoonCount - 12) / 2);
            }

            if (requiredZealots > zealotCount)
            {
                mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Zealot, requiredZealots - zealotCount, higherPriorityCount);
            }

            if (buildCorsairs())
            {
                mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Corsair, 1, higherPriorityCount, 1);
            }
            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Dragoon, -1, higherPriorityCount);
            mainArmyProduction(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Zealot, -1, higherPriorityCount);

            handleUpgrades(prioritizedProductionGoals);
            buildAntiSneakAttackCannons();

            break;
        }
    }
}

void PvZ::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1)
    {
        CherryVis::setBoardValue("natural", "complete");
        return;
    }

    // If we have a backdoor natural, expand when our third goon is being produced or we have lots of money
    if (Map::mapSpecificOverride()->hasBackdoorNatural())
    {
        if (BWAPI::Broodwar->self()->minerals() > 450 ||
            Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon) > 2)
        {
            CherryVis::setBoardValue("natural", "take-backdoor");

            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
    }

    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiAllIn:
        case OurStrategy::AntiSunkenContain:
        case OurStrategy::Defensive:
            // Don't take our natural if the enemy could be rushing or doing an all-in
            CherryVis::setBoardValue("natural", "wait-defensive");
            break;

        case OurStrategy::FastExpansion:
            CherryVis::setBoardValue("natural", "take-fast-expo");
            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;

        case OurStrategy::Normal:
        case OurStrategy::MidGame:
        {
            // In this case we want to expand when we consider it safe to do so: we have an attacking or containing army
            // that is close to the enemy base

            auto mainArmyPlay = getPlay<AttackEnemyBase>(plays);
            if (!mainArmyPlay)
            {
                CherryVis::setBoardValue("natural", "no-attack-play");
                break;
            }

            auto squad = mainArmyPlay->getSquad();
            if (!squad || squad->getUnits().size() < 5)
            {
                CherryVis::setBoardValue("natural", "attack-play-too-small");
                break;
            }

            int dist;
            auto vanguardCluster = squad->vanguardCluster(&dist);
            if (!vanguardCluster)
            {
                CherryVis::setBoardValue("natural", "no-vanguard-cluster");
                break;
            }

            // Cluster should be past our own natural
            int naturalDist = PathFinding::GetGroundDistance(natural->getPosition(), mainArmyPlay->base->getPosition());
            if (naturalDist != -1 && dist > (naturalDist - 320))
            {
                CherryVis::setBoardValue("natural", "vanguard-cluster-too-close");
                break;
            }

            // Cluster should not be moving or fleeing
            // In other words, we want the cluster to be in some kind of stable attack or contain state
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
                || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                    && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee))
            {
                // We don't cancel a queued expansion in this case
                CherryVis::setBoardValue("natural", "vanguard-cluster-not-attacking");
                return;
            }

            CherryVis::setBoardValue("natural", "take");
            takeNaturalExpansion(plays, prioritizedProductionGoals);
            return;
        }
    }

    cancelNaturalExpansion(plays, prioritizedProductionGoals);
}

void PvZ::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 5);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, false, true);
    upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier, true);

    // Upgrade observer speed on three gas
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 3)
    {
        upgradeWhenUnitCreated(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    }

    defaultGroundUpgrades(prioritizedProductionGoals);

    // Air weapon upgrades
    if (!Units::isBeingUpgradedOrResearched(BWAPI::UpgradeTypes::Protoss_Air_Weapons))
    {
        // Keep one level ahead of enemy armor when we have 5 corsairs
        if (BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Air_Weapons) <=
            BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Zerg_Flyer_Carapace))
        {
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Protoss_Air_Weapons, BWAPI::UnitTypes::Protoss_Corsair, 5);
        }
    }
}

void PvZ::handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
    // The logic here is to look ahead to make sure we already have detection available when we need it

    // Break out if we already have an observer
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0 || Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0)
    {
        return;
    }

    // If the opponent has done a lurker rush recently, build a cannon at the choke to protect our main
    auto lurkerInMain = Opponent::minValueInPreviousGames("firstLurkerAtOurMain", INT_MAX, 20, 0);
    if (lurkerInMain < 12000)
    {
        buildDefensiveCannons(prioritizedProductionGoals, true, lurkerInMain - 500, 2);
    }

    // Build an observer when we are on two gas or the enemy has lurker tech
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1 ||
        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 1 && currentFrame > 10000) ||
        Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Lurker_Egg) ||
        Units::hasEnemyBuilt(BWAPI::UnitTypes::Zerg_Lurker) ||
        Players::hasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Lurker_Aspect))
    {
        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 "SE",
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    }
}
