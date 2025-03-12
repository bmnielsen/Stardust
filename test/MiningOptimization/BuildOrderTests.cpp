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
    class BuildOrderTestsStrategyEngine : public DoNothingStrategyEngine
    {
        void initialize(std::vector<std::shared_ptr<Play>> &plays, bool transitioningFromRandom, const std::string &openingOverride) override
        {
            plays.clear();
            plays.emplace_back(std::make_shared<SaturateBases>());
            plays.emplace_back(std::make_shared<DefendMyMain>());
        }
    };

    class OneBaseZealotsStrategyEngine : public BuildOrderTestsStrategyEngine
    {
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

    std::string toString(std::vector<int> frames)
    {
        std::ostringstream o;
        std::string sep;
        for (const auto &frame : frames)
        {
            o << sep << frame;
            sep = ", ";
        }
        return o.str();
    }

    std::string toString(std::map<BWAPI::UnitType, std::vector<int>> unitCreationFrames)
    {
        std::ostringstream o;
        std::string lineSep;
        for (const auto &[type, timings] : unitCreationFrames)
        {
            o << lineSep << type << ": " << toString(timings);
            lineSep = "\n";
        }
        return o.str();
    }

    struct Result
    {
        std::map<BWAPI::UnitType, std::vector<int>> unitCreationFrames;
        int fiftiethMineralFrame = -1;
        std::vector<int> thousandMineralFrames;
        WorkerMiningInstrumentation::Efficiency miningEfficiency = {0,0,0,0,0};
    };

    template<typename T>
    Result runTest(BWTest &test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentRace = BWAPI::Races::Random;
        if (test.frameLimit == 30000) test.frameLimit = 10000;
        test.expectWin = false;

        test.onStartMine = []()
        {
            Strategist::setStrategyEngine(std::make_unique<T>());
        };

        // Tracks statistics on what units have been created at what frames and supply counts
        // The logic takes advantage of the fact that in these tests no units are ever lost
        Result result;
        std::vector<std::pair<BWAPI::UnitType, int>> buildOrder;
        test.onFrameMine = [&]()
        {
            // Get the count of all units
            std::map<BWAPI::UnitType, int> unitCounts;
            for (auto &unit : Units::allMine())
            {
                if (unit->completionFrame == 0) continue;
                unitCounts[unit->type]++;
            }

            // Add timing data for anything not yet tracked
            for (const auto &[type, count] : unitCounts)
            {
                for (auto i = result.unitCreationFrames[type].size(); i < count; i++)
                {
                    result.unitCreationFrames[type].emplace_back(currentFrame);
                    if (type != BWAPI::UnitTypes::Protoss_Probe)
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
            Log::Get() << toString(result.unitCreationFrames);

            // Output the build order
            Log::Get() << "Build order:";
            for (const auto &[type, supply] : buildOrder)
            {
                if (supply > 40) break;
                Log::Get() << supply << " " << type;
            }

            result.fiftiethMineralFrame = WorkerMiningInstrumentation::getFiftiethMineralFrame();
            result.thousandMineralFrames = WorkerMiningInstrumentation::getThousandMineralFrames();
            result.miningEfficiency = WorkerMiningInstrumentation::getEfficiency();
        };

        test.run();
        return result;
    }

    template<typename T>
    void measure(BWTest &test, int iterations)
    {
        std::vector<Result> results;
        results.reserve(iterations);
        for (int i=0; i<iterations; i++)
        {
            results.push_back(runTest<T>(test));
        }

        auto averageFrameTimes = [](const std::vector<const std::vector<int>*> &vectors)
        {
            std::vector<int> result;
            if (vectors.empty()) return result;

            int minSize = INT_MAX;
            for (const auto &vector : vectors)
            {
                minSize = std::min((int)vector->size(), minSize);
            }

            result.resize(minSize);
            for (int i = 0; i < minSize; i++)
            {
                for (const auto &vector : vectors)
                {
                    result[i] += (*vector)[i];
                }
                result[i] = (int)std::round((double)result[i] / (double)vectors.size());
            }

            return result;
        };

        std::vector<const std::vector<int>*> thousandMineralFramesVectors;
        std::map<BWAPI::UnitType, std::vector<const std::vector<int>*>> unitCreationFramesVectors;
        for (const auto &result : results)
        {
            thousandMineralFramesVectors.push_back(&result.thousandMineralFrames);
            for (const auto &[unitType, timings] : result.unitCreationFrames)
            {
                unitCreationFramesVectors[unitType].push_back(&timings);
            }
        }

        std::vector<int> thousandMineralFrames = averageFrameTimes(thousandMineralFramesVectors);
        std::map<BWAPI::UnitType, std::vector<int>> unitCreationFrames;
        for (const auto &[unitType, vectors] : unitCreationFramesVectors)
        {
            unitCreationFrames[unitType] = averageFrameTimes(vectors);
        }

        double fiftiethMineral = 0.0;
        double singleRotation = 0.0;
        double doubleRotation = 0.0;
        for (const auto &result : results)
        {
            fiftiethMineral += result.fiftiethMineralFrame;
            singleRotation += result.miningEfficiency.singleWorkerRotationTime;
            doubleRotation += result.miningEfficiency.doubleWorkerRotationTime;
        }
        if (!results.empty())
        {
            fiftiethMineral /= (double)results.size();
            singleRotation /= (double)results.size();
            doubleRotation /= (double)results.size();
        }

        std::cout << std::fixed << std::setprecision(1)
                  << "Overall results:" << std::endl
                  << "50th mineral frame: " << fiftiethMineral << std::endl
                  << "Single rotation time: " << singleRotation << std::endl
                  << "Double rotation time: " << doubleRotation << std::endl
                  << "Thousand mineral frames: " << toString(thousandMineralFrames) << std::endl
                  << "Unit creation frames:" << std::endl << toString(unitCreationFrames) << std::endl;
    }
}

TEST(BuildOrderTests, OneBaseZealots_VermeerFive)
{
    BWTest test;
    test.map = Maps::GetOne("aiide2024/(4)Vermeer");
    measure<OneBaseZealotsStrategyEngine>(test, 5);
}

TEST(BuildOrderTests, OneBaseZealots_VermeerOne)
{
    BWTest test;
    test.map = Maps::GetOne("aiide2024/(4)Vermeer");
    runTest<OneBaseZealotsStrategyEngine>(test);
}
