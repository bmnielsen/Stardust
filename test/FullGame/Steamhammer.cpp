#include "BWTest.h"
#include "UAlbertaBotModule.h"

TEST(Steamhammer, RunUntilLoss)
{
    int count = 0;
    while (true)
    {
        BWTest test;
        test.map = "maps/sscai/(2)Benzene.scx";
        test.randomSeed = 54776;
//        test.map = "";
//        test.randomSeed = -1;
        test.opponentRace = BWAPI::Races::Zerg;
        test.opponentModule = []()
        {
            auto module = new UAlbertaBot::UAlbertaBotModule();
            Config::StardustTestStrategyName = "11HatchTurtleLurker";
            return module;
        };
        test.onStartOpponent = [&test]()
        {
            std::cout << "Steamhammer strategy: " << Config::Strategy::StrategyName << std::endl;
            if (test.sharedMemory)
            {
                strncpy(test.sharedMemory, Config::Strategy::StrategyName.c_str(), std::min(255UL, Config::Strategy::StrategyName.size()));
            }
        };
        test.onEndMine = [&](bool won)
        {
            std::string mapFilename = test.map.substr(test.map.rfind('/') + 1);
            std::replace(mapFilename.begin(), mapFilename.end(), ' ', '_');

            std::ostringstream replayName;
            replayName << "Steamhammer_" << (mapFilename.substr(0, mapFilename.rfind('.')));
            if (!won)
            {
                replayName << "_LOSS";
            }
            if (test.sharedMemory) replayName << "_" << test.sharedMemory;
            replayName << "_" << test.randomSeed;
            test.replayName = replayName.str();

            count++;
            std::cout << "---------------------------------------------" << std::endl;
            std::cout << "PLAYED " << count << " GAME(S)" << std::endl;
            std::cout << "---------------------------------------------" << std::endl;
        };
        test.run();

        if (::testing::UnitTest::GetInstance()->current_test_info()->result()->Failed()) break;
    }
}

TEST(Steamhammer, RunForever)
{
    int count = 0;
    int lost = 0;
    while (true)
    {
        BWTest test;
        test.map = "";
        test.randomSeed = -1;
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
        test.onEndMine = [&](bool won)
        {
            std::string mapFilename = test.map.substr(test.map.rfind('/') + 1);
            std::replace(mapFilename.begin(), mapFilename.end(), ' ', '_');

            std::ostringstream replayName;
            replayName << "Steamhammer_" << (mapFilename.substr(0, mapFilename.rfind('.')));
            if (!won)
            {
                replayName << "_LOSS";
                lost++;
            }
            if (test.sharedMemory) replayName << "_" << test.sharedMemory;
            replayName << "_" << test.randomSeed;
            test.replayName = replayName.str();

            count++;
            std::cout << "---------------------------------------------" << std::endl;
            std::cout << "STATUS AFTER " << count << " GAME" << (count == 1 ? "" : "S") << ": "
                      << (count - lost) << " won; " << lost << " lost" << std::endl;
            std::cout << "---------------------------------------------" << std::endl;
        };
        test.expectWin = false;
        test.run();
    }
}

TEST(Steamhammer, 4PoolHard)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "4PoolHard";
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
        Config::StardustTestStrategyName = "4PoolSoft";
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
        Config::StardustTestStrategyName = "OverhatchExpoMuta";
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
        Config::StardustTestStrategyName = "OverpoolSpeed";
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
        Config::StardustTestStrategyName = "11Gas10PoolLurker";
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
        Config::StardustTestStrategyName = "4HatchBeforeLair";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 9PoolSpeed)
{
    BWTest test;
    test.map = "maps/sscai/(4)Python.scx";
    test.randomSeed = 30841;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "9PoolSpeed";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 8Hatch7Pool)
{
    BWTest test;
    test.map = "maps/sscai/(4)Circuit Breaker.scx";
    test.randomSeed = 59756;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "8Hatch7Pool";
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
        Config::StardustTestStrategyName = "9PoolSpeedAllIn";
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
        Config::StardustTestStrategyName = "2HatchLurkerAllIn";
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
        Config::StardustTestStrategyName = "OverhatchExpoLing";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 9HatchExpo9Pool9Gas)
{
    BWTest test;
    test.map = "maps/sscai/(2)Destination.scx";
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "9HatchExpo9Pool9Gas";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 11HatchTurtleLurker)
{
    BWTest test;
    test.map = "maps/sscai/(2)Heartbreak Ridge.scx";
    test.randomSeed = 9020;
//    test.map = "maps/sscai/(2)Benzene.scx";
//    test.randomSeed = 54776;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "11HatchTurtleLurker";
        Config::StardustTestForceGasSteal = true;
        return module;
    };

    test.run();
}

TEST(Steamhammer, Over10Hatch1Sunk)
{
    BWTest test;
    test.map = "maps/sscai/(4)La Mancha1.1.scx";
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Over10Hatch1Sunk";
        return module;
    };

    test.run();
}

TEST(Steamhammer, GasSteal)
{
    BWTest test;
    test.map = "maps/sscai/(4)Python.scx";
    test.randomSeed = 1234;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestForceGasSteal = true;
        Config::StardustTestStrategyName = "11Gas10PoolLurker";
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
    test.onEndMine = [&test](bool won)
    {
        if (test.sharedMemory)
        {
            test.replayName = (std::ostringstream() << "Steamhammer_" << test.sharedMemory).str();
        }
    };
    test.run();
}
