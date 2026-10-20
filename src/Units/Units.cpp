#include "Units.h"
#include "Geo.h"
#include "Players.h"
#include "Workers.h"
#include "Map.h"
#include "NoGoAreas.h"
#include "BuildingPlacement.h"
#include "MyCannon.h"
#include "MyCarrier.h"
#include "MyCorsair.h"
#include "MyDragoon.h"
#include "MyWorker.h"
#include "UnitUtil.h"
#include "Opponent.h"
#include "OpponentEconomicModel.h"
#include "Strategist.h"
#include "Bullets.h"
#include "OrderProcessTimer.h"

#include "DebugFlag_GridUpdates.h"

/* These defines configure a per-frame summary of various unit type's orders, commands, etc. */
#if INSTRUMENTATION_ENABLED_VERBOSE
#define DEBUG_PROBE_STATUS false
#define DEBUG_ZEALOT_STATUS false
#define DEBUG_DRAGOON_STATUS false
#define DEBUG_DT_STATUS false
#define DEBUG_CARRIER_STATUS false
#define DEBUG_SHUTTLE_STATUS false
#define DEBUG_OBSERVER_STATUS false
#define DEBUG_ARBITER_STATUS false
#define DEBUG_CANNON_STATUS false
#define DEBUG_PRODUCINGBUILDING_STATUS false
#define DEBUG_ENEMY_STATUS false
#define DEBUG_PREDICTED_POSITIONS false
#define DEBUG_RESOURCE_UPDATES true
#define DEBUG_ORDER_TIMERS false
#endif

#if INSTRUMENTATION_ENABLED
#define DEBUG_ENEMY_TIMINGS true
#define CVIS_LOG_ENEMY_UNIT_BASES true
#endif

namespace UnitMgr
{
    namespace
    {
#if LOGGING_ENABLED
        bool logUnitsCreatedAndLost = true;
#endif

        std::map<BWAPI::TilePosition, Resource> tileToResource;

        std::unordered_set<OwnedUnit> myUnits;
        std::unordered_set<Unit> enemyUnits;

        std::unordered_map<int, OwnedUnit> unitIdToMyUnit;
        std::unordered_map<int, Unit> unitIdToEnemyUnit;

        std::map<BWAPI::UnitType, std::unordered_set<OwnedUnit>> myCompletedUnitsByType;
        std::map<BWAPI::UnitType, std::unordered_set<OwnedUnit>> myIncompleteUnitsByType;

        std::map<BWAPI::UnitType, std::unordered_set<Unit>> enemyUnitsByType;
        std::map<BWAPI::UnitType, std::vector<std::pair<int, int>>> enemyUnitTimings;

        std::unordered_map<Unit, Base *> enemyUnitsToBase;
        std::unordered_map<Base *, std::unordered_set<Unit>> basesToEnemyUnits;

        std::set<BWAPI::UpgradeType> upgradesInProgress;
        std::set<BWAPI::TechType> researchInProgress;

        void updateResource(const BWAPI::Unit &rawUnit, const Unit &refinery = nullptr)
        {
            auto iterRef = tileToResource.find(rawUnit->getTilePosition());
            if (iterRef == tileToResource.end())
            {
#if LOGGING_ENABLED
                AppLog::Get() << "ERROR: Resource not found for " << rawUnit->getType() << " @ " << rawUnit->getTilePosition();
#endif
                return;
            }

            if (refinery)
            {
#if DEBUG_RESOURCE_UPDATES
                if (refinery != iterRef->second->refinery)
                {
                    Telemetry::log(rawUnit->getID()) << "Attached refinery " << *refinery;
                }
#endif
                iterRef->second->refinery = refinery;

                /* Break out if the refinery is owned by another player, as we aren't supposed to have access to resource counts */
                if (refinery->player != BWAPI::Broodwar->self()) return;
            }
            else
            {
#if DEBUG_RESOURCE_UPDATES
                if (iterRef->second->refinery)
                {
                    Telemetry::log(rawUnit->getID()) << "Detached refinery " << *iterRef->second->refinery;
                }
#endif
                iterRef->second->refinery = nullptr;
            }

#if DEBUG_RESOURCE_UPDATES
            if (rawUnit->getResources() != iterRef->second->currentAmount)
            {
                Telemetry::log(rawUnit->getID())
                    << "Resources updated from " << iterRef->second->currentAmount << " to " << rawUnit->getResources();
            }
#endif
            iterRef->second->currentAmount = rawUnit->getResources();
        }

        void resourceDestroyed(const BWAPI::TilePosition &tilePos)
        {
            auto iterRef = tileToResource.find(tilePos);
            if (iterRef == tileToResource.end()) return;

#if DEBUG_RESOURCE_UPDATES
            Telemetry::log(iterRef->second->ident) << "Destroyed";
#endif
            iterRef->second->destroyed = true;
            iterRef->second->currentAmount = 0;
            WorkerMgr::onMineralPatchDestroyed(iterRef->second);
            tileToResource.erase(iterRef);
        }

        void trackResearch(const BWAPI::Unit &rawUnit)
        {
            if (rawUnit->getPlayer() == BWAPI::Broodwar->self())
            {
                /* Terran */
                if (rawUnit->isLockedDown()) PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Lockdown);
                if (rawUnit->isIrradiated()) PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Irradiate);
                if (rawUnit->isBlind()) PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Optical_Flare);

