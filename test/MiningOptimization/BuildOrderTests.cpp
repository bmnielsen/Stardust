#include "BWTest.h"

#include "DoNothingModule.h"
#include "DoNothingStrategyEngine.h"

#include "WorkerMiningInstrumentation.h"

#include "Strategist.h"
#include "Units.h"

#include "Plays/Macro/SaturateBases.h"
#include "Plays/MainArmy/DefendMyMain.h"

// This file is used to test our mining efficiency in situations that resemble real games, but are still in a controlled enough environment
// to allow benchmarking.
namespace
{
    class OneBaseZealotsStrategyEngine : public DoNothingStrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride) override
        {
            plays.clear();
            plays.emplace_back(std::make_shared<SaturateBases>());
            plays.emplace_back(std::make_shared<DefendMyMain>());
        }

        void updateProduction(std::vector<std::shared_ptr<Play>> &plays,
                              std::map<int, std::vector<ProductionGoal>> &prioritizedProductionGoals,
                              std::vector<std::pair<int, int>> &mineralReservations) override
        {
            prioritizedProductionGoals[PRIORITY_NORMAL].emplace_back(std::in_place_type<UnitProductionGoal>,
                                                                     "test",
                                                                     BWAPI::UnitTypes::Protoss_Zealot,
                                                                     -1,
                                                                     -1);
        }
    };

    template<typename T>
    void runTest(BWTest &test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentRace = BWAPI::Races::Random;
        test.frameLimit = 10000;
        test.expectWin = false;

        test.onStartMine = []()
        {
            Strategist::setStrategyEngine(std::make_unique<T>());
        };

        // Tracks statistics on what units have been created at what frames and supply counts
        // The logic takes advantage of the fact that in these tests no units are ever lost
        std::map<BWAPI::UnitType, std::vector<int>> unitCreationFrames;
        std::vector<std::pair<BWAPI::UnitType, int>> buildOrder;
        test.onFrameMine = [&]()
        {
            // Get the count of all units
            std::map<BWAPI::UnitType, int> unitCounts;
            for (auto &unit : Units::allMine())
            {
                unitCounts[unit->type]++;
            }

            // Add timing data for anything not yet tracked
            for (const auto &[type, count] : unitCounts)
            {
                for (auto i = unitCreationFrames[type].size(); i < count; i++)
                {
                    unitCreationFrames[type].emplace_back(currentFrame);
                    if (currentFrame > 0 && type != BWAPI::UnitTypes::Protoss_Probe)
                    {
                        buildOrder.emplace_back(type, BWAPI::Broodwar->self()->supplyUsed() / 2);
                    }
                }
            }
        };

        test.onEndMine = [&](bool)
        {
            // Output the timings for each unit type
            Log::Get() << "Unit creation frames:";
            for (const auto &[type, timings] : unitCreationFrames)
            {
                std::ostringstream o;
                o << type << ": ";
                std::string sep;
                for (const auto &frame : timings)
                {
                    o << sep << frame;
                    sep = ", ";
                }
                Log::Get() << o.str();
            }

            // Output the build order
            Log::Get() << "Build order:";
            for (const auto &[type, supply] : buildOrder)
            {
                Log::Get() << supply << " " << type;
            }
        };

        test.run();
    }
}

TEST(BuildOrderTests, OneBaseZealots_Vermeer)
{
    Maps::RunOnEachStartLocationPairAndRandomRace(Maps::Get("aiide2024/(4)Vermeer"), [](BWTest test)
    {
        runTest<OneBaseZealotsStrategyEngine>(test);
    });
}
