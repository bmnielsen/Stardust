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

// Thresholds that determine our cannon timings, if lings can arrive before these frames we build (a) cannon(s)
#define LINGARRIVAL_CANNONSBEFORENEXUS 4500
#define LINGARRIVAL_ONECANNONBEFOREGATEWAY 5000

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
    int worstCaseZerglingArrivalFrame()
    {
        // Get pool start frame
        // If we have scouted the enemy base, defaults to now, as we assume the enemy hasn't built it yet
        // If we haven't scouted the enemy base, defaults to 9 pool
        int poolStartFrame;
        if (Strategist::hasWorkerScoutCompletedInitialBaseScan())
        {
            poolStartFrame = currentFrame;
        }
        else
        {
            poolStartFrame = 1600;
        }

        auto poolTimings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Spawning_Pool);
        if (!poolTimings.empty())
        {
            auto &[startFrame, seenFrame] = *poolTimings.begin();

            // We do not trust the start frame if we first saw the pool after it was finished
            // In this case we assume 9 pool
            if (startFrame <= (seenFrame - UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Spawning_Pool)))
            {
                poolStartFrame = 1600;
            }
            else
            {
                poolStartFrame = startFrame;
            }
        }

        auto &wall = BuildingPlacement::getForgeGatewayWall();
        int lowestTravelTime = INT_MAX;
        for (auto &base : Map::allStartingLocations())
        {
            if (base == Map::getMyMain()) continue;
            if (Map::getEnemyStartingMain() && base != Map::getEnemyStartingMain()) continue;

            int frames = PathFinding::ExpectedTravelTime(base->getPosition(),
                                                         wall.gapCenter,
                                                         BWAPI::UnitTypes::Zerg_Zergling,
                                                         PathFinding::PathFindingOptions::Default,
                                                         1.2,
                                                         INT_MAX);
            if (frames < lowestTravelTime) lowestTravelTime = frames;
        }

        if (lowestTravelTime == INT_MAX)
        {
            Log::Get() << "ERROR: Unable to compute zergling travel time to wall";
            lowestTravelTime = 650; // short rush distance on Python
        }

        int arrivalTime = poolStartFrame
                          + UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Spawning_Pool)
                          + UnitUtil::BuildTime(BWAPI::UnitTypes::Zerg_Zergling)
                          + lowestTravelTime;

        // If our scout is still active, use scouted zergling information as well
        if (Map::getEnemyStartingMain() && !Strategist::isWorkerScoutComplete())
        {
            auto timings = Units::getEnemyUnitTimings(BWAPI::UnitTypes::Zerg_Zergling);
            if (timings.empty())
            {
                // Haven't seen a ling yet, compute the time if some popped now
                arrivalTime = std::max(arrivalTime, currentFrame + lowestTravelTime);
            }
            else
            {
                // Base off of the time we saw the first ling and assume it was close to the enemy main
                // If we didn't get a scout in the base early enough to see a ling, we would have overbuilt cannons anyway
                arrivalTime = std::max(arrivalTime, timings.begin()->second + lowestTravelTime);
            }
        }

        CherryVis::setBoardValue("worstCaseLingArrival", (std::ostringstream() << arrivalTime).str());

        return arrivalTime;
    }

    void addBuildingToGoals(std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                            BWAPI::UnitType type,
                            BWAPI::TilePosition tile,
                            int priority = PRIORITY_DEPOTS)
    {
        auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile),
                                                              BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                               tile,
                                                                                               type),
                                                              0,
                                                              0);
        prioritizedProductionGoals[priority].emplace_back(
                std::in_place_type<UnitProductionGoal>,
                "ForgeFastExpand",
                type,
                1,
                1,
                buildLocation);
    };

    void buildAirDefenseCannons(Base *base, std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals, int count, int frame)
    {
        if (!base) return;

        auto &baseStaticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
        if (!baseStaticDefenseLocations.isValid()) return;

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

        if (cannonLocations.empty()) return;
        if (currentCannons >= count) return;

        // TODO: When producer can support it, order the buildings at the correct frame

        int startFrame = frame - UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon);

        auto pylon = Units::myBuildingAt(baseStaticDefenseLocations.powerPylon);
        if (!pylon)
        {
            startFrame -= UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Pylon);
        }

        if (currentFrame > (startFrame - 500))
        {
            if (!pylon)
            {
                addBuildingToGoals(prioritizedProductionGoals,
                                   BWAPI::UnitTypes::Protoss_Pylon,
                                   baseStaticDefenseLocations.powerPylon,
                                   PRIORITY_BASEDEFENSE);
            }

            int queued = 0;
            for (auto cannonLocation : cannonLocations)
            {
                addBuildingToGoals(prioritizedProductionGoals,
                                   BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                   cannonLocation,
                                   PRIORITY_BASEDEFENSE);
                queued++;
                if ((queued + currentCannons) >= count) break;
            }
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
        auto frameForBase = [&mutaPositionsAndFrames](Base *base)
        {
            int earliestArrivalFrame = INT_MAX;
            for (const auto &[pos, frame] : mutaPositionsAndFrames)
            {
                int arrivalFrame = frame + PathFinding::ExpectedTravelTime(pos, base->getPosition(), BWAPI::UnitTypes::Zerg_Mutalisk);
                if (arrivalFrame < earliestArrivalFrame)
                {
                    earliestArrivalFrame = arrivalFrame;
                }
            }
            return earliestArrivalFrame;
        };
        buildAirDefenseCannons(Map::getMyMain(), prioritizedProductionGoals, 2, frameForBase(Map::getMyMain()));
        buildAirDefenseCannons(Map::getMyNatural(), prioritizedProductionGoals, 1, frameForBase(Map::getMyNatural()));
    }

    int antiZerglingAllInCannons()
    {
        if (!Strategist::isEnemyStrategy(PvZ::ZergStrategy::ZerglingAllIn)) return 0;

        // TODO
        return 0;
    }

    int antiHydraBustCannons()
    {
        if (!Strategist::isEnemyStrategy(PvZ::ZergStrategy::HydraBust)) return 0;

        // TODO
        return 0;
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
                currentState = State::STATE_ANTIFASTRUSH;
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
                Builder::cancel(wall.gateway);
                currentState = State::STATE_ANTIFASTRUSH;
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

        case State::STATE_ANTIFASTRUSH:
            // Transition when we've completed the gateway in our main
            if (Units::countCompleted(BWAPI::UnitTypes::Protoss_Gateway) > 0)
            {
                status.transitionTo = std::make_shared<DefendMyMain>();
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

    auto addCannons = [&](int desiredCount, int priority = PRIORITY_DEPOTS)
    {
        int currentCannons = 0;
        for (int i = 0; i < wall.cannons.size() && i < desiredCount; i++)
        {
            auto tile = wall.cannons[i];
            if (Units::myBuildingAt(tile))
            {
                currentCannons++;
                continue;
            }

            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(tile),
                                                                  BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                                   tile,
                                                                                                   BWAPI::UnitTypes::Protoss_Photon_Cannon),
                                                                  0,
                                                                  0);
            prioritizedProductionGoals[priority].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    label,
                    BWAPI::UnitTypes::Protoss_Photon_Cannon,
                    1,
                    1,
                    buildLocation);
        }
        return currentCannons;
    };
    auto addUnits = [&](BWAPI::UnitType type, int desiredCount, int priority = PRIORITY_DEPOTS, int producers = 1)
    {
        int currentCount = Units::countAll(type);
        if (desiredCount > currentCount || desiredCount == -1)
        {
            prioritizedProductionGoals[priority].emplace_back(
                    std::in_place_type<UnitProductionGoal>,
                    label,
                    type,
                    (desiredCount == -1) ? -1 : (desiredCount - currentCount),
                    producers);
        }
        return currentCount;
    };
    auto pauseProbeProduction = [&](int probeCountToPauseAt, int newPriority = PRIORITY_DEPOTS)
    {
        // Abort if there is no probe production in progress
        auto &workerGoals = prioritizedProductionGoals[PRIORITY_WORKERS];
        if (workerGoals.empty()) return;
        for (auto &workerGoal : workerGoals)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&workerGoal);
            if (!probeGoal || !probeGoal->unitType().isWorker())
            {
                Log::Get() << "ERROR: Non-probe production goal in worker priority queue; unable to pause probe production";
                return;
            }
        }

        auto &newGoals = prioritizedProductionGoals[newPriority];

        // Get number of probes without counting scout worker
        int currentProbes = Units::countAll(BWAPI::UnitTypes::Protoss_Probe);
        if (Strategist::getWorkerScoutStatus() != Strategist::WorkerScoutStatus::Unstarted && !Strategist::isWorkerScoutComplete()) currentProbes--;

        // Move the required amount of probe production to a lower priority
        int probes = currentProbes;
        auto it = workerGoals.begin();
        while (it != workerGoals.end() && probes < probeCountToPauseAt)
        {
            auto probeGoal = std::get_if<UnitProductionGoal>(&*it);
            probes += probeGoal->countToProduce();
            if (probes > probeCountToPauseAt)
            {
                probeGoal->setCountToProduce(probeCountToPauseAt - (probes - probeGoal->countToProduce()));

                newGoals.emplace_back(std::in_place_type<UnitProductionGoal>,
                                      probeGoal->requester,
                                      BWAPI::UnitTypes::Protoss_Probe,
                                      probes - probeCountToPauseAt,
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
    };

    switch (currentState)
    {
        case State::STATE_PYLON_PENDING:
        case State::STATE_FORGE_PENDING:
            // Initially we assume no scouting information and plan for two cannons
            if (currentState == State::STATE_PYLON_PENDING)
            {
                addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Pylon, wall.pylon);
            }
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Forge, wall.forge);
            addCannons(2);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            pauseProbeProduction(14);
            addUnits(BWAPI::UnitTypes::Protoss_Corsair, 1);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        case State::STATE_NEXUS_PENDING:
        {
            bool buildCannonsBeforeNexus = worstCaseZerglingArrivalFrame() < LINGARRIVAL_CANNONSBEFORENEXUS;
            if (buildCannonsBeforeNexus) addCannons(2);
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Nexus, Map::getMyNatural()->getTilePosition());
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            if (buildCannonsBeforeNexus) pauseProbeProduction(14);
            addUnits(BWAPI::UnitTypes::Protoss_Corsair, 1);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        }
        case State::STATE_GATEWAY_PENDING:
        {
            bool cannonBeforeGateway = false;
            int lingArrivalFrame = worstCaseZerglingArrivalFrame();
            if (lingArrivalFrame < LINGARRIVAL_CANNONSBEFORENEXUS)
            {
                cannonBeforeGateway = true;
                addCannons(2);
            } else if (lingArrivalFrame < LINGARRIVAL_ONECANNONBEFOREGATEWAY)
            {
                cannonBeforeGateway = true;
                addCannons(1);
            }
            addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Gateway, wall.gateway);
            if (cannonBeforeGateway) pauseProbeProduction(14);
            addUnits(BWAPI::UnitTypes::Protoss_Corsair, 1);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);
            break;
        }
        case State::STATE_FINISHED:
        {
            // Determine cannons needed
            int cannonCount = 1;

            // Add a second cannon if we need it before a zealot is out
            int lingArrivalFrame = worstCaseZerglingArrivalFrame();
            if (lingArrivalFrame < (currentFrame + UnitUtil::BuildTime(BWAPI::UnitTypes::Protoss_Photon_Cannon)) &&
                Units::countAll(BWAPI::UnitTypes::Protoss_Zealot) == 0)
            {
                cannonCount = 2;
            }

            // Scale up cannons if needed based on enemy strategy
            cannonCount = std::max(cannonCount, antiZerglingAllInCannons());
            cannonCount = std::max(cannonCount, antiHydraBustCannons());

            addCannons(cannonCount);

            addUnits(BWAPI::UnitTypes::Protoss_Corsair, 1);
            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 2);

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
                    auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(cannonLocation),
                                                                          BuildingPlacement::builderFrames(Map::getMyMain()->getPosition(),
                                                                                                           cannonLocation,
                                                                                                           BWAPI::UnitTypes::Protoss_Photon_Cannon),
                                                                          0,
                                                                          0);
                    prioritizedProductionGoals[PRIORITY_BASEDEFENSE].emplace_back(
                            std::in_place_type<UnitProductionGoal>,
                            label,
                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                            1,
                            1,
                            buildLocation);
                }

                break;
            }

            // Adds air defense cannons at the bases if needed
            handleMutaRush(prioritizedProductionGoals);

            break;
        }
        case State::STATE_ANTIFASTRUSH:
        {
            // Basic idea is to build a pylon, cannon and gateway in the main
            auto defenseLocations = BuildingPlacement::baseStaticDefenseLocations(Map::getMyMain());
            if (!defenseLocations.powerPylon.isValid() || defenseLocations.workerDefenseCannons.empty())
            {
                status.transitionTo = std::make_shared<DefendMyMain>();
                break;
            }

            auto pylon = Units::myBuildingAt(defenseLocations.powerPylon);
            if (!pylon)
            {
                addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Pylon, defenseLocations.powerPylon, PRIORITY_EMERGENCY);
            }

            auto cannonLocation = *defenseLocations.workerDefenseCannons.begin();
            if (!Units::myBuildingAt(cannonLocation))
            {
                addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Photon_Cannon, cannonLocation, PRIORITY_EMERGENCY);
            }

            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 1, PRIORITY_EMERGENCY);

            if (defenseLocations.startBlockCannon.isValid() && !Units::myBuildingAt(defenseLocations.startBlockCannon))
            {
                addBuildingToGoals(prioritizedProductionGoals, BWAPI::UnitTypes::Protoss_Photon_Cannon, defenseLocations.startBlockCannon, PRIORITY_EMERGENCY);
            }

            addUnits(BWAPI::UnitTypes::Protoss_Zealot, 10, PRIORITY_EMERGENCY, 2);
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