#include "BWTest.h"
#include "DoNothingModule.h"

#include "Map.h"
#include "Plays/SpecialTeams/Elevator.h"

TEST(Initialization, AllCOGMaps)
{
    Maps::RunOnEach(Maps::Get("cog"), [](BWTest test)
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

TEST(Initialization, AllCOG2022Maps)
{
    Maps::RunOnEach(Maps::Get("cog2022"), [](BWTest test)
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

TEST(Initialization, AllCOGStartLocations)
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

TEST(Initialization, AllCOG2022StartLocations)
{
    Maps::RunOnEachStartLocation(Maps::Get("cog2022"), [](BWTest test)
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

TEST(Initialization, AllTauCrossStartLocations)
{
    Maps::RunOnEachStartLocation(Maps::Get("Tau"), [](BWTest test)
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

TEST(Initialization, AllOutsiderStartLocations)
{
    Maps::RunOnEachStartLocation(Maps::Get("Outsider"), [](BWTest test)
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

TEST(Initialization, Outsider6Oclock)
{
    BWTest test;
    test.map = Maps::GetOne("Outsider");
    test.randomSeed = 83721;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
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

TEST(Initialization, NeoMoonGlaive)
{
    BWTest test;
    test.map = Maps::GetOne("Glaive");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, Destination)
{
    BWTest test;
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, NeoHeartbreakerRidge)
{
    BWTest test;
    test.map = Maps::GetOne("NeoHeartbreakerRidge");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, CircuitBreaker)
{
    BWTest test;
    test.map = Maps::GetOne("Breaker");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, Python)
{
    BWTest test;
    test.map = Maps::GetOne("Python");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, Benzene)
{
    BWTest test;
    test.map = Maps::GetOne("Benzene");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;
    test.run();
}

TEST(Initialization, Outsider)
{
    BWTest test;
    test.map = Maps::GetOne("Outsider");
    test.randomSeed = 40072;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.frameLimit = 10;
    test.expectWin = false;

    test.onStartMine = []()
    {
        Elevator::selectPositions(Map::getMyMain());
    };
    test.run();
}
