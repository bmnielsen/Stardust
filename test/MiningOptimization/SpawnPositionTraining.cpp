#include "BWTest.h"

#include "DoNothingModule.h"
#include "ClearOpponentUnitsModule.h"
#include "DoNothingStrategyEngine.h"
#include "StardustAIModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"
#include "WorkerMiningInstrumentation.h"
#include "MiningOptimization/WorkerMiningOptimization.h"
#include "Units.h"
#include "Workers.h"
#include "BuildingPlacement.h"
#include "Geo.h"

#include <algorithm>
#include <random>

// This file is used to train paths to each patch from the worker spawn positions
// It supports training of paths from the initial worker spawn points at game start and the spawn locations from the depot
// For the spawn locations from the depot, it supports both with and without cannons and other buildings
// Training is done iteratively:
// - First we explore just the initial path to the patch
// - Then, once we have collected enough data to know the optimal path to the patch, we explore the next 3 collections
namespace
{
    struct TrainingCase
    {
        BWAPI::Position spawnPosition;
        Resource resource;
        Base *base;

        // States:
        // 0: waiting to create worker
        // 1: waiting for worker to be created
        // 2: worker mining on first round-trip
        // 3: worker mining on second round-trip
        int state = 0;

        // The frame of the last state change
        int stateChangeFrame = -1;

        // Reference to the worker
        MyWorker worker = nullptr;

        void reset()
        {
            state = 0;
            stateChangeFrame = -1;
            worker = nullptr;
        }

        void setState(int newState)
        {
            state = newState;
            stateChangeFrame = BWAPI::Broodwar->getFrameCount();
        }
    };

    void runSpawnLocationsTest(BWTest &test, bool startingWorkers, int cannons, bool filledStartBlock)
    {
        BuildingPlacement::setUseStartBlocksForAllStartingLocations(true);

        std::cout << "Starting spawn locations training with following parameters"
                  << ": startingWorkers=" << startingWorkers
                  << "; cannons=" << cannons
                  << "; filledStartBlock=" << filledStartBlock
                  << std::endl;
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };
        test.myModule = []()
        {
            auto module = new StardustAIModule();
            module->enableFrameLimit = false;
            return module;
        };
#if INSTRUMENTATION_ENABLED_VERBOSE
        test.frameLimit = 1000;
#else
        test.frameLimit = 100000;
#endif
        test.expectWin = false;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "MiningTraining_" << test.map->shortname() << "_";
        if (startingWorkers)
        {
            replayNameBuilder << "startingWorkers";
        }
        else
        {
            replayNameBuilder << "buildSpawn_" << cannons << "cannons";
            if (filledStartBlock) replayNameBuilder << "_filledStartBlock";
        }
        test.replayName = replayNameBuilder.str();

        WorkerMiningOptimization::setExploring(true);
        Units::setLogUnitsCreatedAndLost(false);

