#include "BWTest.h"
#include "DoNothingModule.h"
#include "ClearOpponentUnitsModule.h"
#include "DoNothingStrategyEngine.h"
#include "StardustAIModule.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"
#include "WorkerMiningInstrumentation.h"
#include "MiningOptimization/WorkerMiningOptimization.h"
#include "Units.h"
#include "Workers.h"

#include <algorithm>
#include <random>

namespace
{
    const std::string dataBasePath = "/Users/bmnielsen/BW/mining-timings/";

    struct TestResult
    {
        double rotationTime = 0.0;
        double miningPercentage = 0.0;
        double collisionRate = 0.0;

        TestResult& operator+= (const TestResult &other)
        {
            rotationTime += other.rotationTime;
            miningPercentage += other.miningPercentage;
            collisionRate += other.collisionRate;
            return *this;
        }

        TestResult& operator/= (unsigned int val)
        {
            rotationTime /= (double)val;
            miningPercentage /= (double)val;
            collisionRate /= (double)val;
            return *this;
        }

        friend std::ostream & operator << (std::ostream &os, const TestResult &obj)
        {
            std::ostringstream buffer;
            buffer << std::fixed << std::setprecision(2);
            buffer << "Rotation: " << obj.rotationTime;
            buffer << "; Percentage: " << obj.miningPercentage << "%";
            buffer << "; Collision rate: " << obj.collisionRate;
            os << buffer.str();
            return os;
        }
    };

    TestResult runEfficiencyTestImpl(BWTest &test,
                                     unsigned int workersPerPatch,
                                     unsigned int cannons,
                                     bool onlyOneWorker,
                                     bool measureOnly,
                                     unsigned int iterations = 1,
                                     bool patchResults = false)
    {
        std::cout << "Starting mining training with following parameters"
            << ": workersPerPatch=" << workersPerPatch
            << "; cannons=" << cannons
            << "; onlyOneWorker=" << onlyOneWorker
            << "; measureOnly=" << measureOnly
            << "; iterations=" << iterations
            << "; patchResults=" << patchResults
            << std::endl;
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };
        test.myModule = []()
        {
            auto module = new StardustAIModule();
            module->enableFrameLimit = false;
            return module;
        };
        if (iterations > 1)
        {
            test.frameLimit = (int)(10000 * iterations);
        }
        else if (test.frameLimit > 10000)
        {
            test.frameLimit = 10000;
        }
        test.expectWin = false;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "MiningTraining_" << test.map->shortname() << "_";
        replayNameBuilder << workersPerPatch << "wpp_" << cannons << "cannons";
        test.replayName = replayNameBuilder.str();

        WorkerMiningOptimization::setExploring(!measureOnly);

        test.onStartMine = []()
        {
            Strategist::setStrategyEngine(std::make_unique<DoNothingStrategyEngine>());

            // Add a dummy main army play since one is needed
            std::vector<std::shared_ptr<Play>> openingPlays;
            openingPlays.emplace_back(std::make_shared<TestMainArmyAttackBasePlay>(Map::getMyMain()));
            Strategist::setOpening(openingPlays);
        };

        std::map<BWAPI::Position, std::pair<int, Base*>> workerCreationOrderAndBase;
        test.onFrameMine = [&]()
        {
            // Ensure all mineral patches keep enough minerals
            for (auto unit : BWAPI::Broodwar->getNeutralUnits())
            {
                if (!unit->getType().isMineralField()) continue;
                if (unit->getResources() < 100) unit->setResources(1500);
            }

            // Initialization steps:
            // - Kill initial workers, blocking neutrals and critters
            // - Add observers at expansions
            // - Add depots at expansions
            // - Add pylons at each base
            // - Add forge at main base
            // - Add required number of cannons
            // - Create workers

            if (BWAPI::Broodwar->getFrameCount() % 10000 == 0 && BWAPI::Broodwar->getFrameCount() < test.frameLimit)
            {
                Log::SetOutputToConsole(false);

                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType().isWorker())
                    {
                        BWAPI::Broodwar->killUnit(unit);
                    }
                }

                workerCreationOrderAndBase.clear();

                if (BWAPI::Broodwar->getFrameCount() == 0)
                {
                    for (auto base : Map::allBases())
                    {
                        for (const auto &blockingNeutral : base->blockingNeutrals)
                        {
                            BWAPI::Broodwar->killUnit(blockingNeutral);
                        }
                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, base->getPosition());
                    }

