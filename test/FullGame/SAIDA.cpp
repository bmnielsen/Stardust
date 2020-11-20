#include "BWTest.h"
#include "MyBotModule.h"
#include "StardustAIModule.h"

TEST(SAIDA, RunForever)
{
    int count = 0;
    int lost = 0;
    while (count < 40)
    {
        BWTest test;
        test.maps = Maps::Get("aiide");
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new MyBot::MyBotModule();
        };
        test.onStartOpponent = [&]()
        {
            std::cout.setstate(std::ios_base::failbit);
        };
        test.onEndMine = [&](bool won)
        {
            std::ostringstream replayName;
            replayName << "SAIDA_" << test.map->shortname();
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

TEST(SAIDA, RunOne)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.maps = Maps::Get("aiide");
    test.opponentModule = []()
    {
        return new MyBot::MyBotModule();
    };
    test.onStartOpponent = []()
    {
        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [&test](bool won)
    {
        std::ostringstream replayName;
        replayName << "SAIDA_" << test.map->shortname();
        if (!won)
        {
            replayName << "_LOSS";
        }
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.run();
}

TEST(SAIDA, RunAsSAIDA)
{
    BWTest test;
    test.myRace = BWAPI::Races::Terran;
    test.opponentRace = BWAPI::Races::Protoss;
    test.myModule = []()
    {
        return new MyBot::MyBotModule();
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
        std::ostringstream replayName;
        replayName << "SAIDA_" << test.map->shortname();
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
