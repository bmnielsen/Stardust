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

TEST(InitializeMapData, VermeerOnePatch)
{
    ExploreStartPositionsModuleOptions options;
    options.onePatch = BWAPI::TilePosition(2, 11);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, options);
}

TEST(InitializeMapData, VermeerOneBase)
{
    ExploreStartPositionsModuleOptions options;
    options.oneBase = BWAPI::TilePosition(7, 6);

    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    run(test, options);
}

TEST(InitializeMapData, AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [&](BWTest test)
    {
        run(test, {});
    });
}

TEST(InitializeMapData, Benzene)
{
    BWTest test;
    test.map = Maps::GetOne("Benzene");
    run(test, {});
}

TEST(InitializeMapData, Destination)
{
    BWTest test;
    test.map = Maps::GetOne("Destination");
    run(test, {});
}
