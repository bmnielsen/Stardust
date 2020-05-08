#include "StrategyEngine.h"

#include "Map.h"
#include "Units.h"
#include "Workers.h"
#include "Strategist.h"

#include "Plays/Macro/TakeExpansion.h"

void StrategyEngine::defaultExpansions(std::vector<std::shared_ptr<Play>> &plays)
{
    // This logic does not handle the first decision to take our natural expansion, so if this hasn't been done, bail out now
    auto natural = Map::getMyNatural();
    if (natural->ownedSince == -1) return;

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
    bool wantToExpand = true;

    // Count our completed basic gateway units
    int army = Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon);

    // Never expand if we don't have a reasonable-sized army
    if (army < 5) wantToExpand = false;

    // Expand if we have no bases with more than 3 available mineral assignments
    // If the enemy is contained, set the threshold to 6 instead
    if (wantToExpand)
    {
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

    // TODO: Checks that depend on what the enemy is doing, whether the enemy is contained, etc.

    if (wantToExpand)
    {
        // TODO: Logic for when we should queue multiple expansions simultaneously
        if (takeExpansionPlays.empty())
        {
            // Create a TakeExpansion play for the next expansion
            // TODO: Take island expansions where appropriate
            auto &untakenExpansions = Map::getUntakenExpansions();
            if (!untakenExpansions.empty())
            {
                auto play = std::make_shared<TakeExpansion>((*untakenExpansions.begin())->getTilePosition());
                plays.emplace(plays.begin(), play);

                Log::Get() << "Queued expansion to " << play->depotPosition;
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
