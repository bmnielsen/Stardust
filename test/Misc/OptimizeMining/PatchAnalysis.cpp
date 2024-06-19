#include "PatchAnalysis.h"

#include <bwem.h>
#include <set>

namespace
{
    std::set<std::pair<int, int>> blockedPositions;
    std::string blockedPositionsMapHash;

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

    BWAPI::TilePosition getDepotTile(BWAPI::TilePosition patchTile)
    {
        auto patch = getPatchUnit(patchTile);
        if (!patch)
        {
            Log::Get() << "ERROR: Could not get patch unit for patch @ " << patchTile;
            return BWAPI::TilePositions::Invalid;
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
        }

        return depotTile;
    }
}

PatchAnalysis::PatchAnalysis(BWAPI::TilePosition patchTile)
        : depotTile(getDepotTile(patchTile))
        , patchTile(patchTile)
        , state(0)
        , lastStateChange(0)
        , worker(nullptr)
        , complete(false)
{
    // Generate blocked positions lazily and share with other instantiations
    if (blockedPositions.empty() || blockedPositionsMapHash != BWAPI::Broodwar->mapHash())
    {
        blockedPositionsMapHash = BWAPI::Broodwar->mapHash();

        auto addBlockedAroundBox = [](BWAPI::Position topLeft, BWAPI::Position size)
        {
            for (int x = topLeft.x - 11; x < topLeft.x + size.x + 11; x++)
            {
                for (int y = topLeft.y - 11; y < topLeft.y + size.y + 11; y++)
                {
                    blockedPositions.emplace(x, y);
                }
            }
        };
        for (const auto unit : BWAPI::Broodwar->getStaticNeutralUnits())
        {
            if (!unit->getType().isMineralField() && unit->getType() != BWAPI::UnitTypes::Resource_Vespene_Geyser) continue;

            addBlockedAroundBox(BWAPI::Position(unit->getInitialTilePosition()), BWAPI::Position(unit->getType().tileSize()));

            auto walkPos = BWAPI::WalkPosition(unit->getInitialTilePosition());
            auto walkSize = BWAPI::WalkPosition(unit->getType().tileSize());
            for (int walkX = walkPos.x - 3; walkX < walkPos.x + walkSize.x + 3; walkX++)
            {
                for (int walkY = walkPos.y - 3; walkY < walkPos.y + walkSize.y + 3; walkY++)
                {
                    auto hereWalk = BWAPI::WalkPosition(walkX, walkY);
                    if (!hereWalk.isValid()) continue;
                    if (BWAPI::Broodwar->isWalkable(hereWalk)) continue;
                    addBlockedAroundBox(BWAPI::Position(hereWalk), BWAPI::Position(8, 8));
                }
            }
        }
    }

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

    auto addPosition = [&](int x, int y)
    {
        auto pos = BWAPI::Position(x, y);
        if (pos.isValid() && !blockedPositions.contains(std::make_pair(x, y)))
        {
            probeStartingPositions.emplace_back(pos);
        }
    };

    if (left)
    {
        for (int y = topRight.y; y <= bottomRight.y; y++) addPosition(topRight.x, y);
        if (top) for (int x = center.x; x < bottomRight.x; x++) addPosition(x, bottomRight.y);
        if (bottom) for (int x = center.x; x < topRight.x; x++) addPosition(x, topRight.y);
    }
    else if (right)
    {
        for (int y = topLeft.y; y <= bottomLeft.y; y++) addPosition(topLeft.x, y);
        if (top) for (int x = center.x; x > bottomLeft.x; x--) addPosition(x, bottomLeft.y);
        if (bottom) for (int x = center.x; x > topLeft.x; x--) addPosition(x, topLeft.y);
    }
    else
    {
        if (top) for (int x = bottomLeft.x; x < bottomRight.x; x++) addPosition(x, bottomRight.y);
        if (bottom) for (int x = topLeft.x; x < topRight.x; x++) addPosition(x, topLeft.y);
    }    
}

