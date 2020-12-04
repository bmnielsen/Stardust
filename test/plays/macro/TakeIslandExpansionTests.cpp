#include "BWTest.h"
#include "DoNothingModule.h"

#include "Strategist.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeIslandExpansion.h"
#include "Map.h"
#include "PathFinding.h"

namespace
{
    class DoNothingStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {}
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