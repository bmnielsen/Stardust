#include "BWTest.h"
#include "MiningOptimizationTraining/PathExploration/FullSaturationModule.h"
#include "ClearOpponentUnitsModule.h"

namespace
{
    void initializeTest(BWTest &test)
    {
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule(false);
        };
        test.allowOpponentOutput = true;
        test.expectWin = false;
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

TEST(PathExploration, Vermeer)
{
    BWTest test;
    test.map = Maps::GetOne("Vermeer");
    runFullSaturationTest(test, 0, 1);
}
