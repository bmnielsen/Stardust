#include "BWTest.h"

#include "StardustAIModule.h"
#include "Strategist.h"
#include "Builder.h"

namespace
{
    class HidePylonStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            prioritizedProductionGoals[PRIORITY_MAINARMY].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Dragoon,
                                                                       -1,
                                                                       1);

            if (built) return;

            BWAPI::TilePosition pylonLocation(89, 116);
            if (Builder::isPendingHere(pylonLocation))
            {
                built = true;
                return;
            }

            auto buildLocation = BuildingPlacement::BuildLocation(Block::Location(pylonLocation), 0, 0, 0);
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                       BWAPI::UnitTypes::Protoss_Pylon,
                                                                       buildLocation);
        }

    private:
        bool built = false;
    };
}

TEST(MopUp, FindsHiddenPylon)
{
    BWTest test;
    test.map = Maps::GetOne("Andromeda");
    test.randomSeed = 42;
    test.opponentRace = BWAPI::Races::Protoss;
    test.expectWin = true;
    test.frameLimit = 20000;
    test.opponentModule = []()
    {
        return new StardustAIModule();
    };

    test.onStartOpponent = []()
    {
        Strategist::setStrategyEngine(std::make_unique<HidePylonStrategyEngine>());
    };

    test.run();
}