#include "WorkerOrderTimer.h"

#include <fstream>
#include <filesystem>
#include <ranges>
#include "Units.h"
#include "Workers.h"
#include "Map.h"

#if INSTRUMENTATION_ENABLED
#define TRACK_MINING_EFFICIENCY true
#endif

namespace WorkerOrderTimer
{
    namespace
    {
        std::vector<std::string> dataLoadPaths = {
                "bwapi-data/AI/",
                "bwapi-data/read/",
                "bwapi-data/write/"
        };
        std::string dataWritePath = "bwapi-data/write/";

        struct PositionAndVelocity
        {
            BWAPI::Position position;
            int velocityX;
            int velocityY;

            PositionAndVelocity(BWAPI::Position position, int velocityX, int velocityY)
                    : position(position), velocityX(velocityX), velocityY(velocityY) {}

            explicit PositionAndVelocity(const MyUnit &unit)
                    : position(unit->lastPosition)
                    , velocityX((int) (unit->bwapiUnit->getVelocityX() * 100.0))
                    , velocityY((int) (unit->bwapiUnit->getVelocityY() * 100.0)) {}
        };

        std::ostream &operator<<(std::ostream &out, const PositionAndVelocity &p)
        {
            out << p.position.x << "," << p.position.y << "," << p.velocityX << "," << p.velocityY;
            return out;
        }

        inline bool operator<(const PositionAndVelocity &a, const PositionAndVelocity &b)
        {
            return (a.position < b.position) ||
                   (a.position == b.position && a.velocityX < b.velocityX) ||
                   (a.position == b.position && a.velocityX == b.velocityX && a.velocityY < b.velocityY);
        }

        std::map<Resource, std::set<PositionAndVelocity>> resourceToOptimalOrderPositions;
        std::map<MyUnit, std::map<int, PositionAndVelocity>> workerPositionHistory;

#if TRACK_MINING_EFFICIENCY
        // This map records statistics of mining efficiency for each patch
        // At each frame a resource has at least one worker assigned to it, a value is written:
        // 0 = one worker assigned, worker is moving to minerals
        // 1 = one worker assigned, worker is waiting to mine
        // 2 = one worker assigned, worker is mining
        // 3 = one worker assigned, worker is moving to return cargo
        // 4 = one worker assigned, worker is waiting to return cargo
        // 5 = one worker assigned, worker is waiting to move from depot
        // 10 = two workers assigned, a worker is mining
        // 11 = two workers assigned, one worker is returning cargo, other worker is moving to minerals
        // 12 = two workers assigned, one worker is returning cargo, other worker is waiting to mine
        std::map<Resource, std::vector<std::pair<int, int>>> resourceToMiningStatus;
#endif

        std::string resourceOptimalOrderPositionsFilename(bool writing = false)
        {
            if (writing)
            {
                return (std::ostringstream() << dataWritePath << BWAPI::Broodwar->mapHash() << "_resourceOptimalOrderPositions.csv").str();
            }

            for (auto &path : dataLoadPaths)
            {
                auto filename = (std::ostringstream() << path << BWAPI::Broodwar->mapHash() << "_resourceOptimalOrderPositions.csv").str();
                if (std::filesystem::exists(filename)) return filename;
            }

            return "";
        }

        std::vector<int> readCsvLine(std::istream &str)
        {
            std::vector<int> result;

            std::string line;
            std::getline(str, line);

            try
            {
                std::stringstream lineStream(line);
                std::string cell;

                while (std::getline(lineStream, cell, ','))
                {
                    result.push_back(std::stoi(cell));
                }
            }
            catch (std::exception &ex)
            {
                Log::Get() << "Exception caught parsing optimal order position: " << ex.what() << "; line: " << line;
            }
            return result;
        }
    }

    void initialize()
    {
        resourceToOptimalOrderPositions.clear();
        workerPositionHistory.clear();

#if TRACK_MINING_EFFICIENCY
        resourceToMiningStatus.clear();
#endif

        // Attempt to open a CSV file storing the optimal positions found in previous matches on this map
        std::ifstream file;
        file.open(resourceOptimalOrderPositionsFilename());
        if (file.good())
        {
            try
            {
                // Read and parse each position
                while (true)
                {
                    auto line = readCsvLine(file);
                    if (line.size() != 6) break;

                    BWAPI::TilePosition tile(line[0], line[1]);
                    auto resource = Units::resourceAt(tile);
                    if (resource)
                    {
                        resourceToOptimalOrderPositions[resource].emplace(BWAPI::Position(line[2], line[3]), line[4], line[5]);
                    }
                }
            }
            catch (std::exception &ex)
            {
                Log::Get() << "Exception caught attempting to read optimal order positions: " << ex.what();
            }
        }
    }

