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
    int worstCaseEnemyArrivalFrame()
    {
        // For PvP or PvT proxy rushes assume arrival frame of 3700
        if (Strategist::getStrategyEngine()->isEnemyProxy())
        {
            CherryVis::setBoardValue("worstCaseEnemyArrival", (std::ostringstream() << 3700).str());
            return 3700;
        }

        // Set the types we are looking for, random assumes zerg as the worst-case situation (besides proxies)
        BWAPI::UnitType builderType;
        BWAPI::UnitType unitType;
        if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
        {
            builderType = BWAPI::UnitTypes::Protoss_Gateway;
            unitType = BWAPI::UnitTypes::Protoss_Zealot;
        }
        else if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Protoss)
        {
            builderType = BWAPI::UnitTypes::Terran_Barracks;
            unitType = BWAPI::UnitTypes::Terran_Marine;
        }
        else
        {
            builderType = BWAPI::UnitTypes::Zerg_Spawning_Pool;
            unitType = BWAPI::UnitTypes::Zerg_Zergling;
        }

        // Get start frame of the building needed to produce combat units

        // If we have scouted the enemy base, default to the last time we scouted it
        // If we haven't scouted the enemy base, default to 9 pool
        int builderStartFrame;
        if (Strategist::hasWorkerScoutCompletedInitialBaseScan())
        {
            builderStartFrame = Map::getEnemyStartingMain()->lastScouted;
        }
        else
        {
            builderStartFrame = 1600;
        }

        // Add observed information about the building construction
        auto builderTimings = Units::getEnemyUnitTimings(builderType);
        if (!builderTimings.empty())
        {
            auto &[startFrame, seenFrame] = *builderTimings.begin();

            // We do not trust the start frame if we first saw the building was finished
            // In this case we assume 9 pool
            if (startFrame <= (seenFrame - UnitUtil::BuildTime(builderType)))
            {
                builderStartFrame = 1600;
            }
            else
            {
                builderStartFrame = startFrame;
            }
        }

        // Get travel time from the enemy base
        auto &wall = BuildingPlacement::getForgeGatewayWall();
        int lowestTravelTime = INT_MAX;
        for (auto &base : Map::allStartingLocations())
        {
            if (base == Map::getMyMain()) continue;
            if (Map::getEnemyStartingMain() && base != Map::getEnemyStartingMain()) continue;

            int frames = PathFinding::ExpectedTravelTime(base->getPosition(),
                                                         wall.gapCenter,
                                                         unitType,
                                                         PathFinding::PathFindingOptions::Default,
                                                         1.2,
                                                         INT_MAX);
            if (frames < lowestTravelTime) lowestTravelTime = frames;
        }

        if (lowestTravelTime == INT_MAX)
        {
            Log::Get() << "ERROR: Unable to compute enemy unit travel time to wall";
            lowestTravelTime = 650; // short rush distance on Python
        }

        // Put it all together to get the arrival time
        int arrivalTime = builderStartFrame
                          + UnitUtil::BuildTime(builderType)
                          + UnitUtil::BuildTime(unitType)
                          + lowestTravelTime;

        // If our scout is still active, use scouted zergling information as well
        if (Map::getEnemyStartingMain() && !Strategist::isWorkerScoutComplete())
        {
            auto timings = Units::getEnemyUnitTimings(unitType);
            if (timings.empty())
            {
                // Haven't seen a unit yet, compute the time if some completed now
                arrivalTime = std::max(arrivalTime, currentFrame + lowestTravelTime);
            }
            else
            {
                // Base off of the time we saw the first unit and assume it was close to the enemy main
                // If we didn't get a scout in the base early enough to see a unit, we would have overbuilt cannons anyway
                arrivalTime = std::max(arrivalTime, timings.begin()->second + lowestTravelTime);
            }
        }

        CherryVis::setBoardValue("worstCaseEnemyArrival", (std::ostringstream() << arrivalTime).str());

        return arrivalTime;
    }

    void addBuildingToGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                            BWAPI::UnitType type,
                            BWAPI::TilePosition tile,
                            int frame = 0,
                            int priority = PRIORITY_DEPOTS,
                            int framesUntilPowered = 0)
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile),
                                                              BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                               tile,
                                                                                               type),
                                                              framesUntilPowered,
                                                              0);
        prioritizedProductionGoals[priority].emplace_back(
                std::in_place_type<UnitProductionGoal>,
                "ForgeFastExpand",
                type,
                1,
                1,
                buildLocation,
                frame);
    }

    void moveWorkerProductionToLowerPriority(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                             int afterWorkers,
                                             int newPriority = PRIORITY_DEPOTS)
    {
        // Jump out if there is no probe production in progress
        auto &workerGoals = prioritizedProductionGoals[PRIORITY_WORKERS];
        if (workerGoals.empty()) return;
        for (auto &workerGoal : workerGoals)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&workerGoal);
            if (!probeGoal || !probeGoal->unitType().isWorker())
            {
                return;
            }
        }

        auto &newGoals = prioritizedProductionGoals[newPriority];

        // Get current number of probes without counting scout worker
        int currentProbes = Units::countAll(BWAPI::UnitTypes::Protoss_Probe);
        if (Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::Unstarted && !Strategist::isWorkerScoutComplete()) currentProbes--;

        // Adjust probe production at high priority to the number requested
        int probes = currentProbes;
        auto it = workerGoals.begin();
        while (it != workerGoals.end() && probes < afterWorkers)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&*it);
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

            it++;
        }

        if (it != workerGoals.end())
        {
            newGoals.insert(newGoals.end(), std::make_move_iterator(it), std::make_move_iterator(workerGoals.end()));
            workerGoals.erase(it, workerGoals.end());
        }
    }

    int buildBaseDefenseCannons(Base *base,
                                 std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                                 int count,
                                 int frame,
                                 int priority = PRIORITY_BASEDEFENSE)
    {
        if (!base) return 0;

        auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
        if (!baseStaticDefenseLocations.isValid()) return 0;

        int currentCannons = 0;
        auto cannonLocations = baseStaticDefenseLocations.workerDefenseCannons;
        for (auto it = cannonLocations.begin(); it != cannonLocations.end();)
        {
            auto cannon = Units::myBuildingAt(*it);
            if (cannon)
            {
                currentCannons++;
                it = cannonLocations.erase(it);
            }
            else
            {
                it++;
            }
        }

        if (cannonLocations.empty()) return 0;
        if (currentCannons >= count) return 0;

        int startFrame = frame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
        int powerFrame = Builder::framesUntilCompleted(
                baseStaticDefenseLocations.powerPylon,
                UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon) + 240);

        auto pylon = Units::myBuildingAt(baseStaticDefenseLocations.powerPylon);
        if (!pylon)
        {
            addBuildingToGoals(prioritizedProductionGoals,
                               BWAPI::UnitTypes::Protoss_Pylon,
                               baseStaticDefenseLocations.powerPylon,
                               startFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon),
                               priority);
        }

        if (pylon && !pylon->completed)
        {
            startFrame = std::max(startFrame, pylon->completionFrame);
        }

        int queued = 0;
        for (auto cannonLocation : cannonLocations)
        {
            addBuildingToGoals(prioritizedProductionGoals,
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

    int currentWallCannons(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
    {
        int count = 0;
        for (const auto &tile : BuildingPlacement::getForgeGatewayWall().cannons)
        {
            if (Units::myBuildingAt(tile))
            {
                count++;
                continue;
            }

            for (const auto &priorityGoals : prioritizedProductionGoals)
            {
                for (const auto &goal : priorityGoals.second)
                {
                    if (auto unitProductionGoal = std::get_if<UnitProductionGoal>(&goal))
                    {
                        if (unitProductionGoal->unitType() != BWAPI::UnitTypes::Protoss_Photon_Cannon) continue;

                        const auto& location = unitProductionGoal->getLocation();
                        if (auto buildLocation = std::get_if<BuildingPlacement::BuildLocation>(&location))
                        {
                            if (buildLocation->location.tile == tile) count++;
                        }
                    }
                }
            }
        }

        return count;
    }

    void buildWallCannons(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                          int count,
                          int frame,
                          int priority = PRIORITY_EMERGENCY)
    {
        if (count <= 0) return;

        auto &wall = BuildingPlacement::getForgeGatewayWall();

        int remaining = count;
        for (int i = 0; i < wall.cannons.size() && remaining > 0; i++)
        {
            auto tile = wall.cannons[i];
            if (Units::myBuildingAt(tile)) continue;

            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Photon_Cannon, tile, frame, priority);

            remaining--;
        }
    }

    void handleMutaRush(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
    {
        if (!Strategist::isEnemyStrategy(PvZ::ZergStrategy::MutaRush)) return;

        // Determine potential positions and frames where a muta will complete
        std::vector<std::pair<BWAPI::Position, int>> mutaPositionsAndFrames;

        // Case where we haven't seen them yet
        if (Units::countEnemy(BWAPI::UnitTypes::Zerg_Mutalisk) == 0)
        {
            int spireStartFrame = 0;
            auto spireTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Spire);
            if (!spireTimings.empty())
            {
                auto &[startFrame, seenFrame] = *spireTimings.begin();

                // We trust the start frame if we first saw it while it was incomplete
                if (startFrame > (seenFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Spire)))
                {
                    spireStartFrame = startFrame;
                }
            }
            if (spireStartFrame == 0)
            {
                auto lairTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Lair);
                if (!lairTimings.empty())
                {
                    auto &[startFrame, seenFrame] = *lairTimings.begin();

                    // We trust the start frame if we first saw it while it was incomplete
                    if (startFrame > (seenFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Lair)))
                    {
                        spireStartFrame = startFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Lair);
                    }
                }
            }

            // If we don't have data now, we didn't scout the spire or lair while they were incomplete
            // So default to a muta completing now
            int mutaCompletionFrame;
            if (spireStartFrame == 0)
            {
                mutaCompletionFrame = currentFrame;
            }
            else
            {
                mutaCompletionFrame =
                        spireStartFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Spire) + UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Mutalisk);
            }

            // Add one muta from each known enemy hatchery or lair
            for (const auto &hatch : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Hatchery))
            {
                if (hatch->completed)
                {
                    mutaPositionsAndFrames.emplace_back(hatch->lastPosition, mutaCompletionFrame);
                }
            }
            for (const auto &lair : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Lair))
            {
                mutaPositionsAndFrames.emplace_back(lair->lastPosition, mutaCompletionFrame);
            }
        }
        else
        {
            // Add each muta's last position
            for (const auto &muta : Units::allEnemyOfType(BWAPI::UnitTypes::Zerg_Mutalisk))
            {
                mutaPositionsAndFrames.emplace_back(muta->lastPosition, muta->lastSeen);
            }
        }

        // Now determine when the earliest muta could reach the main and natural
        auto frameForPosition = [&mutaPositionsAndFrames](BWAPI::Position target)
        {
            int earliestArrivalFrame = INT_MAX;
            for (const auto &[pos, frame] : mutaPositionsAndFrames)
            {
                int arrivalFrame = frame + PathFinding::ExpectedTravelTime(pos, target, BWAPI::UnitTypes::Zerg_Mutalisk);
                if (arrivalFrame < earliestArrivalFrame)
                {
                    earliestArrivalFrame = arrivalFrame;
                }
            }
            return earliestArrivalFrame;
        };
        buildBaseDefenseCannons(Map::getMyMain(), prioritizedProductionGoals, 2, frameForPosition(Map::getMyMain()->getPosition()));
        buildBaseDefenseCannons(Map::getMyNatural(), prioritizedProductionGoals, 1, frameForPosition(Map::getMyNatural()->getPosition()));

        // Add a second cannon at the wall
        int currentCannons = currentWallCannons(prioritizedProductionGoals);
        if (currentCannons < 2)
        {
            buildWallCannons(prioritizedProductionGoals,
                             2 - currentCannons,
                             frameForPosition(Map::getMyNatural()->getPosition()) - 100,
                             PRIORITY_BASEDEFENSE);
        }
    }

    void handleZerglingAllIn(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
    {
        if (!Strategist::isEnemyStrategy(PvZ::ZergStrategy::ZerglingAllIn)) return;

        int currentCannons = currentWallCannons(prioritizedProductionGoals);
        if (currentCannons < 2) buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, 0);
        if (currentCannons < 3) buildWallCannons(prioritizedProductionGoals, 1, 4000);
        if (currentCannons < 4) buildWallCannons(prioritizedProductionGoals, 1, 5000);
        if (currentCannons < 5) buildWallCannons(prioritizedProductionGoals, 1, 6000);
    }

    void handleHydraBust(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
    {
        if (!Strategist::isEnemyStrategy(PvZ::ZergStrategy::HydraBust)) return;

        int currentCannons = currentWallCannons(prioritizedProductionGoals);
        if (currentCannons < 2) buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, 5000);
        if (currentCannons < 3) buildWallCannons(prioritizedProductionGoals, 1, 6000);
        if (currentCannons < 4) buildWallCannons(prioritizedProductionGoals, 1, 7000);
        if (currentCannons < 5) buildWallCannons(prioritizedProductionGoals, 1, 8000);
        if (currentCannons < 6) buildWallCannons(prioritizedProductionGoals, 1, 9000);
    }

    void handleNonZergRush(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
    {
        int baseFrame = 4000 - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
        if (Strategist::isEnemyStrategy(PvP::ProtossStrategy::ProxyRush) ||
            Strategist::isEnemyStrategy(PvT::TerranStrategy::ProxyRush))
        {
            baseFrame -= 500;
        }
        else if (!Strategist::isEnemyStrategy(PvP::ProtossStrategy::ZealotRush) &&
            !Strategist::isEnemyStrategy(PvT::TerranStrategy::MarineRush))
        {
            return;
        }

        int currentCannons = currentWallCannons(prioritizedProductionGoals);
        if (currentCannons < 2) buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, baseFrame);
        if (currentCannons < 3) buildWallCannons(prioritizedProductionGoals, 1, baseFrame + 1000);
        if (currentCannons < 4) buildWallCannons(prioritizedProductionGoals, 1, baseFrame + 2000);

        if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Cybernetics_Core) == 0)
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    "ForgeFastExpand",
                    BWAPI::UnitTypes::Protoss_Zealot,
                    1,
                    1);
        }
    }
}

