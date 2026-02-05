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
            return new ClearOpponentUnitsModule();
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
        test.timeLimit = INT_MAX;
        test.run();
    }
}

TEST(PathExploration, VermeerSingleWorker)
{
    ExploreStartPositionsModuleOptions options;
    options.onePatch = BWAPI::TilePosition(2, 11);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, options);
}

TEST(PathExploration, VermeerOneBase)
{
    ExploreStartPositionsModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, options);
}

TEST(PathExploration, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, {});
}
