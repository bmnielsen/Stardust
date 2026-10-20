#include <utility>
#include <iomanip>
#include <Strategist/Plays/MainArmy/AttackEnemyBase.h>
#include "Units.h"
#include "PathFinding.h"
#include "Opponent.h"
#include "Map.h"
#include "UnitUtil.h"

#include "OpponentEconomicModel.h"

#include "StrategyEngine.h"
#include "StrategyEngines/PvP.h"
#include "StrategyEngines/PvT.h"
#include "StrategyEngines/PvZ.h"
#include "StrategyEngines/PvU.h"

#include "Play.h"
#include "Strategist.h"

#include "Plays/MainArmy/MopUp.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_BOARD_VALUES true
#endif

/*
 * Broadly, the Strategist (via a StrategyEngine) decides on a prioritized list of plays to run, each of which can order
 * units from the producer and influence the organization and behaviour of its managed units. The StrategyEngine also
 * orders production for our main army.
 */
namespace Director
{
    namespace
    {
        std::unique_ptr<StrategyEngine> engine;
        std::unordered_map<OwnedUnit, std::shared_ptr<PlayItem>> unitToPlay;
        std::vector<std::shared_ptr<PlayItem>> playList;
        std::vector<ProductionGoal> productionGoals;
        std::vector<std::pair<int, int>> mineralReservations;
        bool enemyContained;
        int enemyContainedChanged;
        Director::WorkerScoutStatus workerScoutStatus;

        void setStrategyEngine(bool transitioningFromRandom)
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

            engine->initialize(playList, transitioningFromRandom, "");
        }

