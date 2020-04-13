#include <utility>
#include "Units.h"
#include "PathFinding.h"
#include "General.h"
#include "Workers.h"

#include "Play.h"
#include "Plays/Defensive/DefendMainBase.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeNaturalExpansion.h"
#include "Plays/Macro/TakeExpansion.h"
#include "Plays/Offensive/RallyArmy.h"
#include "Plays/Scouting/EarlyGameWorkerScout.h"

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
        std::unordered_map<MyUnit, std::shared_ptr<Play>> unitToPlay;
        std::vector<std::shared_ptr<Play>> plays;
        std::unordered_set<std::shared_ptr<TakeExpansion>> takeExpansionPlays;
        std::vector<ProductionGoal> productionGoals;
        std::vector<std::pair<int, int>> mineralReservations;

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

        void upgradeAtCount(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                            BWAPI::UpgradeType upgradeType,
                            BWAPI::UnitType unitType,
                            int unitCount)
        {
            // First bail out if the upgrade is already done or queued
            if (BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) > 0) return;
            if (Units::isBeingUpgraded(upgradeType)) return;

            // Now loop through all of the prioritized production goals, keeping track of how many of the desired unit we have
            int units = Units::countCompleted(unitType);
            for (auto &priorityAndProductionGoals : prioritizedProductionGoals)
            {
                for (auto it = priorityAndProductionGoals.second.begin(); it != priorityAndProductionGoals.second.end(); it++)
                {
                    // Bail out if this upgrade is already queued
                    auto upgradeProductionGoal = std::get_if<UpgradeProductionGoal>(&*it);
                    if (upgradeProductionGoal && upgradeProductionGoal->upgradeType() == upgradeType) return;

                    auto unitProductionGoal = std::get_if<UnitProductionGoal>(&*it);
                    if (!unitProductionGoal) continue;

                    if (unitProductionGoal->countToProduce() == -1)
                    {
                        // If we are producing an unlimited number of a different unit type first, bail out
                        if (unitProductionGoal->unitType() != unitType) return;

                        // If this isn't the main army production, bail out - we don't want to upgrade in emergencies
                        if (priorityAndProductionGoals.first != PRIORITY_MAINARMY) return;

                        // Insert the upgrade here
                        it = priorityAndProductionGoals.second.emplace(it, UpgradeProductionGoal(upgradeType));

                        // Insert remaining units beforehand
                        if (units < unitCount)
                        {
                            priorityAndProductionGoals.second.emplace(it,
                                                                      std::in_place_type<UnitProductionGoal>,
                                                                      unitType,
                                                                      unitCount - units,
                                                                      unitProductionGoal->getProducerLimit(),
                                                                      unitProductionGoal->getLocation());
                        }

                        return;
                    }

                    if (unitProductionGoal->unitType() == unitType) units += unitProductionGoal->countToProduce();
                }
            }
        }

        void upgradeWhenUnitStarted(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                    BWAPI::UpgradeType upgradeType,
                                    BWAPI::UnitType unitType,
                                    bool requireProducer = false)
        {
            // First bail out if the upgrade is already done or queued
            if (BWAPI::Broodwar->self()->getUpgradeLevel(upgradeType) > 0) return;
            if (Units::isBeingUpgraded(upgradeType)) return;

            // Now check if we have at least one of the unit
            if (Units::countIncomplete(unitType) == 0 && Units::countCompleted(unitType) == 0) return;

            // Now check if we have the required producer, if specified
            if (requireProducer &&
                Units::countIncomplete(upgradeType.whatUpgrades()) == 0 &&
                Units::countCompleted(upgradeType.whatUpgrades()) == 0)
            {
                return;
            }

            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(UpgradeProductionGoal(upgradeType));
        }

        void handleUpgrades(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
        {
            // Basic infantry skill upgrades are queued when we have enough of them and are still building them
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Leg_Enhancements, BWAPI::UnitTypes::Protoss_Zealot, 6);
            upgradeAtCount(prioritizedProductionGoals, BWAPI::UpgradeTypes::Singularity_Charge, BWAPI::UnitTypes::Protoss_Dragoon, 2);

            // Cases where we want the upgrade as soon as we start building one of the units
            upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Boosters, BWAPI::UnitTypes::Protoss_Observer);
            upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Gravitic_Drive, BWAPI::UnitTypes::Protoss_Shuttle, true);
            upgradeWhenUnitStarted(prioritizedProductionGoals, BWAPI::UpgradeTypes::Carrier_Capacity, BWAPI::UnitTypes::Protoss_Carrier);

            // Ground upgrades
            // Start when we have completed our second nexus
            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Nexus) >= 2)
            {
                // Upgrade past 1 and on two forges when we have completed our third gas
                int maxLevel, forgeCount;
                if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) >= 3)
                {
                    maxLevel = 3;
                    forgeCount = 2;
                }
                else
                {
                    maxLevel = 1;
                    forgeCount = 1;
                }

                int weaponLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Weapons);
                int armorLevel = BWAPI::Broodwar->self()->getUpgradeLevel(BWAPI::UpgradeTypes::Protoss_Ground_Armor);

                // Weapons -> 1, Armor -> 1, Weapons -> 3, Armor -> 3
                if ((weaponLevel == 0 || armorLevel >= 1) && weaponLevel <= maxLevel &&
                    !Units::isBeingUpgraded(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
                {
                    prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                             BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                             weaponLevel + 1,
                                                                             forgeCount);
                }
                if (!Units::isBeingUpgraded(BWAPI::UpgradeTypes::Protoss_Ground_Armor) && armorLevel <= maxLevel)
                {
                    prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                             BWAPI::UpgradeTypes::Protoss_Ground_Armor,
                                                                             armorLevel + 1,
                                                                             forgeCount);
                }
                if (weaponLevel > 0 && armorLevel == 0 && weaponLevel <= maxLevel &&
                    !Units::isBeingUpgraded(BWAPI::UpgradeTypes::Protoss_Ground_Weapons))
                {
                    prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UpgradeProductionGoal>,
                                                                             BWAPI::UpgradeTypes::Protoss_Ground_Weapons,
                                                                             weaponLevel + 1,
                                                                             forgeCount);
                }

                // TODO: Upgrade shields when maxed
            }

            // TODO: Air upgrades
        }

        void handleDetection(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
        {
            // The main army play will reactively request mobile detection when it sees a cloaked enemy unit
            // The logic here is to look ahead to make sure we already have detection available when we need it

            // Break out if we already have an observer
            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Observer) > 0 || Units::countIncomplete(BWAPI::UnitTypes::Protoss_Observer) > 0)
            {
                return;
            }

            // TODO: Use scouting information

            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Assimilator) > 1)
            {
                prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                         BWAPI::UnitTypes::Protoss_Observer,
                                                                         1,
                                                                         1);
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

        // Add various high-levelthings to the production goals
        handleUpgrades(prioritizedProductionGoals);
        handleDetection(prioritizedProductionGoals);

        // Flatten the production goals into a vector ordered by priority
        productionGoals.clear();
        for (auto &priorityAndProductionGoals : prioritizedProductionGoals)
        {
            productionGoals.reserve(productionGoals.size() + priorityAndProductionGoals.second.size());
            std::move(priorityAndProductionGoals.second.begin(), priorityAndProductionGoals.second.end(), std::back_inserter(productionGoals));
        }

        // TODO: Logic engine to add and remove plays based on scouting information, etc.

        writeInstrumentation();
    }

    void initialize()
    {
        unitToPlay.clear();
        plays.clear();
        takeExpansionPlays.clear();
        productionGoals.clear();
        mineralReservations.clear();

        plays.emplace_back(std::make_shared<DefendMainBase>());
        plays.emplace_back(std::make_shared<SaturateBases>());
        plays.emplace_back(std::make_shared<EarlyGameWorkerScout>());
        plays.emplace_back(std::make_shared<TakeNaturalExpansion>());
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
