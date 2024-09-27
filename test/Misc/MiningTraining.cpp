#include "BWTest.h"
#include "DoNothingModule.h"
#include "ClearOpponentUnitsModule.h"
#include "DoNothingStrategyEngine.h"

#include "Map.h"
#include "Strategist.h"
#include "TestMainArmyAttackBasePlay.h"
#include "Plays/Macro/SaturateBases.h"
#include "WorkerMiningInstrumentation.h"
#include "Units.h"
#include "Workers.h"

namespace
{
    const std::string dataBasePath = "/Users/bmnielsen/BW/mining-timings/";

    std::map<std::string, std::map<std::pair<int, int>, double>> mapHashToConfigurationToEfficiency;

    double runEfficiencyTest(BWTest &test, int workersPerPatch, int cannons, bool onlyOneWorker = false)
    {
        test.opponentRace = BWAPI::Races::Terran;
        test.opponentModule = []()
        {
            return new ClearOpponentUnitsModule();
        };
        if (test.frameLimit > 10000) test.frameLimit = 10000;
        test.expectWin = false;

        std::ostringstream replayNameBuilder;
        replayNameBuilder << "MiningTraining_" << test.map->shortname() << "_";
        replayNameBuilder << workersPerPatch << "wpp_" << cannons << "cannons";
        test.replayName = replayNameBuilder.str();

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
            // Initialization steps:
            // - Kill initial workers
            // - Add observers at expansions
            // - Add depots at expansions
            // - Add pylons at each base
            // - Add forge at main base
            // - Add required number of cannons
            // - Create workers

            switch (BWAPI::Broodwar->getFrameCount())
            {
                case 0:
                {
                    for (auto unit : BWAPI::Broodwar->self()->getUnits())
                    {
                        if (unit->getType().isWorker())
                        {
                            BWAPI::Broodwar->killUnit(unit);
                        }
                    }

                    for (auto base : Map::allBases())
                    {
                        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, base->getPosition());
                    }

                    break;
                }
                case 5:
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

                    break;
                }
                case 10:
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

                    break;
                }
                case 15:
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

                    break;
                }
                case 20:
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

                    break;
                }
                case 23:
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
                                Log::Get() << "ERROR: Couldn't get base patches for worker @ " << worker->lastPosition;
                            }
                        }
                        else
                        {
                            Log::Get() << "ERROR: Couldn't get index for worker @ " << worker->lastPosition;
                        }
                    }
                }
                default:
                {
                    break;
                }
            }
        };

        double result = 0.0;
        test.onEndMine = [&](bool)
        {
            auto efficiency = WorkerMiningInstrumentation::getEfficiency();
            if (workersPerPatch == 1)
            {
                result = efficiency.first;
            }
            else
            {
                result = efficiency.second;
            }

            mapHashToConfigurationToEfficiency[BWAPI::Broodwar->mapHash()][std::make_pair(workersPerPatch, cannons)] = result;
        };

        test.run();

        std::cout << "Mining efficiency: " << result << std::endl;
        return result;
    }
}

TEST(MiningTraining, FightingSpirit)
{
    BWTest test;
    test.map = Maps::GetOne("Fighting Spirit");
    test.randomSeed = 42;
    double totalSingle = 0.0;
    double totalDouble = 0.0;
    for (auto workersPerPatch = 1; workersPerPatch <= 2; workersPerPatch++)
    {
        for (auto cannons = 0; cannons <= 2; cannons++)
        {
            (workersPerPatch == 1 ? totalSingle : totalDouble) += runEfficiencyTest(test, workersPerPatch, cannons);
        }
    }
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Single: " << (totalSingle / 3) << "%" << std::endl
        << "Double: " << (totalDouble / 3) << "%" << std::endl;
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
        << "Single: " << sgl << "%" << std::endl
        << "Double: " << dbl << "%" << std::endl;
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
        << "Single: " << sgl << "%" << std::endl;
}

TEST(MiningTraining, ChupungRyeongSingle)
{
    BWTest test;
    test.map = Maps::GetOne("Chupung");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
        << "Overall efficiency: " << std::endl
        << "Single: " << sgl << "%" << std::endl;
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
        << "One cannon: " << c1 << "%" << std::endl
        << "Two cannons: " << c2 << "%" << std::endl;
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
                  << "Single: " << sgl << "%" << std::endl;
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
        << "Double: " << dbl << "%" << std::endl;
}

