#include <utility>
#include <Strategist/Plays/MainArmy/AttackEnemyBase.h>
#include "Units.h"
#include "PathFinding.h"
#include "Opponent.h"
#include "Map.h"
#include "UnitUtil.h"

#include "StrategyEngine.h"
#include "StrategyEngines/PvP.h"
#include "StrategyEngines/PvT.h"
#include "StrategyEngines/PvZ.h"
#include "StrategyEngines/PvU.h"

#include "Play.h"
#include "Strategist.h"

/*
 * Broadly, the Strategist (via a StrategyEngine) decides on a prioritized list of plays to run, each of which can order
 * units from the producer and influence the organization and behaviour of its managed units. The StrategyEngine also
 * orders production for our main army.
 */
namespace Strategist
{
    namespace
    {
        std::unique_ptr<StrategyEngine> engine;
        std::unordered_map<MyUnit, std::shared_ptr<Play>> unitToPlay;
        std::vector<std::shared_ptr<Play>> plays;
        std::vector<ProductionGoal> productionGoals;
        std::vector<std::pair<int, int>> mineralReservations;
        bool enemyContained;
        int enemyContainedChanged;
        Strategist::WorkerScoutStatus workerScoutStatus;

        void setStrategyEngine()
        {
            engine = Map::mapSpecificOverride()->createStrategyEngine();
            if (!engine)
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
                else
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
                if (play->receivesUnassignedUnits())
                {
                    playReceivingUnassignedUnits = play;
                }

                // Remove reassignable units already belonging to this play
                // This also makes them unavailable to later (lower-priority) plays
                // TODO: Need to figure out how to allow the scout squad to take a detector from the main army squad when it doesn't need it
                for (auto &typeAndReassignableUnits : typeToReassignableUnits)
                {
                    for (auto it = typeAndReassignableUnits.second.begin(); it != typeAndReassignableUnits.second.end();)
                    {
                        if (it->currentPlay == play)
                        {
                            it = typeAndReassignableUnits.second.erase(it);
                        }
                        else
                        {
                            it++;
                        }
                    }
                }

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
                    auto reassign = [&](bool checkGridNode)
                    {
                        for (auto it = reassignableUnits.begin();
                             it != reassignableUnits.end() && unitRequirement.count > 0;)
                        {
                            if (it->distance > unitRequirement.distanceLimit) break;

                            if (!unitRequirement.allowFromVanguardCluster && it->currentPlay && it->currentPlay->getSquad()
                                && !it->currentPlay->getSquad()->canReassignFromVanguardCluster(it->unit))
                            {
                                it++;
                                continue;
                            }
                            if (checkGridNode && unitRequirement.gridNodePredicate &&
                                !PathFinding::checkGridPath(it->unit->getTilePosition(),
                                                            BWAPI::TilePosition(unitRequirement.position),
                                                            unitRequirement.gridNodePredicate))
                            {
                                it++;
                                continue;
                            }

                            if (it->currentPlay != nullptr)
                            {
                                it->currentPlay->removeUnit(it->unit);
                                CherryVis::log(it->unit->id) << "Removed from play: " << it->currentPlay->label;
                            }

                            unitToPlay[it->unit] = play;
                            play->addUnit(it->unit);
                            CherryVis::log(it->unit->id) << "Added to play: " << play->label;

                            unitRequirement.count--;

                            it = reassignableUnits.erase(it);
                        }
                    };
                    reassign(true);
                    if (unitRequirement.allowFailingGridNodePredicate) reassign(false);

                    // "Reserve" incomplete units if possible
                    auto incompleteUnits = typeToIncompleteUnits.find(unitRequirement.type);
                    while (incompleteUnits != typeToIncompleteUnits.end() && incompleteUnits->second > 0 && unitRequirement.count > 0)
                    {
                        unitRequirement.count--;
                        incompleteUnits->second--;
                        play->assignedIncompleteUnits[unitRequirement.type]++;
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
                        if (reassignableUnit.currentPlay != nullptr)
                        {
                            Log::Get() << "WARNING: Unit assigned to unknown play: " << *reassignableUnit.unit
                                       << " in " << reassignableUnit.currentPlay->label;
                            unitToPlay.erase(reassignableUnit.unit);
                        }

                        // For now skip for units we don't yet support in main army plays
                        if (reassignableUnit.unit->isFlying && !reassignableUnit.unit->type.isDetector()) continue;

                        unitToPlay[reassignableUnit.unit] = playReceivingUnassignedUnits;
                        playReceivingUnassignedUnits->addUnit(reassignableUnit.unit);
                        CherryVis::log(reassignableUnit.unit->id) << "Added to play: " << playReceivingUnassignedUnits->label;
                    }
                }
                for (const auto &typeAndIncompleteUnits : typeToIncompleteUnits)
                {
                    if (typeAndIncompleteUnits.second > 0)
                    {
                        playReceivingUnassignedUnits->assignedIncompleteUnits[typeAndIncompleteUnits.first] += typeAndIncompleteUnits.second;
                    }
                }
            }
        }

        bool enemyIsContained()
        {
            // Only change our minds after at least 5 seconds
            if (BWAPI::Broodwar->getFrameCount() - enemyContainedChanged < 120) return enemyContained;

            // We consider the enemy to be contained if the following is true:
            // - The enemy has a known main base
            // - Our main army play is AttackEnemyBase
            // - The vanguard cluster in that play is either attacking or containing the enemy main or natural
            // - We have no knowledge of enemy combat units outside the enemy main and natural

            // If we don't know where the enemy main is, we don't have it contained
            auto enemyMain = Map::getEnemyMain();
            if (!enemyMain) return false;

            // We only consider the enemy contained if our main army play is AttackEnemyBase
            Play *attackMainPlay = nullptr;
            for (auto &spPlay : plays)
            {
                Play *play = spPlay.get();
                if (typeid(*play) == typeid(AttackEnemyBase))
                {
                    attackMainPlay = play;
                    break;
                }
            }
            if (!attackMainPlay) return false;

            // Get the vanguard cluster with its distance to the enemy main
            int vanguardDist;
            auto vanguardCluster = attackMainPlay->getSquad()->vanguardCluster(&vanguardDist);
            if (!vanguardCluster) return false;

            // Don't consider the enemy contained if our main army is retreating
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping &&
                vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee)
            {
                return false;
            }

            // Now gather the areas considered to be part of the enemy main and natural
            std::set<const BWEM::Area *> enemyMainAreas;
            enemyMainAreas.insert(enemyMain->getArea());
            if (Map::getEnemyStartingMain() == enemyMain)
            {
                auto enemyNatural = Map::getEnemyStartingNatural();
                if (enemyNatural)
                {
                    enemyMainAreas.insert(enemyNatural->getArea());
                    for (const auto &choke : PathFinding::GetChokePointPath(enemyMain->getPosition(), enemyNatural->getPosition()))
                    {
                        enemyMainAreas.insert(choke->GetAreas().first);
                        enemyMainAreas.insert(choke->GetAreas().second);
                    }

                    // Update the vanguard cluster's distance if it is closer to the natural
                    int vanguardDistanceNatural = PathFinding::GetGroundDistance(
                            vanguardCluster->vanguard ? vanguardCluster->vanguard->lastPosition : vanguardCluster->center,
                            enemyNatural->getPosition());
                    if (vanguardDistanceNatural != -1 && vanguardDistanceNatural < vanguardDist)
                    {
                        vanguardDist = vanguardDistanceNatural;
                    }
                }
            }

            // The enemy is not contained if the vanguard unit is not in one of the identified areas and we aren't doing a contain
            auto vanguardArea = BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(vanguardCluster->vanguard
                                                                                         ? vanguardCluster->vanguard->lastPosition
                                                                                         : vanguardCluster->center));
            if (enemyMainAreas.find(vanguardArea) == enemyMainAreas.end() &&
                (vanguardDist > 1000 || (vanguardCluster->currentSubActivity != UnitCluster::SubActivity::ContainChoke
                                         && vanguardCluster->currentSubActivity != UnitCluster::SubActivity::ContainStaticDefense)))
            {
                return false;
            }

            // The enemy is not contained if it either has a combat unit or something capable of training units outside of the identified areas
            for (const auto &unit : Units::allEnemy())
            {
                // Any flying non-building indicates the enemy isn't contained
                if (unit->isFlying && !unit->type.isBuilding()) return false;

                if (!unit->lastPositionValid) continue;
                if (unit->type.isWorker() && unit->lastSeenAttacking < (BWAPI::Broodwar->getFrameCount() - 120)) continue;
                if (!(unit->type.isBuilding() && unit->type.canProduce()) && !UnitUtil::CanAttackGround(unit->type)) continue;

                auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(unit->lastPosition));
                if (!area) continue;

                if (enemyMainAreas.find(area) != enemyMainAreas.end()) continue;

                // Allow units close to our vanguard cluster, since these are units that might be contained but just on the other side of the choke
                if (unit->getDistance(vanguardCluster->center) < 640) continue;

                return false;
            }

            return true;
        }

        std::string workerScoutStatusToString()
        {
            switch (workerScoutStatus)
            {
                case WorkerScoutStatus::Unstarted:
                    return "Unstarted";
                case WorkerScoutStatus::LookingForEnemyBase:
                    return "LookingForEnemyBase";
                case WorkerScoutStatus::MovingToEnemyBase:
                    return "MovingToEnemyBase";
                case WorkerScoutStatus::EnemyBaseScouted:
                    return "EnemyBaseScouted";
                case WorkerScoutStatus::MonitoringEnemyChoke:
                    return "MonitoringEnemyChoke";
                case WorkerScoutStatus::ScoutingBlocked:
                    return "ScoutingBlocked";
                case WorkerScoutStatus::ScoutingFailed:
                    return "ScoutingFailed";
                case WorkerScoutStatus::ScoutingCompleted:
                    return "ScoutingCompleted";
            }

            return "Unknown";
        }

        void writeInstrumentation()
        {
#if CHERRYVIS_ENABLED
            std::vector<std::string> values;
            values.reserve(plays.size());
            for (auto &play : plays)
            {
                values.emplace_back(play->label);
            }
            CherryVis::setBoardListValue("play", values);

            values.clear();
            values.reserve(productionGoals.size());
            for (auto &productionGoal : productionGoals)
            {
                values.emplace_back((std::ostringstream() << productionGoal).str());
            }
            CherryVis::setBoardListValue("prodgoal", values);

            values.clear();
            values.reserve(mineralReservations.size());
            for (auto &mineralReservation : mineralReservations)
            {
                values.emplace_back((std::ostringstream() << mineralReservation.first << ":" << mineralReservation.second).str());
            }
            CherryVis::setBoardListValue("mineralreservations", values);

            if (engine)
            {
                CherryVis::setBoardValue("strategy", engine->getOurStrategy());
                CherryVis::setBoardValue("strategyEnemy", engine->getEnemyStrategy());
            }

            CherryVis::setBoardValue("enemyContained", enemyContained ? "true" : "false");
            CherryVis::setBoardValue("workerScoutStatus", workerScoutStatusToString());
#endif
        }
    }

    void initialize()
    {
        unitToPlay.clear();
        plays.clear();
        productionGoals.clear();
        mineralReservations.clear();
        enemyContained = false;
        enemyContainedChanged = 0;
        workerScoutStatus = WorkerScoutStatus::Unstarted;

        setStrategyEngine();
    }

    void update()
    {
        // Change the strategy engine when we discover the race of a random opponent
        if (Opponent::hasRaceJustBeenDetermined())
        {
            // We first need to clear all of our existing plays, as the new strategy engine will add its own
            auto removeUnit = [&](const MyUnit unit)
            {
                unitToPlay.erase(unit);
            };
            for (auto &play : plays)
            {
                play->disband(removeUnit, removeUnit);
            }
            plays.clear();
            setStrategyEngine();
        }

        if (enemyContained != enemyIsContained())
        {
            Log::Get() << "Enemy is " << (enemyContained ? "no longer " : "") << "contained";
            enemyContained = !enemyContained;
            enemyContainedChanged = BWAPI::Broodwar->getFrameCount();
        }

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
            play->assignedIncompleteUnits.clear();
            play->update();
        }

        // Allow the strategy engine to change our plays
        engine->updatePlays(plays);

        // Process the changes signalled by the PlayStatus objects
        for (auto it = plays.begin(); it != plays.end();)
        {
            auto removeUnit = [&](const MyUnit unit)
            {
                CherryVis::log(unit->id) << "Removed from play: " << (*it)->label;

                (*it)->removeUnit(unit);
                unitToPlay.erase(unit);
            };

            // Update our unit map for units released from the play
            for (const auto &unit : (*it)->status.removedUnits)
            {
                removeUnit(unit);
            }

            // Handle play transition
            // This replaces the current play with a new one, moving all units
            if ((*it)->status.transitionTo != nullptr)
            {
                auto moveUnit = [&](const MyUnit unit)
                {
                    unitToPlay[unit] = (*it)->status.transitionTo;
                    (*it)->status.transitionTo->addUnit(unit);
                    CherryVis::log(unit->id) << "Removed from play: " << (*it)->label;
                    CherryVis::log(unit->id) << "Added to play: " << (*it)->status.transitionTo->label;
                };

                (*it)->disband(removeUnit, moveUnit);

                CherryVis::log() << "Play transition: " << (*it)->label << "->" << (*it)->status.transitionTo->label;

                *it = (*it)->status.transitionTo;
                it++;
            }

                // Erase the play if it is marked complete
            else if ((*it)->status.complete)
            {
                CherryVis::log() << "Play complete: " << (*it)->label;
                (*it)->disband(removeUnit, removeUnit);
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
        engine->updateProduction(plays, prioritizedProductionGoals, mineralReservations);

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

    bool isEnemyContained()
    {
        return enemyContained;
    }

    WorkerScoutStatus getWorkerScoutStatus()
    {
        return workerScoutStatus;
    }

    void setWorkerScoutStatus(WorkerScoutStatus status)
    {
        if (workerScoutStatus != status)
        {
            workerScoutStatus = status;
            Log::Get() << "Worker scout status changed to: " << workerScoutStatusToString();
        }
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
