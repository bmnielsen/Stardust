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
    const std::string dataBasePath = "/Users/bmnielsen/BW/mining-timings/";

    void writePatchesFile(BWTest &test)
    {
        test.opponentModule = test.myModule = []()
        {
            return new DoNothingModule();
        };
        test.frameLimit = 10;
        test.expectWin = false;
        test.writeReplay = false;

        test.onStartMine = []()
        {
            std::ofstream file;
            file.open((std::ostringstream() << dataBasePath << BWAPI::Broodwar->mapHash() << "_patches.csv").str(), std::ofstream::trunc);
            file << "Patch Tile X;Patch Tile Y\n";

            for (auto unit : BWAPI::Broodwar->getNeutralUnits())
            {
                if (!unit->getType().isMineralField()) continue;
                if (unit->getInitialResources() < 200) continue;

                file << unit->getInitialTilePosition().x
                     << ";" << unit->getInitialTilePosition().y
                     << "\n";
            }

            file.close();
        };

        test.run();
    }

    std::vector<std::string> readCsvLine(std::istream &str)
    {
        std::vector<std::string> result;
        try
        {
            std::string line;
            std::getline(str, line);

            std::stringstream lineStream(line);
            std::string cell;

            while (std::getline(lineStream, cell, ';'))
            {
                result.push_back(cell);
            }
        }
        catch (std::exception &ex)
        {
            std::cout << "Exception reading CSV line: " << ex.what() << std::endl;
        }
        return result;
    }

    class OptimizePatchModule : public DoNothingModule
    {
        BWAPI::TilePosition patchTile;

    public:
        OptimizePatchModule(BWAPI::TilePosition patchTile) : patchTile(patchTile) {}

        void onFrame() override
        {

        }
    };

    void optimizePatch(BWTest &test, BWAPI::TilePosition patch)
    {
        test.myModule = [&patch]()
        {
            return new OptimizePatchModule(patch);
        };

        // Remove the enemy's depot so we can test patches at that location
        test.onFrameOpponent = []()
        {
            if (BWAPI::Broodwar->getFrameCount() != 5 && BWAPI::Broodwar->getFrameCount() != 10) return;

            for (auto depot : BWAPI::Broodwar->self()->getUnits())
            {
                if (!depot->getType().isResourceDepot()) continue;

                if (BWAPI::Broodwar->getFrameCount() == 10)
                {
                    BWAPI::Broodwar->removeUnit(depot);
                    return;
                }

                int totalX = 0;
                int totalY = 0;
                int count = 0;
                for (auto mineral : BWAPI::Broodwar->getNeutralUnits())
                {
                    if (!mineral->getType().isMineralField()) continue;
                    if (depot->getDistance(mineral) > 320) continue;
                    totalX += mineral->getPosition().x;
                    totalY += mineral->getPosition().y;
                    count++;
                }

                auto diffX = (totalX / count) - depot->getPosition().x;
                auto diffY = (totalY / count) - depot->getPosition().y;
                BWAPI::TilePosition pylon;
                if (diffX > 0 && diffY > 0)
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(0, 3);
                }
                else if (diffX > 0 && diffY < 0)
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(-2, 0);
                }
                else if (diffX < 0 && diffY < 0)
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(4, 1);
                }
                else
                {
                    pylon = depot->getTilePosition() + BWAPI::TilePosition(2, -2);
                }

                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Pylon,
                                            Geo::CenterOfUnit(pylon, BWAPI::UnitTypes::Protoss_Pylon));
            }
        };

        test.run();
    }

    void optimizeAllPatches(BWTest &test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentRace = BWAPI::Races::Protoss;
        test.frameLimit = 15000;
        test.expectWin = false;
        test.writeReplay = true;

        std::vector<BWAPI::TilePosition> patches;

        // Try to load the patch coordinates
        std::ifstream file;
        file.open((std::ostringstream() << dataBasePath << test.map->openbwHash << "_patches.csv").str());
        if (!file.good())
        {
            std::cout << "No patches available for " << test.map->filename << std::endl;
            return;
        }
        try
        {
            readCsvLine(file); // Header row; ignored
            while (true)
            {
                auto line = readCsvLine(file);
                if (line.size() != 2) break;

                patches.emplace_back(std::stoi(line[0]), std::stoi(line[1]));
            }
        }
        catch (std::exception &ex)
        {
            std::cout << "Exception caught attempting to read patch locations: " << ex.what() << std::endl;
            return;
        }

        for (const auto &patch : patches)
        {
            optimizePatch(test, patch);
            return;
        }
    }
}

TEST(OptimizeMining, WritePatchesFiles_AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [](BWTest test)
    {
        writePatchesFile(test);
    });
}

TEST(OptimizeMining, OptimizeAllPatches_FightingSpirit)
{
    Maps::RunOnEach(Maps::Get("sscai/(4)Fighting"), [](BWTest test)
    {
        optimizeAllPatches(test);
    });
}

TEST(OptimizeMining, OptimizeAllPatches_AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [](BWTest test)
    {
        optimizeAllPatches(test);
    });
}
