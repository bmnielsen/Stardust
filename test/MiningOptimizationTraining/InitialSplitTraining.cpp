#include "BWTest.h"

#include "DoNothingModule.h"
#include "DataModel/Serialization.h"
#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"
#include "MiningOptimizationTraining/PathExploration/InitialWorkerSplitTester.h"
#include "ClearOpponentUnitsModule.h"

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
                // frame 10
                if (BWAPI::Broodwar->getFrameCount() != 10) return;

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
        test.myModule = [&]()
        {
            ExploreStartPositionsModuleOptions options;
            options.loadMapData = false;
            return new ExploreStartPositionsModule<ExploreInitialWorkerStartPosition>(options);
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
            test.writeReplay = false;
            test.frameLimit = 500;
            test.run();
            std::cout << "Seventh delivery: " << module.seventhDeliveryFrame << std::endl;
        };

//         BWTest test;
//         test.maps = {map};
//         test.randomSeed = 61581;
//         test.opponentRace = BWAPI::Races::Random;
// //        knownEnemyRace = BWAPI::Races::Zerg;
//         runner(test);
//         return;

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
}

TEST(InitializeOrderProcessTimerResetValues, Vermeer)
{
    initializeOrderProcessTimerResetValues(*Maps::GetOne("Vermeer"));
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

TEST(InitialSplitTest, Vermeer)
{
    initialSplitTest(*Maps::GetOne("Vermeer"));
}

TEST(InitialSplitTest, Benzene)
{
    initialSplitTest(*Maps::GetOne("Benzene"));
}
