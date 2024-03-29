#include "StrategyEngine.h"

#include "Map.h"
#include "Units.h"
#include "Workers.h"
#include "Strategist.h"
#include "Builder.h"
#include "General.h"
#include "UnitUtil.h"

#include "Plays/Macro/TakeExpansion.h"
#include "Plays/Macro/TakeIslandExpansion.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/MainArmy/MopUp.h"
#include "Plays/SpecialTeams/Elevator.h"

namespace
{
    auto addIslandExpansionPlay(std::vector<std::shared_ptr<Play>> &plays, Base *base, bool canCancel = true, bool transferWorkers = true)
    {
        auto play = std::make_shared<TakeIslandExpansion>(base, canCancel, transferWorkers);

        // Taking an island base is always lower priority than an elevator
        auto it = StrategyEngine::beforePlayIt<Elevator>(plays);
        if (it == plays.end())
        {
            it = plays.begin();
        }
        else
        {
            it++;
        }
        plays.emplace(it, play);

        return play;
    }
}

void StrategyEngine::defaultExpansions(std::vector<std::shared_ptr<Play>> &plays)
{
    // This logic does not handle the first decision to take our natural expansion, so if this hasn't been done, bail out now
    auto natural = Map::getMyNatural();
    if (natural && ((natural->ownedSince == -1 && Map::getMyBases().size() == 1) || (natural->resourceDepot && !natural->resourceDepot->completed)))
    {
        return;
    }

    // If the natural is "owned" by our opponent, it's probably because of some kind of proxy play
    // In this case, delay doing any expansions until mid-game
    if (natural && natural->owner != BWAPI::Broodwar->self() && currentFrame < 12000) return;

    // Collect any existing TakeExpansion plays
    std::vector<std::shared_ptr<TakeExpansion>> takeExpansionPlays;
    std::vector<std::shared_ptr<TakeIslandExpansion>> takeIslandExpansionPlays;
    for (auto &play : plays)
    {
        if (auto takeExpansionPlay = std::dynamic_pointer_cast<TakeExpansion>(play))
        {
            if (auto takeIslandExpansionPlay = std::dynamic_pointer_cast<TakeIslandExpansion>(play))
            {
                takeIslandExpansionPlays.push_back(takeIslandExpansionPlay);
            }
            else
            {
                takeExpansionPlays.push_back(takeExpansionPlay);
            }
        }
    }

    // Determines if we consider it safe to expand to an island
    auto safeToIslandExpand = [&]()
    {
        // Only expand when our army is on the offensive
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        if (!mainArmyPlay) return false;
        if (typeid(*mainArmyPlay) == typeid(MopUp)) return true;
        if (typeid(*mainArmyPlay) != typeid(AttackEnemyBase)) return false;

        // Sanity check that the play has a squad
        auto squad = mainArmyPlay->getSquad();
        if (!squad) return false;

        // Ensure the vanguard cluster is at least 20 tiles away from our natural, or main if map has a backdoor natural
        auto vanguardCluster = squad->vanguardCluster();
        if (!vanguardCluster) return false;

        auto natural = Map::mapSpecificOverride()->hasBackdoorNatural() ? Map::getMyMain() : Map::getMyNatural();
        if (natural && vanguardCluster->vanguard)
        {
            int naturalDist = PathFinding::GetGroundDistance(natural->getPosition(), vanguardCluster->vanguard->lastPosition);
            if (naturalDist != -1 && naturalDist < 640)
            {
                return false;
            }
        }

        return true;
    };

    // Check if we want to cancel an active TakeIslandExpansionPlay
    if (!takeIslandExpansionPlays.empty())
    {
        if (!safeToIslandExpand())
        {
            for (auto &takeIslandExpansionPlay : takeIslandExpansionPlays)
            {
                if (takeIslandExpansionPlay->cancellable())
                {
                    takeIslandExpansionPlay->status.complete = true;

                    Log::Get() << "Cancelled island expansion to " << takeIslandExpansionPlay->depotPosition;
                }
            }
        }

        // For most island expansions, we would return here and leave just the island expo play enabled
        // But if we are taking an island expansion with a blocking neutral that takes a while to clear, drop through
        // and allow normal expansions
        if ((*takeIslandExpansionPlays.begin())->framesToClearBlocker() < 750)
        {
            return;
        }
    }

    // Determine whether we want to expand now
    bool gasStarved = BWAPI::Broodwar->self()->minerals() > 1500 && BWAPI::Broodwar->self()->gas() < 500;
    int excessMineralAssignments = 0;
    if (!gasStarved)
    {
        // Expand if we have no bases with more than 3 available mineral assignments
        // Adjust by the number of pending expansions to avoid instability when a build worker is reserved for the expansion
        int availableMineralAssignmentsThreshold = 3 + (int)takeExpansionPlays.size();

        // If the enemy is contained, boost the threshold
        // This is for now only applied to terran, since we can lose games against protoss and zerg by overexpanding
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran && Strategist::isEnemyContained())
        {
            availableMineralAssignmentsThreshold += 3;
        }

        for (auto base : Map::getMyBases())
        {
            excessMineralAssignments = std::max(excessMineralAssignments,
                                                Workers::availableMineralAssignments(base) - availableMineralAssignmentsThreshold);
        }
    }

    // Determines if we consider it safe to expand to a normal expansion
