#include "BWTest.h"
#include "Iron.h"
#include "StardustAIModule.h"

TEST(Iron, RunForever)
{
    int count = 0;
    int lost = 0;
    while (count < 40)
    {
        BWTest test;
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new iron::Iron();
        };
        test.onStartOpponent = [&]()
        {
            std::cout.setstate(std::ios_base::failbit);
        };
        test.onEndMine = [&](bool won)
        {
            std::string mapFilename = test.map->shortname();
            std::replace(mapFilename.begin(), mapFilename.end(), ' ', '_');

            std::ostringstream replayName;
            replayName << "Iron_" << (mapFilename.substr(0, mapFilename.rfind('.')));
            if (!won)
            {
                replayName << "_LOSS";
                lost++;
            }
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

TEST(Iron, RunAsIron)
{
    BWTest test;
    test.myRace = BWAPI::Races::Terran;
    test.opponentRace = BWAPI::Races::Protoss;
    test.myModule = []()
    {
        return new iron::Iron();
    };
    test.opponentModule = []()
    {
        return new StardustAIModule();
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
        replayName << "Iron_" << (mapFilename.substr(0, mapFilename.rfind('.')));
        if (won)
        {
            replayName << "_LOSS";
        }
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.expectWin = false;
    test.run();
}