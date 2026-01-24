#include "BWTest.h"

#include "MiningOptimizationTraining/PathExploration/ExploreRemainingStartPositionsModule.h"
#include "ClearOpponentUnitsModule.h"

using namespace MiningOptimizationTraining;

namespace
{
    void runTest(BWTest &test, const ExploreRemainingStartPositionsModuleOptions &options)
    {
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule(false);
        };
        test.myModule = [&]()
        {
            return new ExploreRemainingStartPositionsModule(options);
        };
        test.allowOpponentOutput = false;
        test.writeReplay = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.frameLimit = 100;

        test.run();
    }
}

TEST(PathExploration, VermeerSingleWorker)
{
    ExploreRemainingStartPositionsModuleOptions options;
    options.onePatch = BWAPI::TilePosition(5, 12);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.frameLimit = 1000;
    runTest(test, options);
}

//
//TEST(PathExploration, VermeerSingleWorkerContinuous)
//{
//    while (true)
//    {
//        BWTest test;
//        test.map = Maps::GetOne("Vermeer");
//        test.frameLimit = 50000;
//        if (runSingleWorkerTest(test, BWAPI::TilePosition(5, 12))) return;
//    }
//}
//
//TEST(PathExploration, VermeerOneBase)
//{
//    BWTest test;
//    test.map = Maps::GetOne("Vermeer");
//    test.frameLimit = 500;
//    runFullSaturationTest(test, 0, 1, true);
//}
//
//TEST(PathExploration, VermeerOneIteration)
//{
//    BWTest test;
//    test.map = Maps::GetOne("Vermeer");
//    runFullSaturationTest(test, 0, 1);
//}
//
//TEST(PathExploration, VermeerTenIterations)
//{
//    BWTest test;
//    test.map = Maps::GetOne("Vermeer");
//    runFullSaturationTest(test, 0, 10);
//}
//
//TEST(PathExploration, VermeerContinuous)
//{
//    while (true)
//    {
//        BWTest test;
//        test.map = Maps::GetOne("Vermeer");
//        if (runFullSaturationTest(test, 0, 10)) return;
//    }
//}
//
//TEST(PathExploration, VermeerOneBaseContinuous)
//{
//    while (true)
//    {
//        BWTest test;
//        test.map = Maps::GetOne("Vermeer");
//        if (runFullSaturationTest(test, 0, 10, true)) return;
//    }
//}
