#include "StrategyEngine.h"

#include "Map.h"
#include "Units.h"
#include "Workers.h"
#include "Strategist.h"

#include "Plays/Macro/TakeExpansion.h"
#include "Plays/MainArmy/AttackEnemyMain.h"

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
    for (auto &play : plays)
    {
        if (auto takeExpansionPlay = std::dynamic_pointer_cast<TakeExpansion>(play))
        {
            takeExpansionPlays.push_back(takeExpansionPlay);
        }
    }

    // Determine whether we want to expand now
    auto wantToExpand = [&]()
    {
        // Expand if we have no bases with more than 3 available mineral assignments
        // If the enemy is contained, set the threshold to 6 instead
        // Adjust by the number of pending expansions to avoid instability when a build worker is reserved for the expansion
        int availableMineralAssignmentsThreshold = (Strategist::isEnemyContained() ? 6 : 3) + takeExpansionPlays.size();
        for (auto base : Map::getMyBases())
        {
            if (Workers::availableMineralAssignments(base) > availableMineralAssignmentsThreshold)
            {
                return false;
            }
        }

        // Only expand when our army is on the offensive
        auto mainArmyPlay = getPlay<MainArmyPlay>(plays);
        if (!mainArmyPlay || typeid(*mainArmyPlay) != typeid(AttackEnemyMain)) return false;

        // Sanity check that the play has a squad
        auto squad = mainArmyPlay->getSquad();
        if (!squad) return false;

        // Get the vanguard cluster with its distance to the target base
        int dist;
        auto vanguardCluster = squad->vanguardCluster(&dist);
        if (!vanguardCluster) return false;

        // Don't expand if the cluster has fewer than five units
        if (vanguardCluster->units.size() < 5) return false;

        // Don't expand if the cluster is not at least 2/3 of the way towards the target base
        int distToMain = PathFinding::GetGroundDistance(Map::getMyMain()->getPosition(), vanguardCluster->center);
        if (dist * 2 > distToMain) return false;

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

    if (wantToExpand())
    {
        // TODO: Logic for when we should queue multiple expansions simultaneously
        if (takeExpansionPlays.empty())
        {
            // Create a TakeExpansion play for the next expansion
            // TODO: Take island expansions where appropriate
            // TODO: Take mineral-only expansions where appropriate
            for (const auto &expansion : Map::getUntakenExpansions())
            {
                if (expansion->gas() == 0) continue;

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
    }
    else
    {
        // Cancel any active TakeExpansion plays where the nexus hasn't started yet
        for (auto &takeExpansionPlay : takeExpansionPlays)
        {
            if (!takeExpansionPlay->constructionStarted())
            {
                takeExpansionPlay->status.complete = true;

                Log::Get() << "Cancelled expansion to " << takeExpansionPlay->depotPosition;
            }
        }
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
                hasNaturalPlay = true;
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