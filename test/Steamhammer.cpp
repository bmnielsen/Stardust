#include "BWTest.h"
#include "UAlbertaBotModule.h"

TEST(Steamhammer, FourPool)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = new UAlbertaBot::UAlbertaBotModule();
    Config::LocutusTestStrategyName = "4PoolHard";
    test.run();

    EXPECT_EQ(1, 1);
}