                    for (auto unit : BWAPI::Broodwar->getNeutralUnits())
                    {
                        if (unit->getType().isCritter()) BWAPI::Broodwar->killUnit(unit);
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 5)
            {
                for (auto base : Map::allBases())
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Nexus,
                                                Geo::CenterOfUnit(base->getTilePosition(), BWAPI::UnitTypes::Protoss_Nexus));

                    auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                    if (staticDefenseLocations.powerPylon.isValid())
                    {
                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                    BWAPI::UnitTypes::Protoss_Pylon,
                                                    Geo::CenterOfUnit(staticDefenseLocations.powerPylon, BWAPI::UnitTypes::Protoss_Pylon));
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 10)
            {
                for (auto unit : BWAPI::Broodwar->self()->getUnits())
                {
                    if (unit->getType() == BWAPI::UnitTypes::Protoss_Observer)
                    {
                        BWAPI::Broodwar->killUnit(unit);
                    }
                }

                auto &locations = BuildingPlacement::getBuildLocations()[to_underlying(BuildingPlacement::Neighbourhood::MainBase)][3];
                if (!locations.empty())
                {
                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                BWAPI::UnitTypes::Protoss_Forge,
                                                Geo::CenterOfUnit(locations.begin()->location.tile, BWAPI::UnitTypes::Protoss_Forge));
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() == 15)
            {
                for (auto base : Map::allBases())
                {
                    auto &staticDefenseLocations = BuildingPlacement::baseStaticDefenseLocations(base);
                    if (staticDefenseLocations.powerPylon.isValid())
                    {
                        auto cannonLocations = std::set<BWAPI::TilePosition>(staticDefenseLocations.workerDefenseCannons.begin(),
                                                                             staticDefenseLocations.workerDefenseCannons.end());

                        auto buildCannon = [&]()
                        {
                            BWAPI::TilePosition best = BWAPI::TilePositions::Invalid;
                            int bestDist = INT_MAX;
                            for (auto tile : cannonLocations)
                            {
                                int dist = base->mineralLineCenter.getApproxDistance(Geo::CenterOfUnit(tile,
                                                                                                       BWAPI::UnitTypes::Protoss_Photon_Cannon));
                                if (dist < bestDist)
                                {
                                    bestDist = dist;
                                    best = tile;
                                }
                            }

                            if (best != BWAPI::TilePositions::Invalid)
                            {
                                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                            BWAPI::UnitTypes::Protoss_Photon_Cannon,
                                                            Geo::CenterOfUnit(best, BWAPI::UnitTypes::Protoss_Photon_Cannon));
                                cannonLocations.erase(best);
                            }
                        };

                        for (int builtCannons = 0; builtCannons < cannons; builtCannons++)
                        {
                            buildCannon();
                        }
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() % 10000 == 20)
            {
                // Create workers
                int idx = 0;
                for (auto &base : Map::allBases())
                {
                    auto wpDepot = BWAPI::WalkPosition(base->getPosition());
                    std::set<std::pair<int, BWAPI::WalkPosition>> positionsByDistToDepot;
                    std::set<BWAPI::WalkPosition> availablePositions;
                    for (auto tile : base->mineralLineTiles)
                    {
                        if (!Map::isWalkable(tile)) continue;

                        for (int x = 0; x < 4; x++)
                        {
                            for (int y = 0; y < 4; y++)
                            {
                                auto here = BWAPI::WalkPosition(tile) + BWAPI::WalkPosition(x, y);
                                if (Geo::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Nexus,
                                                             base->getPosition(),
                                                             BWAPI::Position(here) + BWAPI::Position(4, 4)) < 4)
                                {
                                    continue;
                                }
                                if (BWAPI::Broodwar->isWalkable(here))
                                {
                                    availablePositions.insert(here);
                                    positionsByDistToDepot.insert(std::make_pair(here.getApproxDistance(wpDepot), here));
                                }
                            }
                        }
                    }

                    for (int built = 0; built < (base->mineralPatchCount() * workersPerPatch); built++)
                    {
                        for (auto [_, start] : positionsByDistToDepot)
                        {
                            for (int x = 0; x < 3; x++)
                            {
                                for (int y = 0; y < 3; y++)
                                {
                                    if (!availablePositions.contains(start + BWAPI::WalkPosition(x, y)))
                                    {
                                        goto nextStartPosition;
                                    }
                                }
                            }

                            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                                        BWAPI::UnitTypes::Protoss_Probe,
                                                        Geo::CenterOfUnit(BWAPI::Position(start), BWAPI::UnitTypes::Protoss_Probe));
                            workerCreationOrderAndBase[Geo::CenterOfUnit(BWAPI::Position(start),
                                                                         BWAPI::UnitTypes::Protoss_Probe)] = std::make_pair(++idx, base);

                            for (int x = 0; x < 3; x++)
                            {
                                for (int y = 0; y < 3; y++)
                                {
                                    availablePositions.erase(start + BWAPI::WalkPosition(x, y));
                                }
                            }

                            if (onlyOneWorker) return;

                            goto nextWorker;

                            nextStartPosition:;
                        }

                        Log::Get() << "ERROR: Could not build worker " << built << " at base " << base->getTilePosition();

                        nextWorker:;
                    }
                }
            }
            else if (BWAPI::Broodwar->getFrameCount() % 10000 == 23)
            {
                // Gather all available mineral assignments for each base, and sort them to ensure stability between test runs
                std::map<Base *, std::vector<Resource>> baseMineralPatches;
                for (auto &base : Map::allBases())
                {
                    auto patches = base->mineralPatches();
                    if (workersPerPatch == 2)
                    {
                        patches.insert(patches.end(), base->mineralPatches().begin(), base->mineralPatches().end());
                    }

                    std::sort(patches.begin(), patches.end(), [](const Resource &a, const Resource &b)
                    {
                        return a->tile < b->tile;
                    });

                    baseMineralPatches[base] = patches;
                }

                // Sort the workers by location so we get stable behaviour across runs
                auto workerSet = Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Probe);
                std::vector<MyUnit> workers(workerSet.begin(), workerSet.end());
                std::sort(workers.begin(), workers.end(), [](const MyUnit &a, const MyUnit &b)
                {
                    return a->lastPosition < b->lastPosition;
                });

                // After first iteration, shuffle using same seeds so we vary which workers are assigned to which patch
                auto rng = std::default_random_engine(BWAPI::Broodwar->getFrameCount());
                std::shuffle(std::begin(workers), std::end(workers), rng);

                // Rewrite the order process index for all workers, as the way they are created for this test (where they all appear on the same
                // frame) prevents our usual logic from working
                // Also override the mineral patch assignments to ensure stability between runs
                for (auto &worker : workers)
                {
                    auto it = workerCreationOrderAndBase.find(worker->lastPosition);
                    if (it != workerCreationOrderAndBase.end())
                    {
                        worker->orderProcessIndex = it->second.first;

                        auto &basePatches = baseMineralPatches[it->second.second];
                        if (!basePatches.empty())
                        {
                            Workers::setWorkerMineralPatch(std::static_pointer_cast<MyWorkerImpl>(worker),
                                                           *basePatches.rbegin(),
                                                           it->second.second);
                            basePatches.pop_back();
                        }
                        else
                        {
                            Log::SetOutputToConsole(true);
                            Log::Get() << "ERROR: Couldn't get base patches for worker @ " << worker->lastPosition;
                            Log::SetOutputToConsole(false);
                        }
                    }
                    else
                    {
                        Log::SetOutputToConsole(true);
                        Log::Get() << "ERROR: Couldn't get index for worker @ " << worker->lastPosition;
                        Log::SetOutputToConsole(false);
                        BWAPI::Broodwar->leaveGame();
                    }
                }

                Log::SetOutputToConsole(true);
            }
        };

        TestResult result;
        test.onEndMine = [&](bool)
        {
            auto getTestResult = [&](const WorkerMiningInstrumentation::Efficiency &e)
            {
                if (workersPerPatch == 1)
                {
                    return TestResult{e.singleWorkerRotationTime, e.singleWorkerMiningPercentage, e.collisionRate * 100.0};
                }
                return TestResult{e.doubleWorkerRotationTime, e.doubleWorkerMiningPercentage, e.collisionRate * 100.0};
            };

            auto efficiency = WorkerMiningInstrumentation::getEfficiency();
            result = getTestResult(efficiency);

            if (patchResults)
            {
                auto patchEfficiencyMap = WorkerMiningInstrumentation::getEfficiencyByPatch();
                std::vector<std::pair<Resource, WorkerMiningInstrumentation::Efficiency>> patchEfficiency;
                std::copy(patchEfficiencyMap.begin(), patchEfficiencyMap.end(), std::back_inserter(patchEfficiency));
                std::sort(patchEfficiency.begin(), patchEfficiency.end(), [](
                        const std::pair<Resource, WorkerMiningInstrumentation::Efficiency> &a,
                        const std::pair<Resource, WorkerMiningInstrumentation::Efficiency> &b)
                {
                    return a.first->tile < b.first->tile;
                });

                std::ostringstream str;
                str.imbue(std::locale("da_DK"));
                str << std::fixed << std::showpoint << std::setprecision(4)
                    << "Efficiency per patch:";
                for (const auto &[patch, e] : patchEfficiency)
                {
                    int nearestDepotDist = INT_MAX;
                    for (const auto &depot : Units::allMineCompletedOfType(BWAPI::UnitTypes::Protoss_Nexus))
                    {
                        int dist = patch->getDistance(depot);
                        if (dist < nearestDepotDist)
                        {
                            nearestDepotDist = dist;
                        }
                    }

                    auto patchResult = getTestResult(e);
                    str << "\n" << patch->tile.x
                        << ";" << patch->tile.y
                        << ";" << nearestDepotDist
                        << ";" << patchResult.rotationTime
                        << ";" << patchResult.miningPercentage
                        << ";" << patchResult.collisionRate;
                }
                std::cout << str.str() << std::endl;
            }
        };

        test.run();

        std::cout << "Mining efficiency: " << result << std::endl;
        return result;
    }

    TestResult runEfficiencyTest(BWTest &test,
                                 unsigned int workersPerPatch,
                                 unsigned int cannons,
                                 bool onlyOneWorker = false,
                                 bool measureOnly = false,
                                 bool patchResults = false)
    {
#if !INSTRUMENTATION_ENABLED
        return runEfficiencyTestImpl(test, workersPerPatch, cannons, onlyOneWorker, measureOnly, 1, patchResults);
#else
        for (int i=0; i<10; i++)
        {
            auto result = runEfficiencyTestImpl(test, workersPerPatch, cannons, onlyOneWorker, measureOnly, 1, patchResults);
            if (result.rotationTime > 0.0001) return result;
        }

        Log::Get() << "ERROR: Could not get a stable test run after 10 tries!";
        return {};
#endif
    }

    TestResult runTestSuite(BWTest &test, unsigned int workersPerPatch, int cannons, bool measureOnly = false)
    {
        if (!measureOnly) runEfficiencyTestImpl(test, workersPerPatch, cannons, false, false, 10);
        return runEfficiencyTest(test, workersPerPatch, cannons, false, true);
    }

    void testRunWithResults(const std::string &mapSearch, int workers = 0, bool cannons = true, bool measureOnly = false)
    {
        std::map<std::string, std::map<std::pair<int, int>, TestResult>> mapHashToConfigurationToEfficiency;

        unsigned int minWorkers = (workers < 2) ? 1 : 2;
        unsigned int maxWorkers = (workers == 1) ? 1 : 2;

        unsigned int maxCannons = cannons ? 2 : 0;

        Maps::RunOnEach(Maps::Get(mapSearch), [&](BWTest test)
        {
            TestResult totalSingle;
            TestResult totalDouble;
            for (auto workersPerPatch = minWorkers; workersPerPatch <= maxWorkers; workersPerPatch++)
            {
                for (auto cannons = 0; cannons <= maxCannons; cannons++)
                {
                    auto result = runTestSuite(test, workersPerPatch, cannons, measureOnly);
                    (workersPerPatch == 1 ? totalSingle : totalDouble) += result;
                    mapHashToConfigurationToEfficiency[test.map->openbwHash][std::make_pair(workersPerPatch, cannons)] = result;
                }
            }
            if (maxCannons > 0)
            {
                totalSingle /= (maxCannons + 1);
                totalDouble /= (maxCannons + 1);
            }
            std::cout << std::fixed << std::showpoint << std::setprecision(4)
                      << "Overall efficiency: " << std::endl
                      << "Single: " << totalSingle << std::endl
                      << "Double: " << totalDouble << std::endl;
        });

        {
            std::ofstream file;
            auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto tm = std::localtime(&tt);
            file.open((std::ostringstream()
                        << dataBasePath << "miningtraining"
                        << "_" << workers << "workers"
                        << "_" << (!cannons ? "no" : "") << "cannons"
                        << "_" << std::put_time(tm, "%Y%m%d_%H%M%S")
                        << "_g_" << GATHER_EXPLORE_BEFORE << "_" << GATHER_EXPLORE_AFTER
                        << "_r_" << RETURN_EXPLORE_BEFORE << "_" << RETURN_EXPLORE_AFTER
                        << ".csv").str(),
                      std::ofstream::trunc);
            file << "Hash;1-0R;1-0P;1-1R;1-1P;1-2R;1-2P;2-0R;2-0P;2-1R;2-1P;2-2R;2-2P\n";
            std::map<std::pair<int, int>, TestResult> totals;
            for (const auto &[mapHash, _] : mapHashToConfigurationToEfficiency)
            {
                file << mapHash;

                for (int w = 1; w <= 2; w++)
                {
                    for (int c = 0; c <= 2; c++)
                    {
                        auto key = std::make_pair(w, c);
                        auto result = mapHashToConfigurationToEfficiency[mapHash][key];
                        file << ";" << result.rotationTime << ";" << result.miningPercentage;
                        totals[key] += result;
                    }
                }

                file << "\n";
            }

            file << "Average";
            for (int w = 1; w <= 2; w++)
            {
                for (int c = 0; c <= 2; c++)
                {
                    auto key = std::make_pair(w, c);
                    totals[key] /= mapHashToConfigurationToEfficiency.size();
                    file << ";" << totals[key].rotationTime << ";" << totals[key].miningPercentage;
                }
            }
            file << "\n";

            file.close();
        }
    }
}

