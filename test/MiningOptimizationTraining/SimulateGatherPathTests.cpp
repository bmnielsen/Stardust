#include "BWTest.h"
#include "MiningOptimizationTraining/PathExploration/FullSaturationModule.h"
#include "MiningOptimizationTraining/PathExploration/SingleWorkerModule.h"
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

    void runSingleWorkerTest(BWTest &test)
    {
        initializeTest(test);
        test.myModule = [&]()
        {
            return new MiningOptimizationTraining::SingleWorkerModule<MiningOptimizationTraining::SimulateGatherPathTester>();
        };
        test.frameLimit = 1000;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "SimulateGatherPathTests_" << test.map->shortname() << "_SingleWorker";
        test.replayName = replayNameBuilder.str();

        test.run();
    }

    void runFullSaturationTest(BWTest &test, unsigned int cannons, unsigned int iterations)
    {
        initializeTest(test);
        test.myModule = [&cannons]()
        {
            return new MiningOptimizationTraining::FullSaturationModule<MiningOptimizationTraining::SimulateGatherPathTester>(cannons);
        };
        test.frameLimit = 10000 * (int)iterations;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "SimulateGatherPathTests_" << test.map->shortname();
        replayNameBuilder << "_" << cannons << "cannons";
        test.replayName = replayNameBuilder.str();

        test.run();
    }
}

TEST(SimulateGatherPathTests, VermeerSingleWorker)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runSingleWorkerTest(test);
}

TEST(SimulateGatherPathTests, VermeerOneIteration)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runFullSaturationTest(test, 0, 1);
}

TEST(SimulateGatherPathTests, VermeerCannonsOneIteration)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runFullSaturationTest(test, 2, 1);
}

TEST(SimulateGatherPathTests, VermeerTenIterations)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runFullSaturationTest(test, 0, 10);
}

TEST(SimulateGatherPathTests, AllSSCAITOneIteration)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runFullSaturationTest(test, 0, 1);
    });
}

TEST(SimulateGatherPathTests, AllSSCAITCannonsOneIteration)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        runFullSaturationTest(test, 2, 1);
    });
}
