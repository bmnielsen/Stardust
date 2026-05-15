#include "BWTest.h"

#include "DoNothingModule.h"
#include "DataModel/Serialization.h"
#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"
#include "MiningOptimizationTraining/PathExploration/InitialWorkerSplitTester.h"
#include "ClearOpponentUnitsModule.h"

#include <random>

#include "DataTransformation/DataTransformer.h"

#define VERBOSE_LOGGING false

using namespace MiningOptimizationTraining;

namespace
{
    void initializeOrderProcessTimerResetValues(const Maps::MapMetadata &map)
    {
        InitialWorkerMapData data;
        Serialization::setGameParameters(map.openbwHash);
        Serialization::readMapData(data);
        data.startingWorkerPositionToOrderProcessTimerReset.clear();

        bool opponentIsZerg;

        auto runner = [&](BWTest test)
        {
            test.opponentModule = []()
            {
                return new DoNothingModule();
            };
            test.myModule = []()
            {
                return new DoNothingModule();
            };
            test.allowOpponentOutput = false;
            test.expectWin = false;
            test.writeReplay = false;
            test.frameLimit = 15;
            test.onFrameMine = [&]()
            {
                // The timer changes on the engine's frame 8, but there is misalignment due to how the test hooks are applied so we need to look at
                // frame 9
                if (BWAPI::Broodwar->getFrameCount() != 9) return;

                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (!unit->getType().isWorker()) continue;

                    // Get the order process timer and rewind, since the value we read is after the unit's orders have been processed
                    auto orderProcessTimer = unit->getOrderProcessTimer() + 1;
                    if (orderProcessTimer == 9) orderProcessTimer = 0;

                    auto &startingPositionData = data.startingWorkerPositionToOrderProcessTimerReset[unit->getPosition()];
                    bool found = false;
                    for (auto &existingData : startingPositionData)
                    {
                        if (existingData.value != orderProcessTimer) continue;
                        if (existingData.opponentIsZerg != opponentIsZerg) continue;

                        found = true;
                        existingData.opponentStartLocationsCount++;
                    }
                    if (!found)
                    {
                        startingPositionData.emplace_back(orderProcessTimer, opponentIsZerg, 1, test.randomSeed);
                    }
                }
            };
            test.run();
        };

        opponentIsZerg = true;
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Zerg);

        opponentIsZerg = false;
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Protoss);

#if VERBOSE_LOGGING
        for (const auto &[startPosition, observations] : data.startingWorkerPositionToOrderProcessTimerReset)
        {
            std::ostringstream dbg;
            dbg << "\n" << startPosition << ":\n";
            std::string sep;
            for (const auto &observation : observations)
            {
                dbg << sep << " " << observation;
                sep = "\n";
            }
            Log::Get() << dbg.str();
        }
#endif

        Serialization::writeMapData(data);
    }

    void initialPathExploration(const Maps::MapMetadata &map)
    {
        BWTest test;
        test.map = std::make_shared<Maps::MapMetadata>(map);
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };

        ExploreStartPositionsModuleOptions options;
        options.loadMapData = false;
        ExploreStartPositionsModule<ExploreInitialWorkerStartPosition> myModule(options);
        test.myModule = [&]()
        {
            return &myModule;
        };

        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.timeLimit = INT_MAX;
        test.run();
    }

    void initialSplitTest(const Maps::MapMetadata &map)
    {
        InitialWorkerMapData data;
        Serialization::setGameParameters(map.openbwHash);
        Serialization::readMapData(data);

        BWAPI::Race knownEnemyRace = BWAPI::Races::Unknown;
        auto runner = [&](BWTest test)
        {
            auto module = InitialWorkerSplitTesterModule(data, knownEnemyRace, false);
            test.opponentModule = []()
            {
                return new DoNothingModule();
            };
            test.myModule = [&]()
            {
                return &module;
            };
            test.allowOpponentOutput = false;
            test.expectWin = false;
            test.writeReplay = true;
            test.frameLimit = 500;
            test.run();
            std::cout << "Seventh delivery: " << module.seventhDeliveryFrame << std::endl;
        };

        // BWTest test;
        // test.maps = {map};
        // test.randomSeed = 61302;
        // test.opponentRace = BWAPI::Races::Zerg;
        // knownEnemyRace = BWAPI::Races::Zerg;
        // runner(test);
        // return;

        // Run with zerg and non-zerg where we don't know the enemy race
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Zerg);
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Protoss);

        // Run with zerg and non-zerg where we know the enemy race
        knownEnemyRace = BWAPI::Races::Zerg;
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Zerg);

        knownEnemyRace = BWAPI::Races::Protoss;
        Maps::RunOnEachStartLocationPair({map}, runner, BWAPI::Races::Protoss);

        // Run another 100 iterations on random settings
        knownEnemyRace = BWAPI::Races::Unknown;
        for (int i = 0; i < 100; i++)
        {
            BWTest test;
            test.maps = {map};
            test.opponentRace = BWAPI::Races::Random;
            runner(test);
        }
    }

    double initialSplitMeasurement(const Maps::MapMetadata &map)
    {
        auto rng = std::default_random_engine(42);
        std::uniform_int_distribution<> randomSeedDistribution(1, 100000);

        unsigned long accumulator = 0;
        unsigned long count = 0;
        auto runner = [&](BWTest test, BWAPI::Race opponentRace, bool writeReplay, std::optional<int> randomSeed)
        {
            test.opponentModule = []()
            {
                return new DoNothingModule();
            };
            test.opponentRace = opponentRace;
            test.allowOpponentOutput = false;
            test.randomSeed = randomSeed ? *randomSeed : randomSeedDistribution(rng);
            test.expectWin = false;
            test.writeReplay = writeReplay;
            test.frameLimit = 500;
            bool seventhCollectionDone = false;
            test.onFrameMine = [&]()
            {
                if (writeReplay && currentFrame == 0)
                {
                    for (auto worker : BWAPI::Broodwar->self()->getUnits())
                    {
                        if (worker->getType().isWorker()) std::cout << PositionAndVelocity(worker) << std::endl;
                    }
                }

                if (seventhCollectionDone) return;
                if (BWAPI::Broodwar->self()->gatheredMinerals() >= 100)
                {
                    seventhCollectionDone = true;
                    accumulator += currentFrame;
                    count++;
                    BWAPI::Broodwar->leaveGame();
                }
            };
            test.run();
        };

        // BWTest test;
        // test.maps = {map};
        // runner(test, BWAPI::Races::Random, true, 89767);
        // return 0.0;

        std::vector<BWAPI::Race> races = {BWAPI::Races::Random, BWAPI::Races::Zerg, BWAPI::Races::Terran};
        for (int i = 0; i < 30; i++)
        {
            BWTest test;
            test.maps = {map};
            runner(test, races[i % 3], false, std::nullopt);
        }

        double result = ((double)accumulator / (double)count);
        std::cout << std::fixed << std::setprecision(1)
                  << "Overall results: " << std::endl
                  << " Count of games: " << count << std::endl
                  << " Average seventh collection frame: " << result;

        return result;
    }
}

