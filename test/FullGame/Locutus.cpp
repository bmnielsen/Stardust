#include "BWTest.h"
#include "LocutusBotModule.h"
#include "StardustAIModule.h"

TEST(Locutus, RunForever)
{
    int count = 0;
    int lost = 0;
    while (true)
    {
        BWTest test;
        test.opponentRace = BWAPI::Races::Protoss;
        test.opponentModule = []()
        {
            return new UAlbertaBot::LocutusBotModule();
        };
        test.onStartOpponent = [&test]()
        {
            std::cout << "Locutus strategy: " << UAlbertaBot::LocutusBotModule::getStrategyName() << std::endl;
            if (test.sharedMemory)
            {
                strncpy(test.sharedMemory,
                        UAlbertaBot::LocutusBotModule::getStrategyName().c_str(),
                        std::min(255UL, UAlbertaBot::LocutusBotModule::getStrategyName().size()));
            }

            std::cout.setstate(std::ios_base::failbit);
        };
        test.onEndMine = [&](bool won)
        {
            std::ostringstream replayName;
            replayName << "Locutus_" << test.map->shortname();
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

TEST(Locutus, RunAsLocutus)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.myModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("ForgeExpand");
        return new UAlbertaBot::LocutusBotModule();
    };
    test.opponentModule = []()
    {
        return new StardustAIModule();
    };
    test.onStartMine = []()
    {
        std::cout << "Locutus strategy: " << UAlbertaBot::LocutusBotModule::getStrategyName() << std::endl;
    };
    test.onStartOpponent = []()
    {
        Log::SetOutputToConsole(true);
    };
    test.onEndMine = [&](bool won)
    {
        std::ostringstream replayName;
        replayName << "Locutus_" << test.map->shortname();
        if (won)
        {
            replayName << "_LOSS";
        }
        replayName << "_" << UAlbertaBot::LocutusBotModule::getStrategyName();
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.expectWin = false;
    test.run();
}

TEST(Locutus, 4GateGoon)
{
    BWTest test;
    test.map = Maps::GetOne("Roadrunner");
    test.randomSeed = 24270;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("4GateGoon");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, ForgeExpand)
{
    BWTest test;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 2450;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("ForgeExpand");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, 99Gate)
{
    BWTest test;
    test.map = Maps::GetOne("Python");
    test.randomSeed = 49094;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("9-9Gate");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, Proxy99Gate)
{
    BWTest test;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 91613;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("Proxy9-9Gate");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, 1012Gate)
{
    BWTest test;
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 10473;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("10-12Gate");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, 2GateDTRush)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("2GateDTRush");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, CannonFirst4GateGoon)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("CannonFirst4GateGoon");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, PlasmaProxy2Gate)
{
    BWTest test;
    test.map = Maps::GetOne("Plasma");
    test.randomSeed = 97016;
//    test.frameLimit = 14000;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        UAlbertaBot::LocutusBotModule::setStrategy("PlasmaProxy2Gate");
        return new UAlbertaBot::LocutusBotModule();
    };

    test.run();
}
