#include "StrategyEngines/PvP.h"

#include "Units.h"
#include "Map.h"
#include "Builder.h"
#include "UnitUtil.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"
#include "Plays/MainArmy/AttackEnemyMain.h"
#include "Plays/MainArmy/MopUp.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"

#if INSTRUMENTATION_ENABLED_VERBOSE
#define OUTPUT_DETECTION_DEBUG false
#endif

namespace
{
    std::map<BWAPI::UnitType, int> emptyUnitCountMap;

    template<class T, class ...Args>
    void setMainPlay(MainArmyPlay *current, Args &&...args)
    {
        if (typeid(*current) == typeid(T)) return;
        current->status.transitionTo = std::make_shared<T>(std::forward<Args>(args)...);
    }
}

void PvP::initialize(std::vector<std::shared_ptr<Play>> &plays)
{
    plays.emplace_back(std::make_shared<SaturateBases>());
    plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
    plays.emplace_back(std::make_shared<DefendMyMain>());
}

void PvP::updatePlays(std::vector<std::shared_ptr<Play>> &plays)
{
    auto newEnemyStrategy = recognizeEnemyStrategy();
    auto newStrategy = chooseOurStrategy(newEnemyStrategy, plays);

    if (enemyStrategy != newEnemyStrategy)
    {
        Log::Get() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Enemy strategy changed from " << ProtossStrategyNames[enemyStrategy] << " to " << ProtossStrategyNames[newEnemyStrategy];
#endif

        enemyStrategy = newEnemyStrategy;
        enemyStrategyChanged = BWAPI::Broodwar->getFrameCount();
    }

    if (ourStrategy != newStrategy)
    {
        Log::Get() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Our strategy changed from " << OurStrategyNames[ourStrategy] << " to " << OurStrategyNames[newStrategy];
#endif

        ourStrategy = newStrategy;
    }

    // Ensure we have the correct main army play
    auto mainArmyPlay = getMainArmyPlay(plays);
    if (mainArmyPlay)
    {
        if (enemyStrategy == ProtossStrategy::GasSteal)
        {
            setMainPlay<DefendMyMain>(mainArmyPlay);
        }
        else
        {
            switch (ourStrategy)
            {
                case OurStrategy::EarlyGameDefense:
                case OurStrategy::AntiZealotRush:
                case OurStrategy::Defensive:
                    setMainPlay<DefendMyMain>(mainArmyPlay);
                    break;
                case OurStrategy::FastExpansion:
                case OurStrategy::Normal:
                case OurStrategy::MidGame:
                {
                    // Transition from a defend squad when the vanguard cluster has 3 units
                    if (typeid(*mainArmyPlay) == typeid(DefendMyMain))
                    {
                        auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
                        if (vanguard && vanguard->units.size() >= 3)
                        {
                            auto enemyMain = Map::getEnemyMain();
                            if (enemyMain)
                            {
                                setMainPlay<AttackEnemyMain>(mainArmyPlay, Map::getEnemyMain());
                            }
                            else
                            {
                                setMainPlay<MopUp>(mainArmyPlay);
                            }
                        }
                    }

                    break;
                }
            }
        }
    }

    UpdateDefendBasePlays(plays);

    defaultExpansions(plays);
}