TEST(MiningTraining, ChupungRyeong)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0);
    auto dbl = runEfficiencyTest(test, 2, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Single: " << sgl << std::endl
        << "Double: " << dbl << std::endl;
}

TEST(MiningTraining, DebugMiningCommands)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    test.frameLimit = 500;
    auto sgl = runEfficiencyTest(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, ChupungRyeongSingle)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, ChupungRyeongSingleTwoIterations)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTestImpl(test, 1, 0, false, false, 2);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, ChupungRyeongSingleCannons)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    auto c1 = runEfficiencyTest(test, 1, 1);
    auto c2 = runEfficiencyTest(test, 1, 2);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "One cannon: " << c1 << std::endl
        << "Two cannons: " << c2 << std::endl;
}

TEST(MiningTraining, ChupungRyeongSingle10)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    for (int i = 0; i < 10; i++)
    {
        auto sgl = runEfficiencyTest(test, 1, 0);
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Single: " << sgl << std::endl;
    }
}

TEST(MiningTraining, ChupungRyeongDouble)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    test.frameLimit = 3500;
    auto dbl = runEfficiencyTest(test, 2, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Double: " << dbl << std::endl;
}

TEST(MiningTraining, EddySingle)
{
    BWTest test;
    test.map = Maps::GetOne("Eddy");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, RoadkillSingle)
{
    BWTest test;
    test.map = Maps::GetOne("Roadkill");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveSingle)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveSingleOneCannon)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 1);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveTestSuite)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    auto sgl = runTestSuite(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveTestSuiteContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
        test.randomSeed = 42;
        auto sgl = runTestSuite(test, 1, 0);
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Single: " << sgl << std::endl;
    }
}

