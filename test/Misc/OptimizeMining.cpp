#include "BWTest.h"
#include "DoNothingModule.h"

#include <bwem.h>

#include "Log.h"
#include "CherryVis.h"
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

    BWAPI::Unit getPatchUnit(BWAPI::TilePosition patchTile)
    {
        if (patchTile == BWAPI::TilePositions::Invalid) return nullptr;

        for (auto &unit : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (unit->getInitialTilePosition() == patchTile) return unit;
        }

        return nullptr;
    }

    BWAPI::Unit getDepotUnit(BWAPI::TilePosition depotTile)
    {
        if (depotTile == BWAPI::TilePositions::Invalid) return nullptr;

        for (auto &unit : BWAPI::Broodwar->self()->getUnits())
        {
            if (unit->getTilePosition() == depotTile) return unit;
        }

        return nullptr;
    }

    class OptimizePatchModule : public DoNothingModule
    {
        BWAPI::TilePosition patchTile;
        BWAPI::TilePosition depotTile;

        int state;
        int lastStateChange;
        std::vector<BWAPI::Position> probeStartingPositions;
        BWAPI::Unit probe;

    public:
        explicit OptimizePatchModule(BWAPI::TilePosition patchTile)
                : patchTile(patchTile)
                , depotTile(BWAPI::TilePositions::Invalid)
                , state(0)
                , lastStateChange(0)
                , probe(nullptr)
        {}

        void onStart() override
        {
            Log::initialize();
            Log::SetDebug(true);
            Log::SetOutputToConsole(true);
            CherryVis::initialize();
            CherryVis::setBoardValue("state", "initializing");

            BWEM::Map::ResetInstance();
            BWEM::Map::Instance().Initialize(BWAPI::BroodwarPtr);
            BWEM::Map::Instance().EnableAutomaticPathAnalysis();
            BWEM::Map::Instance().FindBasesForStartingLocations();

            auto patch = getPatchUnit(patchTile);
            if (!patch)
            {
                Log::Get() << "ERROR: Could not get patch unit";
                return;
            }

            int bestDist = INT_MAX;
            for (const auto &area : BWEM::Map::Instance().Areas())
            {
                for (const auto &base : area.Bases())
                {
                    int dist = base.Center().getApproxDistance(patch->getPosition());
                    if (dist < bestDist)
                    {
                        depotTile = base.Location();
                        bestDist = dist;
                    }
                }
            }

            if (depotTile == BWAPI::TilePositions::Invalid)
            {
                Log::Get() << "ERROR: Could not get depot tile";
                return;
            }

            auto depotCenter = BWAPI::Position(depotTile) + BWAPI::Position(64, 48);

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, patch->getPosition());
            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                        BWAPI::UnitTypes::Protoss_Observer,
                                        depotCenter);

            // Kill our starting workers so they can't get in the way
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }

            // Generate all positions we want to start mining from
            // The "face" of the mineral patch in play is the one between the two closest corners
            auto corners = {BWAPI::Position(patchTile),
                            BWAPI::Position(patchTile) + BWAPI::Position(63, 0),
                            BWAPI::Position(patchTile) + BWAPI::Position(0, 31),
                            BWAPI::Position(patchTile) + BWAPI::Position(63, 31)};
            BWAPI::Position closest = BWAPI::Positions::Invalid;
            BWAPI::Position nextClosest = BWAPI::Positions::Invalid;
            int closestDist = INT_MAX;
            int nextClosestDist = INT_MAX;
            for (const auto &corner : corners)
            {
                int dist = Geo::EdgeToPointDistance(BWAPI::UnitTypes::Protoss_Nexus, depotCenter, corner);
                if (dist < closestDist)
                {
                    nextClosestDist = closestDist;
                    nextClosest = closest;

                    closestDist = dist;
                    closest = corner;
                }
                else if (dist < nextClosestDist)
                {
                    nextClosestDist = dist;
                    nextClosest = corner;
                }
            }

            if (closest.x == nextClosest.x)
            {
                for (int y = std::min(closest.y, nextClosest.y); y <= std::max(closest.y, nextClosest.y); y++)
                {
                    probeStartingPositions.emplace_back((BWAPI::Position(depotTile).x > closest.x) ? (closest.x + 11) : (closest.x - 11), y);
                }
            }
            else
            {
                for (int x = std::min(closest.x, nextClosest.x); x <= std::max(closest.x, nextClosest.x); x++)
                {
                    probeStartingPositions.emplace_back(x, (BWAPI::Position(depotTile).y > closest.y) ? (closest.y + 11) : (closest.y - 11));
                }
            }

            for (auto &pos : probeStartingPositions)
            {
                CherryVis::log() << "Starting position @ " << BWAPI::WalkPosition(pos) << "; " << pos;
            }
        }

        void onFrame() override
        {
            currentFrame++;

            auto patch = getPatchUnit(patchTile);
            if (!patch || !depotTile.isValid() || probeStartingPositions.empty())
            {
                if (!patch)
                {
                    Log::Get() << "Complete; patch unit not available";
                }
                else if (!depotTile.isValid())
                {
                    Log::Get() << "Complete; depot tile not available";
                }
                else
                {
                    Log::Get() << "Complete; no more probe starting positions";
                }
                BWAPI::Broodwar->leaveGame();
                CherryVis::frameEnd(currentFrame);
                return;
            }

            auto depot = getDepotUnit(depotTile);
            if (!depot)
            {
                CherryVis::setBoardValue("status", "creating-depot");
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Nexus,
                                            Geo::CenterOfUnit(depotTile, BWAPI::UnitTypes::Protoss_Nexus));
                CherryVis::frameEnd(currentFrame);
                return;
            }

            auto currentStartingPosition = *probeStartingPositions.rbegin();

            switch (state)
            {
                case 0:
                {
                    CherryVis::setBoardValue("status", "creating-probe");

                    CherryVis::log() << "Creating probe @ " << BWAPI::WalkPosition(currentStartingPosition) << "; "
                                     << currentStartingPosition;

                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Probe, currentStartingPosition);
                    state = 1;
                    lastStateChange = currentFrame;
                    break;
                }
                case 1:
                {
                    CherryVis::setBoardValue("status", "detecting-probe");

                    // If it took too long to create the probe, this position is probably blocked
                    if ((currentFrame - lastStateChange) > 10)
                    {
                        CherryVis::log() << "Gave up creating probe @ " << BWAPI::WalkPosition(currentStartingPosition) << "; "
                                         << currentStartingPosition;

                        state = 100;
                        lastStateChange = currentFrame;
                        break;
                    }

                    for (auto &unit : BWAPI::Broodwar->self()->getUnits())
                    {
                        if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
                        {
                            CherryVis::log() << "Detected new probe @ " << BWAPI::WalkPosition(unit->getPosition()) << "; " << unit->getPosition();

                            probe = unit;
                            state = 2;
                            lastStateChange = currentFrame;
                        }
                    }
                }
                case 2:
                {
                    CherryVis::setBoardValue("status", "waiting-to-kill-probe");
                    if ((currentFrame - lastStateChange) > 50)
                    {
                        BWAPI::Broodwar->killUnit(probe);
                        probe = nullptr;

                        state = 100;
                        lastStateChange = currentFrame;
                        break;
                    }

                    break;
                }
                case 100:
                {
                    CherryVis::setBoardValue("status", "resetting-for-next-probe");
                    if ((currentFrame - lastStateChange) > 10)
                    {
                        probeStartingPositions.pop_back();
                        state = 0;
                        lastStateChange = currentFrame;
                        break;
                    }

                    break;
                }
            }

            CherryVis::frameEnd(currentFrame);
        }

        void onEnd(bool isWinner) override
        {
            CherryVis::gameEnd();
        }

        void onUnitCreate(BWAPI::Unit unit) override
        {
            CherryVis::unitFirstSeen(unit);
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
                if (!depot->getType().isResourceDepot())
                {
                    BWAPI::Broodwar->killUnit(depot);
                    continue;
                }

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
