#include "ForgeFastExpand.h"

#include "Geo.h"
#include "General.h"
#include "Units.h"
#include "Map.h"
#include "Workers.h"
#include "Strategist.h"
#include "UnitUtil.h"
#include "Builder.h"
#include <Strategist/Plays/MainArmy/DefendMyMain.h>
#include <Strategist/Plays/Defensive/DefendBase.h>
#include "Strategies.h"

#if INSTRUMENTATION_ENABLED
#define CVIS_LOG_STATE_CHANGES true
#endif

/*
 * Our FFE play handles both performing the build order, defending the wall, and transitioning
 * into normal base defense if the wall is at any point determined to be indefensible.
 *
 * The play orders zealots it needs to defend the wall, but other army production is handled by the strategy engine.
 *
 * The logic is implemented as a state machine, see states defined in the class.
 */

namespace
{
    bool isValidBuilding(const Unit& gameUnit)
    {
        if (!gameUnit->lastPositionValid) return false;
        if (!gameUnit->type.isBuilding()) return false;
        if (gameUnit->isFlying) return false;
        return true;
    }

    bool nexusPositionBlocked()
    {
        auto nexusPosition = Map::getMyNatural()->getTilePosition();
        for (const auto &gameUnit : UnitMgr::allEnemy())
        {
            if (!isValidBuilding(gameUnit)) continue;

            if (Geom::Overlaps(nexusPosition, 4, 3, gameUnit->getTilePosition(), gameUnit->type.tileWidth(), gameUnit->type.tileHeight()))
            {
                return true;
            }
        }

        return false;
    }

    bool proxyBehindOurWall()
    {
        if (!Director::isEnemyStrategy(PvP::ProtossStrategy::ProxyRush) &&
            !Director::isEnemyStrategy(PvT::TerranStrategy::ProxyRush))
        {
            return false;
        }

        auto mainAndNaturalAreas = Map::getMyMainAreas();
        mainAndNaturalAreas.insert(Map::getMyNatural()->getArea());

        for (const auto &gameUnit : UnitMgr::allEnemy())
        {
            if (!isValidBuilding(gameUnit)) continue;

            if (mainAndNaturalAreas.contains(BWEM::Map::Instance().GetArea(BWAPI::WalkPosition(gameUnit->prevPos))))
            {
                AppLog::Get() << "Proxy detected behind our wall";
                return true;
            }
        }

        return false;
    }

    int worstCaseEnemyArrivalFrame()
    {
        /* For PvP or PvT proxy rushes assume arrival frame of 3700 */
        if (Director::getStrategyEngine()->isEnemyProxy())
        {
            Telemetry::setBoardValue("worstCaseEnemyArrival", (std::ostringstream() << 3700).str());
            return 3700;
        }

        /* Set the types we are looking for, random assumes zerg as the worst-case situation (besides proxies) */
        BWAPI::UnitType builderType;
        BWAPI::UnitType unitType;
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
        {
            builderType = BWAPI::UnitTypes::Protoss_Gateway;
            unitType = BWAPI::UnitTypes::Protoss_Zealot;
        }
        else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran)
        {
            builderType = BWAPI::UnitTypes::Terran_Barracks;
            unitType = BWAPI::UnitTypes::Terran_Marine;
        }
        else
        {
            builderType = BWAPI::UnitTypes::Zerg_Spawning_Pool;
            unitType = BWAPI::UnitTypes::Zerg_Zergling;
        }

        /* Get start frame of the building needed to produce combat units */

        /* If we have scouted the enemy base, default to the last time we scouted it */
        /* If we haven't scouted the enemy base, default to 9 pool */
        int builderStartFrame;
        if (Director::hasWorkerScoutCompletedInitialBaseScan())
        {
            builderStartFrame = Map::getEnemyStartingMain()->lastScouted;
        }
        else
        {
            builderStartFrame = 1600;
        }

        /* Add observed information about the building construction */
        auto builderTimings = UnitMgr::getEnemyUnitTimings(builderType);
        if (!builderTimings.empty())
        {
            auto &[startFrame, seenFrame] = *builderTimings.begin();

            /* We do not trust the start frame if we first saw the building was finished */
            /* In this case we assume 9 pool */
            if (startFrame <= (seenFrame - UnitHelper::BuildTime(builderType)))
            {
                builderStartFrame = 1600;
            }
            else
            {
                builderStartFrame = startFrame;
            }
        }

