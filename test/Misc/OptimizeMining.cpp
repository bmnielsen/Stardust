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

    void optimizeAllPatches(BWTest &test)
    {
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

        std::cout << "Read " << patches.size() << " patches to optimize" << std::endl;
    }
}

TEST(OptimizeMining, WritePatchesFiles_AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [](BWTest test)
    {
        writePatchesFile(test);
    });
}

TEST(OptimizeMining, OptimizeAllPatches_AllSSCAIT)
{
    Maps::RunOnEach(Maps::Get("sscai"), [](BWTest test)
    {
        optimizeAllPatches(test);
    });
}
