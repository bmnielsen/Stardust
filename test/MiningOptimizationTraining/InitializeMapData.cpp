#include "BWTest.h"

#include "MiningOptimizationTraining/PathExploration/ExploreStartPositionsModule.h"

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
            return new ExploreStartPositionsModule<InitializeStartPosition>(options);
        };
        test.allowOpponentOutput = false;
        test.expectWin = false;
        test.randomSeed = 42;
        test.writeReplay = false;
        test.frameLimit = 100;
        test.run();
    }
}

TEST(InitializeMapData, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, {});
}
