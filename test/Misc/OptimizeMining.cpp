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
        explicit MiningPath(BWAPI::Position gatherPosition)
            : gatherPosition(gatherPosition)
            , returnPosition(BWAPI::Positions::Invalid)
            , hasCachedHash(false)
            , cachedHash(0)
        {
            returnPath.emplace_back(gatherPosition);
        }

        BWAPI::Position gatherPosition;
        BWAPI::Position returnPosition;
        std::vector<BWAPI::Position> gatherPath;
        std::vector<BWAPI::Position> returnPath;
        bool hasCachedHash;
        uint32_t cachedHash;

        // Adapted from https://stackoverflow.com/questions/20511347/a-good-hash-function-for-a-vector/72073933#72073933
        uint32_t hash()
        {
            if (hasCachedHash) return cachedHash;

            uint32_t seed = gatherPath.size() + returnPath.size() + 2;

            auto addNumber = [&seed](uint32_t x)
            {
                x = ((x >> 16) ^ x) * 0x45d9f3b;
                x = ((x >> 16) ^ x) * 0x45d9f3b;
                x = (x >> 16) ^ x;
                seed ^= x + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            };

            auto addPosition = [&addNumber](const BWAPI::Position &pos)
            {
                addNumber(pos.x);
                addNumber(pos.y);
            };

            addPosition(gatherPosition);
            addPosition(returnPosition);
            for (const auto &pos : gatherPath) addPosition(pos);
            for (const auto &pos : returnPath) addPosition(pos);

            cachedHash = seed;
            hasCachedHash = true;
            return seed;
        }
    };

    std::ostream &operator<<(std::ostream &os, const MiningPath &path)
    {
        auto outputPositionVector = [&os](const std::vector<BWAPI::Position> &positions)
        {
            std::string separator = "";
            for (const auto &pos : positions)
            {
                os << separator << pos;
                separator = ",";
            }
        };

        os << "Start: " << path.gatherPosition << "; End: " << path.returnPosition;
        os << "\nReturn: ";
        outputPositionVector(path.returnPath);
        os << "\nGather: ";
        outputPositionVector(path.gatherPath);

        return os;
    }

    struct StartingPositionInfo
    {
        BWAPI::TilePosition patchTile;
        BWAPI::Position startingPosition;
        std::vector<MiningPath> miningPaths;

        bool stable = false;
        size_t shortestLength = INT_MAX;
        size_t longestLength = 0;

        void finalize()
        {
            if (miningPaths.size() != 5)
            {
                Log::Get() << "ERROR: Start position " << startingPosition << " only has " << miningPaths.size() << " mining paths";
                return;
            }

            // We look at the last three mining paths where we assume the path should have stabilized
            for (int i = 2; i < 5; i++)
            {
                size_t length = miningPaths[i].gatherPath.size() + miningPaths[i].returnPath.size();
                if (length < shortestLength) shortestLength = length;
                if (length > longestLength) longestLength = length;
            }

            stable = (miningPaths[2].hash() == miningPaths[3].hash() && miningPaths[2].hash() == miningPaths[4].hash());

            if (!stable)
            {
                CherryVis::log() << "Path unstable: shortest=" << shortestLength << "; longest=" << longestLength;
                if (longestLength > shortestLength)
                {
                    int shortest;
                    int longest;
                    for (int i = 2; i < 5; i++)
                    {
                        size_t length = miningPaths[i].gatherPath.size() + miningPaths[i].returnPath.size();
                        if (length == shortestLength) shortest = i;
                        if (length == longestLength) longest = i;
                    }
                    CherryVis::log() << "Shortest path (" << shortest << "): \n" << miningPaths[shortest];
                    CherryVis::log() << "Longest path (" << longest << "): \n" << miningPaths[longest];
                }
            }
        }
    };

    struct PatchInfo
    {
        BWAPI::TilePosition patchTile;
        BWAPI::TilePosition depotTile;
        std::vector<BWAPI::Position> probeStartingPositions;
        std::map<BWAPI::Position, StartingPositionInfo> startingPositionInfos;

        void dumpResults()
        {
            int countStable = 0;
            int countUnstable = 0;
            std::vector<BWAPI::Position> unstableStartPositions;
            std::vector<std::pair<BWAPI::Position, size_t>> stableStartPositionsWithLength;
            size_t bestLength = INT_MAX;
            for (auto &[startingPosition, startingPositionInfo] : startingPositionInfos)
            {
                if (startingPositionInfo.stable)
                {
                    countStable++;
                    stableStartPositionsWithLength.emplace_back(startingPosition, startingPositionInfo.shortestLength);
                    if (startingPositionInfo.shortestLength < bestLength) bestLength = startingPositionInfo.shortestLength;
                }
                else
                {
                    countUnstable++;
                    unstableStartPositions.push_back(startingPosition);
                }
            }

            std::vector<std::pair<BWAPI::Position, size_t>> pathsExceedingLength;
            std::copy_if(
                    stableStartPositionsWithLength.begin(),
                    stableStartPositionsWithLength.end(),
                    std::back_inserter(pathsExceedingLength),
                    [&bestLength](const auto &posAndLength){ return posAndLength.second > bestLength; });

            Log::Get() << "Patch " << patchTile << " results:"
                << "\nStable paths: " << countStable
                << "\nUnstable paths: " << countUnstable
                << "\nBest length: " << bestLength
                << "\nStable paths exceeding length: " << pathsExceedingLength.size();
        }
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

            // Generate a set of all blocked positions
            std::set<std::pair<int, int>> blockedPositions;
            for (const auto &patchTile : patchTiles)
            {
                auto pos = BWAPI::Position(patchTile);
                for (int x = pos.x - 11; x < pos.x + 75; x++)
                {
                    for (int y = pos.y - 11; y < pos.y + 43; y++)
                    {
                        blockedPositions.emplace(x, y);
                    }
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

                auto topLeft = BWAPI::Position(patchTile) + BWAPI::Position(-12, -12);
                auto topRight = BWAPI::Position(patchTile) + BWAPI::Position(75, -12);
                auto bottomLeft = BWAPI::Position(patchTile) + BWAPI::Position(-12, 43);
                auto bottomRight = BWAPI::Position(patchTile) + BWAPI::Position(75, 43);
                auto center = BWAPI::Position(patchTile) + BWAPI::Position(32, 16);

                bool left = (patchTile.x < (depotTile.x - 1));
                bool hmid = !left && (patchTile.x < (depotTile.x + 4));
                bool right = !left && !hmid;
                bool top = (patchTile.y < depotTile.y);
                bool vmid = !top && (patchTile.y < (depotTile.y + 3));
                bool bottom = !top && !vmid;

                std::vector<BWAPI::Position> probeStartingPositions;
                auto addPosition = [&probeStartingPositions, &blockedPositions](int x, int y)
                {
                    auto pos = BWAPI::Position(x, y);
                    if (pos.isValid() && !blockedPositions.contains(std::make_pair(x, y)))
                    {
                        probeStartingPositions.emplace_back(pos);
                    }
                };

                if (left)
                {
                    for (int y = topRight.y; y <= bottomRight.y; y += 4) addPosition(topRight.x, y);
                    if (top) for (int x = center.x; x < bottomRight.x; x += 4) addPosition(x, bottomRight.y);
                    if (bottom) for (int x = center.x; x < topRight.x; x += 4) addPosition(x, topRight.y);
                }
                else if (right)
                {
                    for (int y = topLeft.y; y <= bottomLeft.y; y += 4) addPosition(topLeft.x, y);
                    if (top) for (int x = center.x; x > bottomLeft.x; x -= 4) addPosition(x, bottomLeft.y);
                    if (bottom) for (int x = center.x; x > topLeft.x; x -= 4) addPosition(x, topLeft.y);
                }
                else
                {
                    if (top) for (int x = bottomLeft.x; x < bottomRight.x; x += 4) addPosition(x, bottomRight.y);
                    if (bottom) for (int x = topLeft.x; x < topRight.x; x += 4) addPosition(x, topLeft.y);
                }

                tileToPatchInfo.emplace(patchTile, PatchInfo{patchTile, depotTile, probeStartingPositions});
            }

            // Create an observer at each depot location so we have vision when we want to create it later
            for (const auto &depotCenter : depotCenters)
            {
                BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Observer, depotCenter);
            }

            Log::Get() << "Initialized test; ready to optimize " << patchTiles.size() << " patches";
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
            if (probeStartingPositions.empty())
            {
                patchInfo.dumpResults();
                Log::Get() << "Finished analyzing " << patchTile << "; " << (patchTiles.size() - 1) << " remaining";
                patchTiles.pop_back();
                CherryVis::frameEnd(currentFrame);
                return;
            }

            auto &currentStartingPosition = *probeStartingPositions.rbegin();
            auto startingPositionInfoIt = patchInfo.startingPositionInfos.find(currentStartingPosition);
            if (startingPositionInfoIt == patchInfo.startingPositionInfos.end())
            {
                startingPositionInfoIt = patchInfo.startingPositionInfos.emplace(currentStartingPosition,
                                                                                 StartingPositionInfo{patchTile, currentStartingPosition}).first;
            }
            auto &startingPositionInfo = startingPositionInfoIt->second;

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

                    CherryVis::log() << "Starting case " << patchTile << " : " << currentStartingPosition;

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

                    setState(3);
                    break;
                }
                case 3:
                {
                    CherryVis::setBoardValue("status", "measure-gather-paths-mine");

                    auto &miningPaths = startingPositionInfo.miningPaths;

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

                    // If the order timer might reset while the probe is returning minerals, resend the order
                    // Otherwise we may see "random" deliveries that maintain speed and skew the results
                    if (probe->getOrder() == BWAPI::Orders::MiningMinerals)
                    {
                        CherryVis::log(probe->getID()) << probe->getOrderTimer() << " - " << (150 - ((BWAPI::Broodwar->getFrameCount() - 8) % 150));

                        int framesToReset = (150 - ((BWAPI::Broodwar->getFrameCount() - 8) % 150));
                        if (framesToReset >= probe->getOrderTimer() && (framesToReset - probe->getOrderTimer()) < 70)
                        {
                            probe->gather(patch);
                        }
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
                    auto &miningPaths = startingPositionInfo.miningPaths;
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
                        startingPositionInfo.finalize();
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
        test.frameLimit = 10000000;
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
            for (auto worker : BWAPI::Broodwar->self()->getUnits())
            {
                if (worker->getType().isWorker()) BWAPI::Broodwar->killUnit(worker);
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
