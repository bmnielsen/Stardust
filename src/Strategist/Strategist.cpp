#include <utility>
#include "Units.h"
#include "PathFinding.h"
#include "General.h"
#include "Workers.h"
#include "Opponent.h"

#include "StrategyEngine.h"
#include "StrategyEngines/PvP.h"
#include "StrategyEngines/PvT.h"
#include "StrategyEngines/PvZ.h"
#include "StrategyEngines/PvU.h"

#include "Play.h"
#include "Plays/Defensive/DefendMainBase.h"
#include "Plays/Macro/TakeExpansion.h"

/*
 * Broadly, the Strategist decides on a prioritized list of plays to run, each of which can order units from the producer
 * and influence the organization and behaviour of its managed units.
 *
 * On each frame, each play decides whether it is finished or should transition into a different play. If not, it decides
 * what units (if any) it needs to perform its task.
 *
 * Units are then (re)assigned accordingly, and any missing units are ordered from the producer by the individual plays.
 *
 * The Strategist also coordinates scouting.
 *
 * Scenarios:
 *
 * Early game normal tactics.
 * - Scout is sent to find enemy base -> triggered by frame or building creation
 * - Initial units are added to a defensive play -> it keeps tabs on what units it thinks it needs based on observations
 * - Enemy base is found, attack base play is added -> triggered by map calling into strategist?
 * - When the defensive play no longer feels there is a threat, its units are released and go to the attack base play
 * - Later, a scouting play is added to keep tabs on possible enemy expansions -> triggered by frame or observations
 * - Whenever an expansion is found, strategist adds one or more plays to attack it in some way
 * - Attack plays keep track of how they are doing, e.g. if an attack play does not feel it can succeed, it might transition
 *   into a contain enemy play
 *
 * Default play:
 *
 * The last play will always be the play for the bulk of our army. Generally it will be tasked with attacking the enemy's main base. If we haven't
 * found the enemy's main base yet, the army will rally at our base. Once the enemy's main base is destroyed, we will either consider a new enemy
 * base to be its main base, or transition to a mop up play that searches for remaining buildings to destroy.
 *
 */
namespace Strategist
{
    namespace
    {
        std::unique_ptr<StrategyEngine> engine;
        std::unordered_map<MyUnit, std::shared_ptr<Play>> unitToPlay;
        std::vector<std::shared_ptr<Play>> plays;
        std::unordered_set<std::shared_ptr<TakeExpansion>> takeExpansionPlays;
        std::vector<ProductionGoal> productionGoals;
        std::vector<std::pair<int, int>> mineralReservations;

        void setStrategyEngine()
        {
            if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
            {
                engine = std::make_unique<PvP>();
            }
            else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
            {
                engine = std::make_unique<PvT>();
            }
            else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
            {
                engine = std::make_unique<PvZ>();
            }
            else
            {
                engine = std::make_unique<PvU>();
            }

            engine->initialize(plays);
        }

