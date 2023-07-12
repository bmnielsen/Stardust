#include "BWTest.h"
#include "UAlbertaBotModule.h"
#include "BuildingPlacement.h"

TEST(Steamhammer, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        test.opponentName = "Steamhammer";
        test.opponentRace = BWAPI::Races::Random;
        test.maps = Maps::Get("cog2022");
        test.opponentModule = []()
        {
            auto module = new UAlbertaBot::UAlbertaBotModule();
//            Config::StardustTestStrategyName = "11HatchTurtleHydra";
            return module;
        };
//        test.onFrameMine = [&test]()
//        {
//            if (currentFrame == 1)
//            {
//                if (!BuildingPlacement::hasForgeGatewayWall())
//                {
//                    test.writeReplay = false;
//                    BWAPI::Broodwar->leaveGame();
//                }
//            }
//        };
        test.onStartOpponent = [&test]()
        {
            std::cout << "Steamhammer strategy: " << Config::Strategy::StrategyName << std::endl;
            if (test.sharedMemory)
            {
                strncpy(test.sharedMemory, Config::Strategy::StrategyName.c_str(), std::min(255UL, Config::Strategy::StrategyName.size()));
            }

            std::cout.setstate(std::ios_base::failbit);
            std::cerr.setstate(std::ios_base::failbit);
        };
        test.onEndMine = [&](bool won)
        {
            if (currentFrame < 100) return;

            std::ostringstream replayName;
            replayName << "Steamhammer_" << test.map->shortname();
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
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Zerg;
//    test.map = Maps::GetOne("Destination");
//    test.randomSeed = 53123;
    test.maps = Maps::Get("sscait");
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
        std::ostringstream replayName;
        replayName << "Steamhammer_" << test.map->shortname();
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Python");
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 84773;
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Jade");
    test.randomSeed = 22113;
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Python");
    test.randomSeed = 30841;
    test.opponentRace = BWAPI::Races::Zerg;
    test.frameLimit = 10000;
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Benzene");
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Outsider");
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

TEST(Steamhammer, 11HatchTurtleHydra)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 94484;
    test.opponentRace = BWAPI::Races::Zerg;
    test.frameLimit = 20000;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "11HatchTurtleHydra";
        Config::StardustTestForceGasSteal = true;
        return module;
    };

    test.run();
}

TEST(Steamhammer, ZZZKMutaRush)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 94484;
    test.opponentRace = BWAPI::Races::Zerg;
    test.frameLimit = 20000;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "ZZZKMutaRush";
        return module;
    };

    test.run();
}

TEST(Steamhammer, 12HatchTurtle)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Python");
    test.opponentRace = BWAPI::Races::Zerg;
    test.frameLimit = 10000;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "12HatchTurtle";
        return module;
    };

    test.run();
}

TEST(Steamhammer, ZvZ_12PoolLing)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Fighting");
    test.opponentRace = BWAPI::Races::Zerg;
    test.randomSeed = 73549;
    test.frameLimit = 10000;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "ZvZ_12PoolLing";
        return module;
    };

    test.run();
}

TEST(Steamhammer, Over10Hatch1Sunk)
{
    BWTest test;
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
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
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 60426;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "973HydraBust";
        return module;
    };

    test.run();
}

TEST(Steamhammer, OneHatchMuta)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 71869;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Sparkle 1HatchMuta";
        return module;
    };

    test.run();
}

TEST(Steamhammer, GasSteal)
{
    BWTest test;
    test.opponentName = "Steamhammer";
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

TEST(Steamhammer, Plasma)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Plasma");
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        return module;
    };

    test.run();
}

TEST(Steamhammer, 1012Gate)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "10-12Gate";
        return module;
    };

    test.run();
}

TEST(Steamhammer, DTRush)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.randomSeed = 16752;
    test.map = Maps::GetOne("Benzene");
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "DTRush";
        return module;
    };

    test.run();
}

TEST(Steamhammer, Vultures)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.randomSeed = 53781;
    test.map = Maps::GetOne("Destination");
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Vultures";
        return module;
    };

    test.run();
}

TEST(Steamhammer, BBS)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Terran;
    test.frameLimit = 8000;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "BBS";
        return module;
    };

    test.run();
}

TEST(Steamhammer, SiegeExpand)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "SiegeExpand";
        return module;
    };

    test.run();
}

TEST(Steamhammer, UAlbertaBotMarineRush)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.map = Maps::GetOne("Luna");
    test.randomSeed = 95324;
    test.opponentRace = BWAPI::Races::Random;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "UAlbertaBotMarineRush";
        return module;
    };

    test.run();
}

TEST(Steamhammer, SCVRush)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Terran;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "SCVRush";
        return module;
    };

    test.run();
}

TEST(Steamhammer, MatchPointRandom)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Random;
    test.randomSeed = 63033;
    test.map = Maps::GetOne("MatchPoint");
    test.frameLimit = 10000;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "Sparkle 1HatchMuta";
        return module;
    };

    test.run();
}

TEST(Steamhammer, NeoSylphidRandomProtoss)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Random;
    test.randomSeed = 89034;
    test.map = Maps::GetOne("Sylphid");
    test.frameLimit = 10000;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "9-9Gate";
        return module;
    };

    test.run();
}

TEST(Steamhammer, RunOneRandom)
{
    BWTest test;
    test.opponentName = "Steamhammer";
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
        std::ostringstream replayName;
        replayName << "Steamhammer_" << test.map->shortname();
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

TEST(Steamhammer, RunOneWithIslandExpo)
{
    BWTest test;
    test.opponentName = "Steamhammer";
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Andromeda");
    test.frameLimit = 20000;
    test.opponentModule = []()
    {
        return new UAlbertaBot::UAlbertaBotModule();
    };
    test.removeStatic = {
            BWAPI::TilePosition(63, 116) // Blocking neutral at bottom island expo
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Overlord, BWAPI::TilePosition(62, 119)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Hatchery, BWAPI::TilePosition(62, 119))
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
        std::ostringstream replayName;
        replayName << "Steamhammer_" << test.map->shortname();
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
