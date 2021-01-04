#include "StrategyEngine.h"

#include "Map.h"
#include "Units.h"
#include "Workers.h"
#include "Strategist.h"
#include "Builder.h"

#include "Plays/Macro/TakeExpansion.h"
#include "Plays/Macro/TakeIslandExpansion.h"
#include "Plays/MainArmy/AttackEnemyBase.h"
#include "Plays/MainArmy/MopUp.h"

void StrategyEngine::defaultExpansions(std::vector<std::shared_ptr<Play>> &plays)
{
    // This logic does not handle the first decision to take our natural expansion, so if this hasn't been done, bail out now
    auto natural = Map::getMyNatural();
    if (natural && natural->ownedSince == -1) return;

    // If the natural is "owned" by our opponent, it's probably because of some kind of proxy play
    // In this case, delay doing any expansions until mid-game
    if (natural && natural->owner != BWAPI::Broodwar->self() && BWAPI::Broodwar->getFrameCount() < 12000) return;

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

        // Ensure the vanguard cluster is at least 20 tiles away from our natural
        auto vanguardCluster = squad->vanguardCluster();
        if (!vanguardCluster) return false;

        auto natural = Map::getMyNatural();
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

        return;
    }

    // Determine whether we want to expand now
    bool wantToExpand = true;
    {
        // Expand if we have no bases with more than 3 available mineral assignments
        // If the enemy is contained, set the threshold to 6 instead
        // Adjust by the number of pending expansions to avoid instability when a build worker is reserved for the expansion
        int availableMineralAssignmentsThreshold = (Strategist::isEnemyContained() ? 6 : 3) + takeExpansionPlays.size();
        for (auto base : Map::getMyBases())
        {
            if (Workers::availableMineralAssignments(base) > availableMineralAssignmentsThreshold)
            {
                wantToExpand = false;
                break;
            }
        }
    }

    // Determines if we consider it safe to expand to a normal expansion
    auto safeToExpand = [&]()
    {
        // Only expand when our army is on the offensive
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        if (!mainArmyPlay) return false;
        if (typeid(*mainArmyPlay) == typeid(MopUp)) return true;
        if (typeid(*mainArmyPlay) != typeid(AttackEnemyBase)) return false;

        // Sanity check that the play has a squad
        auto squad = mainArmyPlay->getSquad();
        if (!squad) return false;

        // Get the vanguard cluster
        auto vanguardCluster = squad->vanguardCluster();
        if (!vanguardCluster) return false;

        // Don't expand if the cluster has fewer than five units
        if (vanguardCluster->units.size() < 5) return false;

        // Expand if we are gas blocked - we have the resources for the nexus anyway
        if (BWAPI::Broodwar->self()->minerals() > 500 && BWAPI::Broodwar->self()->gas() < 100) return true;

        // Cluster should not be moving or fleeing
        // In other words, we want the cluster to be in some kind of stable attack or contain state
        if (vanguardCluster->currentActivity == UnitCluster::Activity::Moving
            || (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping
                && vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee))
        {
            return false;
        }

        return true;
    };

    // Check if we want to cancel an active TakeExpansionPlay
    if (!takeExpansionPlays.empty())
    {
        if (!wantToExpand || !safeToExpand())
        {
            for (auto &takeExpansionPlay : takeExpansionPlays)
            {
                if (!takeExpansionPlay->constructionStarted())
                {
                    takeExpansionPlay->status.complete = true;

                    Log::Get() << "Cancelled expansion to " << takeExpansionPlay->depotPosition;
                }
            }
        }

        return;
    }

    // Take an island expansion when we are on two bases, want to expand and it is safe to do so
    if (wantToExpand && Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) >= 2)
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
            auto play = std::make_shared<TakeIslandExpansion>(closestIslandBase);
            plays.emplace(plays.begin(), play);

            Log::Get() << "Queued island expansion to " << play->depotPosition;
            CherryVis::log() << "Added TakeIslandExpansion play for base @ " << BWAPI::WalkPosition(play->depotPosition);
            return;
        }
    }

    // Break out if we don't want to expand to a normal base
    if (!wantToExpand || !safeToExpand()) return;

    // Determine if we want to consider a mineral-only base
    auto shouldTakeMineralOnly = []()
    {
        // Take a mineral-only if we have an excess of gas
        if (BWAPI::Broodwar->self()->gas() > 1500) return true;

        // Count the number of active gas and mineral-only bases we have
        int gasBases = 0;
        int mineralOnlyBases = 0;
        for (const auto &base : Map::getMyBases())
        {
            if (base->gas() > 0)
            {
                gasBases++;
            }
            else if (base->minerals() > 1000)
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
        if (!takeMineralOnly && expansion->gas() == 0) continue;

        // Don't take expansions that are blocked by the enemy and that we don't know how to unblock
        if (expansion->blockedByEnemy)
        {
            auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(expansion);
            if (baseStaticDefenseLocations.first == BWAPI::TilePositions::Invalid || baseStaticDefenseLocations.second.empty())
            {
                continue;
            }
        }

        auto play = std::make_shared<TakeExpansion>(expansion);
        plays.emplace(plays.begin(), play);

        Log::Get() << "Queued expansion to " << play->depotPosition;
        CherryVis::log() << "Added TakeExpansion play for base @ " << BWAPI::WalkPosition(play->depotPosition);
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
            Log::Get() << "Added TakeExpansion play for natural to handle blocking enemy unit";
            CherryVis::log() << "Added TakeExpansion play for natural to handle blocking enemy unit";

            plays.emplace(plays.begin(), std::make_shared<TakeExpansion>(natural));
        }

        return;
    }

    // Otherwise just queue the natural nexus as any normal macro item

    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(natural->getTilePosition()),
                                                          BuildingPlacement::builderFrames(BuildingPlacement::Neighbourhood::MainBase,
                                                                                           natural->getTilePosition(),
                                                                                           BWAPI::UnitTypes::Protoss_Nexus),
                                                          0, 0);
    prioritizedProductionGoals[PRIORITY_DEPOTS].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                             BWAPI::UnitTypes::Protoss_Nexus,
                                                             buildLocation);
}

void StrategyEngine::cancelNaturalExpansion(std::vector<std::shared_ptr<Play>> &plays,
                                          std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    auto natural = Map::getMyNatural();

    for (auto it = plays.begin(); it != plays.end(); )
    {
        if (auto takeExpansionPlay = std::dynamic_pointer_cast<TakeExpansion>(*it))
        {
            if (takeExpansionPlay->depotPosition == natural->getTilePosition())
            {
                Log::Get() << "Cancelled TakeExpansion play for natural";
                CherryVis::log() << "Cancelled TakeExpansion play for natural";

                it = plays.erase(it);
                continue;
            }
        }

        it++;
    }

    Builder::cancel(natural->getTilePosition());
}