void PatchAnalysis::onFrame(int batch, const std::string &dataBasePath)
{
    auto patch = getPatchUnit(patchTile);
    if (!patch)
    {
        Log::Get() << "ERROR: Patch unit for " << patchTile << " not available";
        complete = true;
        return;
    }

    auto depot = getDepotUnit(depotTile);
    if (!depot)
    {
        BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Nexus, depotCenter());
        return;
    }

    if (probeStartingPositions.empty())
    {
        dumpResults(batch, dataBasePath);
        complete = true;
        return;
    }

    auto &currentStartingPosition = *probeStartingPositions.rbegin();
    auto startingPositionIt = startingPositionAnalysis.find(currentStartingPosition);
    if (startingPositionIt == startingPositionAnalysis.end())
    {
        startingPositionIt = startingPositionAnalysis.emplace(currentStartingPosition,
                                                                  StartingPositionAnalysis{patchTile, currentStartingPosition}).first;
    }
    auto &startingPosition = startingPositionIt->second;

    auto setState = [&](int toState)
    {
        state = toState;
        lastStateChange = currentFrame;
    };

    switch (state)
    {
        case 0:
        {
            CherryVis::log() << "Case " << patchTile << ":" << currentStartingPosition << ": starting";

            BWAPI::Broodwar->createUnit(BWAPI::Broodwar->self(), BWAPI::UnitTypes::Protoss_Probe, currentStartingPosition);

            setState(1);
            break;
        }
        case 1:
        {
            // If it took too long to create the worker, this position is probably blocked
            if ((currentFrame - lastStateChange) > 10)
            {
                CherryVis::log() << "Case " << patchTile << ":" << currentStartingPosition << ": gave up creating worker";

                setState(100);
                break;
            }

            for (auto &unit : BWAPI::Broodwar->self()->getUnits())
            {
                if (!unit->getType().isWorker()) continue;

                int dist = unit->getDistance(depot);
                if (dist > 400) continue;

                worker = unit;

                if (unit->getPosition() != currentStartingPosition)
                {
                    CherryVis::log() << "Case " << patchTile << ":" << currentStartingPosition << ": wrong position "
                                     << unit->getPosition() << "; assume this starting position is blocked";
                    setState(100);
                    break;
                }

                setState(2);
                break;
            }

            break;
        }
        case 2:
        {
            worker->gather(patch);

            setState(3);
            break;
        }
        case 3:
        {
            auto &miningPaths = startingPosition.miningPaths;

            if (!miningPaths.empty())
            {
                auto &gatherPath = (*miningPaths.rbegin()).gatherPath;
                if (worker->getDistance(patch) != 0 || !gatherPath.rbegin()->positionEquals(worker))
                {
                    gatherPath.emplace_back(worker);
                }
            }

            if (miningPaths.size() == 5 && worker->getDistance(patch) == 0)
            {
                setState(100);
                break;
            }

            // If the order timer might reset while the worker is returning minerals, resend the order
            // Otherwise we may see "random" deliveries that maintain speed and skew the results
            if (worker->getOrder() == BWAPI::Orders::MiningMinerals)
            {
                CherryVis::log(worker->getID()) << worker->getOrderTimer() << " - " << (150 - ((BWAPI::Broodwar->getFrameCount() - 8) % 150));

                int framesToReset = (150 - ((BWAPI::Broodwar->getFrameCount() - 8) % 150));
                if (framesToReset >= worker->getOrderTimer() && (framesToReset - worker->getOrderTimer()) < 70)
                {
                    worker->gather(patch);
                }
            }

            if (worker->isCarryingMinerals())
            {
                patch->setResources(patch->getInitialResources());
                miningPaths.emplace_back(worker);
                setState(4);
                break;
            }

            break;
        }
        case 4:
        {
            auto &miningPaths = startingPosition.miningPaths;
            auto &returnPath = (*miningPaths.rbegin()).returnPath;
            returnPath.emplace_back(worker);

            if (!worker->isCarryingMinerals()) setState(3);

            break;
        }
        case 100:
        {
            if (worker)
            {
                BWAPI::Broodwar->killUnit(worker);
                worker = nullptr;
            }

            if ((currentFrame - lastStateChange) > 10)
            {
                startingPosition.analyze();
                probeStartingPositions.pop_back();
                setState(0);
                break;
            }

            break;
        }
    }
}