TEST(MiningTraining, NeoMoonGlaiveSingleMeasure)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0, false, true, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, PowerBondSingleTwoCannonsMeasure)
{
    BWTest test;
    test.map = Maps::GetOne("PowerBond");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 2, false, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveSingleMeasureShort)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    test.frameLimit = 1000;
    auto sgl = runEfficiencyTest(test, 1, 0, false, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveSingleContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
        test.randomSeed = 42;
        auto sgl = runEfficiencyTest(test, 1, 0);
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Single: " << sgl << std::endl;
    }
}

TEST(MiningTraining, NeoMoonGlaiveSingleOneWorker)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    test.frameLimit = 10000;
    auto sgl = runEfficiencyTest(test, 1, 0, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveSingleOneWorkerMeasure)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    test.frameLimit = 10000;
    auto sgl = runEfficiencyTest(test, 1, 0, true, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveDouble)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
//    test.frameLimit = 3500;
    auto dbl = runEfficiencyTest(test, 2, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Double: " << dbl << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveDoubleContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
        test.randomSeed = 42;
//    test.frameLimit = 3500;
        auto dbl = runEfficiencyTest(test, 2, 0);
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Double: " << dbl << std::endl;
    }
}

TEST(MiningTraining, VermeerTestSuite)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto sgl = runTestSuite(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, VermeerTestSuiteContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("VermeerSE_2.1");
        test.randomSeed = 42;
        auto sgl = runTestSuite(test, 1, 0);
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Single: " << sgl << std::endl;
    }
}

