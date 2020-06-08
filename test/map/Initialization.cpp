#include "BWTest.h"
#include "DoNothingModule.h"

TEST(Initialization, AllCOG)
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

TEST(Initialization, GreatBarrierReef)
{
    BWTest test;
    test.map = Maps::GetOne("Reef");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, Arcadia)
{
    BWTest test;
    test.map = Maps::GetOne("Arcadia");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}