void PvP::updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                           std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                           std::vector<std::pair<int, int>> &mineralReservations)
{
    handleNaturalExpansion(plays, prioritizedProductionGoals);
    handleDetection(prioritizedProductionGoals);

    // Main army production
    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        {
            // Start with one-gate core with two zealots until we have more scouting information

            auto mainArmyPlay = getMainArmyPlay(plays);
            auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
            auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

            int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
            int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

            if (zealotCount == 0)
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           1,
                                                                           1);
            }
            if (dragoonCount == 0)
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           BWAPI::UnitTypes::Protoss_Dragoon,
                                                                           1,
                                                                           1);
            }
            if (zealotCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                           BWAPI::UnitTypes::Protoss_Zealot,
                                                                           1,
                                                                           1);
            }

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            break;
        }
        case OurStrategy::AntiZealotRush:
        {
            auto mainArmyPlay = getMainArmyPlay(plays);
            auto completedUnits = mainArmyPlay ? mainArmyPlay->getSquad()->getUnitCountByType() : emptyUnitCountMap;
            auto &incompleteUnits = mainArmyPlay ? mainArmyPlay->assignedIncompleteUnits : emptyUnitCountMap;

            int zealotCount = completedUnits[BWAPI::UnitTypes::Protoss_Zealot] + incompleteUnits[BWAPI::UnitTypes::Protoss_Zealot];
            int dragoonCount = completedUnits[BWAPI::UnitTypes::Protoss_Dragoon] + incompleteUnits[BWAPI::UnitTypes::Protoss_Dragoon];

            // Get four zealots before starting the dragoon transition
            int zealotsRequired = 4 - zealotCount;

            // Get two zealots at highest priority
            if (zealotCount < 2)
            {
                prioritizedProductionGoals[PRIORITY_EMERGENCY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                            BWAPI::UnitTypes::Protoss_Zealot,
                                                                            2 - zealotCount,
                                                                            2);
                zealotsRequired -= 2 - zealotCount;
            }

            if (zealotsRequired > 0)
            {
                prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                              BWAPI::UnitTypes::Protoss_Zealot,
                                                                              zealotsRequired,
                                                                              -1);
            }

            // If the dragoon transition is just beginning, only order one so we keep producing zealots
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       dragoonCount == 0 ? 1 : -1,
                                                                       -1);

            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            // Upgrade goon range at 2 dragoons
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            break;
        }

        case OurStrategy::FastExpansion:
        case OurStrategy::Defensive:
        case OurStrategy::Normal:
        case OurStrategy::MidGame:
        {
            // For now all of these just go mass dragoon
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       -1);
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Zealot,
                                                                       -1,
                                                                       -1);

            // Default upgrades
            handleUpgrades(prioritizedProductionGoals);

            break;
        }
    }
}

void PvP::handleNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Hop out if the natural has already been (or is being) taken
    auto natural = Map::getMyNatural();
    if (!natural || natural->ownedSince != -1) return;
    if (Builder::isPendingHere(natural->getTilePosition())) return;

    // Return if our strategy doesn't warrant taking the natural now, otherwise fall through to queue it
    switch (ourStrategy)
    {
        case OurStrategy::EarlyGameDefense:
        case OurStrategy::AntiZealotRush:
        case OurStrategy::Defensive:
            // Don't take our natural if the enemy could be rushing or doing an all-in
            return;

        case OurStrategy::FastExpansion:
            // We want to expand now, so fall through
            break;

        case OurStrategy::Normal:
        case OurStrategy::MidGame:
        {
            // In this case we want to expand when we consider it safe to do so: we have an attacking or containing army
            // that is close to the enemy base

            // We never expand before frame 10000 unless the enemy has done so
            if (BWAPI::Broodwar->getFrameCount() < 10000 && Units::countEnemy(BWAPI::UnitTypes::Protoss_Nexus) < 2) return;

            auto mainArmyPlay = getMainArmyPlay(plays);
            if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyMain)) return;

            auto squad = mainArmyPlay->getSquad();
            if (!squad || squad->getUnits().size() < 5) return;

            int dist;
            auto vanguardCluster = squad->vanguardCluster(&dist);
            if (!vanguardCluster) return;

            // Cluster should be at least 2/3 of the way to the target base
            int distToMain = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->center);
            if (dist * 2 > distToMain) return;

            // Cluster should not be moving or fleeing
            // In other words, we want the cluster to be in some kind of stable attack or contain state
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
                || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                    && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee))
            {
                return;
            }
            break;
        }
    }

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           natural->getTilePosition(),
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0, 0);
    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Nexus, buildLocation);
}

void PvP::handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // Basic infantry skill upgrades are queued when we have enough of them and are still building them
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
    upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

    // Cases where we want the upgrade as soon as we start building one of the units
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, true);
    upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier);

    defaultGroundUpgrades(prioritizedProductionGoals);

    // TODO: Air upgrades
}