ForgeFastExpand::ForgeFastExpand()
        : MainArmyPlay("ForgeFastExpand")
        , currentState(State::STATE_PYLON_PENDING)
        , squad(std::make_shared<DefendWallSquad>())
        , mainBaseWorkerDefenseSquad(std::make_unique<WorkerDefenseSquad>(Map::getMyMain()))
{
    General::addSquad(squad);
}

void ForgeFastExpand::update()
{
    // Perform worker defense in main base
    std::set<Unit> enemyUnits(Units::enemyAtBase(Map::getMyMain()).begin(), Units::enemyAtBase(Map::getMyMain()).end());
    auto workersAndTargets = mainBaseWorkerDefenseSquad->selectTargets(enemyUnits);
    std::vector<std::pair<MyUnit, Unit>> emptyUnitsAndTargets;
    mainBaseWorkerDefenseSquad->execute(workersAndTargets, emptyUnitsAndTargets);

    auto &wall = BuildingPlacement::getForgeGatewayWall();

    // Update the current state
    switch (currentState)
    {
        case State::STATE_PYLON_PENDING:
            if (Units::myBuildingAt(wall.pylon))
            {
                currentState = State::STATE_FORGE_PENDING;
            }
            break;

        case State::STATE_FORGE_PENDING:
            if (Units::myBuildingAt(wall.forge))
            {
                currentState = State::STATE_NEXUS_PENDING;
            }
            break;

        case State::STATE_NEXUS_PENDING:
        {
            auto natural = Map::getMyNatural();
            if (Strategist::getStrategyEngine()->isEnemyRushing())
            {
                Builder::cancelBase(natural);

                if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
                {
                    currentState = State::STATE_ANTIFASTRUSHZERG;
                }
                else
                {
                    currentState = State::STATE_ANTIFASTRUSH_GATEWAY_PENDING;
                }
            }
            if (natural->resourceDepot && natural->resourceDepot->exists() && natural->owner == BWAPI::Broodwar->self())
            {
                currentState = State::STATE_GATEWAY_PENDING;
            }
            break;
        }

        case State::STATE_GATEWAY_PENDING:
        {
            auto natural = Map::getMyNatural();
            if (Strategist::getStrategyEngine()->isEnemyRushing())
            {
                Builder::cancelBase(natural);

                if (BWAPI::Broodwar->enemy()->getRace() == BWAPI::Races::Zerg)
                {
                    Builder::cancel(wall.gateway);
                    currentState = State::STATE_ANTIFASTRUSHZERG;
                }
                else
                {
                    currentState = State::STATE_ANTIFASTRUSH_GATEWAY_PENDING;
                }
            }

            if (Units::myBuildingAt(wall.gateway))
            {
                currentState = State::STATE_FINISHED;
            }
            break;
        }

        case State::STATE_FINISHED:
            // Final state
            break;

        case State::STATE_ANTIFASTRUSHZERG:
            // Transition when we've completed the gateway in our main
            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Gateway) > 0)
            {
                status.transitionTo = std::make_shared<DefendMyMain>();
            }
            break;

        case State::STATE_ANTIFASTRUSH_GATEWAY_PENDING:
            if (Units::myBuildingAt(wall.gateway))
            {
                currentState = State::STATE_ANTIFASTRUSH_NEXUS_PENDING;
            }
            break;

        case State::STATE_ANTIFASTRUSH_NEXUS_PENDING:
            auto natural = Map::getMyNatural();
            if (natural->resourceDepot && natural->resourceDepot->exists() && natural->owner == BWAPI::Broodwar->self())
            {
                currentState = State::STATE_FINISHED;
            }
            break;
    }
}

