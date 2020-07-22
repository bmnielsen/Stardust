#include "BWTest.h"
#include "DoNothingModule.h"

TEST(GetChokeCannonLocations, AllCOG)
{
    Maps::RunOnEachStartLocation(Maps::Get("cog"), [](BWTest test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 10;
        test.expectWin = false;
        test.run();
    });
}

TEST(GetChokeCannonLocations, AllSSCAIT)
{
    Maps::RunOnEachStartLocation(Maps::Get("sscai"), [](BWTest test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 10;
        test.expectWin = false;
        test.run();
    });
}
