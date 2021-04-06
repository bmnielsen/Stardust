#include "BWTest.h"
#include "MyBotModule.h"

TEST(SAIDA, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        test.maps = Maps::Get("sscait");
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
    test.maps = Maps::Get("sscait");
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

TEST(SAIDA, FightingSpirit)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Fighting");
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