void ForgeFastExpand::addPrioritizedProductionGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals)
{
    /*
     * For all of the early states, this generates the current build order based on the information we have on the enemy.
     * When this is called, we expect the SaturateBases play to have added its probe goal, but nothing else to be added yet.
     */
    
    auto &wall = BuildingPlacement::getForgeGatewayWall();

    // Common timing-based logic needed by multiple states
    int enemyArrivalFrame = worstCaseEnemyArrivalFrame();
    int cannonFrame = enemyArrivalFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);
    int currentCannons = currentWallCannons(prioritizedProductionGoals);

    switch (currentState)
    {
        case State::STATE_PYLON_PENDING:
        case State::STATE_FORGE_PENDING:
        {
            if (currentState == State::STATE_PYLON_PENDING)
            {
                addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Pylon, wall.pylon);
            }
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            buildWallCannons(prioritizedProductionGoals, 2, cannonFrame, PRIORITY_DEPOTS);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            break;
        }
        case State::STATE_NEXUS_PENDING:
        {
            buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, cannonFrame);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            moveWorkerProductionToLowerPriority(prioritizedProductionGoals, 14);
            break;
        }
        case State::STATE_GATEWAY_PENDING:
        {
            buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, cannonFrame);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            moveWorkerProductionToLowerPriority(prioritizedProductionGoals, 14);
            break;
        }
        case State::STATE_FINISHED:
        {
            buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, cannonFrame);

            // Handle additional cannons based on enemy strategy
            handleMutaRush(prioritizedProductionGoals);
            handleZerglingAllIn(prioritizedProductionGoals);
            handleHydraBust(prioritizedProductionGoals);
            handleNonZergRush(prioritizedProductionGoals);

            // If the enemy did a gas steal, build a cannon in our main to kill it
            auto main = Map::getMyMain();
            for (const auto &gasSteal : Units::allEnemyOfType(BWAPI::Broodwar->enemy()->getRace().getRefinery()))
            {
                if (!main->hasGeyserAt(gasSteal->getTilePosition())) continue;

                // Find the best cannon location
                auto defenseLocations = BuildingPlacement::baseStaticDefenseLocations(main);
                if (!defenseLocations.powerPylon.isValid()) break;

                auto pylon = Units::myBuildingAt(defenseLocations.powerPylon);
                if (!pylon || !pylon->completed) break;

                for (const auto cannonLocation : defenseLocations.workerDefenseCannons)
                {
                    auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                        BWAPI::Position(cannonLocation) + BWAPI::Position(32, 32),
                                                        gasSteal->type,
                                                        gasSteal->lastPosition);
                    if (dist > (BWAPI::UnitTypes::Protoss_Photon_Cannon.groundWeapon().maxRange() - 16)) continue;

                    if (Units::myBuildingAt(cannonLocation)) break;
                    addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Photon_Cannon, cannonLocation, 0, PRIORITY_BASEDEFENSE);
                }

                break;
            }

            break;
        }
        case State::STATE_ANTIFASTRUSHZERG:
        {
            // Basic idea is to build a pylon and cannons in the main, then add a gateway
            auto defenseLocations = BuildingPlacement::baseStaticDefenseLocations(Map::getMyMain());
            if (!defenseLocations.powerPylon.isValid() || defenseLocations.workerDefenseCannons.empty())
            {
                status.transitionTo = std::make_shared<DefendMyMain>();
                break;
            }

            int powerFrame = buildBaseDefenseCannons(Map::getMyMain(), prioritizedProductionGoals, 2, 0, PRIORITY_EMERGENCY);

            if (defenseLocations.startBlockCannon.isValid() && !Units::myBuildingAt(defenseLocations.startBlockCannon))
            {
                addBuildingToGoals(prioritizedProductionGoals,
                                   BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                   defenseLocations.startBlockCannon,
                                   0,
                                   PRIORITY_EMERGENCY,
                                   powerFrame);
            }

            prioritizedProductionGoals[PRIORITY_EMERGENCY].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    label,
                    BWAPI::UnitTypes::Protoss_Zealot,
                    1,
                    1);

            break;
        }
        case State::STATE_ANTIFASTRUSH_GATEWAY_PENDING:
        {
            buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, cannonFrame);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway, 0, PRIORITY_EMERGENCY);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            moveWorkerProductionToLowerPriority(prioritizedProductionGoals, 16);
            break;
        }
        case State::STATE_ANTIFASTRUSH_NEXUS_PENDING:
        {
            buildWallCannons(prioritizedProductionGoals, 2 - currentCannons, cannonFrame);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            moveWorkerProductionToLowerPriority(prioritizedProductionGoals, 16);
            break;
        }
    }
}

void ForgeFastExpand::disband(const std::function<void(const MyUnit)> &removedUnitCallback,
             const std::function<void(const MyUnit)> &movableUnitCallback)
{
    MainArmyPlay::disband(removedUnitCallback, movableUnitCallback);
    mainBaseWorkerDefenseSquad->disband();
}