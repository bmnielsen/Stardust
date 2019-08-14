#include "BWTest.h"

#include "LocutusAIModule.h"

TEST(AiideMapAnalysis, Benzene)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(2)Benzene.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, Destination)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(2)Destination.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, HeartbreakRidge)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(2)HeartbreakRidge.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, Aztec)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(3)Aztec.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, TauCross)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(3)TauCross.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, Andromeda)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(4)Andromeda.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, CircuitBreaker)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(4)CircuitBreaker.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, EmpireoftheSun)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(4)EmpireoftheSun.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, Fortress)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(4)Fortress.scx";
    test.frameLimit = 100;
    test.run();
}

TEST(AiideMapAnalysis, Python)
{
    BWTest test;
    test.opponentModule = new LocutusAIModule();
    test.map = "maps/aiide/(4)Python.scx";
    test.frameLimit = 100;
    test.run();
}

