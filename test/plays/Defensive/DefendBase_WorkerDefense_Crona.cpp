#include "BWTest.h"

#include "BananaBrain.h"

TEST(DefendBase, WorkerDefenseCrona)
{
    BWTest test;
    test.opponentName = "BananaBrain";
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("NeoHeartbreakerRidge");
    test.randomSeed = 42;
    test.frameLimit = 6000;
    test.expectWin = false;
    test.opponentModule = []()
    {
        auto bbModule = new BananaBrain();
        bbModule->strategyName = ZergStrategy::kZvP_5Pool;
        return bbModule;
    };

    test.run();
}
