#include "BWTest.h"
#include "MiningOptimizationTraining/PathExploration/GatheringWorkersModule.h"
#include "MiningOptimizationTraining/PathExploration/InitialWorkerSimulateGatherPathTester.h"
#include "MiningOptimizationTraining/PathExploration/SimulateGatherPathTester.h"
#include "ClearOpponentUnitsModule.h"

#include <thread>

using namespace MiningOptimizationTraining;

namespace
{
    void initializeTest(BWTest &test)
    {
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule(false);
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42; // We use a constant seed to ensure the same initial headings on the created probes
    }

    void runTest(BWTest &test, unsigned int iterations, const GatheringWorkersModuleOptions &_options)
    {
        auto options = _options;
        options.loadMapData = false;

        initializeTest(test);
        test.myModule = [&]()
        {
            return new GatheringWorkersModule<SimulateGatherPathTester>(options);
        };
        if (test.frameLimit == 30000) test.frameLimit = 10000 * (int)iterations + 2;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "SimulateGatherPathTests_" << test.map->shortname();
        replayNameBuilder << "_" << options.cannonsPerBase << "cannons";
        if (options.useStartBlockCannonsForStartingLocations) replayNameBuilder << "[sb]";
        if (options.oneBase.isValid()) replayNameBuilder << "_base" << options.oneBase;
        if (options.onePatch.isValid()) replayNameBuilder << "_patch" << options.onePatch;
        test.replayName = replayNameBuilder.str();

        test.run();
    }

    void runInitialWorkersTest(BWTest &test)
    {
        initializeTest(test);
        test.frameLimit = 500;
        test.myModule = [&]()
        {
            return new InitialWorkerSimulateGatherPathTesterModule();
        };

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "SimulateGatherPathTests_" << test.map->shortname() << "_initialWorkers";
        test.replayName = replayNameBuilder.str();

        test.onEndMine = [&](bool)
        {
            test.addClockPositionToReplayName();
        };
        test.run();
    }
}

TEST(SimulateGatherPathTests, VermeerSingleWorker)
{
    GatheringWorkersModuleOptions options;
    options.onePatch = BWAPI::TilePosition(5, 12);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerOneBaseOneIteration)
{
    GatheringWorkersModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerOneIteration)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, {});
}

TEST(SimulateGatherPathTests, VermeerCannonsOneIteration)
{
    GatheringWorkersModuleOptions options;
    options.cannonsPerBase = 2;

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerStartBlockCannonsOneIteration)
{
    GatheringWorkersModuleOptions options;
    options.cannonsPerBase = 2;

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerTenIterations)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 10, {});
}

TEST(SimulateGatherPathTests, AllSSCAITOneIteration)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runTest(test, 1, {});
    });
}

TEST(SimulateGatherPathTests, AllSSCAITCannonsOneIteration)
{
    GatheringWorkersModuleOptions options;
    options.cannonsPerBase = 2;

    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runTest(test, 1, options);
    });
}


TEST(SimulateGatherPathTests, VermeerInitialWorkers)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runInitialWorkersTest(test);
}