void PatchAnalysis::clearOutputFiles(const std::string &dataBasePath, const std::string &mapHash)
{
    auto clear = [&](const std::string &type)
    {
        {
            std::ofstream file;
            file.open((std::ostringstream() << dataBasePath << mapHash << "_patchoverview_" << type << ".csv").str(),
                      std::ofstream::trunc);
            file << "Patch Tile X;Patch Tile Y;Batch;Stable Paths;Unique Stable Paths;Unstable Paths;Unstable Extra Wait Cycle;Unstable One Frame;Unstable Same Length;Unstable Other;Two-Cycle Stable Paths;Same Last Two;Best Length;Stable Paths Exceeding Length;Unstable Start Positions;Start Positions Exceeded Length\n";
            file.close();
        }
        {
            std::ofstream file;
            file.open((std::ostringstream() << dataBasePath << mapHash << "_patch" << type << "paths.csv").str(),
                      std::ofstream::trunc);
            file << "Patch Tile X;Patch Tile Y;Batch;Path Hash;Starting Position;Iteration;Length\n";
            file.close();
        }
    };

    clear("gather");
    clear("return");
}

std::set<BWAPI::TilePosition> PatchAnalysis::getAnalyzedPatches(const std::string &dataBasePath, const std::string &mapHash)
{
    std::set<BWAPI::TilePosition> result;

    std::ifstream file;
    file.open((std::ostringstream() << dataBasePath << mapHash << "_patchoverview_gather.csv").str());
    if (!file.good())
    {
        std::cout << "No patch stats available for " << mapHash << std::endl;
        return result;
    }

    try
    {
        CsvTools::readNextLine(file); // Header row; ignored
        while (true)
        {
            auto line = CsvTools::readNextLine(file);
            if (line.size() < 2) break;

            result.emplace(std::stoi(line[0]), std::stoi(line[1]));
        }
    }
    catch (std::exception &ex)
    {
        std::cout << "Exception caught attempting to read patch stats: " << ex.what() << std::endl;
        return result;
    }
    file.close();

    return result;
}

