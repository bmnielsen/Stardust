#include "BWTest.h"
#include "Iron.h"

TEST(Iron, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        test.opponentName = "Iron";
        test.maps = Maps::Get("sscait");
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
            std::ostringstream replayName;
            replayName << "Iron_" << test.map->shortname();
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

TEST(Iron, RunOne)
{
    BWTest test;
    test.opponentName = "Iron";
    test.opponentRace = BWAPI::Races::Terran;
    test.maps = Maps::Get("sscait");
    test.opponentModule = []()
    {
        return new iron::Iron();
    };
    test.onStartOpponent = []()
    {
        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [&test](bool won)
    {
        std::ostringstream replayName;
        replayName << "Iron_" << test.map->shortname();
        if (!won)
        {
            replayName << "_LOSS";
        }
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.run();
}

TEST(Iron, TestMopUp)
{
    BWTest test;
    test.opponentName = "Iron";
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 42;
    test.opponentModule = []()
    {
        return new iron::Iron();
    };
    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_SCV, BWAPI::TilePosition(40, 116)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Supply_Depot, BWAPI::TilePosition(41, 117)),
    };
    test.onStartOpponent = []()
    {
        std::cout.setstate(std::ios_base::failbit);
    };
    test.run();
}

TEST(Iron, TestElevator)
{
    BWTest test;
    test.opponentName = "Iron";
    test.opponentRace = BWAPI::Races::Terran;
    test.map = Maps::GetOne("Destination");
    test.frameLimit = 15000;
    test.opponentModule = []()
    {
        return new iron::Iron();
    };
    test.onStartOpponent = []()
    {
        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [&test](bool won)
    {
        std::ostringstream replayName;
        replayName << "Iron_" << test.map->shortname();
        if (!won)
        {
            replayName << "_LOSS";
        }
        replayName << "_" << test.randomSeed;
        test.replayName = replayName.str();
    };
    test.run();
}