        test.onStartMine = []()
        {
            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

            // Add a dummy main army play since one is needed
            std::vector<std::shared_ptr<Play>> openingPlays;
            openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(Map::getMyMain()));
            Strategist::setOpening(openingPlays);
        };

        std::list<std::shared_ptr<TrainingCase>> trainingCases;
        std::set<Resource> patchesInUse;
        std::set<Base*> basesWithWorkerCreationPending;
        test.onFrameMine = [&]()
        {
            // Ensure all mineral patches keep enough minerals
            for (auto unit : BWAPI::Broodwar->getNeutralUnits())
            {
                if (!unit->getType().isMineralField()) continue;
                if (unit->getResources() < 100) unit->setResources(1500);
            }

            // Initialization steps:
            // - Kill initial workers, blocking neutrals and critters
            // - Add observers at expansions
            // - Add depots at expansions
            // - If testing with buildings, add them at each base (pylon, then forge or start block including forge, then cannon(s))
            if (BWAPI::Broodwar->getFrameCount() == 0)
            {
                BWAPI::Broodwar->self()->setMinerals((int)(50 * Map::allBases().size()));
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker())
                    {
                        BWAPI::Broodwar->killUnit(unit);
                    }
                }

                for (auto base : (startingWorkers ? Map::allStartingLocations() : Map::allBases()))
                {
                    for (const auto &blockingNeutral : base->blockingNeutrals)
                    {
                        BWAPI::Broodwar->killUnit(blockingNeutral);
                    }
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, base->getPosition());
                }

                for (auto unit : BWAPI::Broodwar->getNeutralUnits())
                {
                    if (unit->getType().isCritter()) BWAPI::Broodwar->killUnit(unit);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 10)
            {
                for (auto base : (startingWorkers ? Map::allStartingLocations() : Map::allBases()))
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Nexus,
                                                Geo::CenterOfUnit(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus));

                    if (!startingWorkers && (cannons || filledStartBlock))
                    {
                        auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                        if (staticDefenseLocations.powerPylon.isValid())
                        {
                            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                        BWAPI::UnitTypes::Protoss_Pylon,
                                                        Geo::CenterOfUnit(staticDefenseLocations.powerPylon, BWAPI::UnitTypes::Protoss_Pylon));
                        }
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 20)
            {
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                    {
                        BWAPI::Broodwar->killUnit(unit);
                    }
                }

                if (!startingWorkers)
                {
                    if (filledStartBlock)
                    {
                        for (auto base : Map::allStartingLocations())
                        {
                            auto startBlock = BuildingPlacement::startBlockForBase(base);
                            if (!startBlock) continue;

                            auto create = [](std::vector<Block::Location> &locations, BWAPI::UnitType type)
                            {
                                for (auto &location : locations)
                                {
                                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), type, Geo::CenterOfUnit(location.tile, type));
                                }
                            };
                            create(startBlock->large, BWAPI::UnitTypes::Protoss_Gateway);
                            create(startBlock->medium, BWAPI::UnitTypes::Protoss_Forge);
                            create(startBlock->small, BWAPI::UnitTypes::Protoss_Pylon);
                        }
                    }
                    else if (cannons > 0)
                    {
                        auto &locations = BuildingPlacement::getBuildLocations()[to_underlying(BuildingPlacement::Neighbourhood::MainBase)][3];
                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                    BWAPI::UnitTypes::Protoss_Forge,
                                                    Geo::CenterOfUnit(locations.begin()->location.tile, BWAPI::UnitTypes::Protoss_Forge));
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 30 && !startingWorkers)
            {
                if (filledStartBlock && cannons == 2)
                {
                    for (auto base : Map::allStartingLocations())
                    {
                        auto startBlock = BuildingPlacement::startBlockForBase(base);
                        if (!startBlock) continue;

                        for (auto &tile : startBlock->cannons)
                        {
                            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                        BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                        Geo::CenterOfUnit(tile, BWAPI::UnitTypes::Protoss_Photon_Cannon));
                        }
                    }
                }

                for (auto base : Map::allBases())
                {
                    auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                    if (staticDefenseLocations.powerPylon.isValid())
                    {
                        auto cannonLocations = std::set<BWAPI::TilePosition>(staticDefenseLocations.workerDefenseCannons.begin(),
                                staticDefenseLocations.workerDefenseCannons.end());

                        auto buildCannon = [&]()
                        {
                            BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                            int bestDist = INT_MAX;
                            for (auto tile : cannonLocations)
                            {
                                int dist = base->mineralLineCenter.getApproxDistance(Geo::CenterOfUnit(tile,
                                                                                                       BWAPI::UnitTypes::Protoss_Photon_Cannon));
                                if (dist < bestDist)
                                {
                                    bestDist = dist;
                                    best = tile;
                                }
                            }

                            if (best != BWAPI::TilePositions::Invalid)
                            {
                                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                            Geo::CenterOfUnit(best, BWAPI::UnitTypes::Protoss_Photon_Cannon));
                                cannonLocations.erase(best);
                            }
                        };

                        for (int builtCannons = 0; builtCannons < cannons; builtCannons++)
                        {
                            buildCannon();
                        }
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 40 && !startingWorkers)
            {
                // Create a worker at each base to see where it spawns
                for (auto &nexus : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Nexus))
                {
                    nexus->train(BWAPI::UnitTypes::Protoss_Probe);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 345 && !startingWorkers)
            {
                auto spawnedWorkers = Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe);

                // Create the training cases
                for (auto &base : Map::allBases())
                {
                    // Find the closest worker
                    int bestDist = INT_MAX;
                    MyUnit best = nullptr;
                    for (const auto &worker : spawnedWorkers)
                    {
                        int dist = worker->getDistance(base->getPosition());
                        if (dist < bestDist && dist < 200)
                        {
                            bestDist = dist;
                            best = worker;
                        }
                    }
                    if (!best)
                    {
                        std::cout << "ERROR: Could not find worker near base @ " << base->getTilePosition() << std::endl;
                        continue;
                    }

                    for (auto &resource : base->mineralPatches())
                    {
                        trainingCases.emplace_back(std::make_shared<TrainingCase>(best->lastPosition, resource, base));
                    }

                    BWAPI::Broodwar->killUnit(best->bwapiUnit);
                    spawnedWorkers.erase(best);
                }

                for (auto &remainingWorker : spawnedWorkers)
                {
                    std::cout << "ERROR: Unpaired worker @ " << remainingWorker->getTilePosition() << std::endl;
                    BWAPI::Broodwar->killUnit(remainingWorker->bwapiUnit);
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 30 && startingWorkers)
            {
                for (auto &base : Map::allStartingLocations())
                {
                    for (auto &spawnLocation : Map::mapSpecificOverride()->startingWorkerPositions(base->getTilePosition()))
                    {
                        for (auto &resource : base->mineralPatches())
                        {
                            trainingCases.emplace_back(std::make_shared<TrainingCase>(spawnLocation, resource, base));
                        }
                    }
                }
            }

            if (trainingCases.empty()) return;

            // Run the state machine for each training case
            std::list<std::shared_ptr<TrainingCase>> completedCases;
            for (auto it = trainingCases.begin(); it != trainingCases.end(); )
            {
                auto &trainingCase = **it;
                switch (trainingCase.state)
                {
                    case 0:
                    {
                        // We are ready to create a new unit if the situation allows it

                        // Jump out if any training case is waiting for their worker to be created
                        if (basesWithWorkerCreationPending.contains(trainingCase.base)) break;

                        // Jump out if another worker is already working on this patch
                        if (patchesInUse.contains(trainingCase.resource)) break;

                        // Check if any worker will potentially block the spawn location
                        bool potentiallyBlocked = false;
                        for (const auto &worker : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
                        {
                            if (Geo::EdgeToEdgeDistance(worker->type,
                                                        worker->lastPosition,
                                                        BWAPI::UnitTypes::Protoss_Probe,
                                                        trainingCase.spawnPosition) < 20)
                            {
                                potentiallyBlocked = true;
                                break;
                            }
                        }
                        if (potentiallyBlocked) break;

                        // Create the worker
                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                    BWAPI::UnitTypes::Protoss_Probe,
                                                    trainingCase.spawnPosition);
                        trainingCase.setState(1);
                        basesWithWorkerCreationPending.insert(trainingCase.base);
                        break;
                    }
                    case 1:
                    {
                        if (!basesWithWorkerCreationPending.contains(trainingCase.base))
                        {
                            Log::Get() << "ERROR: Training case is in state 1 but workerCreationPending is false";
                        }

                        // Look for a worker at this spawn position
                        for (auto &worker : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe))
                        {
                            if (worker->lastPosition == trainingCase.spawnPosition)
                            {
                                trainingCase.worker = std::static_pointer_cast<MyWorkerImpl>(worker);
                                trainingCase.worker->spawnPosition = trainingCase.spawnPosition;
                                Workers::setWorkerMineralPatch(trainingCase.worker, trainingCase.resource, trainingCase.base);
                                patchesInUse.insert(trainingCase.resource);
                                basesWithWorkerCreationPending.erase(trainingCase.base);
                                trainingCase.setState(2);
                                break;
                            }
                        }

                        // If we have been waiting too long, something went wrong in creating the worker
                        if (!trainingCase.worker
                            && trainingCase.stateChangeFrame < (BWAPI::Broodwar->getFrameCount() - BWAPI::Broodwar->getLatencyFrames()))
                        {
                            Log::Get() << "ERROR: Worker was not spawned at " << trainingCase.spawnPosition;
                            trainingCase.reset();
                            basesWithWorkerCreationPending.erase(trainingCase.base);
                        }

                        break;
                    }
                    case 2:
                    case 3:
                    {
                        // Leave the worker alone until 10 frames after delivery
                        // We give a bit of buffer after delivery to allow us to detect collisions
                        if (trainingCase.worker->carryingResource || (currentFrame - trainingCase.worker->lastCarryingResourceChange) != 10) break;

                        // State 2 moves to state 3
                        if (trainingCase.state == 2)
                        {
                            trainingCase.setState(3);
                            break;
                        }

                        // Reset this training case for the next run
                        BWAPI::Broodwar->killUnit(trainingCase.worker->bwapiUnit);
                        trainingCase.reset();
                        patchesInUse.erase(trainingCase.resource);

                        // Remove this case so it can be moved to the end of the list
                        completedCases.push_back(*it);
                        it = trainingCases.erase(it);
                        continue;
                    }
                }

                it++;
            }

            for (auto &trainingCase : completedCases)
            {
                trainingCases.emplace_back(trainingCase);
            }
        };

        test.run();
    }
}