        void updateUnitAssignments()
        {
            struct ReassignableUnit
            {
                static bool cmp(const ReassignableUnit &a, const ReassignableUnit &b)
                {
                    return a.distance < b.distance;
                }

                explicit ReassignableUnit(MyUnit unit) : unit(std::move(std::move(unit))), currentPlay(nullptr), distance(0) {}

                ReassignableUnit(MyUnit unit, std::shared_ptr<Play> currentPlay)
                        : unit(std::move(std::move(unit))), currentPlay(std::move(currentPlay)), distance(0) {}

                MyUnit unit;
                std::shared_ptr<Play> currentPlay;
                int distance;
            };

            // Gather a map of all reassignable units by type
            std::unordered_map<int, std::vector<ReassignableUnit>> typeToReassignableUnits;
            for (const auto &unit : Units::allMine())
            {
                if (!unit->completed) continue;
                if (unit->type.isBuilding()) continue;
                if (unit->type.isWorker()) continue;

                auto playIt = unitToPlay.find(unit);
                if (playIt == unitToPlay.end())
                {
                    typeToReassignableUnits[unit->type].emplace_back(unit);
                }
                else if (playIt->second->receivesUnassignedUnits())
                {
                    typeToReassignableUnits[unit->type].emplace_back(unit, playIt->second);
                }
            }

            // Grab a count of incomplete units
            auto typeToIncompleteUnits = Units::countIncompleteByType();

            // Process each play
            // They greedily take the reassignable units closest to where they want them
            std::shared_ptr<Play> playReceivingUnassignedUnits = nullptr;
            for (auto &play : plays)
            {
                if (play->receivesUnassignedUnits()) playReceivingUnassignedUnits = play;

                for (auto &unitRequirement : play->status.unitRequirements)
                {
                    if (unitRequirement.count < 1) continue;

                    auto &reassignableUnits = typeToReassignableUnits[unitRequirement.type];

                    // Score the available units by distance to the desired position
                    // TODO: Include some measurement of whether it is safe for the unit to get to the position
                    for (auto &reassignableUnit : reassignableUnits)
                    {
                        reassignableUnit.distance = reassignableUnit.unit->isFlying
                                                    ? reassignableUnit.unit->getDistance(unitRequirement.position)
                                                    : PathFinding::GetGroundDistance(reassignableUnit.unit->lastPosition,
                                                                                     unitRequirement.position,
                                                                                     unitRequirement.type);
                    }

                    // Pick the unit(s) with the lowest distance
                    std::sort(reassignableUnits.begin(), reassignableUnits.end(), ReassignableUnit::cmp);
                    for (auto it = reassignableUnits.begin();
                         it != reassignableUnits.end() && unitRequirement.count > 0;
                         it = reassignableUnits.erase(it))
                    {
                        if (it->currentPlay == play) continue;

                        if (it->currentPlay != nullptr) it->currentPlay->removeUnit(it->unit);
                        play->addUnit(it->unit);
                        unitToPlay[it->unit] = play;
                        unitRequirement.count--;
                    }

                    // "Reserve" incomplete units if possible
                    auto incompleteUnits = typeToIncompleteUnits.find(unitRequirement.type);
                    while (incompleteUnits != typeToIncompleteUnits.end() && incompleteUnits->second > 0 && unitRequirement.count > 0)
                    {
                        unitRequirement.count--;
                        incompleteUnits->second--;
                    }
                }
            }

            // Add any unassigned units to the appropriate play
            if (playReceivingUnassignedUnits != nullptr)
            {
                for (auto &typeAndReassignableUnits : typeToReassignableUnits)
                {
                    for (auto &reassignableUnit : typeAndReassignableUnits.second)
                    {
                        if (reassignableUnit.currentPlay != nullptr) continue;

                        playReceivingUnassignedUnits->addUnit(reassignableUnit.unit);
                        unitToPlay[reassignableUnit.unit] = playReceivingUnassignedUnits;
                    }
                }
            }
        }

