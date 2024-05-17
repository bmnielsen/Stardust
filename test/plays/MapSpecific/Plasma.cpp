#include "BWTest.h"

#include "DoNothingModule.h"

TEST(Plasma, ClearsEggs)
{
    BWTest test;
    test.map = Maps::GetOne("Plasma");
    test.randomSeed = 42;
    test.opponentRace = BWAPI::Races::Protoss;
    test.expectWin = true;
    test.frameLimit = 15000;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };

    test.run();
}
