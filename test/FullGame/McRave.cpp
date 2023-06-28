#include "BWTest.h"

// McRave includes
#include "Header.h"
#include "3rdparty/opponents/McRave/McRave/BuildOrder.h"

TEST(McRave, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        McRaveModule* mcraveModule;
        test.opponentName = "McRave";
        test.opponentRace = BWAPI::Races::Zerg;
        test.maps = Maps::Get("cog2022");
        test.opponentModule = [&]()
        {
            mcraveModule = new McRaveModule();
            return mcraveModule;
        };
        test.onStartOpponent = [&]()
        {
            std::ostringstream os;
            os << McRave::BuildOrder::getCurrentBuild()
               << "_" << McRave::BuildOrder::getCurrentOpener()
               << "_" << McRave::BuildOrder::getCurrentTransition();

            auto build = os.str();

            std::cout << "McRave strategy: " << build << std::endl;
            if (test.sharedMemory)
            {
                strncpy(test.sharedMemory,
                        build.c_str(),
                        std::min(255UL, build.size()));
            }

            std::cout.setstate(std::ios_base::failbit);
        };
        test.onEndMine = [&](bool won)
        {
            std::ostringstream replayName;
            replayName << "McRave_" << test.map->shortname();
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

TEST(McRave, RunOne)
{
    BWTest test;
    McRaveModule* mcraveModule;
    test.opponentName = "McRave";
    test.opponentRace = BWAPI::Races::Zerg;
    test.maps = Maps::Get("sscait");
    test.opponentModule = [&]()
    {
        mcraveModule = new McRaveModule();
        return mcraveModule;
    };
    test.onStartOpponent = [&]()
    {
        std::ostringstream os;
        os << McRave::BuildOrder::getCurrentBuild()
                << "_" << McRave::BuildOrder::getCurrentOpener()
                << "_" << McRave::BuildOrder::getCurrentTransition();

        auto build = os.str();

        std::cout << "McRave strategy: " << build << std::endl;
        if (test.sharedMemory)
        {
            strncpy(test.sharedMemory,
                    build.c_str(),
                    std::min(255UL, build.size()));
        }

        std::cout.setstate(std::ios_base::failbit);
    };
    test.onEndMine = [&test](bool won)
    {
        std::ostringstream replayName;
        replayName << "McRave_" << test.map->shortname();
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

TEST(McRave, PoolHatch_9Pool_2HatchMuta)
{
    BWTest test;
    McRaveModule* mcraveModule;
    test.opponentName = "McRave";
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 20126;
    test.frameLimit = 10000;
    test.opponentModule = [&]()
    {
        mcraveModule = new McRaveModule();
        return mcraveModule;
    };
    test.onStartOpponent = [&]()
    {
        McRave::BuildOrder::setLearnedBuild("PoolHatch", "9Pool", "2HatchMuta");

        std::cout.setstate(std::ios_base::failbit);
    };
    test.run();
}

TEST(McRave, PoolHatch_Overpool_2HatchMuta)
{
    BWTest test;
    McRaveModule* mcraveModule;
    test.opponentName = "McRave";
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Heartbreak");
    test.randomSeed = 20126;
    test.opponentModule = [&]()
    {
        mcraveModule = new McRaveModule();
        return mcraveModule;
    };
    test.onStartOpponent = [&]()
    {
        McRave::BuildOrder::setLearnedBuild("PoolHatch", "Overpool", "2HatchMuta");

        std::cout.setstate(std::ios_base::failbit);
    };
    test.run();
}
