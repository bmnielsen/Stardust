#include "BWTest.h"
#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"
#include "WorkerOrderTimer.h"
#include "Geo.h"

namespace
{
    class ExpandOnlyStrategyEngine : public StrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride) override {}

        void updatePlays(std::vector<std::shared_ptr<Play>> &plays) override {}

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            auto natural = Map::getMyNatural();
            if (natural && natural->ownedSince == -1 && BWAPI::Broodwar->getFrameCount() > 3000)
            {
                takeNaturalExpansion(plays, prioritizedProductionGoals);
            }
        }
    };

    void runEfficiencyTest(BWTest &test)
    {
        test.opponentRace = BWAPI::Races::Zerg;
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 15000;
        test.expectWin = false;

        std::shared_ptr<SaturateBases> saturateBasesPlay;

        test.onStartMine = [&saturateBasesPlay]()
        {
            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

            std::vector<std::shared_ptr<Play>> openingPlays;

            // Create a saturate bases play that initially only adds 1 worker per patch
            saturateBasesPlay = std::make_shared<SaturateBases>();
            saturateBasesPlay->setWorkersPerPatch(1);
            openingPlays.emplace_back(saturateBasesPlay);

            // Create a dummy main army play since one is needed
            openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(Map::getMyMain()));

            Strategist::setOpening(openingPlays);
        };

        test.onFrameMine = [&saturateBasesPlay]()
        {
            // Create a couple of pylons to not interrupt mining
            if (currentFrame == 1 || currentFrame == 5)
            {
                auto &pylonLocations = BuildingPlacement::getBuildLocations()[to_underlying(BuildingPlacement::Neighbourhood::AllMyBases)][2];
                int built = 0;
                for (auto &location : pylonLocations)
                {
                    if (built == 2) break;

                    int dist = Map::getMyMain()->resourceDepot->getDistance(BWAPI::Position(location.location.tile) + BWAPI::Position(32, 32));
                    if (dist < 200) continue;

                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                ((currentFrame == 1) ? BWAPI::UnitTypes::Protoss_Observer : BWAPI::UnitTypes::Protoss_Pylon),
                                                Geo::CenterOfUnit(location.location.tile, BWAPI::UnitTypes::Protoss_Pylon));
                    built++;
                }
            }

            // After letting us run with one worker per patch for a while, go to two workers per patch
            if (currentFrame == 7000)
            {
                saturateBasesPlay->setWorkersPerPatch(2);
            }
        };

        test.onEndMine = [](bool)
        {
            std::cout << "Mining efficiency: " << WorkerOrderTimer::getEfficiency() << std::endl;
        };

        test.run();
    }
}

TEST(MiningEfficiency, FightingSpirit)
{
    BWTest test;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    runEfficiencyTest(test);
}

TEST(MiningEfficiency, AllSSCAIT)
{
    Maps::RunOnEachStartLocation(Maps::Get("sscai"), [](BWTest test)
    {
        runEfficiencyTest(test);
        test.run();
    });
}

/*

Initial: Mining efficiency: 0.526316
After resend of gather immediately after delivery: 0.527495



 */
