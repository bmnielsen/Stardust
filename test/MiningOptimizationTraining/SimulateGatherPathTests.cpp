#include "BWTest.h"
#include "MiningOptimizationTraining/PathExploration/GatheringWorkersModule.h"
#include "MiningOptimizationTraining/PathExploration/SimulateGatherPathTester.h"
#include "ClearOpponentUnitsModule.h"

#include <thread>

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

    void runTest(BWTest &test, unsigned int iterations, const MiningOptimizationTraining::GatheringWorkersModuleOptions &options)
    {
        initializeTest(test);
        test.myModule = [&]()
        {
            return new MiningOptimizationTraining::GatheringWorkersModule<MiningOptimizationTraining::SimulateGatherPathTester>(options);
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
}

TEST(SimulateGatherPathTests, VermeerSingleWorker)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;
    options.onePatch = BWAPI::TilePosition(5, 12);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerOneBaseOneIteration)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerOneIteration)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerCannonsOneIteration)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;
    options.cannonsPerBase = 2;

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerStartBlockCannonsOneIteration)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;
    options.cannonsPerBase = 2;

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 1, options);
}

TEST(SimulateGatherPathTests, VermeerTenIterations)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runTest(test, 10, options);
}

TEST(SimulateGatherPathTests, AllSSCAITOneIteration)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;

    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runTest(test, 1, options);
    });
}

TEST(SimulateGatherPathTests, AllSSCAITCannonsOneIteration)
{
    MiningOptimizationTraining::GatheringWorkersModuleOptions options;
    options.loadMapData = false;
    options.cannonsPerBase = 2;

    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runTest(test, 1, options);
    });
}