        /* Get travel time from the closest producer we have seen */
        /* If the enemy has not yet been scouted, we use the closest possible base position */
        std::vector<std::pair<BWAPI::Position, int>> producers;

        auto producerType = BWAPI::UnitTypes::Zerg_Hatchery;
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss) producerType = BWAPI::UnitTypes::Protoss_Gateway;
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Terran) producerType = BWAPI::UnitTypes::Terran_Barracks;
        for (const auto &gameUnit : UnitMgr::allEnemyOfType(producerType))
        {
            producers.emplace_back(gameUnit->prevPos,
                                   gameUnit->completed ? 0 : std::max(gameUnit->estimatedCompletionFrame
                                                                  - std::max(builderStartFrame + UnitHelper::BuildTime(builderType), frameNow),
                                                                  0));
        }

        for (auto &baseLoc : Map::allStartingLocations())
        {
            if (baseLoc == Map::getMyMain()) continue;
            if (Map::getEnemyStartingMain())
            {
                if (baseLoc != Map::getEnemyStartingMain()) continue;
            }
            else if (baseLoc->lastScouted > -1)
            {
                continue;
            }

            producers.emplace_back(baseLoc->getPosition(), 0);
        }

        auto &wall = BuildingPlacement::getForgeGatewayWall();
        int lowestTravelTime = INT_MAX;
        for (auto &[pt, extraFrames] : producers)
        {
            int frames = extraFrames + Pathfinder::ExpectedTravelTime(pt,
                                                                       wall.gapCenter,
                                                                       unitType,
                                                                       Pathfinder::PathFindingOptions::Default,
                                                                       1.1,
                                                                       INT_MAX);
            if (frames < lowestTravelTime) lowestTravelTime = frames;
        }

        if (lowestTravelTime == INT_MAX)
        {
            AppLog::Get() << "ERROR: Unable to compute enemy unit travel time to wall";
            lowestTravelTime = 650; // short rush distance on Python
        }

        /* Put it all together to get the arrival time */
        int arrivalTime = builderStartFrame
                          + UnitHelper::BuildTime(builderType)
                          + UnitHelper::BuildTime(unitType)
                          + lowestTravelTime;

        Telemetry::setBoardValue("worstCaseEnemyArrival", (std::ostringstream() << arrivalTime).str());

        return arrivalTime;
    }

    void addBuildingToGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                            BWAPI::UnitType type,
                            BWAPI::TilePosition tilePos,
                            int tick = 0,
                            int priority = PRIORITY_DEPOTS,
                            int framesUntilPowered = 0)
    {
        auto buildLocation = BuildingPlacement::BuildLocation(BuildBlock::Location(tilePos),
                                                              BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                               tilePos,
                                                                                               type),
                                                              framesUntilPowered,
                                                              0);
        prioProdGoals[priority].emplace_back(
                std::in_place_type<UnitProductionGoal>,
                "ForgeFastExpand",
                type,
                1,
                1,
                buildLocation,
                tick);
    }

    void addUnitToGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                        BWAPI::UnitType type,
                        int priority = PRIORITY_DEPOTS)
    {
        prioProdGoals[priority].emplace_back(
                std::in_place_type<UnitProductionGoal>,
                "ForgeFastExpand",
                type,
                1,
                1);
    }

    void moveWorkerProductionToLowerPriority(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                                             int afterWorkers,
                                             int newPriority = PRIORITY_DEPOTS)
    {
        /* Jump out if there is no probe production in progress */
        auto &workerGoals = prioProdGoals[PRIORITY_WORKERS];
        if (workerGoals.empty()) return;
        for (auto &workerGoal : workerGoals)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&workerGoal);
            if (!probeGoal || !probeGoal->unitType().isWorker())
            {
                return;
            }
        }

        auto &newGoals = prioProdGoals[newPriority];

        /* Get current number of probes without counting scout worker */
        int currentProbes = UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Probe);
        if (Director::getWorkerScoutStatus() != Director::WorkerScoutStatus::Unstarted && !Director::isWorkerScoutComplete()) currentProbes--;

        /* Adjust probe production at high priority to the number requested */
        int probes = currentProbes;
        auto iterRef = workerGoals.begin();
        while (iterRef != workerGoals.end() && probes < afterWorkers)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&*iterRef);
            probes += probeGoal->countToProduce();
            if (probes > afterWorkers)
            {
                probeGoal->setCountToProduce(afterWorkers - (probes - probeGoal->countToProduce()));

                newGoals.emplace_back(std::in_place_type<UnitProductionGoal>,
                                      probeGoal->requester,
                                      BWAPI::UnitTypes::Protoss_Probe,
                                      probes - afterWorkers,
                                      probeGoal->getProducerLimit(),
                                      probeGoal->getLocation());
            }

            iterRef++;
        }

        if (iterRef != workerGoals.end())
        {
            newGoals.insert(newGoals.end(), std::make_move_iterator(iterRef), std::make_move_iterator(workerGoals.end()));
            workerGoals.erase(iterRef, workerGoals.end());
        }
    }

    int buildBaseDefenseCannons(Base *baseLoc,
                                 std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                                 int count,
                                 int tick,
                                 int priority = PRIORITY_EMERGENCY)
    {
        if (!baseLoc) return 0;

        auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(baseLoc);
        if (!baseStaticDefenseLocations.isValid()) return 0;

        int currentCannons = 0;
        auto cannonLocations = baseStaticDefenseLocations.workerDefenseCannons;
        for (auto iterRef = cannonLocations.begin(); iterRef != cannonLocations.end();)
        {
            auto cannon = UnitMgr::myBuildingAt(*iterRef);
            if (cannon)
            {
                currentCannons++;
                iterRef = cannonLocations.erase(iterRef);
            }
            else
            {
                iterRef++;
            }
        }

        if (cannonLocations.empty()) return 0;
        if (currentCannons >= count) return 0;

        int startFrame = tick - UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
        int powerFrame = Constructor::framesUntilCompleted(
                baseStaticDefenseLocations.powerPylon,
                UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Pylon) + 240);

        auto pylon = UnitMgr::myBuildingAt(baseStaticDefenseLocations.powerPylon);
        if (!pylon)
        {
            addBuildingToGoals(prioProdGoals,
                               BWAPI::UnitTypes::Protoss_Pylon,
                               baseStaticDefenseLocations.powerPylon,
                               startFrame - UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Pylon),
                               priority);
        }

        if (pylon && !pylon->completed)
        {
            startFrame = std::max(startFrame, pylon->completionFrame);
        }

        int queued = 0;
        for (auto cannonLocation : cannonLocations)
        {
            addBuildingToGoals(prioProdGoals,
                               BWAPI::UnitTypes::Protoss_Photon_Cannon,
                               cannonLocation,
                               startFrame,
                               priority,
                               powerFrame);
            queued++;
            if ((queued + currentCannons) >= count) break;
        }

        return powerFrame;
    }

    int currentWallCannons(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                           std::vector<BWAPI::TilePosition> &cannonPlacementsAvailable)
    {
        cannonPlacementsAvailable = BuildingPlacement::getForgeGatewayWall().cannons;

        int count = 0;
        for (auto iterRef = cannonPlacementsAvailable.begin(); iterRef != cannonPlacementsAvailable.end(); )
        {
            auto tilePos = *iterRef;

            if (UnitMgr::myBuildingAt(tilePos))
            {
                goto cannonPlacementUsed;
            }

            for (const auto &priorityGoals : prioProdGoals)
            {
                for (const auto &goal : priorityGoals.second)
                {
                    if (auto unitProductionGoal = std::get_if<UnitProductionGoal>(&goal))
                    {
                        if (unitProductionGoal->unitType() != BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;

                        const auto& location = unitProductionGoal->getLocation();
                        if (auto buildLocation = std::get_if<BuildingPlacement::BuildLocation>(&location))
                        {
                            if (buildLocation->location.tilePos == tilePos)
                            {
                                goto cannonPlacementUsed;
                            }
                        }
                    }
                }
            }

            iterRef++;
            continue;

            cannonPlacementUsed:;

            iterRef = cannonPlacementsAvailable.erase(iterRef);
            count++;
        }

        return count;
    }

    void buildWallCannons(std::map<int, std::vector<ProductionGoal>> &prioProdGoals,
                          std::vector<BWAPI::TilePosition> &cannonPlacementsAvailable,
                          int count,
                          int tick,
                          int priority = PRIORITY_EMERGENCY)
    {
        if (count <= 0) return;

        int remaining = count;
        while (remaining > 0)
        {
            if (cannonPlacementsAvailable.empty()) return;

            addBuildingToGoals(prioProdGoals,
                               BWAPI::UnitTypes::Protoss_Photon_Cannon,
                               *cannonPlacementsAvailable.begin(),
                               tick,
                               priority);
            cannonPlacementsAvailable.erase(cannonPlacementsAvailable.begin());

            remaining--;
        }
    }

    void handleMutaRush(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
    {
        if (!Director::isEnemyStrategy(PvZ::ZergStrategy::MutaRush)) return;

        /* Determine potential positions and frames where a muta will complete */
        std::vector<std::pair<BWAPI::Position, int>> mutaPositionsAndFrames;

        /* Case where we haven't seen them yet */
        if (UnitMgr::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) == 0)
        {
            int spireStartFrame = 0;
            auto spireTimings = UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Spire);
            if (!spireTimings.empty())
            {
                auto &[startFrame, seenFrame] = *spireTimings.begin();

                /* We trust the start frame if we first saw it while it was incomplete */
                if (startFrame > (seenFrame - UnitHelper::BuildTime(BWAPI::UnitTypes::Zerg_Spire)))
                {
                    spireStartFrame = startFrame;
                }
            }
            if (spireStartFrame == 0)
            {
                auto lairTimings = UnitMgr::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Lair);
                if (!lairTimings.empty())
                {
                    auto &[startFrame, seenFrame] = *lairTimings.begin();

                    /* We trust the start frame if we first saw it while it was incomplete */
                    if (startFrame > (seenFrame - UnitHelper::BuildTime(BWAPI::UnitTypes::Zerg_Lair)))
                    {
                        spireStartFrame = startFrame + UnitHelper::BuildTime(BWAPI::UnitTypes::Zerg_Lair);
                    }
                }
            }

            /* If we don't have data now, we didn't scout the spire or lair while they were incomplete */
            /* So default to a muta completing now */
            int mutaCompletionFrame;
            if (spireStartFrame == 0)
            {
                mutaCompletionFrame = frameNow;
            }
            else
            {
                mutaCompletionFrame =
                        spireStartFrame + UnitHelper::BuildTime(BWAPI::UnitTypes::Zerg_Spire) + UnitHelper::BuildTime(BWAPI::UnitTypes::Zerg_Mutalisk);
            }

            /* Add one muta from each known enemy hatchery or lair */
            for (const auto &hatch : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Zerg_Hatchery))
            {
                if (hatch->completed)
                {
                    mutaPositionsAndFrames.emplace_back(hatch->prevPos, mutaCompletionFrame);
                }
            }
            for (const auto &lair : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Zerg_Lair))
            {
                mutaPositionsAndFrames.emplace_back(lair->prevPos, mutaCompletionFrame);
            }
        }
        else
        {
            /* Add each muta's last position */
            for (const auto &muta : UnitMgr::allEnemyOfType(BWAPI::UnitTypes::Zerg_Mutalisk))
            {
                mutaPositionsAndFrames.emplace_back(muta->prevPos, muta->lastSeen);
            }
        }

        /* Now determine when the earliest muta could reach the main and natural */
        auto frameForPosition = [&mutaPositionsAndFrames](BWAPI::Position target)
        {
            int earliestArrivalFrame = INT_MAX;
            for (const auto &[pt, tick] : mutaPositionsAndFrames)
            {
                int arrivalFrame = tick + Pathfinder::ExpectedTravelTime(pt, target, BWAPI::UnitTypes::Zerg_Mutalisk);
                if (arrivalFrame < earliestArrivalFrame)
                {
                    earliestArrivalFrame = arrivalFrame;
                }
            }
            return earliestArrivalFrame;
        };
        buildBaseDefenseCannons(Map::getMyMain(), prioProdGoals, 3, frameForPosition(Map::getMyMain()->getPosition()));
        buildBaseDefenseCannons(Map::getMyNatural(), prioProdGoals, 2, frameForPosition(Map::getMyNatural()->getPosition()));

        /* Add a second cannon at the wall */
        std::vector<BWAPI::TilePosition> cannonPlacementsAvailable;
        int currentCannons = currentWallCannons(prioProdGoals, cannonPlacementsAvailable);
        if (currentCannons < 2)
        {
            buildWallCannons(prioProdGoals,
                             cannonPlacementsAvailable,
                             2 - currentCannons,
                             frameForPosition(Map::getMyNatural()->getPosition()) - 100,
                             PRIORITY_BASEDEFENSE);
        }
    }

    void handleZerglingAllIn(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
    {
        if (!Director::isEnemyStrategy(PvZ::ZergStrategy::ZerglingAllIn)) return;

        std::vector<BWAPI::TilePosition> cannonPlacementsAvailable;
        int currentCannons = currentWallCannons(prioProdGoals, cannonPlacementsAvailable);
        if (currentCannons < 2) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 2 - currentCannons, 0);
        if (currentCannons < 3) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, 4000);
        if (currentCannons < 4) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, 5000);
        if (currentCannons < 5) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, 6000);
    }

    void handleHydraBust(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
    {
        if (!Director::isEnemyStrategy(PvZ::ZergStrategy::HydraBust)) return;

        std::vector<BWAPI::TilePosition> cannonPlacementsAvailable;
        int currentCannons = currentWallCannons(prioProdGoals, cannonPlacementsAvailable);
        if (currentCannons < 2) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 2 - currentCannons, 5000);
        if (currentCannons < 3) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, 6000);
        if (currentCannons < 4) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, 7000);
        if (currentCannons < 5) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, 8000);
        if (currentCannons < 6) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, 9000);
    }

    void handleNonZergRush(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
    {
        int baseFrame = 4000 - UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
        if (Director::isEnemyStrategy(PvP::ProtossStrategy::ProxyRush) ||
            Director::isEnemyStrategy(PvT::TerranStrategy::ProxyRush))
        {
            baseFrame -= 500;
        }
        else if (Director::isEnemyStrategy(PvP::ProtossStrategy::ZealotAllIn))
        {
            baseFrame += 500;
        }
        else if (!Director::isEnemyStrategy(PvP::ProtossStrategy::ZealotRush) &&
            !Director::isEnemyStrategy(PvT::TerranStrategy::MarineRush))
        {
            return;
        }

        std::vector<BWAPI::TilePosition> cannonPlacementsAvailable;
        int currentCannons = currentWallCannons(prioProdGoals, cannonPlacementsAvailable);
        if (currentCannons < 2) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 2 - currentCannons, baseFrame);
        if (currentCannons < 3) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, baseFrame + 1000);
        if (currentCannons < 4) buildWallCannons(prioProdGoals, cannonPlacementsAvailable, 1, baseFrame + 2000);

        if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
        {
            int zealots = UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Zealot);
            int desiredZealots = std::min(1, UnitMgr::countEnemy(BWAPI::UnitTypes::Protoss_Zealot) / 2);
            if (zealots < desiredZealots)
            {
                /* Put priority of zealot slightly lower than cannons if we don't have any yet */
                int zealotPriority = PRIORITY_MAINARMY;
                if (zealots == 0) zealotPriority = PRIORITY_EMERGENCY + 1;
                prioProdGoals[zealotPriority].emplace_back(
                        std::in_place_type<UnitProductionGoal>,
                        "ForgeFastExpand",
                        BWAPI::UnitTypes::Protoss_Zealot,
                        1,
                        1);
            }
        }
    }
}