    void write()
    {
        std::ofstream file;
        file.open(resourceOptimalOrderPositionsFilename(true), std::ofstream::trunc);

        for (auto &resourceAndOptimalOrderPositions : resourceToOptimalOrderPositions)
        {
            for (auto &optimalOrderPosition : resourceAndOptimalOrderPositions.second)
            {
                file << resourceAndOptimalOrderPositions.first->tile.x << ","
                     << resourceAndOptimalOrderPositions.first->tile.y << ","
                     << optimalOrderPosition << "\n";
            }
        }

        file.close();
    }

    void update()
    {
#if TRACK_MINING_EFFICIENCY
        for (auto &[patch, workers] : Workers::mineralsAndAssignedWorkers())
        {
            if (!patch || workers.empty() || workers.size() > 2) continue;

            Base *closestBase = nullptr;
            auto closestBaseDist = INT_MAX;
            for (auto &base : Map::getMyBases())
            {
                auto dist = patch->getDistance(base->getPosition());
                if (dist < closestBaseDist)
                {
                    closestBase = base;
                    closestBaseDist = dist;
                }
            }
            if (!closestBase || !closestBase->resourceDepot || !closestBase->resourceDepot->exists()) continue;
            auto &depot = closestBase->resourceDepot;

            auto &miningStatus = resourceToMiningStatus[patch];

            // Initialize status to be the same as the last status, or -1 if there is no last status
            int status = -1;
            if (!miningStatus.empty())
            {
                auto &[lastStatus, frame] = *miningStatus.rbegin();
                if (frame == (currentFrame - 1))
                {
                    status = lastStatus;
                }
            }

            // Treat one vs. two assigned worker cases separately
            if (workers.size() == 1)
            {
                auto &worker = *workers.begin();
                auto distPatch = patch->getDistance(worker);
                auto distDepot = depot->getDistance(worker);

                CherryVis::log(worker->id) << distPatch << ":" << distDepot
                    << "; " << worker->bwapiUnit->getOrder()
                    << "; " << worker->bwapiUnit->isCarryingMinerals();

                if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals)
                {
                    if (distDepot == 0)
                    {
                        status = 5;
                    }
                    else
                    {
                        status = 0;
                    }
                }
                else if (distPatch == 0 && (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals
                                            || worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals))
                {
                    status = 1;
                }
                else if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    status = 2;
                }
                else if (distDepot > 0 && worker->bwapiUnit->isCarryingMinerals())
                {
                    status = 3;
                }
                else if (distDepot == 0 && worker->bwapiUnit->isCarryingMinerals())
                {
                    status = 4;
                }
            }
            else
            {
                auto &workerA = *workers.begin();
                auto &workerB = *workers.rbegin();
                auto distPatchA = patch->getDistance(workerA);
                auto distPatchB = patch->getDistance(workerB);
                auto distDepotA = depot->getDistance(workerA);
                auto distDepotB = depot->getDistance(workerB);

                CherryVis::log(workerA->id) << distPatchA << ":" << distDepotA
                                           << "; " << workerA->bwapiUnit->getOrder()
                                           << "; " << workerA->bwapiUnit->isCarryingMinerals();
                CherryVis::log(workerB->id) << distPatchB << ":" << distDepotB
                                           << "; " << workerB->bwapiUnit->getOrder()
                                           << "; " << workerB->bwapiUnit->isCarryingMinerals();

                if (workerA->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
                    workerB->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    status = 10;
                }
                else if (workerA->bwapiUnit->isCarryingMinerals() != workerB->bwapiUnit->isCarryingMinerals())
                {
                    auto &otherWorker = (workerA->bwapiUnit->isCarryingMinerals() ? workerB : workerA);
                    auto distPatch = patch->getDistance(otherWorker);
                    if (distPatch > 0)
                    {
                        status = 11;
                    }
                    else
                    {
                        status = 12;
                    }
                }
                else
                {
                    status = -1;
                }
            }

