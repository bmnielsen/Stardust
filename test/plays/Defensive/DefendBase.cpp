#include "BWTest.h"
#include "UAlbertaBotModule.h"


TEST(DefendBase, Mutas)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.map = Maps::GetOne("Destination");
    test.randomSeed = 42;
    test.frameLimit = 15000;
    test.expectWin = false;
    test.opponentModule = []()
    {
        auto module = new UAlbertaBot::UAlbertaBotModule();
        Config::StardustTestStrategyName = "12HatchTurtle";
        return module;
    };
    test.onFrameOpponent = []()
    {
        if (BWAPI::Broodwar->getFrameCount() != 11500) return;

        for (int i = 0; i < 6; i++)
        {
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Zerg_Mutalisk, BWAPI::Position(BWAPI::TilePosition(60, 109)));
        }
    };
    test.run();
}