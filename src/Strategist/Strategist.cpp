#include "Strategist.h"
#include <utility>
#include "Scout.h"
#include "Units.h"
#include "PathFinding.h"
#include "General.h"

#include "Play.h"
#include "Plays/Defensive/DefendBase.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeNaturalExpansion.h"
#include "Plays/Macro/TakeExpansion.h"
#include "Plays/Offensive/RallyArmy.h"

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
        bool startedScouting = false;
        std::unordered_map<BWAPI::Unit, std::shared_ptr<Play>> unitToPlay;
        std::vector<std::shared_ptr<Play>> plays;
        std::vector<ProductionGoal> productionGoals;
        std::vector<std::pair<int, int>> mineralReservations;

        void updateScouting()
        {
            // For now always start scouting when our first pylon has been started
            if (!startedScouting && Units::countIncomplete(BWAPI::UnitTypes::Protoss_Pylon) > 0)
            {
                startedScouting = true;
                Scout::setScoutingMode(Scout::ScoutingMode::Location);
            }
        }

        void updateUnitAssignments()
        {
            struct ReassignableUnit
            {
                static bool cmp(const ReassignableUnit &a, const ReassignableUnit &b)
                {
                    return a.distance < b.distance;
                }

                explicit ReassignableUnit(BWAPI::Unit unit) : unit(unit), currentPlay(nullptr), distance(0) {}

                ReassignableUnit(BWAPI::Unit unit, std::shared_ptr<Play> currentPlay)
                        : unit(unit), currentPlay(std::move(currentPlay)), distance(0) {}

                BWAPI::Unit unit;
                std::shared_ptr<Play> currentPlay;
                int distance;
            };

            // Gather a map of all reassignable units by type
            std::unordered_map<int, std::vector<ReassignableUnit>> typeToReassignableUnits;
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->isCompleted()) continue;
                if (unit->getType().isBuilding()) continue;
                if (unit->getType().isWorker()) continue;

                auto playIt = unitToPlay.find(unit);
                if (playIt == unitToPlay.end())
                {
                    typeToReassignableUnits[unit->getType()].emplace_back(unit);
                }
                else if (playIt->second->receivesUnassignedUnits())
                {
                    typeToReassignableUnits[unit->getType()].emplace_back(unit, playIt->second);
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
                    if (typeToReassignableUnits.find(unitRequirement.type) == typeToReassignableUnits.end())
                    {
                        // "Reserve" incomplete units if possible
                        auto incompleteUnits = typeToIncompleteUnits.find(unitRequirement.type);
                        while (incompleteUnits != typeToIncompleteUnits.end() && incompleteUnits->second > 0 && unitRequirement.count > 0)
                        {
                            unitRequirement.count--;
                            incompleteUnits->second--;
                        }

                        continue;
                    }

                    auto &reassignableUnits = typeToReassignableUnits[unitRequirement.type];

                    // Score the available units by distance to the desired position
                    // TODO: Include some measurement of whether it is safe for the unit to get to the position
                    for (auto &reassignableUnit : reassignableUnits)
                    {
                        reassignableUnit.distance = reassignableUnit.unit->isFlying()
                                                    ? reassignableUnit.unit->getDistance(unitRequirement.position)
                                                    : PathFinding::GetGroundDistance(reassignableUnit.unit->getPosition(),
                                                                                     unitRequirement.position,
                                                                                     unitRequirement.type);
                    }

                    // Pick the unit(s) with the lowest distance
                    std::sort(reassignableUnits.begin(), reassignableUnits.end(), ReassignableUnit::cmp);
                    for (auto it = reassignableUnits.begin();
                         it != reassignableUnits.end() && unitRequirement.count > 0;
                         it = reassignableUnits.erase(it))
                    {
                        if (it->currentPlay != nullptr) it->currentPlay->removeUnit(it->unit);
                        play->addUnit(it->unit);
                        unitToPlay[it->unit] = play;
                        unitRequirement.count--;
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

        void updateProductionGoals()
        {
            // Ask all of the plays for their production goals
            std::map<int, std::vector<ProductionGoal>> prioritizedProductionGoals;
            for (auto &play : plays)
            {
                play->addPrioritizedProductionGoals(prioritizedProductionGoals);
            }

            // Flatten the production goals into a vector ordered by priority
            productionGoals.clear();
            for (auto &priorityAndProductionGoals : prioritizedProductionGoals)
            {
                productionGoals.reserve(productionGoals.size() + priorityAndProductionGoals.second.size());
                std::move(priorityAndProductionGoals.second.begin(), priorityAndProductionGoals.second.end(), std::back_inserter(productionGoals));
            }
        }

        void updateMineralReservations()
        {
            // Ask all of the plays for their mineral reservations
            // We are not prioritizing them right now, as the expectation is that not many plays will need mineral reservations, but this can be
            // added later.
            mineralReservations.clear();
            for (auto &play : plays)
            {
                play->addMineralReservations(mineralReservations);
            }
        }

        void handleMainArmyProduction()
        {
            // TODO
            int percentZealots = 0;

            int currentZealots = Units::countAll(BWAPI::UnitTypes::Protoss_Zealot);
            int currentDragoons = Units::countAll(BWAPI::UnitTypes::Protoss_Dragoon);

            int currentZealotRatio = (100 * currentZealots) / std::max(1, currentZealots + currentDragoons);

            if (currentZealotRatio >= percentZealots)
            {
                productionGoals.emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Dragoon, -1, -1);
            }
            productionGoals.emplace_back(std::in_place_type<UnitProductionGoal>, BWAPI::UnitTypes::Protoss_Zealot, -1, -1);
        }

        void upgradeAtCount(BWAPI::UpgradeType upgradeType, BWAPI::UnitType unitType, int unitCount)
        {
            // First bail out if the upgrade is already done or queued
            if (BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) > 0) return;
            if (Units::isBeingUpgraded(upgradeType)) return;

            int units = Units::countCompleted(unitType);

            // Iterate the goals until we exceed the desired unit count
            auto it = productionGoals.begin();
            for (; units < unitCount && it != productionGoals.end(); it++)
            {
                auto unitProductionGoal = std::get_if<UnitProductionGoal>(&*it);
                if (!unitProductionGoal) continue;
                if (unitProductionGoal->unitType() != unitType) continue;

                // Special case: unit production goal for unlimited units
                // In this case we split the production goal and put the upgrade in the middle
                if (unitProductionGoal->countToProduce() == -1)
                {
                    it = productionGoals.emplace(it, UpgradeProductionGoal(upgradeType));
                    productionGoals.emplace(it,
                                            std::in_place_type<UnitProductionGoal>,
                                            unitType,
                                            unitCount - units,
                                            unitProductionGoal->getProducerLimit(),
                                            unitProductionGoal->getLocation());
                    return;
                }

                units += unitProductionGoal->countToProduce();
            }

            // If we have counted enough units, insert the upgrade goal now
            if (units >= unitCount)
            {
                productionGoals.emplace(it, UpgradeProductionGoal(upgradeType));
                return;
            }
        }

        void handleUpgrades()
        {
            upgradeAtCount(BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
            upgradeAtCount(BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            // TODO: Normal upgrades
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

    void update()
    {
        // Remove all dead or renegaded units from plays
        // This cascades down to squads and unit clusters
        for (auto it = unitToPlay.begin(); it != unitToPlay.end();)
        {
            if (it->first->exists() && it->first->getPlayer() == BWAPI::Broodwar->self())
            {
                it++;
                continue;
            }

            it->second->removeUnit(it->first);
            it = unitToPlay.erase(it);
        }

        // Update the plays
        // They signal interesting changes to the Strategist through their PlayStatus object.
        for (auto it = plays.begin(); it != plays.end();)
        {
            (*it)->status.unitRequirements.clear();
            (*it)->status.removedUnits.clear();

            (*it)->update();

            // Update our unit map for units released from the play
            for (auto unit : (*it)->status.removedUnits)
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
                    for (auto unit : (*it)->getSquad()->getUnits())
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

        // Temporary expansion logic for testing

        if (BWAPI::Broodwar->getFrameCount() == 7000)
        {
            plays.emplace(plays.begin(), std::make_shared<TakeNaturalExpansion>());
        }

        if (BWAPI::Broodwar->getFrameCount() == 12000)
        {
            auto &untakenExpansions = Map::getUntakenExpansions();
            if (!untakenExpansions.empty())
            {
                plays.emplace(plays.begin(), std::make_shared<TakeExpansion>((*untakenExpansions.begin())->getTilePosition()));
            }
        }

        // TODO: Logic engine to add and remove plays based on scouting information, etc.

        updateUnitAssignments();
        updateProductionGoals();
        updateMineralReservations();
        handleMainArmyProduction();
        handleUpgrades();

        updateScouting();

        writeInstrumentation();
    }

    void initialize()
    {
        startedScouting = false;
        unitToPlay.clear();
        plays.clear();
        productionGoals.clear();
        mineralReservations.clear();

        plays.emplace_back(std::make_shared<DefendBase>(Map::getMyMain()));
        plays.emplace_back(std::make_shared<SaturateBases>());
        plays.emplace_back(std::make_shared<RallyArmy>());

        /*
        if (BWAPI::Broodwar->self()->getName() == "Opponent") {

            productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dark_Templar, 2, 2));
            productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
            productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
            productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
            productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
            return;
        }
         */

        /* dt rush 
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dark_Templar, 2, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
        //productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot));
        */

        /* dragoons
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        //productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
        //productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 2, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
        */

        /* dragoons + obs
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 2, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Observer, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
        */

        /* zealots into goons
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 8));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon, 4));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Singularity_Charge));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Dragoon));
        */

        /* sair / zealot 
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Probe, -1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 2, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Corsair, 1, 1));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 4, 2));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Protoss_Ground_Weapons));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Corsair, 2, 1));
        productionGoals.emplace_back(UpgradeProductionGoal(BWAPI::UpgradeTypes::Protoss_Air_Weapons));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot, 6, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Corsair, 8, 2));
        productionGoals.emplace_back(UnitProductionGoal(BWAPI::UnitTypes::Protoss_Zealot));
        */
    }

    void setOpening(std::vector<std::shared_ptr<Play>> openingPlays)
    {
        plays = std::move(openingPlays);
    }

    std::vector<ProductionGoal> &currentProductionGoals()
    {
        return productionGoals;
    }

    std::vector<std::pair<int, int>> &currentMineralReservations()
    {
        return mineralReservations;
    }
}