        void updateExpansions()
        {
            // Clear finished TakeExpansion plays
            for (auto it = takeExpansionPlays.begin(); it != takeExpansionPlays.end();)
            {
                if ((*it)->status.complete)
                {
                    it = takeExpansionPlays.erase(it);
                }
                else
                {
                    it++;
                }
            }

            // The natural is always our first expansion, so until it is taken, don't expand anywhere else
            // The logic for taking our natural is special and is contained in TakeNaturalExpansion
            // If our natural has been taken at one point and lost again, it will be treated as a normal expansion
            auto natural = Map::getMyNatural();
            if (natural->ownedSince == -1) return;

            // Determine whether we want to expand now
            bool wantToExpand = true;

            // Count our completed basic gateway units
            int army = Units::countCompleted(BWAPI::UnitTypes::Protoss_Zealot) + Units::countCompleted(BWAPI::UnitTypes::Protoss_Dragoon);

            // Never expand if we don't have a reasonable-sized army
            if (army < 5) wantToExpand = false;

            // Expand if we have no bases with more than 3 available mineral assignments
            if (wantToExpand)
            {
                // Adjust by the number of pending expansions to avoid instability when a build worker is reserved for the expansion
                int availableMineralAssignmentsThreshold = 3 + takeExpansionPlays.size();

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
                        takeExpansionPlays.insert(play);
                        plays.emplace(plays.begin(), play);

                        Log::Get() << "Queued expansion to " << play->depotPosition;
                    }
                }
            }
            else
            {
                // Cancel any active TakeExpansion plays
                for (auto &takeExpansionPlay : takeExpansionPlays)
                {
                    takeExpansionPlay->cancel();

                    Log::Get() << "Cancelled expansion to " << takeExpansionPlay->depotPosition;
                }
                takeExpansionPlays.clear();
            }
        }

        void writeInstrumentation()
        {
#if CHERRYVIS_ENABLED
            std::vector<std::string> values;
            values.reserve(plays.size());
            for (auto &play : plays)
            {
                values.emplace_back(play->label());
            }
            CherryVis::setBoardListValue("play", values);

            values.clear();
            values.reserve(productionGoals.size());
            for (auto &productionGoal : productionGoals)
            {
                values.emplace_back((std::ostringstream() << productionGoal).str());
            }
            CherryVis::setBoardListValue("prodgoal", values);
#endif
        }
    }

    void initialize()
    {
        unitToPlay.clear();
        plays.clear();
        takeExpansionPlays.clear();
        productionGoals.clear();
        mineralReservations.clear();

        setStrategyEngine();
    }

    void update()
    {
        // Remove all dead or renegaded units from plays
        // This cascades down to squads and unit clusters
        for (auto it = unitToPlay.begin(); it != unitToPlay.end();)
        {
            if (it->first->exists())
            {
                it++;
                continue;
            }

            it->second->removeUnit(it->first);
            it = unitToPlay.erase(it);
        }

        // Change the strategy engine when we discover the race of a random opponent
        if (Opponent::hasRaceJustBeenDetermined()) setStrategyEngine();

        // Allow the strategy engine to change our plays
        engine->updatePlays(plays);

        // Update the plays
        // They signal interesting changes to the Strategist through their PlayStatus object.
        for (auto &play : plays)
        {
            play->status.unitRequirements.clear();
            play->status.removedUnits.clear();
            play->update();
        }

        // Updating expansions goes next, as this may also cause some changes to existing TakeExpansion plays
        updateExpansions();

        // Process the changes signalled by the PlayStatus objects
        for (auto it = plays.begin(); it != plays.end();)
        {
            // Update our unit map for units released from the play
            for (const auto &unit : (*it)->status.removedUnits)
            {
                (*it)->removeUnit(unit);
                unitToPlay.erase(unit);
            }

            // Handle play transition
            // This replaces the current play with a new one, moving all units
            if ((*it)->status.transitionTo != nullptr)
            {
                if ((*it)->getSquad() != nullptr)
                {
                    for (const auto &unit : (*it)->getSquad()->getUnits())
                    {
                        (*it)->status.transitionTo->addUnit(unit);
                        unitToPlay[unit] = (*it)->status.transitionTo;
                    }

                    General::removeSquad((*it)->getSquad());

                    CherryVis::log() << "Play transition: " << (*it)->label() << "->" << (*it)->status.transitionTo->label();
                }

                *it = (*it)->status.transitionTo;
                it++;
            }

                // Erase the play if it is marked complete
            else if ((*it)->status.complete)
            {
                General::removeSquad((*it)->getSquad());
                it = plays.erase(it);
            }
            else
            {
                it++;
            }
        }

        updateUnitAssignments();

        // Ask all of the plays for their mineral reservations
        // We are not prioritizing them right now, as the expectation is that not many plays will need mineral reservations, but this can be
        // added later.
        mineralReservations.clear();
        for (auto &play : plays)
        {
            play->addMineralReservations(mineralReservations);
        }

        // Ask all of the plays for their production goals
        std::map<int, std::vector<ProductionGoal>> prioritizedProductionGoals;
        for (auto &play : plays)
        {
            play->addPrioritizedProductionGoals(prioritizedProductionGoals);
        }

        // Feed everything through the strategy engine
        engine->updateProduction(prioritizedProductionGoals, mineralReservations);

        // Flatten the production goals into a vector ordered by priority
        productionGoals.clear();
        for (auto &priorityAndProductionGoals : prioritizedProductionGoals)
        {
            productionGoals.reserve(productionGoals.size() + priorityAndProductionGoals.second.size());
            std::move(priorityAndProductionGoals.second.begin(), priorityAndProductionGoals.second.end(), std::back_inserter(productionGoals));
        }

        writeInstrumentation();
    }

    std::vector<ProductionGoal> &currentProductionGoals()
    {
        return productionGoals;
    }

    std::vector<std::pair<int, int>> &currentMineralReservations()
    {
        return mineralReservations;
    }

    // Following methods are used by tests to force specific behaviour

    void setOpening(std::vector<std::shared_ptr<Play>> openingPlays)
    {
        plays = std::move(openingPlays);
    }

    void setStrategyEngine(std::unique_ptr<StrategyEngine> strategyEngine)
    {
        engine = std::move(strategyEngine);
    }
}
