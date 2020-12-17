#include "BWTest.h"
#include "DoNothingModule.h"

#include "Strategist.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeIslandExpansion.h"
#include "DoNothingStrategyEngine.h"
#include "Map.h"
#include "PathFinding.h"

namespace
{
    class TakeExpansionsStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            auto natural = Map::getMyNatural();
            if (!natural || natural->ownedSince != -1)
            {
                defaultExpansions(plays);
                return;
            }

            if (BWAPI::Broodwar->getFrameCount() > 3000) takeNaturalExpansion(plays, prioritizedProductionGoals);
        }
    };
}

TEST(TakeIslandExpansion, CanTakeIslandExpansion)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 98086;
    test.frameLimit = 12000;
    test.expectWin = false;

    test.onStartMine = []()
    {
        auto main = Map::getMyMain();
        Base *islandBase = nullptr;
        int minDist = INT_MAX;
        for (auto base : Map::allBases())
        {
            if (PathFinding::GetGroundDistance(main->getPosition(), base->getPosition(), BWAPI::UnitTypes::Protoss_Probe) != -1) continue;
            int dist = main->getPosition().getApproxDistance(base->getPosition());
            if (dist < minDist)
            {
                islandBase = base;
                minDist = dist;
            }
        }

        Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<SaturateBases>());
        openingPlays.emplace_back(std::make_shared<TakeIslandExpansion>(islandBase));
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

TEST(TakeIslandExpansion, TakesSecondExpansion)
{
    BWTest test;
    test.opponentRace = BWAPI::Races::Zerg;
    test.opponentModule = []()
    {
        return new DoNothingModule();
    };
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 98086;
    test.frameLimit = 20000;
    test.expectWin = false;

    test.opponentInitialUnits = {
            UnitTypeAndPosition(BWAPI::UnitTypes::Zerg_Drone, BWAPI::TilePosition(107, 118)),
            UnitTypeAndPosition(BWAPI::UnitTypes::Terran_Barracks, BWAPI::TilePosition(104, 119)),
    };

    test.onFrameOpponent = []()
    {
        for (auto unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getType() != BWAPI::UnitTypes::Terran_Barracks) continue;

            if (!unit->isFlying())
            {
                unit->lift();
            }
            else
            {
                unit->move(BWAPI::Position(BWAPI::TilePosition(79, 122)));
            }
        }
    };

    test.onEndMine = [](bool won)
    {
//        EXPECT_EQ(Map::getMyNatural()->owner, BWAPI::Broodwar->self());
//        EXPECT_FALSE(Map::getMyNatural()->resourceDepot == nullptr);
//        EXPECT_EQ(Map::getMyNatural()->resourceDepot->getTilePosition(), Map::getMyNatural()->getTilePosition());
    };

    test.run();
}
