#include "BWTest.h"
#include "UAlbertaBotModule.h"

TEST(Steamhammer, RunUntilLoss)
{
    while (true)
    {
        BWTest test;
        test.opponentRace = BWAPI::Races::Zerg;
        test.opponentModule = []()
        {
            return new UAlbertaBot::UAlbertaBotModule();
        };
        test.onStartOpponent = [&test]()
        {
            std::cout << "Steamhammer strategy: " << Config::Strategy::StrategyName << std::endl;
            strncpy(test.sharedMemory, Config::Strategy::StrategyName.c_str(), std::min(255UL, Config::Strategy::StrategyName.size()));
        };
        test.onEndMine = [&test]()
        {
            test.replayName = (std::ostringstream() << "Steamhammer_" << test.sharedMemory).str();
        };
        test.run();

        if (::testing::UnitTest::GetInstance()->current_test_info()->result()->Failed()) break;
    }
}

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
    test.onStartOpponent = [&test]()
    {
        std::cout << "Steamhammer strategy: " << Config::Strategy::StrategyName << std::endl;
        strncpy(test.sharedMemory, Config::Strategy::StrategyName.c_str(), std::min(255UL, Config::Strategy::StrategyName.size()));
    };
    test.onEndMine = [&test]()
    {
        test.replayName = (std::ostringstream() << "Steamhammer_" << test.sharedMemory).str();
    };
    test.run();
}