TEST(MiningTraining, NeoMoonGlaiveSingle)
{
    BWTest test;
    test.map = Maps::GetOne("NeoMoonGlaive_2.1_KeSPA");
    test.randomSeed = 42;
    auto sgl = runEfficiencyTest(test, 1, 0);
    std::cout << std::fixed << std::showpoint << std::setprecision(4)
              << "Overall efficiency: " << std::endl
              << "Single: " << sgl << "%" << std::endl;
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
              << "Single: " << sgl << "%" << std::endl;
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
        << "Double: " << dbl << "%" << std::endl;
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
                  << "Double: " << dbl << "%" << std::endl;
    }
}

TEST(MiningTraining, AllAIIDE)
{
    Maps::RunOnEach(Maps::Get("aiide2023"), [](BWTest test)
    {
        double totalSingle = 0.0;
        double totalDouble = 0.0;
        for (auto workersPerPatch = 1; workersPerPatch <= 2; workersPerPatch++)
        {
            for (auto cannons = 0; cannons <= 2; cannons++)
            {
                (workersPerPatch == 1 ? totalSingle : totalDouble) += runEfficiencyTest(test, workersPerPatch, cannons);
            }
        }
        std::cout << std::fixed << std::showpoint << std::setprecision(4)
                  << "Overall efficiency: " << std::endl
                  << "Single: " << (totalSingle / 3) << "%" << std::endl
                  << "Double: " << (totalDouble / 3) << "%" << std::endl;
    });

    {
        std::ofstream file;
        auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        auto tm = std::localtime(&tt);
        file.open((std::ostringstream() << dataBasePath << "miningtraining_" << std::put_time(tm, "%Y%m%d_%H%M%S") << ".csv").str(),
                  std::ofstream::trunc);
        file << "Hash;1-0;1-1;1-2;2-0;2-1;2-2\n";
        std::map<std::pair<int, int>, double> totals;
        for (const auto &[mapHash, _] : mapHashToConfigurationToEfficiency)
        {
            file << mapHash;

            for (int workers = 1; workers <= 2; workers++)
            {
                for (int cannons = 0; cannons <= 2; cannons++)
                {
                    auto key = std::make_pair(workers, cannons);
                    auto result = mapHashToConfigurationToEfficiency[mapHash][key];
                    file << ";" << result;
                    totals[key] += result;
                }
            }

            file << "\n";
        }

        file << "Average";
        for (int workers = 1; workers <= 2; workers++)
        {
            for (int cannons = 0; cannons <= 2; cannons++)
            {
                auto key = std::make_pair(workers, cannons);
                file << ";" << (totals[key] / (double)(mapHashToConfigurationToEfficiency.size()));
            }
        }
        file << "\n";

        file.close();
    }
}

TEST(MiningTraining, AllAIIDEContinuous)
{
    while (true)
    {
        Maps::RunOnEach(Maps::Get("aiide2023"), [](BWTest test)
        {
            double totalSingle = 0.0;
            double totalDouble = 0.0;
            for (auto workersPerPatch = 1; workersPerPatch <= 2; workersPerPatch++)
            {
                for (auto cannons = 0; cannons <= 2; cannons++)
                {
                    (workersPerPatch == 1 ? totalSingle : totalDouble) += runEfficiencyTest(test, workersPerPatch, cannons);
                }
            }
            std::cout << std::fixed << std::showpoint << std::setprecision(4)
                      << "Overall efficiency: " << std::endl
                      << "Single: " << (totalSingle / 3) << "%" << std::endl
                      << "Double: " << (totalDouble / 3) << "%" << std::endl;
        });

        {
            std::ofstream file;
            auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto tm = std::localtime(&tt);
            file.open((std::ostringstream() << dataBasePath << "miningtraining_" << std::put_time(tm, "%Y%m%d_%H%M%S") << ".csv").str(),
                      std::ofstream::trunc);
            file << "Hash;1-0;1-1;1-2;2-0;2-1;2-2\n";
            std::map<std::pair<int, int>, double> totals;
            for (const auto &[mapHash, _] : mapHashToConfigurationToEfficiency)
            {
                file << mapHash;

                for (int workers = 1; workers <= 2; workers++)
                {
                    for (int cannons = 0; cannons <= 2; cannons++)
                    {
                        auto key = std::make_pair(workers, cannons);
                        auto result = mapHashToConfigurationToEfficiency[mapHash][key];
                        file << ";" << result;
                        totals[key] += result;
                    }
                }

                file << "\n";
            }

            file << "Average";
            for (int workers = 1; workers <= 2; workers++)
            {
                for (int cannons = 0; cannons <= 2; cannons++)
                {
                    auto key = std::make_pair(workers, cannons);
                    file << ";" << (totals[key] / (double)(mapHashToConfigurationToEfficiency.size()));
                }
            }
            file << "\n";

            file.close();
        }
    }
}

