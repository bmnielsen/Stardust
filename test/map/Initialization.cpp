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

TEST(Initialization, AllAIIDEStartLocations)
{
    Maps::RunOnEachStartLocation(Maps::Get("aiide"), [](BWTest test)
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

TEST(Initialization, AllAIIDEMaps)
{
    Maps::RunOnEach(Maps::Get("aiide"), [](BWTest test)
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

TEST(Initialization, Roadkill)
{
    BWTest test;
    test.map = Maps::GetOne("Roadkill");
    test.randomSeed = 38186;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, FightingSpirit)
{
    BWTest test;
    test.map = Maps::GetOne("Fighting");
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
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

TEST(Initialization, Alchemist)
{
    BWTest test;
    test.map = Maps::GetOne("Alchemist");
    test.randomSeed = 68326;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, Plasma)
{
    BWTest test;
    test.map = Maps::GetOne("Plasma");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, BlueStorm)
{
    BWTest test;
    test.map = Maps::GetOne("Blue");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}
