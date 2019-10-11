#include "BWTest.h"
#include "UAlbertaBotModule.h"

TEST(Steamhammer, 4Pool)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "4PoolHard";
        return module;
    };
    test.map = "maps/sscai/(4)Andromeda.scx";
    test.run();
}

TEST(Steamhammer, OverpoolSpeed)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "OverpoolSpeed";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 11Gas10PoolLurker)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "11Gas10PoolLurker";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 4HatchBeforeLair)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "4HatchBeforeLair";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 9PoolSpeedAllIn)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "9PoolSpeedAllIn";
        return module;
    };

    test.run();
}

TEST(Steamhammer, Anything)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new UAlbertaBot::UAlbertaBotModule();
    };
    test.onStartOpponent = []()
    {
        std::cout << "Steamhammer is using opening strategy " << Config::Strategy::StrategyName << std::endl;
    };

    test.run();
}

