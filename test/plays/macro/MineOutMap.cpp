#include "BWTest.h"

#include "StardustAIModule.h"
#include "Strategist.h"
#include "Map.h"
#include "Workers.h"
#include "Plays/Macro/SaturateBases.h"
#include "Plays/Macro/TakeExpansion.h"

namespace
{
    class MineOutStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            if (Map::getMyBases().size() == 1)
            {
                if (currentFrame > 1500)
                {
                    takeNaturalExpansion(plays, prioritizedProductionGoals);
                }
                return;
            }

            std::vector<std::shared_ptr<TakeExpansion>> takeExpansionPlays;
            for (auto &play : plays)
            {
                if (auto takeExpansionPlay = std::dynamic_pointer_cast<TakeExpansion>(play))
                {
                    takeExpansionPlays.push_back(takeExpansionPlay);
                }
            }
            if (!takeExpansionPlays.empty()) return;

            int excessMineralAssignments = 0;
            int availableMineralAssignmentsThreshold = 6 + takeExpansionPlays.size();
            for (auto base : Map::getMyBases())
            {
                excessMineralAssignments = std::max(excessMineralAssignments,
                                                    Workers::availableMineralAssignments(base) - availableMineralAssignmentsThreshold);
            }

            if (excessMineralAssignments > 0) return;

            // Create a TakeExpansion play for the next expansion
            for (const auto &expansion : Map::getUntakenExpansions())
            {
                auto play = std::make_shared<TakeExpansion>(expansion, 0);
                plays.emplace(plays.begin(), play);
                break;
            }
        }
    };
}

TEST(MineOutMap, MineOutMap)
{
    BWTest test;
    test.opponentModule = []()
    {
        return new StardustAIModule();
    };
    test.opponentRace = BWAPI::Races::Protoss;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    test.frameLimit = 40000;
    test.expectWin = false;

    test.onStartMine = test.onStartOpponent = []()
    {
        Strategist::setStrategyEngine(std::make_unique<MineOutStrategyEngine>());

        std::vector<std::shared_ptr<Play>> openingPlays;
        openingPlays.emplace_back(std::make_shared<SaturateBases>(150));
        Strategist::setOpening(openingPlays);
    };

    test.run();
}