ForgeFastExpand::ForgeFastExpand()
        : MainArmyPlay("ForgeFastExpand")
        , currentState(State::STATE_UNINITIALIZED)
        , squad(std::make_shared<DefendWallSquad>())
        , mainBaseWorkerDefenseSquad(std::make_unique<WorkerDefenseSquad>(Map::getMyMain()))
{
    Marshal::addSquad(squad);
}

void ForgeFastExpand::update()
{
    /* Perform worker defense in main base */
    std::set<Unit> enemyUnits(UnitMgr::enemyAtBase(Map::getMyMain()).begin(), UnitMgr::enemyAtBase(Map::getMyMain()).end());
    auto workersAndTargets = mainBaseWorkerDefenseSquad->selectTargets(enemyUnits);
    std::vector<std::pair<OwnedUnit, Unit>> emptyUnitsAndTargets;
    mainBaseWorkerDefenseSquad->execute(workersAndTargets, emptyUnitsAndTargets);

    updateState();
}

void ForgeFastExpand::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioProdGoals)
{
    /*
     * For all of the early states, this generates the current build order based on the information we have on the enemy.
     * When this is called, we expect the SaturateBases play to have added its probe goal, but nothing else to be added yet.
     */
    
    auto &wall = BuildingPlacement::getForgeGatewayWall();

    /* Common timing-based logic needed by multiple states */
    int enemyArrivalFrame = worstCaseEnemyArrivalFrame();
    int cannonFrame = enemyArrivalFrame - UnitHelper::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
    std::vector<BWAPI::TilePosition> cannonPlacementsAvailable;
    int currentCannons = currentWallCannons(prioProdGoals, cannonPlacementsAvailable);

    int desiredCannons = 2;

    bool nexusBlocked = nexusPositionBlocked();
    if (nexusBlocked)
    {
        cannonFrame = frameNow;
    }

    switch (currentState)
    {
        case State::STATE_UNINITIALIZED:
        {
            return;
        }
        case State::STATE_PYLON_PENDING:
        case State::STATE_FORGE_PENDING:
        {
            if (currentState == State::STATE_PYLON_PENDING)
            {
                addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Pylon, wall.pylon);
            }
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            buildWallCannons(prioProdGoals, cannonPlacementsAvailable, desiredCannons, cannonFrame, PRIORITY_DEPOTS);
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            addUnitToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot);
            break;
        }
        case State::STATE_NEXUS_PENDING:
        {
            buildWallCannons(prioProdGoals, cannonPlacementsAvailable, desiredCannons - currentCannons, cannonFrame);
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            addUnitToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot);
            moveWorkerProductionToLowerPriority(prioProdGoals, 14);
            break;
        }
        case State::STATE_GATEWAY_PENDING:
        {
            buildWallCannons(prioProdGoals, cannonPlacementsAvailable, desiredCannons - currentCannons, cannonFrame);
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            addUnitToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot);
            moveWorkerProductionToLowerPriority(prioProdGoals, 14);
            break;
        }
        case State::STATE_FINISHED:
        {
            if (UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Zealot) == 0 &&
                !Director::isEnemyStrategy(PvZ::ZergStrategy::MutaRush))
            {
                addUnitToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot, PRIORITY_EMERGENCY);
            }
            buildWallCannons(prioProdGoals, cannonPlacementsAvailable, desiredCannons - currentCannons, cannonFrame);

            /* Handle additional cannons based on enemy strategy */
            handleMutaRush(prioProdGoals);
            handleZerglingAllIn(prioProdGoals);
            handleHydraBust(prioProdGoals);
            handleNonZergRush(prioProdGoals);

            break;
        }
        case State::STATE_ANTIFASTRUSHZERG:
        {
            /* Basic idea is to build a pylon and cannons in the main, then add a gateway */
            auto defenseLocations = BuildingPlacement::baseStaticDefenseLocations(Map::getMyMain());
            if (!defenseLocations.powerPylon.isValid() || defenseLocations.workerDefenseCannons.empty())
            {
                status.transitionTo = std::make_shared<DefendMyMain>();
                break;
            }

            int powerFrame = buildBaseDefenseCannons(Map::getMyMain(), prioProdGoals, 2, 0);

            if (defenseLocations.startBlockCannon.isValid() && !UnitMgr::myBuildingAt(defenseLocations.startBlockCannon))
            {
                addBuildingToGoals(prioProdGoals,
                                   BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                   defenseLocations.startBlockCannon,
                                   0,
                                   PRIORITY_EMERGENCY,
                                   powerFrame);
            }

            prioProdGoals[PRIORITY_EMERGENCY].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    label,
                    BWAPI::UnitTypes::Protoss_Zealot,
                    1,
                    1);

            break;
        }
        case State::STATE_ANTIFASTRUSH_GATEWAY_PENDING:
        {
            buildWallCannons(prioProdGoals, cannonPlacementsAvailable, desiredCannons - currentCannons, cannonFrame);
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway, 0, PRIORITY_EMERGENCY);
            if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
            {
                addUnitToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot, PRIORITY_EMERGENCY);
            }
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            moveWorkerProductionToLowerPriority(prioProdGoals, 16);
            break;
        }
        case State::STATE_ANTIFASTRUSH_NEXUS_PENDING:
        {
            buildWallCannons(prioProdGoals, cannonPlacementsAvailable, desiredCannons - currentCannons, cannonFrame);
            if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss && UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Zealot) == 0)
            {
                addUnitToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Zealot, PRIORITY_EMERGENCY);
            }
            handleMutaRush(prioProdGoals);
            handleZerglingAllIn(prioProdGoals);
            handleHydraBust(prioProdGoals);
            handleNonZergRush(prioProdGoals);
            addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            moveWorkerProductionToLowerPriority(prioProdGoals, 16);
            break;
        }
    }

    /* If the enemy did a gas steal, build two cannons in our main to kill it */
    if (currentState == State::STATE_FINISHED || currentState == State::STATE_ANTIFASTRUSH_NEXUS_PENDING)
    {
        auto main = Map::getMyMain();
        for (const auto &gasSteal : UnitMgr::allEnemyOfType(BWAPI::Broodwar->enemy()->getRace().getRefinery()))
        {
            if (!main->hasGeyserOrRefineryAt(gasSteal->getTilePosition())) continue;

            /* Find the best cannon location */
            auto defenseLocations = BuildingPlacement::baseStaticDefenseLocations(main);
            if (!defenseLocations.isValid()) break;

            auto pylon = UnitMgr::myBuildingAt(defenseLocations.powerPylon);
            if (!pylon)
            {
                addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Pylon, defenseLocations.powerPylon, 0, PRIORITY_BASEDEFENSE);
            }

            int count = 0;
            for (const auto cannonLocation : defenseLocations.workerDefenseCannons)
            {
                if (count >= 2) break;

                auto span = Geom::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                    BWAPI::Position(cannonLocation) + BWAPI::Position(32, 32),
                                                    gasSteal->type,
                                                    gasSteal->prevPos);
                if (span > (BWAPI::UnitTypes::Protoss_Photon_Cannon.groundWeapon().maxRange() - 16)) continue;

                if (!UnitMgr::myBuildingAt(cannonLocation))
                {
                    addBuildingToGoals(prioProdGoals, BWAPI::UnitTypes::Protoss_Photon_Cannon, cannonLocation, 0, PRIORITY_BASEDEFENSE);
                }
                count++;
            }

            break;
        }
    }

    /* If the enemy has blocked our natural nexus position, build one of the natural defense cannons */
    if (nexusBlocked)
    {
        auto naturalDefensePositions = BuildingPlacement::baseStaticDefenseLocations(Map::getMyNatural());
        if (naturalDefensePositions.isValid())
        {
            if (!UnitMgr::myBuildingAt(naturalDefensePositions.powerPylon))
            {
                addBuildingToGoals(prioProdGoals,
                                   BWAPI::UnitTypes::Protoss_Pylon,
                                   naturalDefensePositions.powerPylon,
                                   0,
                                   PRIORITY_EMERGENCY);
            }
            else if (!UnitMgr::myBuildingAt(*naturalDefensePositions.workerDefenseCannons.begin()) &&
                UnitMgr::countAll(BWAPI::UnitTypes::Protoss_Forge) > 0)
            {
                addBuildingToGoals(prioProdGoals,
                                   BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                   *naturalDefensePositions.workerDefenseCannons.begin(),
                                   0,
                                   PRIORITY_EMERGENCY);
            }
        }
    }
}