TEST(MiningTraining, AllAIIDEContinuousSingleOnly)
{
    while (true)
    {
        Maps::RunOnEach(Maps::Get("aiide2023"), [](BWTest test)
        {
            double totalSingle = 0.0;
            for (auto cannons = 0; cannons <= 2; cannons++)
            {
                totalSingle += runEfficiencyTest(test, 1, cannons);
            }
            std::cout << std::fixed << std::showpoint << std::setprecision(4)
                      << "Overall efficiency: " << std::endl
                      << "Single: " << (totalSingle / 3) << "%" << std::endl;
        });

        {
            std::ofstream file;
            auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto tm = std::localtime(&tt);
            file.open((std::ostringstream() << dataBasePath << "miningtraining_" << std::put_time(tm, "%Y%m%d_%H%M%S") << ".csv").str(),
                      std::ofstream::trunc);
            file << "Hash;1-0;1-1;1-2\n";
            std::map<std::pair<int, int>, double> totals;
            for (const auto &[mapHash, _] : mapHashToConfigurationToEfficiency)
            {
                file << mapHash;

                for (int cannons = 0; cannons <= 2; cannons++)
                {
                    auto key = std::make_pair(1, cannons);
                    auto result = mapHashToConfigurationToEfficiency[mapHash][key];
                    file << ";" << result;
                    totals[key] += result;
                }

                file << "\n";
            }

            file << "Average";
            for (int cannons = 0; cannons <= 2; cannons++)
            {
                auto key = std::make_pair(1, cannons);
                file << ";" << (totals[key] / (double)(mapHashToConfigurationToEfficiency.size()));
            }
            file << "\n";

            file.close();
        }
    }
}

TEST(MiningTraining, AllAIIDEContinuousNoCannons)
{
    while (true)
    {
        Maps::RunOnEach(Maps::Get("aiide2023"), [](BWTest test)
        {
            double totalSingle = 0.0;
            double totalDouble = 0.0;
            for (auto workersPerPatch = 1; workersPerPatch <= 2; workersPerPatch++)
            {
                (workersPerPatch == 1 ? totalSingle : totalDouble) += runEfficiencyTest(test, workersPerPatch, 0);
            }
            std::cout << std::fixed << std::showpoint << std::setprecision(4)
                      << "Overall efficiency: " << std::endl
                      << "Single: " << (totalSingle) << "%" << std::endl
                      << "Double: " << (totalDouble) << "%" << std::endl;
        });

        {
            std::ofstream file;
            auto tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto tm = std::localtime(&tt);
            file.open((std::ostringstream() << dataBasePath << "miningtraining_" << std::put_time(tm, "%Y%m%d_%H%M%S") << ".csv").str(),
                      std::ofstream::trunc);
            file << "Hash;1-0;2-0\n";
            std::map<std::pair<int, int>, double> totals;
            for (const auto &[mapHash, _] : mapHashToConfigurationToEfficiency)
            {
                file << mapHash;

                for (int workers = 1; workers <= 2; workers++)
                {
                    auto key = std::make_pair(workers, 0);
                    auto result = mapHashToConfigurationToEfficiency[mapHash][key];
                    file << ";" << result;
                    totals[key] += result;
                }

                file << "\n";
            }

            file << "Average";
            for (int workers = 1; workers <= 2; workers++)
            {
                auto key = std::make_pair(workers, 0);
                file << ";" << (totals[key] / (double)(mapHashToConfigurationToEfficiency.size()));
            }
            file << "\n";

            file.close();
        }
    }
}
