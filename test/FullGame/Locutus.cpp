#include "BWTest.h"
#include "LocutusBotModule.h"
#include "StardustAIModule.h"

TEST(Locutus, RunAsLocutus)
{
    BWTest test;
    test.map = "maps/sscai/(4)Fighting Spirit.scx";
//        test.randomSeed = -1;
    test.randomSeed=45464;
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
        std::string mapFilename = test.map.substr(test.map.rfind('/') + 1);
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