void PatchAnalysis::dumpResults(int batch, const std::string &dataBasePath)
{
    auto analyze = [&](const std::string &type, std::ranges::input_range auto&& range)
    {
        std::map<BWAPI::Position, PathAnalysis> startingPositionPaths(range.begin(), range.end());

        int countStable = 0;
        int countUnstable = 0;
        int countTwoCycleStable = 0;
        int countLastTwoStable = 0;
        int countUnstableExtraWaitCycle = 0;
        int countUnstableOneFrame = 0;
        int countUnstableSameLength = 0;
        int countUnstableOther = 0;
        std::vector<BWAPI::Position> unstableStartPositions;
        std::vector<std::pair<BWAPI::Position, size_t>> stableStartPositionsWithLength;
        size_t bestLength = INT_MAX;
        std::set<int> stablePaths;
        std::map<uint32_t, std::pair<BWAPI::Position, size_t>> uniquePaths;
        for (auto &[startingPosition, pathAnalysis] : startingPositionPaths)
        {
            if (pathAnalysis.paths.size() != 5) continue;

            if (pathAnalysis.stable)
            {
                countStable++;
                stableStartPositionsWithLength.emplace_back(startingPosition, pathAnalysis.shortestLength);
                if (pathAnalysis.shortestLength < bestLength) bestLength = pathAnalysis.shortestLength;
                stablePaths.emplace(pathAnalysis.paths.rbegin()->hash());
            }
            else
            {
                countUnstable++;
                unstableStartPositions.push_back(startingPosition);

                switch (pathAnalysis.unstableType)
                {
                    case 0:
                        countUnstableOther++;
                        break;
                    case 1:
                        countUnstableExtraWaitCycle++;
                        break;
                    case 2:
                        countUnstableOneFrame++;
                        break;
                    case 3:
                        countUnstableSameLength++;
                        break;
                }

                if (pathAnalysis.twoCycleStable)
                {
                    countTwoCycleStable++;
                }

                if (pathAnalysis.lastTwoStable)
                {
                    countLastTwoStable++;
                }
            }

            for (size_t i = 0; i < pathAnalysis.paths.size(); i++)
            {
                if (i == 0 || !uniquePaths.contains(pathAnalysis.paths[i].hash()))
                {
                    uniquePaths[pathAnalysis.paths[i].hash()] = std::make_pair(startingPosition, i);
                }
            }
        }

        std::vector<BWAPI::Position> pathsExceedingLength;
        auto result = stableStartPositionsWithLength
                      | std::views::filter([&bestLength](const auto &posAndLength){ return posAndLength.second > bestLength; })
                      | std::views::transform([](const auto &posAndLength){ return posAndLength.first; });
        std::ranges::copy(result, std::back_inserter(pathsExceedingLength));

        Log::Get() << "Patch " << patchTile << " " << type << " results:"
                   << "\nStable paths: " << countStable
                   << "\nUnique stable paths: " << stablePaths.size()
                   << "\nUnstable paths: " << countUnstable
                   << "\nUnstable extra wait cycle: " << countUnstableExtraWaitCycle
                   << "\nUnstable one frame longer: " << countUnstableOneFrame
                   << "\nUnstable same length: " << countUnstableSameLength
                   << "\nUnstable other: " << countUnstableOther
                   << "\nTwo-cycle stable paths: " << countTwoCycleStable
                   << "\nLast two stable paths: " << countLastTwoStable
                   << "\nBest length: " << bestLength
                   << "\nStable paths exceeding length: " << pathsExceedingLength.size();

        {
            std::ofstream file;
            file.open((std::ostringstream() << dataBasePath << BWAPI::Broodwar->mapHash() << "_patchoverview_" << type << ".csv").str(),
                      std::ofstream::app);
            file << patchTile.x
                 << ";" << patchTile.y
                 << ";" << batch
                 << ";" << countStable
                 << ";" << stablePaths.size()
                 << ";" << countUnstable
                 << ";" << countUnstableExtraWaitCycle
                 << ";" << countUnstableOneFrame
                 << ";" << countUnstableSameLength
                 << ";" << countUnstableOther
                 << ";" << countTwoCycleStable
                 << ";" << countLastTwoStable
                 << ";" << bestLength
                 << ";" << pathsExceedingLength.size()
                 << ";" << unstableStartPositions
                 << ";" << pathsExceedingLength
                 << "\n";
            file.close();
        }

        {
            std::ofstream file;
            file.open((std::ostringstream() << dataBasePath << BWAPI::Broodwar->mapHash() << "_patch" << type << "paths.csv").str(),
                      std::ofstream::app);
            for (const auto &[hash, startingPositionAndIteration] : uniquePaths)
            {
                file << patchTile.x
                     << ";" << patchTile.y
                     << ";" << batch
                     << ";" << hash
                     << ";" << startingPositionAndIteration.first
                     << ";" << startingPositionAndIteration.second
                     << ";"
                     << startingPositionPaths[startingPositionAndIteration.first].paths[startingPositionAndIteration.second].size()
                     << "\n";
            }
            file.close();
        }
    };

    analyze("gather", startingPositionAnalysis
            | std::views::transform([](const auto &posAndAnalysis)
                                    { return std::make_pair(posAndAnalysis.first, posAndAnalysis.second.gatherPathAnalysis); }));
    analyze("return", startingPositionAnalysis
            | std::views::transform([](const auto &posAndAnalysis)
                                    { return std::make_pair(posAndAnalysis.first, posAndAnalysis.second.returnPathAnalysis); }));
}