TEST(MiningTraining, VermeerTestSuiteSingleAllCannonsContinuous)
{
    while (true)
    {
        for (int cannons = 0; cannons < 3; cannons++)
        {
            BWTest test;
            test.map = Maps::GetOne("VermeerSE_2.1");
            test.randomSeed = 42;
            auto sgl = runTestSuite(test, 1, cannons);
            std::cout << std::fixed << std::showpoint << std::setprecision(4)
                      << "Overall efficiency for " << cannons << " cannon(s): " << std::endl
                      << "Single: " << sgl << std::endl;
        }
    }
}

TEST(MiningTraining, VermeerTestSuiteDouble)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto sgl = runTestSuite(test, 2, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, EclipseDouble)
{
    BWTest test;
    test.map = Maps::GetOne("Eclipse");
    test.randomSeed = 42;
    auto dbl = runEfficiencyTestImpl(test, 2, 0, false, false, 3);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Double: " << dbl << std::endl;
}

TEST(MiningTraining, VermeerTestSuiteDoubleContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("VermeerSE_2.1");
        test.randomSeed = 42;
        auto sgl = runTestSuite(test, 2, 0);
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Single: " << sgl << std::endl;
    }
}

TEST(MiningTraining, VermeerTestSuiteSingleAndDoubleContinuous)
{
    while (true)
    {
        BWTest test;
        test.map = Maps::GetOne("VermeerSE_2.1");
        test.randomSeed = 42;
        auto sgl = runTestSuite(test, 1, 0);
        auto dbl = runTestSuite(test, 2, 0);
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Single: " << sgl << std::endl
                  << "Double: " << dbl << std::endl;
    }
}