        void updateUnitAssignments()
        {
            struct ReassignableUnit
            {
                static bool cmp(const ReassignableUnit &a, const ReassignableUnit &b)
                {
                    return a.distance < b.distance;
                }

                explicit ReassignableUnit(OwnedUnit gameUnit) : gameUnit(std::move(std::move(gameUnit))), currentPlay(nullptr), distance(0) {}

                ReassignableUnit(OwnedUnit gameUnit, std::shared_ptr<PlayItem> currentPlay)
                        : gameUnit(std::move(std::move(gameUnit))), currentPlay(std::move(currentPlay)), distance(0) {}

                OwnedUnit gameUnit;
                std::shared_ptr<PlayItem> currentPlay;
                int distance;
            };

            /* Gather a map of all reassignable units by type */
            std::unordered_map<int, std::vector<ReassignableUnit>> typeToReassignableUnits;
            for (const auto &gameUnit : UnitMgr::allMine())
            {
                if (!gameUnit->completed) continue;
                if (gameUnit->type.isBuilding()) continue;
                if (gameUnit->type.isWorker()) continue;

                auto playIt = unitToPlay.find(gameUnit);
                if (playIt == unitToPlay.end())
                {
                    typeToReassignableUnits[gameUnit->type].emplace_back(gameUnit);
                }
                else if (playIt->second->canReassignUnit(gameUnit))
                {
                    typeToReassignableUnits[gameUnit->type].emplace_back(gameUnit, playIt->second);
                }
            }

            /* Grab a copy of our incomplete units */
            auto typeToIncompleteUnits = UnitMgr::allMineIncompleteByType();

            /* Process each play */
            /* They greedily take the reassignable units closest to where they want them */
            std::shared_ptr<PlayItem> playReceivingUnassignedUnits = nullptr;
            for (auto &play : playList)
            {
                if (play->receivesUnassignedUnits())
                {
                    playReceivingUnassignedUnits = play;
                }

                /* Remove reassignable units already belonging to this play */
                /* This also makes them unavailable to later (lower-priority) plays */
                /* TODO: Need to figure out how to allow the scout squad to take a detector from the main army squad when it doesn't need it */
                for (auto &typeAndReassignableUnits : typeToReassignableUnits)
                {
                    for (auto iterRef = typeAndReassignableUnits.second.begin(); iterRef != typeAndReassignableUnits.second.end();)
                    {
                        if (iterRef->currentPlay == play)
                        {
                            iterRef = typeAndReassignableUnits.second.erase(iterRef);
                        }
                        else
                        {
                            iterRef++;
                        }
                    }
                }

                for (auto &unitRequirement : play->status.unitRequirements)
                {
                    if (unitRequirement.count < 1) continue;

                    auto &reassignableUnits = typeToReassignableUnits[unitRequirement.type];

                    /* Score the available units by distance to the desired position */
                    /* TODO: Include some measurement of whether it is safe for the unit to get to the position */
                    for (auto &reassignableUnit : reassignableUnits)
                    {
                        reassignableUnit.distance = reassignableUnit.gameUnit->isFlying
                                                    ? reassignableUnit.gameUnit->getDistance(unitRequirement.position)
                                                    : Pathfinder::GetGroundDistance(reassignableUnit.gameUnit->prevPos,
                                                                                     unitRequirement.position,
                                                                                     unitRequirement.type);
                    }

                    /* Pick the unit(s) with the lowest distance */
                    std::sort(reassignableUnits.begin(), reassignableUnits.end(), ReassignableUnit::cmp);
                    auto reassign = [&](bool checkGridNode)
                    {
                        for (auto iterRef = reassignableUnits.begin();
                             iterRef != reassignableUnits.end() && unitRequirement.count > 0;)
                        {
                            if (iterRef->distance > unitRequirement.distanceLimit) break;

                            if (!unitRequirement.allowFromVanguardCluster && iterRef->currentPlay && iterRef->currentPlay->getSquad()
                                && !iterRef->currentPlay->getSquad()->canReassignFromVanguardCluster(iterRef->gameUnit))
                            {
                                iterRef++;
                                continue;
                            }
                            if (checkGridNode && unitRequirement.gridNodePredicate &&
                                !Pathfinder::checkGridPath(iterRef->gameUnit->getTilePosition(),
                                                            BWAPI::TilePosition(unitRequirement.position),
                                                            unitRequirement.gridNodePredicate))
                            {
                                iterRef++;
                                continue;
                            }

                            if (iterRef->currentPlay != nullptr)
                            {
                                iterRef->currentPlay->removeUnit(iterRef->gameUnit);
                                Telemetry::log(iterRef->gameUnit->ident) << "Removed from play: " << iterRef->currentPlay->label;
                            }

                            unitToPlay[iterRef->gameUnit] = play;
                            play->addUnit(iterRef->gameUnit);
                            Telemetry::log(iterRef->gameUnit->ident) << "Added to play: " << play->label;

                            unitRequirement.count--;

                            iterRef = reassignableUnits.erase(iterRef);
                        }
                    };
                    reassign(true);
                    if (unitRequirement.allowFailingGridNodePredicate) reassign(false);

                    /* "Reserve" incomplete units if possible */
                    auto incompleteUnits = typeToIncompleteUnits.find(unitRequirement.type);
                    while (incompleteUnits != typeToIncompleteUnits.end() && !incompleteUnits->second.empty() && unitRequirement.count > 0)
                    {
                        (*incompleteUnits->second.begin())->setProducerRallyPosition(unitRequirement.position);

                        unitRequirement.count--;
                        incompleteUnits->second.erase(incompleteUnits->second.begin());
                        play->assignedIncompleteUnits[unitRequirement.type]++;
                    }
                }
            }

            /* Add any unassigned units to the appropriate play */
            if (playReceivingUnassignedUnits != nullptr)
            {
                for (auto &typeAndReassignableUnits : typeToReassignableUnits)
                {
                    for (auto &reassignableUnit : typeAndReassignableUnits.second)
                    {
                        if (reassignableUnit.currentPlay != nullptr)
                        {
                            AppLog::Get() << "WARNING: Unit assigned to unknown play: " << *reassignableUnit.gameUnit
                                       << " in " << reassignableUnit.currentPlay->label;
                            unitToPlay.erase(reassignableUnit.gameUnit);
                        }

                        /* Skip units our main army plays don't know how to use */
                        if (reassignableUnit.gameUnit->isFlying &&
                            !reassignableUnit.gameUnit->type.isDetector() &&
                            reassignableUnit.gameUnit->type != BWAPI::UnitTypes::Protoss_Arbiter)
                        {
                            continue;
                        }

                        unitToPlay[reassignableUnit.gameUnit] = playReceivingUnassignedUnits;
                        playReceivingUnassignedUnits->addUnit(reassignableUnit.gameUnit);
                        Telemetry::log(reassignableUnit.gameUnit->ident) << "Added to play: " << playReceivingUnassignedUnits->label;
                    }
                }

                auto playPosition = playReceivingUnassignedUnits->getSquad()
                                    ? playReceivingUnassignedUnits->getSquad()->getTargetPosition()
                                    : Map::getMyMain()->getPosition();
                for (const auto &typeAndIncompleteUnits : typeToIncompleteUnits)
                {
                    if (typeAndIncompleteUnits.first.isBuilding()) continue;
                    if (typeAndIncompleteUnits.first.isWorker()) continue;

                    for (const auto &incompleteUnit : typeAndIncompleteUnits.second)
                    {
                        incompleteUnit->setProducerRallyPosition(playPosition);

                        playReceivingUnassignedUnits->assignedIncompleteUnits[typeAndIncompleteUnits.first]++;
                    }
                }
            }
        }