#if LOGGING_ENABLED
    std::string reasonUnsafe;
#endif
    auto safeToExpand = [&]()
    {
        // Only expand when our army is on the offensive
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        if (!mainArmyPlay)
        {
#if LOGGING_ENABLED
            reasonUnsafe = "No main army play";
#endif
            return false;
        }
        if (typeid(*mainArmyPlay) == typeid(MopUp)) return true;
        if (typeid(*mainArmyPlay) != typeid(AttackEnemyBase))
        {
#if LOGGING_ENABLED
            reasonUnsafe = "Main army play is defensive";
#endif
            return false;
        }

        // Sanity check that the play has a squad
        auto squad = mainArmyPlay->getSquad();
        if (!squad)
        {
#if LOGGING_ENABLED
            reasonUnsafe = "Main army play has no squad";
#endif
            return false;
        }

        // Get the vanguard cluster
        auto vanguardCluster = squad->vanguardCluster();
        if (!vanguardCluster)
        {
#if LOGGING_ENABLED
            reasonUnsafe = "Main army play squad has no units";
#endif
            return false;
        }

        // Don't expand if the cluster has fewer than five units
        if (vanguardCluster->units.size() < 5)
        {
#if LOGGING_ENABLED
            reasonUnsafe = "Main army play vanguard cluster has fewer than 5 units";
#endif
            return false;
        }

        // Expand if we are gas blocked - we have the resources for the nexus anyway
        if (BWAPI::Broodwar->self()->minerals() > 500 && BWAPI::Broodwar->self()->gas() < 100) return true;

        // Cluster should not be moving or fleeing
        // In other words, we want the cluster to be in some kind of stable attack or contain state
        if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
            || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee))
        {
#if LOGGING_ENABLED
            reasonUnsafe = "Main army play vanguard cluster is moving or fleeing";
#endif
            return false;
        }

        return true;
    };

    auto enemyCombatValueAtBase = [](Base *base)
    {
        int result = 0;
        for (const auto &unit : Units::enemyAtBase(base))
        {
            if (unit->isTransport() || UnitUtil::CanAttackGround(unit->type))
            {
                result += CombatSim::unitValue(unit);
            }
        }

        return result;
    };

    bool excessIdleWorkers = (Workers::idleWorkerCount() >= 10);

    // Check if we want to cancel an active TakeExpansionPlay
    if (!takeExpansionPlays.empty())
    {
        if (excessIdleWorkers) return;

        auto safe = safeToExpand();
        for (auto &takeExpansionPlay : takeExpansionPlays)
        {
            if (takeExpansionPlay->cancellable())
            {
                if (excessMineralAssignments > 1)
                {
                    takeExpansionPlay->status.complete = true;
#if LOGGING_ENABLED
                    Log::Get() << "Cancelled expansion to " << takeExpansionPlay->depotPosition
                               << ": excess mineral assignments is now " << excessMineralAssignments;
#endif
                }
                if (!safe)
                {
                    takeExpansionPlay->status.complete = true;
#if LOGGING_ENABLED

                    Log::Get() << "Cancelled expansion to " << takeExpansionPlay->depotPosition
                               << ": no longer safe: " << reasonUnsafe;
#endif
                }
                if (takeExpansionPlay->enemyValue > 4 * CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon))
                {
                    takeExpansionPlay->status.complete = true;
#if LOGGING_ENABLED

                    Log::Get() << "Cancelled expansion to " << takeExpansionPlay->depotPosition
                               << ": enemy combat value at expansion exceeds 4 dragoon equivalent";
#endif
                }
            }
        }

        return;
    }

    // Take an island expansion in the following cases:
    // - We are on three bases and already have a robo facility - DISABLED
    // - We are contained on one base and it is after frame 16000
    if (takeIslandExpansionPlays.empty() && excessMineralAssignments == 0 && (
//        (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) > 2 &&
//         Units::countCompleted(BWAPI::UnitTypes::Protoss_Robotics_Facility) > 0) ||
        (currentFrame > 16000 &&
         Units::countAll(BWAPI::UnitTypes::Protoss_Nexus) == 1 &&
         Strategist::areWeContained())))
    {
        Base *closestIslandBase = nullptr;
        int closestIslandBaseDist = INT_MAX;
        for (auto &islandBase : Map::getUntakenIslandExpansions())
        {
            int dist = Map::getMyMain()->getPosition().getApproxDistance(islandBase->getPosition());
            if (dist < closestIslandBaseDist)
            {
                closestIslandBase = islandBase;
                closestIslandBaseDist = dist;
            }
        }

        if (closestIslandBase && closestIslandBaseDist < 2500 && safeToIslandExpand())
        {
            auto play = addIslandExpansionPlay(plays, closestIslandBase);
#if LOGGING_ENABLED
            Log::Get() << "Queued island expansion to " << play->depotPosition;
#endif
#if CHERRYVIS_ENABLED
            CherryVis::log() << "Added TakeIslandExpansion play for base @ " << BWAPI::WalkPosition(play->depotPosition);
#endif
            return;
        }
    }

    // Break out if we don't want to expand to a normal base
    if (excessMineralAssignments > 0 || (!safeToExpand() && !excessIdleWorkers)) return;

    // Determine if we want to consider a mineral-only base
    auto shouldTakeMineralOnly = [&gasStarved]()
    {
        if (gasStarved) return false;

        // Take a mineral-only if we have an excess of gas
        if (BWAPI::Broodwar->self()->gas() > 1500) return true;

        // Count the number of active gas and mineral-only bases we have
        int gasBases = 0;
        int mineralOnlyBases = 0;
        for (const auto &base : Map::getMyBases())
        {
            if (base->gas > 0)
            {
                gasBases++;
            }
            else if (base->minerals > 1000)
            {
                mineralOnlyBases++;
            }
        }

        // Take a mineral-only base for every three gas bases
        return (gasBases - (mineralOnlyBases * 3) > 2);
    };
    bool takeMineralOnly = shouldTakeMineralOnly();

    // Create a TakeExpansion play for the next expansion
    for (const auto &expansion : Map::getUntakenExpansions())
    {
        if (!takeMineralOnly && expansion->gas == 0) continue;

        // Don't take expansions that are blocked by the enemy and that we don't know how to unblock
        if (expansion->blockedByEnemy)
        {
            auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(expansion);
            if (!baseStaticDefenseLocations.isValid())
            {
                continue;
            }
        }

        auto enemyValue = enemyCombatValueAtBase(expansion);
        if (enemyValue > 4 * CombatSim::unitValue(BWAPI::UnitTypes::Protoss_Dragoon))
        {
            continue;
        }

        auto play = std::make_shared<TakeExpansion>(expansion, enemyValue);
        plays.emplace(plays.begin(), play);

#if LOGGING_ENABLED
        Log::Get() << "Queued expansion to " << play->depotPosition;
#endif
#if CHERRYVIS_ENABLED
        CherryVis::log() << "Added TakeExpansion play for base @ " << BWAPI::WalkPosition(play->depotPosition);
#endif
        break;
    }
}

