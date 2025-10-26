#include "BWTest.h"
#include "MiningOptimizationTraining/PathExploration/FullSaturationModule.h"
#include "MiningOptimizationTraining/PathExploration/SingleWorkerModule.h"
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

    void runSingleWorkerTest(BWTest &test, int resend)
    {
        initializeTest(test);
        test.myModule = [&]()
        {
            return new MiningOptimizationTraining::SingleWorkerModule(resend);
        };
        test.frameLimit = 1000;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "MiningTraining_" << test.map->shortname();
        replayNameBuilder << "_SingleWorker_" << resend;
        test.replayName = replayNameBuilder.str();

        test.run();
    }

    void runFullSaturationTest(BWTest &test, unsigned int cannons, unsigned int iterations)
    {
        initializeTest(test);
        test.myModule = [&cannons]()
        {
            return new MiningOptimizationTraining::FullSaturationModule(cannons);
        };
        test.frameLimit = 10000 * (int)iterations;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "MiningTraining_" << test.map->shortname();
        replayNameBuilder << "_" << cannons << "cannons";
        test.replayName = replayNameBuilder.str();

        test.run();
    }
}

TEST(PathExploration, VermeerSingleWorker)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
//    for (int i = -20; i < -10; i++)
//    {
//        runSingleWorkerTest(test, -1);
//        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
//    }
    for (int i = 12; i < 55; i++) runSingleWorkerTest(test, i);
//    runSingleWorkerTest(test, 22);
//    runSingleWorkerTest(test, -1);
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