TEST(SpawnPositionTraining, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.randomSeed = 42;

    runSpawnLocationsTest(test, true, 0, false);
    runSpawnLocationsTest(test, false, 0, false);
    runSpawnLocationsTest(test, false, 1, false);
    runSpawnLocationsTest(test, false, 2, false);
    runSpawnLocationsTest(test, false, 0, true);
    runSpawnLocationsTest(test, false, 1, true);
    runSpawnLocationsTest(test, false, 2, true);
}

TEST(SpawnPositionTraining, VermeerContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("Vermeer");
        test.randomSeed = 42;

        runSpawnLocationsTest(test, true, 0, false);
        runSpawnLocationsTest(test, false, 0, false);
        runSpawnLocationsTest(test, false, 1, false);
        runSpawnLocationsTest(test, false, 2, false);
        runSpawnLocationsTest(test, false, 0, true);
        runSpawnLocationsTest(test, false, 1, true);
        runSpawnLocationsTest(test, false, 2, true);
    }
}

TEST(SpawnPositionTraining, VermeerStartLocations)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.randomSeed = 42;

    runSpawnLocationsTest(test, true, 0, false);
}

TEST(SpawnPositionTraining, VermeerStartLocationsContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("Vermeer");
        test.randomSeed = 42;

        runSpawnLocationsTest(test, true, 0, false);
    }
}

TEST(SpawnPositionTraining, SSCAIT)
{
    while (true)
    {
        Maps::RunOnEach(Maps::Get("sscai"), [](BWTest test)
        {
            runSpawnLocationsTest(test, true, 0, false);
            runSpawnLocationsTest(test, false, 0, false);
            runSpawnLocationsTest(test, false, 1, false);
            runSpawnLocationsTest(test, false, 2, false);
            runSpawnLocationsTest(test, false, 0, true);
            runSpawnLocationsTest(test, false, 1, true);
            runSpawnLocationsTest(test, false, 2, true);
        });
    }
}

TEST(SpawnPositionTraining, AIIDE2025)
{
    while (true)
    {
        Maps::RunOnEach(Maps::Get("aiide2025"), [](BWTest test)
        {
            runSpawnLocationsTest(test, true, 0, false);
            runSpawnLocationsTest(test, false, 0, false);
            runSpawnLocationsTest(test, false, 1, false);
            runSpawnLocationsTest(test, false, 2, false);
            runSpawnLocationsTest(test, false, 0, true);
            runSpawnLocationsTest(test, false, 1, true);
            runSpawnLocationsTest(test, false, 2, true);
        });
    }
}
