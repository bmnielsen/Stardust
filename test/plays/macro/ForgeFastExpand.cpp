#include "BWTest.h"

#include "DoNothingModule.h"

TEST(ForgeFastExpand, NoOpponentActivity)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Benzene");
    test.randomSeed = 42;
    test.frameLimit = 12000;
    test.expectWin = false;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    // Currently our PvZ engine always uses a FFE build order if the map supports it, so no setup is required

    test.run();
}
