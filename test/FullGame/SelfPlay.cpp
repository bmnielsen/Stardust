#include "BWTest.h"

#include "StardustAIModule.h"

TEST(SelfPlay, ATest)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.expectWin = false;
    test.opponentModule = []()
    {
        return new StardustAIModule();
    };

    test.run();
}