void StrategyEngine::takeNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    auto natural = Map::getMyNatural();

    // If the natural is blocked, use a TakeExpansion play, since it knows how to resolve it
    if (natural->blockedByEnemy)
    {
        bool hasNaturalPlay = false;
        for (auto &play : plays)
        {
            if (auto takeExpansionPlay = std::dynamic_pointer_cast<TakeExpansion>(play))
            {
                if (takeExpansionPlay->depotPosition == natural->getTilePosition())
                {
                    hasNaturalPlay = true;
                }
            }
        }

        if (!hasNaturalPlay)
        {
#if LOGGING_ENABLED
            Log::Get() << "Added TakeExpansion play for natural to handle blocking enemy unit";
#endif
#if CHERRYVIS_ENABLED
            CherryVis::log() << "Added TakeExpansion play for natural to handle blocking enemy unit";
#endif

            plays.emplace(plays.begin(), std::make_shared<TakeExpansion>(natural, 0));
        }

        return;
    }

    // Otherwise just queue the natural nexus as any normal macro item

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                          BuildingPlacement::builderFrames(Map::getMyMain()->mineralLineCenter,
                                                                                           natural->getTilePosition(),
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0, 0);
    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                             "SE-natural",
                                                             BWAPI::UnitTypes::Protoss_Nexus,
                                                             buildLocation);
}

