#include "BWTest.h"
#include "MiningOptimizationTraining/PathExploration/FullSaturationModule.h"
#include "MiningOptimizationTraining/PathExploration/SingleWorkerModule.h"
#include "MiningOptimizationTraining/PathExploration/WorkerPathExploration.h"
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

    bool runSingleWorkerTest(BWTest &test)
    {
        initializeTest(test);
        test.myModule = [&]()
        {
            return new MiningOptimizationTraining::SingleWorkerModule<MiningOptimizationTraining::WorkerPathExploration>();
        };
        test.frameLimit = 10000;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "MiningTraining_" << test.map->shortname() << "_SingleWorker";
        test.replayName = replayNameBuilder.str();

        bool finishedEarly = false;
        test.onEndMine = [&](bool)
        {
            if (BWAPI::Broodwar->getFrameCount() < 500) finishedEarly = true;
        };

        test.run();

        return finishedEarly;
    }

    bool runFullSaturationTest(BWTest &test, unsigned int cannons, unsigned int iterations)
    {
        initializeTest(test);
        test.myModule = [&cannons]()
        {
            return new MiningOptimizationTraining::FullSaturationModule<MiningOptimizationTraining::WorkerPathExploration>(cannons);
        };
        test.frameLimit = 10000 * (int)iterations;
        test.timeLimit = 600 * (int)iterations;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "MiningTraining_" << test.map->shortname();
        replayNameBuilder << "_" << cannons << "cannons";
        test.replayName = replayNameBuilder.str();

        bool finishedEarly = false;
        test.onEndMine = [&](bool)
        {
            if (BWAPI::Broodwar->getFrameCount() < 500) finishedEarly = true;
        };

        test.run();

        return finishedEarly;
    }
}

TEST(PathExploration, VermeerSingleWorker)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runSingleWorkerTest(test);
}

TEST(PathExploration, VermeerOneIteration)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runFullSaturationTest(test, 0, 1);
}

TEST(PathExploration, VermeerTenIterations)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runFullSaturationTest(test, 0, 10);
}

TEST(PathExploration, VermeerContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("Vermeer");
        if (runFullSaturationTest(test, 0, 10)) return;
    }
}
