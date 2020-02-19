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
            if (test.sharedMemory)
            {
                strncpy(test.sharedMemory, Config::Strategy::StrategyName.c_str(), std::min(255UL, Config::Strategy::StrategyName.size()));
            }
        };
        test.onEndMine = [&test]()
        {
            if (test.sharedMemory)
            {
                test.replayName = (std::ostringstream() << "Steamhammer_" << test.sharedMemory).str();
            }
        };
        test.run();

        if (::testing::UnitTest::GetInstance()->current_test_info()->result()->Failed()) break;
    }
}

TEST(Steamhammer, 4PoolHard)
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

TEST(Steamhammer, 4PoolSoft)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "4PoolSoft";
        return module;
    };
    test.run();
}

TEST(Steamhammer, OverhatchExpoMuta)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "OverhatchExpoMuta";
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

TEST(Steamhammer, 2HatchLurkerAllIn)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "2HatchLurkerAllIn";
        return module;
    };

    test.run();
}

TEST(Steamhammer, OverhatchExpoLing)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::LocutusTestStrategyName = "OverhatchExpoLing";
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
        if (test.sharedMemory)
        {
            strncpy(test.sharedMemory, Config::Strategy::StrategyName.c_str(), std::min(255UL, Config::Strategy::StrategyName.size()));
        }
    };
    test.onEndMine = [&test]()
    {
        if (test.sharedMemory)
        {
            test.replayName = (std::ostringstream() << "Steamhammer_" << test.sharedMemory).str();
        }
    };
    test.run();
}