void PvP::handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
    // The logic here is to look ahead to make sure we already have detection available when we need it

    // Break out if we already have an observer
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0 || Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0)
    {
        return;
    }

    auto buildObserver = [&prioritizedProductionGoals](int frameNeeded = 0)
    {
        // If we know what frame the observer is needed, check if we need to start building it now
        if (frameNeeded > 0)
        {
            auto framesNeededFor = [](BWAPI::UnitType buildingType)
            {
                int earliestCompletion = INT_MAX;
                for (const auto &unit : Units::allMineIncompleteOfType(buildingType))
                {
                    if (unit->estimatedCompletionFrame < earliestCompletion)
                    {
                        earliestCompletion = unit->estimatedCompletionFrame;
                    }
                }

                if (earliestCompletion == INT_MAX)
                {
                    return UnitUtil::BuildTime(buildingType);
                }

                return earliestCompletion - BWAPI::Broodwar->getFrameCount();
            };
            int frameStarted = frameNeeded - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Observer);
            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observatory) == 0)
            {
                frameStarted -= framesNeededFor(BWAPI::UnitTypes::Protoss_Observatory);

                if (Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observatory) == 0 &&
                    Units::countCompleted(BWAPI::UnitTypes::Protoss_Robotics_Facility) == 0)
                {
                    frameStarted -= framesNeededFor(BWAPI::UnitTypes::Protoss_Robotics_Facility);
                }
            }

            // TODO: When the Producer understands to build something at a specific frame, use that
            if ((BWAPI::Broodwar->getFrameCount() + 500) < frameStarted) return;
        }

        prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                 BWAPI::UnitTypes::Protoss_Observer,
                                                                 1,
                                                                 1);
    };

    // Always get an observer once we are on more than one gas or if the enemy is known to have a DT
    if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1
        || Units::countEnemy(BWAPI::UnitTypes::Protoss_Dark_Templar) > 0)
    {
        buildObserver();
        return;
    }

    // Break out if we have detected a strategy that precludes dark templar
    if (enemyStrategy == ProtossStrategy::EarlyForge
        || enemyStrategy == ProtossStrategy::ProxyRush
        || enemyStrategy == ProtossStrategy::ZealotRush
        || enemyStrategy == ProtossStrategy::ZealotAllIn
        || enemyStrategy == ProtossStrategy::DragoonAllIn
        || enemyStrategy == ProtossStrategy::FastExpansion
        || enemyStrategy == ProtossStrategy::Turtle)
    {
        return;
    }

    // If we haven't found the enemy main, be conservative and assume we might see DTs by frame 8000
    auto enemyMain = Map::getEnemyMain();
    if (!enemyMain)
    {
        buildObserver(8000);
        return;
    }

    // Otherwise compute when the enemy could get DTs based on our scouting information

    // First estimate when the enemy's templar archives will / might finish
    int templarArchivesFinished;
    auto &templarArchiveTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Templar_Archives);
    if (!templarArchiveTimings.empty())
    {
        // We've scouted a templar archives directly, so use its completion frame
        templarArchivesFinished = templarArchiveTimings.begin()->first + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives);
    }
    else
    {
        auto &citadelTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Protoss_Citadel_of_Adun);
        if (!citadelTimings.empty())
        {
            // We've scouted a citadel, so assume the templar archives is started as soon as the citadel finishes, unless
            // we've scouted the base in the meantime
            templarArchivesFinished = UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives) +
                                      std::max(enemyMain->lastScouted,
                                               citadelTimings.begin()->first + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Citadel_of_Adun));
        }
        else
        {
            // We haven't scouted a templar archives or citadel, so assume the enemy started at frame 4000 unless we have scouted
            // since then
            templarArchivesFinished = UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Templar_Archives) +
                                      UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Citadel_of_Adun) +
                                      std::max(4000, enemyMain->lastScouted);
        }
    }

    // Next compute the transit time from the enemy's closest gateway
    auto myMainChoke = Map::getMyMainChoke();
    auto myPosition = myMainChoke ? myMainChoke->center : Map::getMyMain()->getPosition();
    int closestGatewayFrames = PathFinding::ExpectedTravelTime(enemyMain->getPosition(),
                                                               myPosition,
                                                               BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                               PathFinding::PathFindingOptions::UseNearestBWEMArea);
    for (const auto &unit : Units::allEnemyOfType(BWAPI::UnitTypes::Protoss_Gateway))
    {
        if (!unit->completed) continue;

        int frames = PathFinding::ExpectedTravelTime(unit->lastPosition,
                                                     myPosition,
                                                     BWAPI::UnitTypes::Protoss_Dark_Templar,
                                                     PathFinding::PathFindingOptions::UseNearestBWEMArea);
        if (frames < closestGatewayFrames)
        {
            closestGatewayFrames = frames;
        }
    }

#if OUTPUT_DETECTION_DEBUG
    CherryVis::log() << "detection: expect templar archives @ " << templarArchivesFinished
                     << "; DT @ " << (UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar) + templarArchivesFinished)
                     << "; DT at our choke @ "
                     << (closestGatewayFrames + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar) + templarArchivesFinished);
#endif

    // Now sum everything up to get the frame to order the observer
    buildObserver(templarArchivesFinished + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Dark_Templar) + closestGatewayFrames);
}
