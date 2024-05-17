#include "BWTest.h"
#include "StardustAIModule.h"
#include "General.h"
#include "Map.h"

TEST(Surrender, RunThirty)
{
    int count = 0;
    int lost = 0;
    while (count < 30)
    {
        BWTest test;
        test.opponentName = "BananaBrain";
        test.opponentRace = BWAPI::Races::Random;
        test.maps = Maps::Get("cog2022");
        addBananaBrainModule(test);
        test.myModule = []()
        {
            auto stardustModule = new StardustAIModule();
            stardustModule->enableSurrender = true;
            return stardustModule;
        };
        test.onFrameMine = []()
        {
            // Ignore combat sim to try to ensure we lose
            for (auto base : Map::getEnemyBases())
            {
                auto squad = General::getAttackBaseSquad(base);
                if (squad)
                {
                    squad->ignoreCombatSim = true;
                }
            }
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
