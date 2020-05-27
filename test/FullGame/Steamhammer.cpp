#include "BWTest.h"
#include "UAlbertaBotModule.h"

TEST(Steamhammer, RunForever)
{
    int count = 0;
    int lost = 0;
    while (count < 40)
    {
        BWTest test;
        test.opponentRace = BWAPI::Races::Zerg;
        //test.map = Maps::GetOne("Destination");
        test.opponentModule = []()
        {
            auto module = new UAlbertaBot::UAlbertaBotModule();
            //Config::StardustTestStrategyName = "12HatchTurtle";
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
            std::string mapFilename = test.map->shortname();
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

TEST(Steamhammer, RunOne)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
//    test.map = Maps::GetOne("Destination");
//    test.randomSeed = 53123;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
//        Config::StardustTestStrategyName = "12HatchTurtle";
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
    test.onEndMine = [&test](bool won)
    {
        std::string mapFilename = test.map->shortname();
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
    };
    test.run();
}

TEST(Steamhammer, Microwave_9PoolGasHatchSpeed7D)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Microwave_9PoolGasHatchSpeed7D";
        return module;
    };
    test.run();
}

TEST(Steamhammer, 4PoolHard)
{
    BWTest test;
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 50443;
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

TEST(Steamhammer, OverpoolTurtleZero)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Benzene");
    test.randomSeed = 99546;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "OverpoolTurtle 0";
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

TEST(Steamhammer, 7Pool6GasLurker)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "7Pool6GasLurker";
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
    test.map = Maps::GetOne("Python");
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
    test.map = Maps::GetOne("Circuit Breaker");
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
    test.map = Maps::GetOne("Destination");
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
    test.map = Maps::GetOne("Heartbreak Ridge");
    test.randomSeed = 9020;
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

TEST(Steamhammer, 12HatchTurtle)
{
    BWTest test;
    test.map = Maps::GetOne("Destination");
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "12HatchTurtle";
        return module;
    };

    test.run();
}

TEST(Steamhammer, Over10Hatch1Sunk)
{
    BWTest test;
    test.map = Maps::GetOne("La Mancha");
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Over10Hatch1Sunk";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 7PoolMid_GasSteal)
{
    BWTest test;
    test.map = Maps::GetOne("Circuit Breaker");
    test.randomSeed = 42530;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "7PoolMid";
        Config::StardustTestForceGasSteal = true;
        return module;
    };

    test.run();
}

TEST(Steamhammer, 11HatchTurtleMuta)
{
    BWTest test;
    test.map = Maps::GetOne("Empire");
    test.randomSeed = 75376;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "11HatchTurtleMuta";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 973HydraBust)
{
    BWTest test;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 60426;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestForceGasSteal = true;
        Config::StardustTestStrategyName = "973HydraBust";
        return module;
    };

    test.run();
}

TEST(Steamhammer, GasSteal)
{
    BWTest test;
    test.map = Maps::GetOne("Python");
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

TEST(Steamhammer, 1012Gate)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "10-12Gate";
        return module;
    };

    test.run();
}

TEST(Steamhammer, RunOneRandom)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Random;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
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
    test.onEndMine = [&test](bool won)
    {
        std::string mapFilename = test.map->shortname();
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
    };
    test.run();
}