void StrategyEngine::cancelNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                            std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    auto natural = Map::getMyNatural();

    // If we are taking the natural with a TakeExpansion play, don't cancel it
    // TODO: Refactor how plays are cancelled so we can actually do this safely
    for (const auto &play : plays)
    {
        if (auto takeExpansionPlay = std::dynamic_pointer_cast<TakeExpansion>(play))
        {
            if (takeExpansionPlay->depotPosition == natural->getTilePosition()) return;
        }
    }

    Builder::cancelBase(natural);
}

void StrategyEngine::takeExpansionWithShuttle(std::vector<std::shared_ptr<Play>> &plays)
{
    // This is used when the enemy has us contained, so we want to take an expansion as if it were an island expansion
    // Collect any existing TakeExpansion plays

    // First abort if we have already queued an expansion
    for (auto &play : plays)
    {
        if (auto takeIslandExpansionPlay = std::dynamic_pointer_cast<TakeIslandExpansion>(play))
        {
            return;
        }
    }

    // Now figure out which expansion to take
    Base *baseToTake = nullptr;
    bool transferWorkers = false;

    // If the map has an island expansion, take it
    {
        Base *closestIslandBase = nullptr;
        int closestIslandBaseDist = INT_MAX;
        for (auto &islandBase : Map::getUntakenIslandExpansions())
        {
            int dist = Map::getMyMain()->getPosition().getApproxDistance(islandBase->getPosition());
            if (dist < closestIslandBaseDist)
            {
                closestIslandBase = islandBase;
                closestIslandBaseDist = dist;
            }
        }

        if (closestIslandBase)
        {
            baseToTake = closestIslandBase;
            transferWorkers = true;
        }
    }

    // Otherwise use the hidden base
    if (!baseToTake)
    {
        baseToTake = Map::getHiddenBase();
    }

    if (!baseToTake) return;

    // Queue the play
    auto play = addIslandExpansionPlay(plays, baseToTake, false, transferWorkers);
#if LOGGING_ENABLED
    Log::Get() << "Queued island expansion to " << play->depotPosition;
#endif
#if CHERRYVIS_ENABLED
    CherryVis::log() << "Added TakeIslandExpansion play for base @ " << BWAPI::WalkPosition(play->depotPosition);
#endif
}