                /* Zerg */
                if (rawUnit->isUnderDarkSwarm()) PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Dark_Swarm);
                if (rawUnit->isPlagued()) PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Plague);
                if (rawUnit->isEnsnared()) PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Ensnare);
                if (rawUnit->isParasited()) PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Parasite);

                return;
            }

            /* Terran */
            if (rawUnit->isStimmed())
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Stim_Packs);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Terran_Vulture_Spider_Mine)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Spider_Mines);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Spell_Scanner_Sweep)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Scanner_Sweep);
            }
            if (rawUnit->isSieged())
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Tank_Siege_Mode);
            }
            if (rawUnit->isDefenseMatrixed())
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Defensive_Matrix);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Terran_Wraith && rawUnit->isCloaked())
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Cloaking_Field);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Terran_Ghost && rawUnit->isCloaked())
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Personnel_Cloaking);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Terran_Nuclear_Silo ||
                rawUnit->getType() == BWAPI::UnitTypes::Terran_Nuclear_Missile)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Nuclear_Strike);
            }

            /* Zerg */
            if (rawUnit->getType() != BWAPI::UnitTypes::Zerg_Lurker &&
                (rawUnit->isBurrowed() || rawUnit->getOrder() == BWAPI::Orders::Burrowing || rawUnit->getOrder() == BWAPI::Orders::Unburrowing))
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Burrowing);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Zerg_Broodling)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Spawn_Broodlings);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Spell_Dark_Swarm)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Dark_Swarm);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Zerg_Lurker_Egg ||
                rawUnit->getType() == BWAPI::UnitTypes::Zerg_Lurker)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Lurker_Aspect);
            }

            /* Protoss */
            if (rawUnit->getOrder() == BWAPI::Orders::CastRecall)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Recall);
            }
            if (rawUnit->getOrder() == BWAPI::Orders::CastStasisField)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Stasis_Field);
            }
            if (rawUnit->getType() == BWAPI::UnitTypes::Spell_Disruption_Web)
            {
                PlayerInfo::setHasResearched(BWAPI::Broodwar->enemy(), BWAPI::TechTypes::Disruption_Web);
            }
        }

        void unitCreated(const Unit &gameUnit)
        {
            Map::onUnitCreated(gameUnit);
            BuildingPlacement::onUnitCreate(gameUnit);
            NoGoAreas::onUnitCreate(gameUnit);

#if LOGGING_ENABLED
            if (gameUnit->player == BWAPI::Broodwar->self())
            {
                if (logUnitsCreatedAndLost) AppLog::Get() << "Unit created: " << *gameUnit;
            }

            if (gameUnit->player == BWAPI::Broodwar->enemy() && gameUnit->type.isBuilding())
            {
                if (logUnitsCreatedAndLost) AppLog::Get() << "Enemy discovered: " << *gameUnit;
            }
#endif
        }

        void unitDestroyed(const Unit &gameUnit)
        {
            if (gameUnit->lastPositionValid && !gameUnit->beingManufacturedOrCarried && (!gameUnit->type.isBuilding() || !gameUnit->isFlying))
            {
                PlayerInfo::grid(gameUnit->player).unitDestroyed(gameUnit->type, gameUnit->prevPos, gameUnit->completed, gameUnit->burrowed, gameUnit->immobile);
#if DEBUG_GRID_UPDATES
                Telemetry::log(gameUnit->ident) << "Grid::unitDestroyed " << gameUnit->prevPos;
                AppLog::Debug() << *gameUnit << ": Grid::unitDestroyed " << gameUnit->prevPos;
#endif
            }

            Map::onUnitDestroy(gameUnit);
            WorkerMgr::onUnitDestroy(gameUnit);
            BuildingPlacement::onUnitDestroy(gameUnit);

            gameUnit->rawUnit = nullptr; // Signals to all holding a copy of the pointer that this unit is dead

#if CHERRYVIS_ENABLED
            Telemetry::log(gameUnit->ident) << "Destroyed";
#endif
        }

        void myUnitDestroyed(const OwnedUnit &gameUnit)
        {
#if LOGGING_ENABLED
            if (logUnitsCreatedAndLost) AppLog::Get() << "Unit lost: " << *gameUnit;
#endif

            unitDestroyed(gameUnit);

            myCompletedUnitsByType[gameUnit->type].erase(gameUnit);
            myIncompleteUnitsByType[gameUnit->type].erase(gameUnit);

            myUnits.erase(gameUnit);
            unitIdToMyUnit.erase(gameUnit->ident);
        }

        /* Pass-by-value is required here as we are cleaning up our data structures storing the unit, which may result in the data in the shared */
        /* pointer getting freed before we are finished working with it */
        void enemyUnitDestroyed(Unit gameUnit, bool morphed = false) // NOLINT(performance-unnecessary-value-param)
        {
#if LOGGING_ENABLED
            if (gameUnit->type.isBuilding() && !morphed)
            {
                if (logUnitsCreatedAndLost) AppLog::Get() << "Enemy destroyed: " << *gameUnit;
            }
#endif

            unitDestroyed(gameUnit);

            enemyUnits.erase(gameUnit);
            unitIdToEnemyUnit.erase(gameUnit->ident);
            enemyUnitsByType[gameUnit->type].erase(gameUnit);
            auto current = enemyUnitsToBase.find(gameUnit);
            if (current != enemyUnitsToBase.end())
            {
#if CVIS_LOG_ENEMY_UNIT_BASES
                Telemetry::log(gameUnit->ident) << "Removed from base @ " << BWAPI::WalkPosition(current->second->getPosition());
#endif
                basesToEnemyUnits[current->second].erase(gameUnit);
                enemyUnitsToBase.erase(current);
            }

            OpponentEconomicModel::opponentUnitDestroyed(gameUnit->type, gameUnit->ident);
        }

        void trackEnemyUnitTimings(const Unit &gameUnit, bool includeMorphs)
        {
            /* Don't track eggs or larva */
            if (gameUnit->type == BWAPI::UnitTypes::Zerg_Larva ||
                gameUnit->type == BWAPI::UnitTypes::Zerg_Egg)
            {
                return;
            }

            /* Don't track a gas steal */
            if (gameUnit->type.isRefinery() && gameUnit->getDistance(Map::getMyMain()->getPosition()) < 300) return;

            /* If this is the opponent's initial depot, update the element we already added */
            if (gameUnit->type.isResourceDepot() && !enemyUnitTimings[gameUnit->type].empty() && Map::getEnemyStartingMain()
                && gameUnit->getTilePosition() == Map::getEnemyStartingMain()->getTilePosition())
            {
                enemyUnitTimings[gameUnit->type].begin()->second = frameNow;
                return;
            }

            /* TODO: Do we need to handle the initial units? */

            /* Estimate the frame the unit completed */
            int completionFrame;
            if (gameUnit->type.isBuilding())
            {
                completionFrame = gameUnit->completed ? frameNow : gameUnit->estimatedCompletionFrame;
            }
            else
            {
                auto getClosestProducer = [&gameUnit]()
                {
                    Unit closestProducer = nullptr;
                    int closestDist = INT_MAX;

                    auto handlePossibleProducer = [&](const Unit &producer)
                    {
                        if (!producer->exists()) return;
                        if (!producer->lastPositionValid) return;

                        int span;
                        if (gameUnit->isFlying)
                        {
                            span = producer->prevPos.getApproxDistance(gameUnit->prevPos);
                        }
                        else
                        {
                            span = Pathfinder::GetGroundDistance(producer->prevPos,
                                                                  gameUnit->prevPos,
                                                                  gameUnit->type,
                                                                  Pathfinder::PathFindingOptions::UseNearestBWEMArea);
                        }

                        if (span < closestDist)
                        {
                            closestDist = span;
                            closestProducer = producer;
                        }
                    };

                    /* Assume all Zerg units come from a larva (at a hatchery / lair / hive) */
                    if (gameUnit->player->getRace() == BWAPI::Races::Zerg)
                    {
                        for (const auto &producer : enemyUnits)
                        {
                            if (producer->type != BWAPI::UnitTypes::Zerg_Hatchery &&
                                producer->type != BWAPI::UnitTypes::Zerg_Lair &&
                                producer->type != BWAPI::UnitTypes::Zerg_Hive)
                            {
                                continue;
                            }

                            handlePossibleProducer(producer);
                        }
                    }
                    else
                    {
                        auto producerType = gameUnit->type.whatBuilds().first;
                        if (!producerType.isBuilding()) producerType = producerType.whatBuilds().first;

                        for (const auto &producer : enemyUnitsByType[producerType])
                        {
                            if (producer->isFlying) continue;

                            handlePossibleProducer(producer);
                        }
                    }

                    return closestProducer;
                };

                /* Try to guess how many frames the unit has moved since it was built */
                auto getFramesMoved = [&]()
                {
                    if (gameUnit->type.topSpeed() < 0.001) return 0;

                    /* Assume proxied units need to travel about 5 seconds */
                    if (Director::getStrategyEngine()->isEnemyProxy() && frameNow < 8000)
                    {
                        return 120;
                    }

                    auto closestProducer = getClosestProducer();
                    if (closestProducer)
                    {
                        return Pathfinder::ExpectedTravelTime(gameUnit->prevPos,
                                                               closestProducer->prevPos,
                                                               gameUnit->type,
                                                               Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                               1.1);
                    }

                    /* We don't know the producer, so assume it came from the closest possible enemy main */
                    int framesMoved;
                    auto enemyMain = Map::getEnemyMain();
                    if (!enemyMain) enemyMain = Map::getEnemyStartingMain();
                    if (enemyMain)
                    {
                        framesMoved = Pathfinder::ExpectedTravelTime(gameUnit->prevPos,
                                                                      enemyMain->getPosition(),
                                                                      gameUnit->type,
                                                                      Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                                      1.1);
                    }
                    else
                    {
                        /* Default to the closest unscouted starting position */
                        framesMoved = INT_MAX;
                        for (auto baseLoc : Map::unscoutedStartingLocations())
                        {
                            int frames = Pathfinder::ExpectedTravelTime(gameUnit->prevPos,
                                                                         baseLoc->getPosition(),
                                                                         gameUnit->type,
                                                                         Pathfinder::PathFindingOptions::UseNearestBWEMArea,
                                                                         1.1);
                            if (frames < framesMoved)
                            {
                                framesMoved = frames;
                            }
                        }

                        /* Guard against bugs in scouting */
                        if (framesMoved == INT_MAX) framesMoved = 0;
                    }

                    return framesMoved;
                };

                completionFrame = frameNow - getFramesMoved();
            }

            int startFrame = completionFrame - UnitHelper::BuildTime(gameUnit->type);
            enemyUnitTimings[gameUnit->type].emplace_back(std::make_pair(startFrame, frameNow));

            OpponentEconomicModel::opponentUnitCreated(gameUnit->type, gameUnit->ident, startFrame, !gameUnit->completed);

            if (enemyUnitTimings[gameUnit->type].size() == 1)
            {
                AppLog::Get() << "First enemy of type discovered: " << *gameUnit;

                /* Track some common tech rush units in our opponent model */
                if (gameUnit->type == BWAPI::UnitTypes::Protoss_Dark_Templar)
                {
                    Opponent::setGameValue("firstDarkTemplarCompleted", completionFrame);
                }
                else if (gameUnit->type == BWAPI::UnitTypes::Zerg_Mutalisk)
                {
                    Opponent::setGameValue("firstMutaliskCompleted", completionFrame);
                }
                else if (gameUnit->type == BWAPI::UnitTypes::Zerg_Lurker)
                {
                    /* Get frames it would take this lurker to get to our main choke */
                    auto mainChoke = Map::getMyMainChoke();
                    if (mainChoke)
                    {
                        auto frames = Pathfinder::ExpectedTravelTime(gameUnit->prevPos,
                                                                      mainChoke->midpoint,
                                                                      BWAPI::UnitTypes::Zerg_Lurker,
                                                                      Pathfinder::PathFindingOptions::UseNeighbouringBWEMArea,
                                                                      1.1,
                                                                      1000);
                        Opponent::setGameValue("firstLurkerAtOurMain", completionFrame + frames);
                    }
                }
            }

            if (gameUnit->type.isBuilding() && includeMorphs)
            {
                auto morphsFrom = UnitHelper::MorphsFrom(gameUnit->type);
                if (morphsFrom.first != BWAPI::UnitTypes::None &&
                    morphsFrom.first != BWAPI::UnitTypes::Zerg_Drone)
                {
                    enemyUnitTimings[morphsFrom.first].emplace_back(std::make_pair(startFrame - UnitHelper::BuildTime(morphsFrom.first),
                                                                                   frameNow));
                }
            }
        }

        void assignEnemyUnitsToBases()
        {
            auto getBase = [](const Unit &gameUnit) -> Base *
            {
                if (!gameUnit->lastPositionValid) return nullptr;

                auto combatUnit =
                        (UnitHelper::IsCombatUnit(gameUnit->type) || gameUnit->lastSeenAttacking >= (frameNow - 120))
                        && (gameUnit->isTransport() || UnitHelper::CanAttackGround(gameUnit->type));

                Base *closest = nullptr;
                int closestDist = 1000;
                for (auto &baseLoc : Map::allBases())
                {
                    /* Only consider combat units and units that block untaken bases */
                    if (!combatUnit &&
                        (baseLoc->owner != nullptr ||
                         !Geom::Overlaps(baseLoc->getTilePosition(), 4, 3, gameUnit->getTilePosition(), 2, 2)))
                    {
                        continue;
                    }

                    /* For our bases, ignore units we haven't seen for a while */
                    if (baseLoc->owner == BWAPI::Broodwar->self() &&
                        !gameUnit->type.isBuilding() &&
                        gameUnit->lastSeen < (frameNow - 240))
                    {
                        continue;
                    }

                    int span = gameUnit->isFlying
                               ? gameUnit->prevPos.getApproxDistance(baseLoc->getPosition())
                               : Pathfinder::GetGroundDistance(gameUnit->prevPos, baseLoc->getPosition(), gameUnit->type);
                    if (span == -1 || span > closestDist) continue;

                    /* If the distance is above 500, ignore this unit if the base is unowned or it is moving away from the base */
                    if (span > 500)
                    {
                        if (!baseLoc->owner) continue;

                        auto predictedPosition = gameUnit->predictPosition(1);
                        if (predictedPosition.getApproxDistance(baseLoc->getPosition()) >
                            gameUnit->prevPos.getApproxDistance(baseLoc->getPosition()))
                        {
                            continue;
                        }
                    }

                    closest = baseLoc;
                    closestDist = span;
                }

                return closest;
            };

            for (auto &gameUnit : enemyUnits)
            {
                auto baseLoc = getBase(gameUnit);

                /* Remove from current base if it is different from the new one */
                auto current = enemyUnitsToBase.find(gameUnit);
                if (current != enemyUnitsToBase.end())
                {
                    if (current->second == baseLoc) continue;

#if CVIS_LOG_ENEMY_UNIT_BASES
                    Telemetry::log(gameUnit->ident) << "Removed from base @ " << BWAPI::WalkPosition(current->second->getPosition());
#endif
                    basesToEnemyUnits[current->second].erase(gameUnit);
                    enemyUnitsToBase.erase(current);
                }

                /* Add to the new base if required */
                if (baseLoc)
                {
#if CVIS_LOG_ENEMY_UNIT_BASES
                    Telemetry::log(gameUnit->ident) << "Added to base @ " << BWAPI::WalkPosition(baseLoc->getPosition());
#endif
                    basesToEnemyUnits[baseLoc].emplace(gameUnit);
                    enemyUnitsToBase[gameUnit] = baseLoc;
                }
            }
        }
    }

    void initialize()
    {
        tileToResource.clear();
        myUnits.clear();
        enemyUnits.clear();
        unitIdToMyUnit.clear();
        unitIdToEnemyUnit.clear();
        myCompletedUnitsByType.clear();
        myIncompleteUnitsByType.clear();
        enemyUnitsByType.clear();
        enemyUnitTimings.clear();
        enemyUnitsToBase.clear();
        basesToEnemyUnits.clear();
        upgradesInProgress.clear();
        researchInProgress.clear();

        /* Add a placeholder for the enemy depot to the timings */
        if (Opponent::isUnknownRace())
        {
            /* For a random opponent, we don't know what type of depot they have, so just add one of each */
            /* The matchup-specific strategy engines will never query for offrace types anyway, so the extras are meaningless */
            enemyUnitTimings[BWAPI::UnitTypes::Protoss_Nexus].emplace_back(std::make_pair(0, INT_MAX));
            enemyUnitTimings[BWAPI::UnitTypes::Terran_Command_Center].emplace_back(std::make_pair(0, INT_MAX));
            enemyUnitTimings[BWAPI::UnitTypes::Zerg_Hatchery].emplace_back(std::make_pair(0, INT_MAX));
        }
        else
        {
            enemyUnitTimings[BWAPI::Broodwar->enemy()->getRace().getResourceDepot()].emplace_back(std::make_pair(0, INT_MAX));
        }

        /* Initialize resources */
        for (auto rawUnit : BWAPI::Broodwar->getNeutralUnits())
        {
            if (rawUnit->getType().isMineralField() || rawUnit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
            {
                tileToResource.emplace(rawUnit->getTilePosition(), std::make_shared<ResourceImpl>(rawUnit));
            }
        }
    }

    void update()
    {
        upgradesInProgress.clear();
        researchInProgress.clear();

        /* Start by updating the order timers */
        /* Unit update may reset these later in the frame if something has been observed that reveals the order timer */
        /* Order timers reset to unknown values every 150 frames starting on frame 8 */
        auto updateOrderProcessTimer =
                OrderProcessTimer::isResetFrame()
                ? [](const Unit &gameUnit) { gameUnit->orderProcessTimer = -1; }
                : [](const Unit &gameUnit)
                    {
                        if (gameUnit->orderProcessTimer > 0)
                        {
                            gameUnit->orderProcessTimer--;
                        }
                        else if (gameUnit->orderProcessTimer == 0)
                        {
                            gameUnit->orderProcessTimer = 8;
                        }
                    };
        for (auto &gameUnit : myUnits) updateOrderProcessTimer(gameUnit);
        for (auto &gameUnit : enemyUnits) updateOrderProcessTimer(gameUnit);

        auto ignoreUnit = [](BWAPI::Unit rawUnit)
        {
            return rawUnit->getType() == BWAPI::UnitTypes::Protoss_Interceptor ||
                   rawUnit->getType() == BWAPI::UnitTypes::Protoss_Scarab ||
                   rawUnit->getType().isSpell();
        };

        /* Update our units */
        /* We always have vision of our own units, so we don't have to handle units in fog */
        for (auto rawUnit : BWAPI::Broodwar->self()->getUnits())
        {
            if (!rawUnit || !rawUnit->exists()) continue;

            trackResearch(rawUnit);

            if (ignoreUnit(rawUnit)) continue;

            /* If we just mind controlled an enemy unit, consider the enemy unit destroyed */
            auto enemyIt = unitIdToEnemyUnit.find(rawUnit->getID());
            if (enemyIt != unitIdToEnemyUnit.end())
            {
                enemyUnitDestroyed(enemyIt->second);
            }

            /* Create or update */
            OwnedUnit gameUnit;
            auto iterRef = unitIdToMyUnit.find(rawUnit->getID());
            if (iterRef == unitIdToMyUnit.end() || !iterRef->second || !iterRef->second->exists())
            {
                if (rawUnit->getType().isWorker())
                {
                    gameUnit = std::make_shared<MyWorkerImpl>(rawUnit);
                    gameUnit->created();
                }
                else if (rawUnit->getType() == BWAPI::UnitTypes::Protoss_Dragoon)
                {
                    gameUnit = std::make_shared<MyDragoon>(rawUnit);
                    gameUnit->created();
                }
                else if (rawUnit->getType() == BWAPI::UnitTypes::Protoss_Carrier)
                {
                    gameUnit = std::make_shared<MyCarrier>(rawUnit);
                    gameUnit->created();
                }
                else if (rawUnit->getType() == BWAPI::UnitTypes::Protoss_Corsair)
                {
                    gameUnit = std::make_shared<MyCorsair>(rawUnit);
                    gameUnit->created();
                }
                else if (rawUnit->getType() == BWAPI::UnitTypes::Protoss_Photon_Cannon)
                {
                    gameUnit = std::make_shared<MyCannon>(rawUnit);
                    gameUnit->created();
                }
                else
                {
                    gameUnit = std::make_shared<MyUnitImpl>(rawUnit);
                    gameUnit->created();
                }
                myUnits.insert(gameUnit);
                unitIdToMyUnit.emplace(gameUnit->ident, gameUnit);

                unitCreated(gameUnit);

                if (gameUnit->completed)
                    myCompletedUnitsByType[gameUnit->type].insert(gameUnit);
                else
                    myIncompleteUnitsByType[gameUnit->type].insert(gameUnit);
            }
            else
            {
                gameUnit = iterRef->second;

                if (gameUnit->type == BWAPI::UnitTypes::Protoss_Assimilator && !gameUnit->completed && rawUnit->isCompleted())
                {
                    for (auto &baseLoc : Map::getMyBases())
                    {
                        if (baseLoc->hasGeyserOrRefineryAt(gameUnit->getTilePosition()))
                        {
                            if (!baseLoc->resourceDepot)
                            {
                                AppLog::Get() << "ERROR: Assimilator @ " << gameUnit->getTilePosition() << " completed without nexus";
                            }
                            else if (!baseLoc->resourceDepot->completed)
                            {
                                AppLog::Get() << "ERROR: Assimilator @ " << gameUnit->getTilePosition() << " completed before nexus";
                            }
                        }
                    }
                }

                if (!gameUnit->completed && rawUnit->isCompleted())
                {
                    myCompletedUnitsByType[gameUnit->type].insert(gameUnit);
                    myIncompleteUnitsByType[gameUnit->type].erase(gameUnit);
                }

                gameUnit->update(rawUnit);
            }

            if (rawUnit->getType().isRefinery())
            {
                updateResource(rawUnit, gameUnit);
            }

            if (rawUnit->isUpgrading())
            {
                upgradesInProgress.insert(rawUnit->getUpgrade());
            }
            else if (rawUnit->isResearching())
            {
                researchInProgress.insert(rawUnit->getTech());
            }

            if (rawUnit->getBuildUnit())
            {
                auto producing = unitIdToMyUnit.find(rawUnit->getBuildUnit()->getID());
                if (producing != unitIdToMyUnit.end())
                {
                    producing->second->producer = rawUnit;
                }
            }
        }

        /* If this is the first frame, set the order process index for our initial units */
        /* They are always initialized in this order: depot, leftmost worker to rightmost worker */
        /* Some maps use tricks to move the starting workers. If we have a map-specific override, we use this to get the expected locations, */
        /* otherwise we set the order process indices to the same value since we don't know which comes first. */
        /* We also set the spawn position on the workers */
        if (frameNow == 0)
        {
            auto expectedWorkerPositions = Map::mapSpecificOverride()->startingWorkerPositions(BWAPI::Broodwar->self()->getStartLocation());
            int countFound = 0;
            for (auto &gameUnit : myUnits)
            {
                if (gameUnit->type.isResourceDepot())
                {
                    gameUnit->orderProcessIndex = 0;
                }
                if (!gameUnit->type.isWorker()) continue;

                auto myWorker = std::static_pointer_cast<MyWorkerImpl>(gameUnit);
                myWorker->spawnPosition = gameUnit->prevPos;

                for (int i = 0; i < expectedWorkerPositions.size(); i++)
                {
                    if (gameUnit->prevPos == expectedWorkerPositions[i])
                    {
                        gameUnit->orderProcessIndex = i + 1;
                        countFound++;
                        break;
                    }
                }
            }

            if (countFound != 4)
            {
                AppLog::Get() << "WARNING: Could not determine order process index of all starting workers";
                for (auto &gameUnit : myUnits)
                {
                    if (gameUnit->type.isWorker()) gameUnit->orderProcessIndex = 1;
                }
            }
        }

        /* Update visible enemy units */
        for (auto rawUnit : BWAPI::Broodwar->enemy()->getUnits())
        {
            trackResearch(rawUnit);

            if (ignoreUnit(rawUnit)) continue;
            if (!rawUnit->isVisible()) continue;

#if DEBUG_ENEMY_STATUS
            Telemetry::log(rawUnit->getID()) << "VIS"
                                               << ";cmpl=" << rawUnit->isCompleted()
                                               << ";iSA=" << rawUnit->isStartingAttack()
                                               << ";iA=" << rawUnit->isAttacking()
                                               << ";iAF=" << rawUnit->isAttackFrame()
                                               << ";cdwn=" << rawUnit->getGroundWeaponCooldown();
#endif

            /* If the enemy just mind controlled one of our units, consider our unit destroyed */
            auto myIt = unitIdToMyUnit.find(rawUnit->getID());
            if (myIt != unitIdToMyUnit.end())
            {
                myUnitDestroyed(myIt->second);
            }

            /* Create or update */
            auto iterRef = unitIdToEnemyUnit.find(rawUnit->getID());
            bool morphed = false;
            if (iterRef != unitIdToEnemyUnit.end())
            {
                /* If the type is still the same, update and continue */
                if (iterRef->second->type == rawUnit->getType())
                {
                    iterRef->second->update(rawUnit);
                    if (iterRef->second->type.isRefinery())
                    {
                        updateResource(rawUnit, iterRef->second);
                    }
                    continue;
                }

                /* The unit has morphed - for simplicity consider the old one as destroyed and the new one created */
                morphed = true;
                enemyUnitDestroyed(iterRef->second, true);
            }

            auto gameUnit = std::make_shared<UnitImpl>(rawUnit);
            gameUnit->created();
            enemyUnits.insert(gameUnit);
            unitIdToEnemyUnit.emplace(gameUnit->ident, gameUnit);

            unitCreated(gameUnit);

            if (gameUnit->type.isRefinery())
            {
                updateResource(rawUnit, gameUnit);
            }

            enemyUnitsByType[gameUnit->type].insert(gameUnit);
            trackEnemyUnitTimings(gameUnit, !morphed);
        }

        /* Update enemy units in the fog */
        std::vector<Unit> destroyedEnemyUnits;
        for (auto &gameUnit : enemyUnits)
        {
            if (gameUnit->lastSeen == frameNow) continue;

            gameUnit->updateUnitInFog();

            /* If a building that can't be lifted has disappeared from its last position, treat it as destroyed */
            if (!gameUnit->lastPositionValid && gameUnit->type.isBuilding() && !gameUnit->type.isFlyingBuilding())
            {
                destroyedEnemyUnits.push_back(gameUnit);
                continue;
            }

#if DEBUG_ENEMY_STATUS
            Telemetry::log(gameUnit->ident) << "FOG;cmpl=" << gameUnit->completed << ";lpv=" << gameUnit->lastPositionValid;
#endif
        }
        for (auto &gameUnit : destroyedEnemyUnits)
        {
            enemyUnitDestroyed(gameUnit);
        }

        /* Build a set of build tiles for mineral fields that should be visible */
        std::set<BWAPI::TilePosition> visibleMineralFieldTiles;
        for (const auto &[tilePos, mineralField] : tileToResource)
        {
            if (!mineralField->isMinerals) continue;

            if (BWAPI::Broodwar->isVisible(tilePos) && BWAPI::Broodwar->isVisible(tilePos + BWAPI::TilePosition(1, 0)))
            {
                if (mineralField->seenLastFrame)
                {
                    visibleMineralFieldTiles.insert(tilePos);
                }
                mineralField->seenLastFrame = true;
            }
            else
            {
                mineralField->seenLastFrame = false;
            }
        }

        /* Update visible neutral units to detect addons that have gone neutral or refineries that have become geysers */
        destroyedEnemyUnits.clear();
        for (auto rawUnit : BWAPI::Broodwar->neutral()->getUnits())
        {
            if (!rawUnit->isVisible()) continue;

            /* Update resource counts */
            if (rawUnit->getType().isMineralField() || rawUnit->getType() == BWAPI::UnitTypes::Resource_Vespene_Geyser)
            {
                updateResource(rawUnit);
                visibleMineralFieldTiles.erase(rawUnit->getTilePosition());
            }

            auto iterRef = unitIdToEnemyUnit.find(rawUnit->getID());
            if (iterRef != unitIdToEnemyUnit.end())
            {
                auto &gameUnit = iterRef->second;

                /* Refineries are treated as destroyed */
                if (gameUnit->type.isRefinery())
                {
                    destroyedEnemyUnits.push_back(gameUnit);
                    continue;
                }

                /* All others are treated as destroyed for the grid but otherwise not */
                if (gameUnit->type.isBuilding())
                {
                    AppLog::Get() << "Enemy switched to neutral: " << *gameUnit;
                }

                if (gameUnit->lastPositionValid && !gameUnit->beingManufacturedOrCarried)
                {
                    PlayerInfo::grid(gameUnit->player).unitDestroyed(gameUnit->type, gameUnit->prevPos, gameUnit->completed, gameUnit->burrowed, gameUnit->immobile);
#if DEBUG_GRID_UPDATES
                    Telemetry::log(gameUnit->ident) << "Grid::unitDestroyed " << gameUnit->prevPos;
                    AppLog::Debug() << *gameUnit << ": Grid::unitDestroyed " << gameUnit->prevPos;
#endif
                }

                gameUnit->rawUnit = nullptr; // Signals to all holding a copy of the pointer that this unit is dead

                enemyUnitsByType[gameUnit->type].erase(gameUnit);
                enemyUnits.erase(gameUnit);
                unitIdToEnemyUnit.erase(iterRef);
            }
        }
        for (auto &gameUnit : destroyedEnemyUnits)
        {
            enemyUnitDestroyed(gameUnit);
        }

        /* Mark mineral fields destroyed if we should be able to see them but they aren't there */
        for (const auto &mineralFieldTile : visibleMineralFieldTiles)
        {
            resourceDestroyed(mineralFieldTile);
        }

        assignEnemyUnitsToBases();

        /* Occasionally check for any inconsistencies in the enemy unit collections */
        if (frameNow % 48 == 0)
        {
            auto checkUnit = [](const Unit &gameUnit, const std::string &label)
            {
                if (!gameUnit->exists())
                {
                    AppLog::Get() << "ERROR: " << *gameUnit << " in " << label << " does not exist!";
                    Telemetry::log(gameUnit->ident) << "ERROR: " << *gameUnit << " in " << label << " does not exist!";
                    return false;
                }
                if (gameUnit->player != BWAPI::Broodwar->enemy())
                {
                    AppLog::Get() << "ERROR: " << *gameUnit << " in " << label << " not owned by enemy!";
                    Telemetry::log(gameUnit->ident) << "ERROR: " << *gameUnit << " in " << label << " not owned by enemy!";
                    return false;
                }
                if (gameUnit->rawUnit->isVisible() && gameUnit->rawUnit->getPlayer() != BWAPI::Broodwar->enemy())
                {
                    AppLog::Get() << "ERROR: " << *gameUnit << " in " << label << " not owned by enemy (bwapiUnit)!";
                    Telemetry::log(gameUnit->ident) << "ERROR: " << *gameUnit << " in " << label << " not owned by enemy (bwapiUnit)!";
                    return false;
                }
                return true;
            };

            for (auto iterRef = enemyUnits.begin(); iterRef != enemyUnits.end();)
            {
                if (checkUnit(*iterRef, "enemyUnits"))
                {
                    iterRef++;
                }
                else
                {
                    iterRef = enemyUnits.erase(iterRef);
                }
            }

            for (auto iterRef = unitIdToEnemyUnit.begin(); iterRef != unitIdToEnemyUnit.end();)
            {
                if (checkUnit(iterRef->second, "unitIdToEnemyUnit"))
                {
                    iterRef++;
                }
                else
                {
                    iterRef = unitIdToEnemyUnit.erase(iterRef);
                }
            }

            for (auto &typeAndEnemyUnits : enemyUnitsByType)
            {
                for (auto iterRef = typeAndEnemyUnits.second.begin(); iterRef != typeAndEnemyUnits.second.end();)
                {
                    if (checkUnit(*iterRef, "enemyUnitsByType"))
                    {
                        iterRef++;
                    }
                    else
                    {
                        iterRef = typeAndEnemyUnits.second.erase(iterRef);
                    }
                }
            }

            for (auto iterRef = enemyUnitsToBase.begin(); iterRef != enemyUnitsToBase.end();)
            {
                if (checkUnit(iterRef->first, "enemyUnitsToBase"))
                {
                    iterRef++;
                }
                else
                {
                    iterRef = enemyUnitsToBase.erase(iterRef);
                }
            }

            for (auto &basesAndEnemyUnits : basesToEnemyUnits)
            {
                for (auto iterRef = basesAndEnemyUnits.second.begin(); iterRef != basesAndEnemyUnits.second.end();)
                {
                    if (checkUnit(*iterRef, "basesAndEnemyUnits"))
                    {
                        iterRef++;
                    }
                    else
                    {
                        iterRef = basesAndEnemyUnits.second.erase(iterRef);
                    }
                }
            }
        }

        /* Output debug info for our own units */
        for (auto &gameUnit : myUnits)
        {
            if (!gameUnit->completed) continue;

            bool output = false;
#if DEBUG_PROBE_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Probe;
#endif
#if DEBUG_ZEALOT_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Zealot;
#endif
#if DEBUG_DRAGOON_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Dragoon;
#endif
#if DEBUG_DT_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Dark_Templar;
#endif
#if DEBUG_SHUTTLE_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Shuttle;
#endif
#if DEBUG_CARRIER_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Carrier;
#endif
#if DEBUG_OBSERVER_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Observer;
#endif
#if DEBUG_ARBITER_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Arbiter;
#endif
#if DEBUG_CANNON_STATUS
            output = output || gameUnit->type == BWAPI::UnitTypes::Protoss_Photon_Cannon;
#endif
#if DEBUG_PRODUCINGBUILDING_STATUS
            output = output || (gameUnit->type.isBuilding() && gameUnit->type.canProduce());
#endif

            if (!output) continue;

            std::ostringstream debug;

            /* First line is command */
            debug << "cmd=" << gameUnit->rawUnit->getLastCommand().getType() << ";f="
                  << (frameNow - gameUnit->lastCommandFrame);
            if (gameUnit->rawUnit->getLastCommand().getTarget())
            {
                debug << ";tgt=" << gameUnit->rawUnit->getLastCommand().getTarget()->getType()
                      << "#" << gameUnit->rawUnit->getLastCommand().getTarget()->getID()
                      << "@" << BWAPI::WalkPosition(gameUnit->rawUnit->getLastCommand().getTarget()->getPosition())
                      << ";d=" << gameUnit->rawUnit->getLastCommand().getTarget()->getDistance(gameUnit->rawUnit);
            }
            else if (gameUnit->rawUnit->getLastCommand().getTargetPosition())
            {
                debug << ";tgt=" << BWAPI::WalkPosition(gameUnit->rawUnit->getLastCommand().getTargetPosition());
            }

            /* Next line is order */
            debug << "\nord=" << gameUnit->rawUnit->getOrder() << ";t=" << gameUnit->rawUnit->getOrderTimer();
            if (gameUnit->rawUnit->getOrderTarget())
            {
                debug << ";tgt=" << gameUnit->rawUnit->getOrderTarget()->getType()
                      << "#" << gameUnit->rawUnit->getOrderTarget()->getID()
                      << "@" << BWAPI::WalkPosition(gameUnit->rawUnit->getOrderTarget()->getPosition())
                      << ";d=" << gameUnit->rawUnit->getOrderTarget()->getDistance(gameUnit->rawUnit);
            }
            else if (gameUnit->rawUnit->getOrderTargetPosition())
            {
                debug << ";tgt=" << BWAPI::WalkPosition(gameUnit->rawUnit->getOrderTargetPosition());
            }
            if (gameUnit->player == BWAPI::Broodwar->self())
            {
                auto myUnit = std::static_pointer_cast<MyUnitImpl>(gameUnit);
                debug << ";idx=" << myUnit->orderProcessIndex;
            }

            /* Last line is movement data and other unit-specific stuff */
            debug << "\n";
            if (gameUnit->type.topSpeed() > 0.001)
            {
                auto speed = sqrt(gameUnit->rawUnit->getVelocityX() * gameUnit->rawUnit->getVelocityX()
                                  + gameUnit->rawUnit->getVelocityY() * gameUnit->rawUnit->getVelocityY());
                debug << "spd=" << ((int) (100.0 * speed / gameUnit->type.topSpeed()))
                      << ";mvng=" << gameUnit->rawUnit->isMoving() << ";rdy=" << gameUnit->isReady()
                      << ";stk=" << gameUnit->rawUnit->isStuck()
                      << ";lstmv=" << (gameUnit->getLastMoveFrame() - frameNow);
            }

            if (gameUnit->getUnstickUntil() >= frameNow)
            {
                debug << ";unstck=" << (gameUnit->getUnstickUntil() - frameNow);
            }

            if (gameUnit->type == BWAPI::UnitTypes::Protoss_Dragoon)
            {
                debug << "\n";

                auto myDragoon = std::static_pointer_cast<MyDragoon>(gameUnit);
                debug << "lstatk=" << myDragoon->getLastAttackStartedAt();
                debug << ";nxtatk=" << myDragoon->getNextAttackPredictedAt();
                debug << ";stkf=" << myDragoon->getPotentiallyStuckSince();
            }

            if (gameUnit->type.isWorker())
            {
                debug << "\n";

                auto myWorker = std::static_pointer_cast<MyWorkerImpl>(gameUnit);
                debug << "(x=" << gameUnit->prevPos.x
                   << " y=" << gameUnit->prevPos.y
                   << " dx=" << myWorker->horizontalSpeed8b
                   << " dy=" << myWorker->verticalSpeed8b
                   << " h=" << myWorker->heading8b
                   << ")";
            }

            if (gameUnit->type.isBuilding() && gameUnit->type.canProduce())
            {
                debug << ";q=[";
                bool first = true;
                for (const auto &type : gameUnit->rawUnit->getTrainingQueue())
                {
                    if (first)
                    {
                        first = false;
                    }
                    else
                    {
                        debug << ",";
                    }
                    debug << type;
                }
                debug << "]";
            }

            Telemetry::log(gameUnit->ident) << debug.str();
        }

#if DEBUG_PREDICTED_POSITIONS
        auto outputPredictedPositions = [](const Unit& gameUnit)
        {
            std::ostringstream buf;
            buf << "0: " << gameUnit->prevPos;
            for (int i=1; i <= BWAPI::Broodwar->getLatencyFrames() + 2; i++)
            {
                buf << "\n" << i << ": " << gameUnit->predictPosition(i);
            }
            Telemetry::log(gameUnit->ident) << buf.str();
        };
        for (auto &gameUnit : myUnits) outputPredictedPositions(gameUnit);
        for (auto &gameUnit : enemyUnits) outputPredictedPositions(gameUnit);
#endif

#if DEBUG_ENEMY_TIMINGS
        std::vector<std::string> values;
        for (auto &typeAndTimings : enemyUnitTimings)
        {
            if (typeAndTimings.second.empty()) continue;

            std::ostringstream timings;
            bool first = true;
            for (auto timing : typeAndTimings.second)
            {
                if (first)
                {
                    timings << typeAndTimings.first << ": ";
                    first = false;
                }
                else
                {
                    timings << ", ";
                }
                timings << timing.first << ":" << timing.second;
            }

            values.emplace_back(timings.str());
        }
        Telemetry::setBoardListValue("enemyUnitTimings", values);
#endif

#if DEBUG_ORDER_TIMERS
        for (auto &gameUnit : myUnits) Telemetry::log(gameUnit->ident) << "Order timer: " << gameUnit->orderProcessTimer;
        for (auto &gameUnit : enemyUnits) Telemetry::log(gameUnit->ident) << "Order timer: " << gameUnit->orderProcessTimer;
#endif
    }

    void issueOrders()
    {
        for (auto &gameUnit : myUnits)
        {
            gameUnit->issueMoveOrders();
        }
    }

    void onUnitDestroy(BWAPI::Unit rawUnit)
    {
        auto iterRef = unitIdToMyUnit.find(rawUnit->getID());
        if (iterRef != unitIdToMyUnit.end())
        {
            myUnitDestroyed(iterRef->second);
        }

        auto enemyIt = unitIdToEnemyUnit.find(rawUnit->getID());
        if (enemyIt != unitIdToEnemyUnit.end())
        {
            enemyUnitDestroyed(enemyIt->second);
        }
    }

    void onBulletCreate(BWAPI::Bullet bullet)
    {
        if (!bullet->getTarget()) return;
        auto target = get(bullet->getTarget());
        if (!target) return;

        Unit source = nullptr;
        if (bullet->getSource()) {
            source = get(bullet->getSource());
        }

        if (source) source->lastTarget = target;

        /* If this bullet is a ranged bullet that deals damage after a delay, track it on the unit it is moving towards */
        if (Bullets::dealsDamageAfterDelay(bullet->getType()))
        {
            target->addUpcomingAttack(source, bullet);
        }
    }

    Unit get(BWAPI::Unit gameUnit)
    {
        if (!gameUnit) return nullptr;

        auto iterRef = unitIdToMyUnit.find(gameUnit->getID());
        if (iterRef != unitIdToMyUnit.end())
        {
            return iterRef->second;
        }

        auto enemyIt = unitIdToEnemyUnit.find(gameUnit->getID());
        if (enemyIt != unitIdToEnemyUnit.end())
        {
            return enemyIt->second;
        }

        return nullptr;
    }

    OwnedUnit mine(BWAPI::Unit gameUnit)
    {
        auto iterRef = unitIdToMyUnit.find(gameUnit->getID());
        if (iterRef != unitIdToMyUnit.end())
        {
            return iterRef->second;
        }

        return nullptr;
    }

    OwnedUnit myBuildingAt(BWAPI::TilePosition tilePos)
    {
        for (auto gameUnit : myUnits)
        {
            if (!gameUnit->type.isBuilding()) continue;
            if (gameUnit->getTilePosition() == tilePos) return gameUnit;
        }

        return nullptr;
    }

    std::unordered_set<OwnedUnit> &allMine()
    {
        return myUnits;
    }

    std::unordered_set<OwnedUnit> &allMineCompletedOfType(BWAPI::UnitType type)
    {
        return myCompletedUnitsByType[type];
    }

    std::unordered_set<OwnedUnit> &allMineIncompleteOfType(BWAPI::UnitType type)
    {
        return myIncompleteUnitsByType[type];
    }

    std::map<BWAPI::UnitType, std::unordered_set<OwnedUnit>> &allMineIncompleteByType()
    {
        return myIncompleteUnitsByType;
    }

    std::unordered_set<Unit> &allEnemy()
    {
        return enemyUnits;
    }

    std::unordered_set<Unit> &allEnemyOfType(BWAPI::UnitType type)
    {
        return enemyUnitsByType[type];
    }

    void mine(std::set<OwnedUnit> &units,
              const std::function<bool(const OwnedUnit &)> &predicate)
    {
        for (auto &gameUnit : myUnits)
        {
            if (predicate && !predicate(gameUnit)) continue;
            units.insert(gameUnit);
        }
    }

    void enemy(std::set<Unit> &units,
               const std::function<bool(const Unit &)> &predicate)
    {
        for (auto &gameUnit : enemyUnits)
        {
            if (predicate && !predicate(gameUnit)) continue;
            units.insert(gameUnit);
        }
    }

    void enemyInRadius(std::set<Unit> &units,
                       BWAPI::Position position,
                       int radius,
                       const std::function<bool(const Unit &)> &predicate)
    {
        for (auto &gameUnit : enemyUnits)
        {
            if (predicate && !predicate(gameUnit)) continue;
            if (gameUnit->simPositionValid && gameUnit->simPosition.getApproxDistance(position) <= radius)
                units.insert(gameUnit);
        }
    }

    void enemyInArea(std::set<Unit> &units,
                     const BWEM::Area *area,
                     const std::function<bool(const Unit &)> &predicate)
    {
        for (auto &gameUnit : enemyUnits)
        {
            if (predicate && !predicate(gameUnit)) continue;
            if (gameUnit->lastPositionValid && BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos)) == area)
                units.insert(gameUnit);
        }
    }

    std::unordered_set<Unit> &enemyAtBase(Base *baseLoc)
    {
        return basesToEnemyUnits[baseLoc];
    }

    int countAll(BWAPI::UnitType type)
    {
        return countCompleted(type) + countIncomplete(type);
    }

    int countCompleted(BWAPI::UnitType type)
    {
        return (int)myCompletedUnitsByType[type].size();
    }

    int countIncomplete(BWAPI::UnitType type)
    {
        return (int)myIncompleteUnitsByType[type].size();
    }

    std::map<BWAPI::UnitType, int> countIncompleteByType()
    {
        std::map<BWAPI::UnitType, int> outcome;
        for (auto &typeAndUnits : myIncompleteUnitsByType)
        {
            outcome[typeAndUnits.first] = (int)typeAndUnits.second.size();
        }
        return outcome;
    }

    int countEnemy(BWAPI::UnitType type)
    {
        return (int)enemyUnitsByType[type].size();
    }

    std::vector<std::pair<int, int>> &getEnemyUnitTimings(BWAPI::UnitType type)
    {
        return enemyUnitTimings[type];
    }

    bool hasEnemyBuilt(BWAPI::UnitType type)
    {
        auto iterRef = enemyUnitTimings.find(type);
        return iterRef != enemyUnitTimings.end() && !iterRef->second.empty();
    }

    Resource resourceAt(BWAPI::TilePosition tilePos)
    {
        auto iterRef = tileToResource.find(tilePos);
        if (iterRef == tileToResource.end()) return nullptr;

        return iterRef->second;
    }

    std::vector<Resource> myCompletedRefineries()
    {
        std::vector<Resource> outcome;
        for (auto &[tilePos, resource] : tileToResource)
        {
            if (resource->hasMyCompletedRefinery())
            {
                outcome.emplace_back(resource);
            }
        }

        return outcome;
    }

    bool isBeingUpgradedOrResearched(UpgradeOrTechType type)
    {
        if (type.isTechType())
        {
            return researchInProgress.contains(type.techType);
        }

        return upgradesInProgress.contains(type.upgradeType);
    }

#if LOGGING_ENABLED
    void setLogUnitsCreatedAndLost(bool newValue)
    {
        logUnitsCreatedAndLost = newValue;
    }
#endif
}
