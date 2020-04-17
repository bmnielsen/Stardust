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
            std::string mapFilename = test.map->shortname();
            std::replace(mapFilename.begin(), mapFilename.end(), ' ', '_');

            std::ostringstream replayName;
            replayName << "Locutus_" << (mapFilename.substr(0, mapFilename.rfind('.')));
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
        std::string mapFilename = test.map->shortname();
        std::replace(mapFilename.begin(), mapFilename.end(), ' ', '_');

        std::ostringstream replayName;
        replayName << "Locutus_" << (mapFilename.substr(0, mapFilename.rfind('.')));
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