void ForgeFastExpand::disband(const std::function<void(const OwnedUnit)> &removedUnitCallback,
             const std::function<void(const OwnedUnit)> &movableUnitCallback)
{
    MainArmyPlay::disband(removedUnitCallback, movableUnitCallback);
    mainBaseWorkerDefenseSquad->disband();
}

void ForgeFastExpand::updateState()
{
    auto &wall = BuildingPlacement::getForgeGatewayWall();

    /* Update the current state */
#if CVIS_LOG_STATE_CHANGES
    auto previousState = currentState;
#endif
    int counter = 0;
    while (true)
    {
        auto lastState = currentState;
        switch (currentState)
        {
            case State::STATE_UNINITIALIZED:
            case State::STATE_PYLON_PENDING:
                if (UnitMgr::myBuildingAt(wall.pylon))
                {
                    currentState = State::STATE_FORGE_PENDING;
                }
                else
                {
                    currentState = State::STATE_PYLON_PENDING;
                }
                break;

            case State::STATE_FORGE_PENDING:
                if (UnitMgr::myBuildingAt(wall.forge))
                {
                    currentState = State::STATE_NEXUS_PENDING;
                }
                break;

            case State::STATE_NEXUS_PENDING:
            {
                auto natBase = Map::getMyNatural();
                if (natBase->resourceDepot && natBase->owner == BWAPI::Broodwar->self())
                {
                    currentState = State::STATE_GATEWAY_PENDING;
                    break;
                }
                if (Director::getStrategyEngine()->isEnemyRushing() || Director::isEnemyStrategy(PvP::ProtossStrategy::ZealotAllIn))
                {
                    Constructor::cancelBase(natBase);

                    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg ||
                        proxyBehindOurWall())
                    {
                        currentState = State::STATE_ANTIFASTRUSHZERG;
                    }
                    else
                    {
                        currentState = State::STATE_ANTIFASTRUSH_GATEWAY_PENDING;
                    }
                    break;
                }
                if (nexusPositionBlocked())
                {
                    AppLog::Get() << "Nexus position blocked; building gateway first";
                    currentState = State::STATE_ANTIFASTRUSH_GATEWAY_PENDING;
                    break;
                }

                break;
            }

            case State::STATE_GATEWAY_PENDING:
            {
                auto natBase = Map::getMyNatural();
                if (Director::getStrategyEngine()->isEnemyRushing() || Director::isEnemyStrategy(PvP::ProtossStrategy::ZealotAllIn))
                {
                    Constructor::cancelBase(natBase);

                    if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
                    {
                        Constructor::cancel(wall.gateway);
                        currentState = State::STATE_ANTIFASTRUSHZERG;
                    }
                    else
                    {
                        currentState = State::STATE_ANTIFASTRUSH_GATEWAY_PENDING;
                    }
                }

                if (UnitMgr::myBuildingAt(wall.gateway))
                {
                    currentState = State::STATE_FINISHED;
                }
                break;
            }

            case State::STATE_FINISHED:
                /* Final state */
                break;

            case State::STATE_ANTIFASTRUSHZERG:
                /* Transition when we've completed the gateway in our main */
                if (UnitMgr::countCompleted(BWAPI::UnitTypes::Protoss_Gateway) > 0)
                {
                    status.transitionTo = std::make_shared<DefendMyMain>();
                }
                break;

            case State::STATE_ANTIFASTRUSH_GATEWAY_PENDING:
                if (UnitMgr::myBuildingAt(wall.gateway))
                {
                    currentState = State::STATE_ANTIFASTRUSH_NEXUS_PENDING;
                }
                break;

            case State::STATE_ANTIFASTRUSH_NEXUS_PENDING:
                auto natBase = Map::getMyNatural();
                if (natBase->resourceDepot && natBase->owner == BWAPI::Broodwar->self())
                {
                    currentState = State::STATE_FINISHED;
                }
                break;
        }
        if (lastState == currentState) break;
        counter++;
        if (counter > 20)
        {
            AppLog::Get() << "ERROR: ForgeFastExpand play unstable state, waffling between " << lastState << " and " << currentState;
            break;
        }
    }

#if CVIS_LOG_STATE_CHANGES
    if (currentState != previousState) Telemetry::log() << "ForgeFastExpand: State transition from " << previousState << " to " << currentState;
#endif
}
