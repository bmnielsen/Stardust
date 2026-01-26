#include "BWTest.h"

#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"
#include "ClearOpponentUnitsModule.h"

using namespace MiningOptimizationTraining;

namespace
{
    void run(BWTest &test, const ExploreStartPositionsModuleOptions &options)
    {
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.myModule = [&]()
        {
            return new ExploreStartPositionsModule<ExploreStartPosition>(options);
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.run();
    }
}

TEST(PathExploration, VermeerSingleWorker)
{
    ExploreStartPositionsModuleOptions options;
    options.onePatch = BWAPI::TilePosition(5, 12);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.frameLimit = 1000;
    run(test, options);
}

TEST(PathExploration, VermeerOneBase)
{
    ExploreStartPositionsModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    test.frameLimit = 1000;
    run(test, options);
}

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