TEST(InitializeOrderProcessTimerResetValues, Vermeer)
{
    initializeOrderProcessTimerResetValues(*Maps::GetOne("Vermeer"));
}

TEST(InitializeOrderProcessTimerResetValues, Benzene)
{
    initializeOrderProcessTimerResetValues(*Maps::GetOne("Benzene"));
}

TEST(InitializeOrderProcessTimerResetValues, Destination)
{
    initializeOrderProcessTimerResetValues(*Maps::GetOne("sscai/(2)Destination"));
}

TEST(InitializeOrderProcessTimerResetValues, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        initializeOrderProcessTimerResetValues(*test.map);
    });
}

TEST(ExploreInitialStartPositions, Vermeer)
{
    initialPathExploration(*Maps::GetOne("Vermeer"));
}

TEST(ExploreInitialStartPositions, Benzene)
{
    initialPathExploration(*Maps::GetOne("Benzene"));
}

TEST(ExploreInitialStartPositions, CircuitBreaker)
{
    initialPathExploration(*Maps::GetOne("(4)Circuit Breaker"));
}

TEST(ExploreInitialStartPositions, Roadrunner)
{
    initialPathExploration(*Maps::GetOne("sscai/(4)Roadrunner"));
}

TEST(ExploreInitialStartPositions, Destination)
{
    initialPathExploration(*Maps::GetOne("sscai/(2)Destination"));
}

TEST(ExploreInitialStartPositions, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        initialPathExploration(*test.map);
    });
}

TEST(InitialSplitTest, Vermeer)
{
    initialSplitTest(*Maps::GetOne("Vermeer"));
}

TEST(InitialSplitTest, Benzene)
{
    initialSplitTest(*Maps::GetOne("Benzene"));
}

TEST(InitialSplitMeasurement, Benzene)
{
    initialSplitMeasurement(*Maps::GetOne("Benzene"));
}

TEST(InitialSplitMeasurement, AllSSCAIT)
{
    std::vector<std::pair<std::string, double>> results;
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        results.emplace_back(test.map->shortname(), initialSplitMeasurement(*test.map));
    });

    std::cout << "All results: " << std::endl;
    double average = 0.0;
    for (const auto &[map, result] : results)
    {
        std::cout << std::fixed << std::setprecision(1)
                  << " " << map << ": " << result << std::endl;
        average += result / (double)results.size();
    }
    std::cout << std::fixed << std::setprecision(1)
              << "Overall average: " << average << std::endl;
}

TEST(FullInitialSplit, Benzene)
{
    auto map = *Maps::GetOne("Benzene");
    initialPathExploration(map);
    initialSplitTest(map);

    MapData data;
    data.mapHash = map.openbwHash;
    InitialWorkerMapData initialWorkerData;
    Serialization::setGameParameters(map.openbwHash);
    Serialization::readMapData(initialWorkerData);
    DataTransformer::transform(data, initialWorkerData);

    initialSplitMeasurement(map);
}