TEST(MiningTraining, VermeerTestSuiteSingleAndDouble)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto sgl = runTestSuite(test, 1, 0);
    auto dbl = runTestSuite(test, 2, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl
              << "Double: " << dbl << std::endl;
}

TEST(MiningTraining, VermeerSingleAndDoubleMeasure)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0, false, true, true);
    auto dbl = runEfficiencyTest(test, 2, 0, false, true, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl
              << "Double: " << dbl << std::endl;
}

TEST(MiningTraining, VermeerStabilityTest)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    runEfficiencyTest(test, 1, 0, false);
    runEfficiencyTest(test, 1, 0, false, true, true);
}

TEST(MiningTraining, VermeerSingle)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0, false);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, VermeerDouble)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto dbl = runEfficiencyTest(test, 2, 0, false);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Double: " << dbl << std::endl;
}

TEST(MiningTraining, VermeerSingleMeasure)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0, false, true, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, VermeerDoubleMeasure)
{
    BWTest test;
    test.map = Maps::GetOne("VermeerSE_2.1");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 2, 0, false, true, true);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << std::endl;
}

TEST(MiningTraining, VermeerContinuousWithResults)
{
    while (true)
    {
        testRunWithResults("VermeerSE_2.1", 0, true);
    }
}

TEST(MiningTraining, AllAIIDEMeasure)
{
    testRunWithResults("aiide2024", 0, true, true);
}

TEST(MiningTraining, AllAIIDEContinuous)
{
    while (true)
    {
        testRunWithResults("aiide2024", 0, true);
    }
}

TEST(MiningTraining, AllAIIDEContinuousNoCannons)
{
    while (true)
    {
        testRunWithResults("aiide2024", 0, false);
    }
}

TEST(MiningTraining, AllAIIDESingleOnlyNoCannons)
{
    testRunWithResults("aiide2024", 1, false);
}

TEST(MiningTraining, AllAIIDEDoubleOnlyNoCannons)
{
    testRunWithResults("aiide2024", 2, false);
}

TEST(MiningTraining, AllAIIDEMeasureSingleNoCannons)
{
    testRunWithResults("aiide2024", 1, false, true);
}

TEST(MiningTraining, AllAIIDEMeasureDoubleNoCannons)
{
    testRunWithResults("aiide2024", 2, false, true);
}

TEST(MiningTraining, AllAIIDEMeasureSingleAllCannons)
{
    testRunWithResults("aiide2024", 1, true, true);
}