        bool enemyIsContained()
        {
            /* Only change our minds after at least 5 seconds */
            if (frameNow - enemyContainedChanged < 120) return enemyContained;

            /* We consider the enemy to be contained if the following is true: */
            /* - The enemy has a known main base */
            /* - Our main army play is AttackEnemyBase */
            /* - The vanguard cluster in that play is either attacking or containing the enemy main or natural */
            /* - We have no knowledge of enemy combat units outside the enemy main and natural */

            /* If we don't know where the enemy main is, we don't have it contained */
            auto enemyMain = Map::getEnemyMain();
            if (!enemyMain) return false;

            /* We only consider the enemy contained if our main army play is AttackEnemyBase */
            PlayItem *attackMainPlay = nullptr;
            for (auto &spPlay : playList)
            {
                PlayItem *play = spPlay.get();
                if (typeid(*play) == typeid(AttackEnemyBase))
                {
                    attackMainPlay = play;
                    break;
                }
            }
            if (!attackMainPlay) return false;

            /* Get the vanguard cluster with its distance to the enemy main */
            int vanguardDist;
            auto vanguardCluster = attackMainPlay->getSquad()->vanguardCluster(&vanguardDist);
            if (!vanguardCluster) return false;

            /* Don't consider the enemy contained if our main army is retreating */
            if (vanguardCluster->currentActivity == UnitCluster::Activity::Regrouping &&
                vanguardCluster->currentSubActivity == UnitCluster::SubActivity::Flee)
            {
                return false;
            }

            /* Now gather the areas considered to be part of the enemy main and natural */
            std::set<const BWEM::Area *> enemyMainAreas;
            enemyMainAreas.insert(enemyMain->getArea());
            if (Map::getEnemyStartingMain() == enemyMain)
            {
                auto enemyNatural = Map::getEnemyStartingNatural();
                if (enemyNatural)
                {
                    enemyMainAreas.insert(enemyNatural->getArea());
                    for (const auto &choke : Pathfinder::GetChokePointPath(enemyMain->getPosition(), enemyNatural->getPosition()))
                    {
                        enemyMainAreas.insert(choke->GetAreas().first);
                        enemyMainAreas.insert(choke->GetAreas().second);
                    }

                    /* Update the vanguard cluster's distance if it is closer to the natural */
                    int vanguardDistanceNatural = Pathfinder::GetGroundDistance(
                            vanguardCluster->vanguard ? vanguardCluster->vanguard->prevPos : vanguardCluster->midpoint,
                            enemyNatural->getPosition());
                    if (vanguardDistanceNatural != -1 && vanguardDistanceNatural < vanguardDist)
                    {
                        vanguardDist = vanguardDistanceNatural;
                    }
                }
            }

            /* The enemy is not contained if the vanguard unit is not in one of the identified areas and we aren't doing a contain */
            auto vanguardArea = BWEM::Map::Instance().GetNearestArea(BWAPI::WalkPosition(vanguardCluster->vanguard
                                                                                         ? vanguardCluster->vanguard->prevPos
                                                                                         : vanguardCluster->midpoint));
            if (!enemyMainAreas.contains(vanguardArea) &&
                (vanguardDist > 1000 || (vanguardCluster->currentSubActivity != UnitCluster::SubActivity::ContainChoke
                                         && vanguardCluster->currentSubActivity != UnitCluster::SubActivity::ContainStaticDefense)))
            {
                return false;
            }

            /* The enemy is not contained if it either has a combat unit or something capable of training units outside of the identified areas */
            for (const auto &gameUnit : UnitMgr::allEnemy())
            {
                /* Any flying non-building indicates the enemy isn't contained */
                if (gameUnit->isFlying && !gameUnit->type.isBuilding()) return false;

                if (!gameUnit->lastPositionValid) continue;
                if (gameUnit->type.isWorker() && gameUnit->lastSeenAttacking < (frameNow - 120)) continue;
                if (!(gameUnit->type.isBuilding() && gameUnit->type.canProduce()) && !UnitHelper::CanAttackGround(gameUnit->type)) continue;

                auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos));
                if (!area) continue;