            CherryVis::log(patch->id) << status;

            if (status != -1)
            {
                miningStatus.emplace_back(status, currentFrame);
            }
        }
#endif
    }

    bool optimizeStartOfMining(const MyUnit &worker, const Resource &resource)
    {
        // Break out early if the distance is larger than we need to worry about
        auto dist = resource->getDistance(worker);
        if (dist > 100) return false;

        auto &positionHistory = workerPositionHistory[worker];
        auto &optimalOrderPositions = resourceToOptimalOrderPositions[resource];

        // If the worker is at the resource, record the optimal position
        if (dist == 0)
        {
            // The worker is at the resource, so if we have enough position history recorded,
            // record the optimal position
            int frame = currentFrame - BWAPI::Broodwar->getLatencyFrames() - 11;
            auto positionIt = positionHistory.find(frame);
            if (positionIt != positionHistory.end())
            {
                // Sometimes the probes will take different routes close to the mineral patch, perhaps because
                // of other nearby workers. This is OK, as we would rather send the order a frame late than a frame
                // early, but we still clear positions that are much too late.
                for (auto &frameAndPos : positionHistory)
                {
                    if (frameAndPos.first <= (frame + 2)) continue;
                    optimalOrderPositions.erase(frameAndPos.second);
                }

                optimalOrderPositions.insert(positionIt->second);
            }

            positionHistory.clear();

            return false;
        }

        PositionAndVelocity currentPositionAndVelocity(worker);

        // Check if this worker is at an optimal position to resend the gather order
        bool resent = false;
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals &&
            optimalOrderPositions.contains(currentPositionAndVelocity))
        {
            auto bwapiUnit = resource->getBwapiUnitIfVisible();
            if (bwapiUnit)
            {
                worker->gather(bwapiUnit);
                resent = true;
            }
        }

        // Record the worker's position
        positionHistory.emplace(std::make_pair(currentFrame, currentPositionAndVelocity));
        return resent;
    }

    bool optimizeReturn(const MyUnit &worker, const Resource &resource, const Unit &depot)
    {
        return false;
    }

    void writeInstrumentation()
    {
#if TRACK_MINING_EFFICIENCY
        // For each patch, detect incorrect mining behaviour and draw
        // For the single-worker case, incorrect behaviour means:
        // - Waiting for more than one frame to mine, unless there has been an order timer reset
        // - Waiting for more frames than needed after an order timer reset
        // - Taking a longer path back to the depot
        // For the two-worker case, incorrect behaviour means:
        // - Waiting for more than one frame to mine when there has not been an order timer reset
        // - Waiting for more than the maximum expected number of frames to mine when there has been an order timer reset
        // - Second worker not being ready to mine?
        int framesSinceLastOrderTimerReset = (currentFrame - 8) % 150;
        for (auto &[patch, miningStatus] : resourceToMiningStatus)
        {
            // Get the status and how many frames it has been stable
            int currentStatus = -1;
            int statusFrames = 0;
            for (auto &[status, frame] : std::ranges::reverse_view(miningStatus)) {
                if (frame != (currentFrame - statusFrames)) break;
                if (statusFrames == 0)
                {
                    currentStatus = status;
                }
                else if (status != currentStatus)
                {
                    break;
                }
                statusFrames++;
            }
            if (statusFrames == 0) continue;

            // Check for an inefficiency based on the status
            bool inefficiency = false;
            switch (currentStatus)
            {
                // These cases do not matter
                case 0:
                case 2:
                case 3:
                case 4:
                case 10:
                case 11:
                case 12:
                default:
                    break;

                // Single worker waiting too long to mine
                case 1:
                {
                    if (statusFrames > 1 && framesSinceLastOrderTimerReset > -1)
                    {
                        inefficiency = true;
                        CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                            << ": worker waited too many frames to start mining";
                    }
                    break;
                }

                // Single worker waiting too long to leave the depot
                case 5:
                {
                    if (statusFrames > 6)
                    {
                        inefficiency = true;
                        CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                         << ": worker waited too many frames to leave depot";
                    }
                    break;
                }
            }

            if (inefficiency)
            {
                CherryVis::drawCircle(patch->center.x, patch->center.y, 32, CherryVis::DrawColor::Red);
            }
        }
#endif
    }
}
