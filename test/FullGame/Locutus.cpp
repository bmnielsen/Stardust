#include "BWTest.h"
#include "LocutusBotModule.h"

TEST(Locutus, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        test.opponentName = "Locutus";
        test.maps = Maps::Get("sscait");
        test.opponentRace = BWAPI::Races::Protoss;
        test.opponentModule = []()
        {
//            Locutus::LocutusBotModule::setStrategy("4GateGoon");
//            Locutus::LocutusBotModule::forceGasSteal();
            return new Locutus::LocutusBotModule();
        };
        test.onStartOpponent = [&test]()
        {
            std::cout << "Locutus strategy: " << Locutus::LocutusBotModule::getStrategyName() << std::endl;
            if (test.sharedMemory)
            {
                strncpy(test.sharedMemory,
                        Locutus::LocutusBotModule::getStrategyName().c_str(),
                        std::min(255UL, Locutus::LocutusBotModule::getStrategyName().size()));
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

TEST(Locutus, RunOne)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.maps = Maps::Get("sscait");
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
//            Locutus::LocutusBotModule::setStrategy("4GateGoon");
//            Locutus::LocutusBotModule::forceGasSteal();
        return new Locutus::LocutusBotModule();
    };
    test.onStartOpponent = [&test]()
    {
        std::cout << "Locutus strategy: " << Locutus::LocutusBotModule::getStrategyName() << std::endl;
        if (test.sharedMemory)
        {
            strncpy(test.sharedMemory,
                    Locutus::LocutusBotModule::getStrategyName().c_str(),
                    std::min(255UL, Locutus::LocutusBotModule::getStrategyName().size()));
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
        }
        if (test.sharedMemory) replayName << "_" << test.sharedMemory;
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.expectWin = false;
    test.run();
}

TEST(Locutus, 4GateGoon)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("4GateGoon");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, ForgeExpand)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 2450;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("ForgeExpand");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, 99Gate)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.map = Maps::GetOne("Python");
    test.randomSeed = 49094;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("9-9Gate");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, Proxy99Gate)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 91613;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("Proxy9-9Gate");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, 1012Gate)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.map = Maps::GetOne("Tau");
    test.randomSeed = 10473;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("10-12Gate");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, 2GateDTRush)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("2GateDTRush");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, CannonFirst4GateGoon)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("CannonFirst4GateGoon");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}

TEST(Locutus, PlasmaProxy2Gate)
{
    BWTest test;
    test.opponentName = "Locutus";
    test.map = Maps::GetOne("Plasma");
    test.randomSeed = 97016;
//    test.frameLimit = 14000;
    test.opponentRace = BWAPI::Races::Protoss;
    test.opponentModule = []()
    {
        Locutus::LocutusBotModule::setStrategy("PlasmaProxy2Gate");
        return new Locutus::LocutusBotModule();
    };

    test.run();
}
