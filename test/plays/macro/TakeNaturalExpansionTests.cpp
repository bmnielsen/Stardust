#include "BWTest.h"
#include "DoNothingModule.h"

#include "Strategist.h"
#include "Plays/Macro/TakeNaturalExpansion.h"
#include "Map.h"

TEST(TakeNaturalExpansion, CanTakeNaturalExpansionImmediately)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 42;
    test.frameLimit = 2500;
    test.expectWin = false;

    test.onStartMine = []()
    {
        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<TakeNaturalExpansion>());
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool won)
    {
        EXPECT_EQ(Map::getMyNatural()->owner, BWAPI::Broodwar->self());
        EXPECT_FALSE(Map::getMyNatural()->resourceDepot == nullptr);
        EXPECT_EQ(Map::getMyNatural()->resourceDepot->getTilePosition(), Map::getMyNatural()->getTilePosition());
    };

    test.run();
}