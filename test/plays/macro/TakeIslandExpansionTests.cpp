#include "BWTest.h"
#include "DoNothingModule.h"

#include "Strategist.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeIslandExpansion.h"
#include "Map.h"
#include "PathFinding.h"

TEST(TakeIslandExpansion, CanTakeIslandExpansion)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = "maps/sscai/(4)Python.scx";
    test.frameLimit = 12000;
    test.expectWin = false;

    test.onStartMine = []()
    {
        auto main = Map::getMyMain();
        Base *islandBase = nullptr;
        for (auto base : Map::allBases())
        {
            auto dist = PathFinding::GetGroundDistance(main->getPosition(), base->getPosition(), BWAPI::UnitTypes::Protoss_Probe);
            if (dist == -1)
            {
                islandBase = base;
                break;
            }
        }

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<SaturateBases>());
        openingPlays.emplace_back(std::make_shared<TakeIslandExpansion>(islandBase->getTilePosition()));
        Strategist::setOpening(openingPlays);
    };

    test.onEndMine = [](bool won)
    {
//        EXPECT_EQ(Map::getMyNatural()->owner, BWAPI::Broodwar->self());
//        EXPECT_FALSE(Map::getMyNatural()->resourceDepot == nullptr);
//        EXPECT_EQ(Map::getMyNatural()->resourceDepot->getTilePosition(), Map::getMyNatural()->getTilePosition());
    };

    test.run();
}