                if (enemyMainAreas.contains(area)) continue;

                /* Allow units close to our vanguard cluster, since these are units that might be contained but just on the other side of the choke */
                if (gameUnit->getDistance(vanguardCluster->midpoint) < 640) continue;

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
#if CVIS_BOARD_VALUES
            std::vector<std::string> values;
            values.reserve(playList.size());
            for (const auto &play : playList)
            {
                values.emplace_back(play->label);
            }
            Telemetry::setBoardListValue("play", values);

            values.clear();
            values.reserve(productionGoals.size());
            for (auto &productionGoal : productionGoals)
            {
                values.emplace_back((std::ostringstream() << productionGoal).str());
            }
            Telemetry::setBoardListValue("prodgoal", values);

            values.clear();
            values.reserve(mineralReservations.size());
            for (auto &mineralReservation : mineralReservations)
            {
                values.emplace_back((std::ostringstream() << mineralReservation.first << ":" << mineralReservation.second).str());
            }
            Telemetry::setBoardListValue("mineralreservations", values);

            if (engine)
            {
                Telemetry::setBoardValue("strategy", engine->getOurStrategy());
                Telemetry::setBoardValue("strategyEnemy", engine->getEnemyStrategy());
            }

            Telemetry::setBoardValue("enemyContained", enemyContained ? "true" : "false");
            Telemetry::setBoardValue("workerScoutStatus", workerScoutStatusToString());
            Telemetry::setBoardValue("pressure", (std::ostringstream() << std::setprecision(2) << pressure()).str());
#endif
        }
    }

    void initialize()
    {
        OpponentEconomicModel::initialize();

        unitToPlay.clear();
        playList.clear();
        productionGoals.clear();
        mineralReservations.clear();
        enemyContained = false;
        enemyContainedChanged = 0;
        workerScoutStatus = WorkerScoutStatus::Unstarted;

        setStrategyEngine(false);
    }

    void update()
    {
        OpponentEconomicModel::update();

        /* Change the strategy engine when we discover the race of a random opponent */
        if (Opponent::hasRaceJustBeenDetermined())
        {
            setStrategyEngine(true);
        }

        if (enemyContained != enemyIsContained())
        {
            AppLog::Get() << "Enemy is " << (enemyContained ? "no longer " : "") << "contained";
            enemyContained = !enemyContained;
            enemyContainedChanged = frameNow;
        }

        /* Remove all dead or renegaded units from plays */
        /* This cascades down to squads and unit clusters */
        for (auto iterRef = unitToPlay.begin(); iterRef != unitToPlay.end();)
        {
            if (iterRef->first->exists())
            {
                iterRef++;
                continue;
            }

            iterRef->second->removeUnit(iterRef->first);
            iterRef = unitToPlay.erase(iterRef);
        }

        /* Update the plays */
        /* They signal interesting changes to the Strategist through their PlayStatus object. */
        for (auto &play : playList)
        {
            play->status.unitRequirements.clear();
            play->status.removedUnits.clear();
            play->assignedIncompleteUnits.clear();
            play->update();
        }

        /* Allow the strategy engine to change our plays */
        engine->updatePlays(playList);

        /* Process the changes signalled by the PlayStatus objects */
        for (auto iterRef = playList.begin(); iterRef != playList.end();)
        {
            auto removeUnit = [&](const OwnedUnit& gameUnit)
            {
                Telemetry::log(gameUnit->ident) << "Removed from play: " << (*iterRef)->label;

                (*iterRef)->removeUnit(gameUnit);
                unitToPlay.erase(gameUnit);
            };

            /* Update our unit map for units released from the play */
            /* We don't do this for the play receiving unassigned units to avoid instability */
            if (!(*iterRef)->receivesUnassignedUnits())
            {
                for (const auto &gameUnit : (*iterRef)->status.removedUnits)
                {
                    removeUnit(gameUnit);
                }
            }

            /* Handle play transition */
            /* This replaces the current play with a new one, moving all units */
            if ((*iterRef)->status.transitionTo != nullptr)
            {
                auto moveUnit = [&](const OwnedUnit& gameUnit)
                {
                    unitToPlay[gameUnit] = (*iterRef)->status.transitionTo;
                    (*iterRef)->status.transitionTo->addUnit(gameUnit);
                    Telemetry::log(gameUnit->ident) << "Removed from play: " << (*iterRef)->label;
                    Telemetry::log(gameUnit->ident) << "Added to play: " << (*iterRef)->status.transitionTo->label;
                };

                (*iterRef)->disband(removeUnit, moveUnit);

                Telemetry::log() << "Play transition: " << (*iterRef)->label << "->" << (*iterRef)->status.transitionTo->label;

                *iterRef = (*iterRef)->status.transitionTo;
                (*iterRef)->postTransition();
                iterRef++;
            }

                /* Erase the play if it is marked complete */
            else if ((*iterRef)->status.complete)
            {
                Telemetry::log() << "Play complete: " << (*iterRef)->label;
                (*iterRef)->disband(removeUnit, removeUnit);
                iterRef = playList.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }

        updateUnitAssignments();

        /* Ask all of the plays for their mineral reservations */
        /* We are not prioritizing them right now, as the expectation is that not many plays will need mineral reservations, but this can be */
        /* added later. */
        mineralReservations.clear();
        for (auto &play : playList)
        {
            play->addMineralReservations(mineralReservations);
        }

        /* Ask all of the plays for their production goals */
        std::map<int, std::vector<ProductionGoal>> prioProdGoals;
        for (auto &play : playList)
        {
            play->addPrioritizedProductionGoals(prioProdGoals);
        }

        /* Feed everything through the strategy engine */
        engine->updateProduction(playList, prioProdGoals, mineralReservations);

        /* Flatten the production goals into a vector ordered by priority */
        productionGoals.clear();
        for (auto &priorityAndProductionGoals : prioProdGoals)
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

    bool areWeContained()
    {
        /* We consider ourselves to be contained if our main army is in our base and wants to attack, but can't */
        auto mainArmyPlay = StrategyEngine::getPlay<MainArmyPlay>(playList);
        if (!mainArmyPlay) return false;

        if (typeid(*mainArmyPlay) != typeid(AttackEnemyBase)) return false;

        auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
        if (!vanguard) return false;
        if (vanguard->currentActivity != UnitCluster::Activity::Regrouping) return false;

        auto area = BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(vanguard->vanguard ? vanguard->vanguard->prevPos : vanguard->midpoint));
        if (!area) return false;

        return Map::getMyMainAreas().contains(area);
    }

    double pressure()
    {
        auto mainArmyPlay = StrategyEngine::getPlay<MainArmyPlay>(playList);
        if (!mainArmyPlay) return 1.0;

        if (mainArmyPlay->isDefensive()) return 1.0;
        if (typeid(*mainArmyPlay) == typeid(MopUp)) return 0.0;

        auto vanguard = mainArmyPlay->getSquad()->vanguardCluster();
        if (!vanguard) return 1.0;

        return 1.0 - vanguard->percentageToEnemyMain;
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
            AppLog::Get() << "Worker scout status changed to: " << workerScoutStatusToString();
        }
    }

    bool isWorkerScoutComplete()
    {
        return workerScoutStatus == Director::WorkerScoutStatus::ScoutingCompleted ||
               workerScoutStatus == Director::WorkerScoutStatus::ScoutingFailed ||
               workerScoutStatus == Director::WorkerScoutStatus::ScoutingBlocked;
    }

    bool hasWorkerScoutCompletedInitialBaseScan()
    {
        return
                workerScoutStatus == Director::WorkerScoutStatus::EnemyBaseScouted ||
                workerScoutStatus == Director::WorkerScoutStatus::MonitoringEnemyChoke ||
                workerScoutStatus == Director::WorkerScoutStatus::ScoutingCompleted;
    }


    /* Following methods are used by tests to force specific behaviour */

    void setOpening(std::vector<std::shared_ptr<PlayItem>> openingPlays)
    {
        playList = std::move(openingPlays);
    }

    void setStrategyEngine(std::unique_ptr<StrategyEngine> strategyEngine, const std::string &opening)
    {
        engine = std::move(strategyEngine);
        engine->initialize(playList, false, opening);
    }

    StrategyEngine *getStrategyEngine()
    {
        return engine.get();
    }

    MainArmyPlay *getMainArmyPlay()
    {
        return StrategyEngine::getPlay<MainArmyPlay>(playList);
    }
}
