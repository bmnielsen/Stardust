#include "BWTest.h"

#include "LocutusAIModule.h"

TEST(SelfPlay, ATest)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []() {
        return new LocutusAIModule();
    };

    test.run();
}