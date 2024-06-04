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

    struct MiningPath
    {
        explicit MiningPath(BWAPI::Position gatherPosition) : gatherPosition(gatherPosition), returnPosition(BWAPI::Positions::Invalid)
        {
            returnPath.emplace_back(gatherPosition);
        }

        BWAPI::Position gatherPosition;
        BWAPI::Position returnPosition;
        std::vector<BWAPI::Position> gatherPath;
        std::vector<BWAPI::Position> returnPath;
    };

    struct PatchInfo
    {
        BWAPI::TilePosition depotTile;
        std::vector<BWAPI::Position> probeStartingPositions;
        std::map<BWAPI::Position, std::vector<MiningPath>> startingPositionToMiningPaths;
    };

    class OptimizePatchModule : public DoNothingModule
    {
        int state;
        int lastStateChange;
        BWAPI::Unit probe;

        std::vector<BWAPI::TilePosition> patchTiles;
        std::map<BWAPI::TilePosition, PatchInfo> tileToPatchInfo;

    public:
        explicit OptimizePatchModule(const std::vector<BWAPI::TilePosition> &patchTiles)
                : state(0)
                , lastStateChange(0)
                , probe(nullptr)
                , patchTiles(patchTiles)
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

            // Kill our starting workers so they can't get in the way
            for (auto unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (unit->getType().isWorker())
                {
                    BWAPI::Broodwar->killUnit(unit);
                }
            }

            std::set<BWAPI::Position> depotCenters;

            for (const auto &patchTile : patchTiles)
            {
                auto patch = getPatchUnit(patchTile);
                if (!patch)
                {
                    Log::Get() << "ERROR: Could not get patch unit for patch @ " << patchTile;
                    continue;
                }

                BWAPI::TilePosition depotTile = BWAPI::TilePositions::Invalid;
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
                    Log::Get() << "ERROR: Could not get depot tile for patch @ " << patchTile;
                    continue;
                }

                auto depotCenter = BWAPI::Position(depotTile) + BWAPI::Position(64, 48);
                depotCenters.insert(depotCenter);

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

                std::vector<BWAPI::Position> probeStartingPositions;
                if (closest.x == nextClosest.x)
                {
                    for (int y = std::min(closest.y, nextClosest.y) - 12; y <= std::max(closest.y, nextClosest.y) + 12; y+=4)
                    {
                        auto pos = BWAPI::Position((BWAPI::Position(depotTile).x > closest.x) ? (closest.x + 12) : (closest.x - 12), y);
                        if (pos.isValid()) probeStartingPositions.emplace_back(pos);
                    }
                }
                else
                {
                    for (int x = std::min(closest.x, nextClosest.x) - 12; x <= std::max(closest.x, nextClosest.x) + 12; x+=4)
                    {
                        auto pos = BWAPI::Position(x, (BWAPI::Position(depotTile).y > closest.y) ? (closest.y + 12) : (closest.y - 12));
                        if (pos.isValid()) probeStartingPositions.emplace_back(pos);
                    }
                }

                tileToPatchInfo.emplace(patchTile, PatchInfo{depotTile, probeStartingPositions});
            }

            for (const auto &depotCenter : depotCenters)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, depotCenter);
            }
        }

        void onFrame() override
        {
            if (state == -1) return;

            currentFrame++;

            if (patchTiles.empty())
            {
                Log::Get() << "Complete; no more patches to analyze";

                // TODO Analyze and dump results

                BWAPI::Broodwar->leaveGame();
                CherryVis::frameEnd(currentFrame);
                state = -1;
                return;
            }

            auto &patchTile = *patchTiles.rbegin();

            auto patch = getPatchUnit(patchTile);
            if (!patch)
            {
                Log::Get() << "ERROR: Patch unit for " << patchTile << " not available";
                patchTiles.pop_back();
                CherryVis::frameEnd(currentFrame);
                return;
            }

            auto &patchInfo = tileToPatchInfo[patchTile];
            auto depot = getDepotUnit(patchInfo.depotTile);
            if (!depot)
            {
                CherryVis::setBoardValue("status", "creating-depot");
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(),
                                            BWAPI::UnitTypes::Protoss_Nexus,
                                            Geo::CenterOfUnit(patchInfo.depotTile, BWAPI::UnitTypes::Protoss_Nexus));
                CherryVis::frameEnd(currentFrame);
                return;
            }

            auto &probeStartingPositions = patchInfo.probeStartingPositions;
            auto &startingPositionToMiningPaths = patchInfo.startingPositionToMiningPaths;

            if (probeStartingPositions.empty())
            {
                patchTiles.pop_back();
                CherryVis::frameEnd(currentFrame);
                return;
            }

            auto &currentStartingPosition = *probeStartingPositions.rbegin();

            auto setState = [&](int toState)
            {
                state = toState;
                lastStateChange = currentFrame;
            };

            switch (state)
            {
                case 0:
                {
                    CherryVis::setBoardValue("status", "creating-probe");

                    CherryVis::log() << "Creating probe @ " << BWAPI::WalkPosition(currentStartingPosition) << "; "
                                     << currentStartingPosition;

                    BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Probe, currentStartingPosition);

                    setState(1);
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

                        setState(100);
                        break;
                    }

                    for (auto &unit : BWAPI::Broodwar->self()->getUnits())
                    {
                        if (unit->getType() == BWAPI::UnitTypes::Protoss_Probe)
                        {
                            CherryVis::log() << "Detected new probe @ " << BWAPI::WalkPosition(unit->getPosition()) << "; " << unit->getPosition();

                            probe = unit;

                            if (unit->getPosition() != currentStartingPosition)
                            {
                                CherryVis::log() << "Wrong position, assume this starting position is blocked";
                                setState(100);
                                break;
                            }

                            setState(2);
                            break;
                        }
                    }

                    break;
                }
                case 2:
                {
                    CherryVis::setBoardValue("status", "init-measuring-gather-paths");

                    probe->gather(patch);
                    startingPositionToMiningPaths.emplace(currentStartingPosition, std::vector<MiningPath>());

                    setState(3);
                    break;
                }
                case 3:
                {
                    CherryVis::setBoardValue("status", "measure-gather-paths-mine");

                    auto &miningPaths = startingPositionToMiningPaths[currentStartingPosition];

                    if (!miningPaths.empty())
                    {
                        auto &gatherPath = (*miningPaths.rbegin()).gatherPath;
                        if (probe->getDistance(patch) != 0 || probe->getPosition() != *gatherPath.rbegin())
                        {
                            gatherPath.emplace_back(probe->getPosition());
                        }
                    }

                    if (miningPaths.size() == 5 && probe->getDistance(patch) == 0)
                    {
                        setState(100);
                        break;
                    }

                    if (probe->isCarryingMinerals())
                    {
                        patch->setResources(patch->getInitialResources());
                        miningPaths.emplace_back(probe->getPosition());
                        setState(4);
                        break;
                    }

                    break;
                }
                case 4:
                {
                    auto &miningPaths = startingPositionToMiningPaths[currentStartingPosition];
                    auto &returnPath = (*miningPaths.rbegin()).returnPath;
                    if (probe->getDistance(depot) != 0 || probe->getPosition() != *returnPath.rbegin())
                    {
                        returnPath.push_back(probe->getPosition());
                        if (probe->getDistance(depot) == 0)
                        {
                            (*miningPaths.rbegin()).returnPosition = probe->getPosition();
                        }
                    }

                    if (!probe->isCarryingMinerals()) setState(3);

                    break;
                }
                case 100:
                {
                    CherryVis::setBoardValue("status", "resetting");

                    if (probe)
                    {
                        BWAPI::Broodwar->killUnit(probe);
                        probe = nullptr;
                    }

                    if ((currentFrame - lastStateChange) > 10)
                    {
                        probeStartingPositions.pop_back();
                        setState(0);
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

    void optimizeAllPatches(BWTest &test)
    {
        test.opponentModule = []()
        {
            return new DoNothingModule();
        };
        test.opponentRace = BWAPI::Races::Protoss;
        test.frameLimit = 1000000;
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

        test.myModule = [&patches]()
        {
            return new OptimizePatchModule(patches);
        };

        // Remove the enemy's depot so we can test patches at that location
        test.onFrameOpponent = []()
        {
            if (BWAPI::Broodwar->getFrameCount() == 1)
            {
                for (auto worker : BWAPI::Broodwar->self()->getUnits())
                {
                    if (worker->getType().isWorker()) BWAPI::Broodwar->killUnit(worker);
                }
            }

            if (BWAPI::Broodwar->getFrameCount() == 10)
            {
                for (auto depot : BWAPI::Broodwar->self()->getUnits())
                {
                    if (!depot->getType().isResourceDepot())
                    {
                        continue;
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
            }

            if (BWAPI::Broodwar->getFrameCount() == 20)
            {
                for (auto depot : BWAPI::Broodwar->self()->getUnits())
                {
                    if (depot->getType().isResourceDepot()) BWAPI::Broodwar->killUnit(depot);
                }
            }
        };

        test.run();
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
