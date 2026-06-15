#include "WorkerMiningInstrumentation.h"

#include <ranges>

#include "Workers.h"
#include "Map.h"
#include "OrderProcessTimer.h"
#include "MiningOptimization/WorkerMiningOptimization.h"

#define EPSILON 0.000001

#define TRACK_MINING_EFFICIENCY true
#define TRACK_MINING_EFFICIENCY_VERBOSE false
#define TRACK_RESOURCE_FORECAST_ACCURACY false

#if INSTRUMENTATION_ENABLED
#define LOG_WORKER_ORDERS false
#define LOG_PATCH_STATUS false
#define LOG_TWOPATCH_TAKEOVER false
#define LOG_TWOPATCH_TAKEOVER_ERRORS false
#define LOG_INEFFICIENCIES false
#define DRAW_INEFFICIENCIES false
#define LOG_NOTFACINGPATCH false
#endif

namespace WorkerMiningInstrumentation
{
    namespace
    {
    #if TRACK_MINING_EFFICIENCY
        // Indirection to the Workers::mineralsAndAssignedWorkers function allowing it to be overridden by tests
        std::function<std::map<Resource, std::set<MyWorker>> &()> getMineralsAndAssignedWorkers = Workers::mineralsAndAssignedWorkers;

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
        // 12 = two workers assigned, one worker is returning cargo, other worker is waiting to mine, worker returning cargo has orders processed first
        // 13 = same as 12, but where worker returning cargo might not have had its orders processed first
        // 14 = two workers assigned, both workers are moving to minerals
        std::map<Resource, std::vector<std::tuple<int, int, int>>> resourceToMiningStatus;

        // Map storing alerts for each patch, so we can show them for a while
        std::map<Resource, std::pair<int, std::string>> patchToAlert;

        // Frame when we gathered the 50th mineral
        int fiftiethMineralFrame;

        // Frames when we reached each 1000 of minerals gathered
        std::vector<int> thousandMineralFrames;

        // This status is used for identifying some conditions that require observing the worker over several frames:
        // - Whether the worker wasn't facing the patch prior to mining
        // - Whether the worker collided with the patch or depot after mining
        struct WorkerStatus
        {
            int lastUpdated = -2;
            int miningStartFrame = -2;
            int preminingFrameCounter = 0;
            std::vector<BWAPI::Position> startOfPathHistory;
        };
        std::map<MyWorker, WorkerStatus> workersToStatus;

        // All gather collision observations for each patch
        std::map<Resource, std::vector<std::pair<int, bool>>> resourceToGatherCollisionObservations;

        // All return collision observations for each patch
        std::map<Resource, std::vector<std::pair<int, bool>>> resourceToReturnCollisionObservations;

        // All facing patch observations for each patch
        std::map<Resource, std::vector<std::pair<int, bool>>> resourceToFacingPatchObservations;

        struct PatchData
        {
            unsigned int framesMined = 0;
            unsigned int framesNotMined = 0;
            unsigned int totalRotationFrames = 0;
            unsigned int rotationCount = 0;
            std::vector<unsigned int> allRotationTimes;
        };

        void addPatchData(PatchData &patchData,
                          const Resource &patch,
                          const std::vector<std::tuple<int, int, int>> &miningStatus,
                          int miningState,
                          const std::set<int> &nonMiningStates,
                          int fromFrame,
                          int toFrame,
                          bool trackAllRotationTimes)
        {
            bool recording = false;
            int lastStatus = -1;
            int currentPeriodMined = 0;
            int currentPeriodNotMined = 0;
            int currentPeriodStartFrame = 0;
            int lastFrame = -1;
            int lastExtraData = -2;
            for (auto &[status, frame, extraData] : miningStatus)
            {
                if (fromFrame != -1 && frame < fromFrame) continue;
                if (toFrame != -1 && frame >= toFrame) break;

                // If there is an interruption in the data, stop recording
                if (frame != (lastFrame + 1))
                {
                    recording = false;
                    lastStatus = -1;
                }
                lastFrame = frame;

                bool extraDataChanged = (extraData != lastExtraData && lastExtraData != -2);
                lastExtraData = extraData;

                if (status == miningState && (nonMiningStates.contains(lastStatus) || extraDataChanged))
                {
                    // This is the transition from not mining to mining, which we use as our start of a measurement period
                    // It may also be the transition from one worker mining to a different worker mining, which we treat the same

                    // If we were recording a period, flush it now
                    if (recording)
                    {
                        patchData.framesMined += currentPeriodMined;
                        patchData.framesNotMined += currentPeriodNotMined;
                        patchData.totalRotationFrames += (frame - currentPeriodStartFrame);
                        patchData.rotationCount++;
                        if (trackAllRotationTimes)
                        {
                            patchData.allRotationTimes.emplace_back(frame - currentPeriodStartFrame);
                        }
                    }

                    // Start recording a new period
                    recording = true;
                    currentPeriodMined = 1;
                    currentPeriodNotMined = 0;
                    currentPeriodStartFrame = frame;
                }
                else if (status == miningState)
                {
                    currentPeriodMined++;
                }
                else if (nonMiningStates.contains(status))
                {
                    currentPeriodNotMined++;
                }
                else
                {
                    // The status is for a different number of workers than we are interested in, so cancel this period
                    recording = false;
                }
                lastStatus = status;
            }
        }

        void addSinglePatchData(PatchData &patchData,
                                const Resource &patch,
                                const std::vector<std::tuple<int, int, int>> &miningStatus,
                                int fromFrame,
                                int toFrame,
                                bool trackAllRotationTimes = false)
        {
            addPatchData(patchData, patch, miningStatus, 2, {0, 1, 3, 4, 5}, fromFrame, toFrame, trackAllRotationTimes);
        }

        void addDoublePatchData(PatchData &patchData,
                                const Resource &patch,
                                const std::vector<std::tuple<int, int, int>> &miningStatus,
                                int fromFrame,
                                int toFrame,
                                bool trackAllRotationTimes = false)
        {
            addPatchData(patchData, patch, miningStatus, 10, {11, 12, 13, 14}, fromFrame, toFrame, trackAllRotationTimes);
        }

        void addObservationMapData(unsigned int &trueObservations,
                        unsigned int &falseObservations,
                        const std::map<Resource, std::vector<std::pair<int, bool>>> &data,
                        const Resource &patch,
                        int fromFrame,
                        int toFrame)
        {
            auto it = data.find(patch);
            if (it == data.end()) return;

            for (const auto &[frame, value] : it->second)
            {
                if (fromFrame != -1 && frame < fromFrame) continue;
                if (toFrame != -1 && frame >= toFrame) continue;

                (value ? trueObservations : falseObservations)++;
            }
        }

        double computeObservationRate(unsigned int trueObservations, unsigned int falseObservations)
        {
            unsigned int totalObservations = trueObservations + falseObservations;
            if (totalObservations == 0) return 0.0;

            return (double)trueObservations / (double)totalObservations;
        }

        Efficiency computeEfficiency(const PatchData &sgl,
                                     const PatchData &dbl,
                                     double gatherCollisionRate,
                                     double returnCollisionRate,
                                     double facingPatchRate)
        {
            auto computeMiningPercentage = [](const PatchData &pd)
            {
                if (pd.framesMined == 0 && pd.framesNotMined == 0) return 0.0;

                return 100.0 * (double)pd.framesMined / (double)(pd.framesMined + pd.framesNotMined);
            };

            return Efficiency{
                    (sgl.rotationCount == 0) ? 0.0 : ((double)sgl.totalRotationFrames / (double)sgl.rotationCount),
                    computeMiningPercentage(sgl),
                    (dbl.rotationCount == 0) ? 0.0 : ((double)dbl.totalRotationFrames / (double)dbl.rotationCount),
                    computeMiningPercentage(dbl),
                    gatherCollisionRate,
                    returnCollisionRate,
                    facingPatchRate
            };
        }
    #endif

#if TRACK_RESOURCE_FORECAST_ACCURACY
    // Stores the previous forecasts for each patch
    std::map<Resource, std::deque<std::array<double, GATHER_FORECAST_FRAMES>>> resourceToPreviousForecasts;

    // Stores the count of predictions at each number of frames into the future for each patch
    std::map<Resource, std::array<int, GATHER_FORECAST_FRAMES>> resourceToFramePredictionCounts;

    // Stores the total error for all predictions at each number of frames into the future for each patch
    std::map<Resource, std::array<double, GATHER_FORECAST_FRAMES>> resourceToTotalFramePredictionError;

    // Stores the count of false positives at each number of frames into the future for each patch
    std::map<Resource, std::array<int, GATHER_FORECAST_FRAMES>> resourceToCountOfFalsePositives;

    // Stores the count of frames where the other patches are forecast to be saturated
    std::map<Resource, std::array<int, GATHER_FORECAST_FRAMES>> resourceToOtherPatchesSaturated;
    std::map<Resource, std::array<int, GATHER_FORECAST_FRAMES>> resourceToOtherPatchesSaturatedCount;
#endif
    }

    void initialize(const std::function<std::map<Resource, std::set<MyWorker>> &()> &getMineralsAndAssignedWorkersOverride)
    {
#if TRACK_MINING_EFFICIENCY
        if (getMineralsAndAssignedWorkersOverride)
        {
            getMineralsAndAssignedWorkers = getMineralsAndAssignedWorkersOverride;
        }
        resourceToMiningStatus.clear();
        patchToAlert.clear();
        fiftiethMineralFrame = -1;
        thousandMineralFrames.clear();
        workersToStatus.clear();
        resourceToGatherCollisionObservations.clear();
        resourceToReturnCollisionObservations.clear();
        resourceToFacingPatchObservations.clear();
#endif

#if TRACK_RESOURCE_FORECAST_ACCURACY
        resourceToPreviousForecasts.clear();
        resourceToFramePredictionCounts.clear();
        resourceToTotalFramePredictionError.clear();
        resourceToCountOfFalsePositives.clear();
        resourceToOtherPatchesSaturated.clear();
        resourceToOtherPatchesSaturatedCount.clear();
#endif
    }

    void update()
    {
#if TRACK_MINING_EFFICIENCY
        if (fiftiethMineralFrame == -1 && BWAPI::Broodwar->self()->gatheredMinerals() >= 100)
        {
            fiftiethMineralFrame = currentFrame;
            if (!WorkerMiningOptimization::isExploring())
            {
                Log::Get() << "Gathered 50th mineral";
            }
        }

#if TRACK_MINING_EFFICIENCY_VERBOSE
        if ((BWAPI::Broodwar->self()->gatheredMinerals() - 50) >= (1000 * (thousandMineralFrames.size() + 1)))
        {
            thousandMineralFrames.push_back(currentFrame);
            if (!WorkerMiningOptimization::isExploring())
            {
                Log::Get() << "Gathered " << (1000 * thousandMineralFrames.size()) << "th mineral";
            }
        }
#endif

        for (auto &[patch, workers] : getMineralsAndAssignedWorkers())
        {
            if (!patch || workers.empty() || workers.size() > 2 || !(*workers.begin())->exists() || !(*workers.rbegin())->exists()) continue;

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
            
            // If there isn't a completed nexus near the patch, don't track this collection as it's likely distance mining
            if (!closestBase || !closestBase->resourceDepot || !closestBase->resourceDepot->exists() || !closestBase->resourceDepot->completed)
            {
                continue;
            }
            auto &depot = closestBase->resourceDepot;

            // Update the worker status in certain situations
            for (auto &worker : workers)
            {
                auto &status = workersToStatus[worker];
                if (status.lastUpdated != (currentFrame - 1))
                {
                    status.miningStartFrame = -2;
                    status.preminingFrameCounter = 0;
                    status.startOfPathHistory.clear();
                }

                // Check if the premining phase takes too long, and record this as a not facing patch
                // By "premining" we mean when the worker has transitioned to MiningMinerals but the order timer hasn't started counting down yet
                if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    if (status.miningStartFrame == -2)
                    {
                        status.miningStartFrame = currentFrame;
                    }

                    // This check also succeeds when the worker is finished mining, but we don't care since the data will be cleared anyway
                    if (worker->bwapiUnit->getOrderTimer() == 0)
                    {
                        status.preminingFrameCounter++;
                        status.lastUpdated = currentFrame;
                    }

                    // Check the counter when the order timer starts counting down
                    // However skip it if there was an order process timer reset exactly after the mining start frame, as this can also add a delay
                    if (worker->bwapiUnit->getOrderTimer() == 75 &&
                        !OrderProcessTimer::isResetFrame(status.miningStartFrame + 1))
                    {
#if LOG_NOTFACINGPATCH
                        if (status.preminingFrameCounter > 1)
                        {
                            Log::Get() << "Not facing patch"
                                       << "; worker " << worker->id << " @ " << worker->getTilePosition();
                        }
#endif

                        // The worker was facing the patch if there was at most one frame of delay
                        resourceToFacingPatchObservations[patch].emplace_back(currentFrame, (status.preminingFrameCounter <= 1));
                        status.lastUpdated = currentFrame;
                    }
                }

                // If we have recently gained or delivered minerals, track the path and check for collisions
                if ((currentFrame - worker->lastCarryingResourceChange) < 14)
                {
                    status.startOfPathHistory.push_back(worker->lastPosition);
                    status.lastUpdated = currentFrame;

                    if ((currentFrame - worker->lastCarryingResourceChange) == 13)
                    {
                        // TODO: Skip if there was a command sent in the meantime

                        // Go through the history and check if the worker stalled (stayed in the same position) for more than 7 frames
                        int stallFrames = 0;
                        int maxStallFrames = 0;
                        for (auto it = status.startOfPathHistory.begin() + 1; it != status.startOfPathHistory.end(); it++)
                        {
                            if ((*it) == (*(it - 1)))
                            {
                                stallFrames++;
                                if (stallFrames > maxStallFrames)
                                {
                                    maxStallFrames = stallFrames;
                                }
                            }
                            else
                            {
                                stallFrames = 0;
                            }
                        }
                        bool collision = (maxStallFrames > 7);
                        (worker->carryingResource
                            ? resourceToGatherCollisionObservations
                            : resourceToReturnCollisionObservations)[patch].emplace_back(currentFrame, collision);
                    }
                }
            }

            auto &miningStatus = resourceToMiningStatus[patch];

            // Initialize status to be the same as the last status, or -1 if there is no last status
            int status = -1;
            if (!miningStatus.empty())
            {
                auto &[lastStatus, frame, extraData] = *miningStatus.rbegin();
                if (frame == (currentFrame - 1))
                {
                    status = lastStatus;
                }
            }

            // Treat one vs. two assigned worker cases separately
            int extraData = -1;
            if (workers.size() == 1)
            {
                auto &worker = *workers.begin();
                auto distPatch = patch->getDistance(worker);
                auto distDepot = depot->getDistance(worker);

#if LOG_WORKER_ORDERS
                CherryVis::log(worker->id) << distPatch << ":" << distDepot
                    << "; " << worker->bwapiUnit->getOrder()
                    << "; " << worker->bwapiUnit->isCarryingMinerals();
#endif

                if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MoveToMinerals
                    || worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
                {
                    if (distPatch == 0)
                    {
                        status = 1;
                    }
                    else if (distDepot == 0 && currentFrame > 100 && worker->frameLastMoved != currentFrame)
                    {
                        status = 5;
                    }
                    else
                    {
                        status = 0;
                    }
                }
                else if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    status = 2;
                }
                else if (distDepot > 0 && worker->bwapiUnit->isCarryingMinerals())
                {
                    status = 3;
                }
                else if (worker->bwapiUnit->isCarryingMinerals())
                {
                    status = 4;
                }
            }
            else
            {
                auto &workerA = *workers.begin();
                auto &workerB = *workers.rbegin();

#if LOG_WORKER_ORDERS
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
#endif
                if (workerA->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    status = 10;
                    extraData = workerA->id;
                }
                else if (workerB->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    status = 10;
                    extraData = workerB->id;
                }
                else if (workerA->bwapiUnit->isCarryingMinerals() != workerB->bwapiUnit->isCarryingMinerals())
                {
                    auto &miningWorker = (workerA->bwapiUnit->isCarryingMinerals() ? workerB : workerA);
                    auto &returningWorker = (workerA->bwapiUnit->isCarryingMinerals() ? workerA : workerB);

                    extraData = returningWorker->lastStartedMining;

                    auto distPatch = patch->getDistance(miningWorker);
                    if (distPatch > 0)
                    {
                        status = 11;
                    }
                    else
                    {
                        if (returningWorker->orderProcessIndex > miningWorker->orderProcessIndex)
                        {
                            status = 12;
                        }
                        else
                        {
                            status = 13;
                        }
                    }
                }
                else
                {
                    status = 14;
                }
            }

#if LOG_PATCH_STATUS
            CherryVis::log(patch->id) << status;
#endif
            
            if (status != -1)
            {
                miningStatus.emplace_back(status, currentFrame, extraData);
            }
        }
#endif

#if TRACK_RESOURCE_FORECAST_ACCURACY
        for (const auto &base : Map::allBases())
        {
            for (auto &patch : base->mineralPatches())
            {
                // Zero the arrays the first time a patch is seen
                if (!resourceToPreviousForecasts.contains(patch))
                {
                    resourceToFramePredictionCounts[patch] = {0};
                    resourceToTotalFramePredictionError[patch] = {0.0};
                    resourceToCountOfFalsePositives[patch] = {0};
                    resourceToOtherPatchesSaturated[patch] = {0};
                    resourceToOtherPatchesSaturatedCount[patch] = {0};
                }

                auto &previousForecasts = resourceToPreviousForecasts[patch];
                auto &framePredictionCounts = resourceToFramePredictionCounts[patch];
                auto &totalFramePredictionError = resourceToTotalFramePredictionError[patch];
                auto &countOfFalsePositives = resourceToCountOfFalsePositives[patch];
                auto &otherPatchesSaturated = resourceToOtherPatchesSaturated[patch];
                auto &otherPatchesSaturatedCount = resourceToOtherPatchesSaturatedCount[patch];

                // Determine if the patch is being mined
                bool isBeingMined = false;
                for (const auto &worker : Workers::getWorkersAssignedTo(patch))
                {
                    if (!worker->exists()) continue;
                    if (worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                    {
                        isBeingMined = true;
                        break;
                    }
                }
                double actualProbability = isBeingMined ? 1.0 : 0.0;

                // Check the accuracy of each of the previous forecasts
                int frameIdx = -1;
                for (auto it = previousForecasts.begin(); it != previousForecasts.end(); it++)
                {
                    frameIdx++;

                    framePredictionCounts[frameIdx]++;
                    totalFramePredictionError[frameIdx] += (actualProbability - (*it)[frameIdx]);
                    if (!isBeingMined && (*it)[frameIdx] > (1.0 - EPSILON))
                    {
                        countOfFalsePositives[frameIdx]++;
                    }
                }

                // Add the forecast
                previousForecasts.push_front(patch->getGatherProbabilityForecast());
                if (previousForecasts.size() > GATHER_FORECAST_FRAMES) previousForecasts.pop_back();

                // Update the count of other patches saturated at various frame counts
                frameIdx = -1;
                for (auto &otherPatchesGatheredProbability : patch->getAllOtherPatchesGatheredProbabilityForecast())
                {
                    frameIdx++;

                    otherPatchesSaturatedCount[frameIdx]++;
                    if (otherPatchesGatheredProbability > (1.0 - EPSILON))
                    {
                        otherPatchesSaturated[frameIdx]++;
                    }
                }
            }
        }
#endif
    }

    void writeInstrumentation()
    {
#if TRACK_MINING_EFFICIENCY
        // For each patch, detect incorrect mining behaviour and draw
        // For the single-worker case, incorrect behaviour means:
        // - Waiting for more than one frame to mine, unless there has been an order timer reset
        // - Waiting for more frames than needed after an order timer reset
        // - Taking a longer path back to the depot (TODO)
        // - Waiting too long to leave the depot
        // For the two-worker case, incorrect behaviour means:
        // - Waiting for more than one frame to mine when there has not been an order timer reset
        // - Waiting for more than the maximum expected number of frames to mine when there has been an order timer reset
        // - Second worker not arriving quickly enough (TODO)
        for (auto &[patch, miningStatus] : resourceToMiningStatus)
        {
            // Get the status and how many frames it has been stable
            bool current = true;
            int currentStatus = -1;
            int currentFrames = 0;
            int previousStatus = -1;
            int previousFrames = 0;
            int extraData = -1;
            for (auto &[status, frame, frameExtraData] : std::ranges::reverse_view(miningStatus)) {
                // Gap in data: break immediately
                if (frame != (currentFrame - currentFrames - previousFrames)) break;

                if (current)
                {
                    if (currentFrames == 0)
                    {
                        currentStatus = status;
                        extraData = frameExtraData;
                    }
                    else if (status != currentStatus)
                    {
                        current = false;
                        previousStatus = status;
                    }
                }
                else if (status != previousStatus)
                {
                    break;
                }
                (current ? currentFrames : previousFrames)++;
            }
            if (currentFrames == 0) continue;

            // Check for an inefficiency based on the status
            std::string inefficiency;
            switch (currentStatus)
            {
                // These cases do not matter
                case 0:
                case 2:
                case 3:
                case 4:
                case 10:
                default:
                    break;

                case 1:
                {
                    // Single worker waiting too long to mine
                    if (currentFrames > 1)
                    {
                        int orderTimerResetBeforeArrival = OrderProcessTimer::framesToPreviousReset(currentFrame - currentFrames + 1);
                        if (orderTimerResetBeforeArrival > 11)
                        {
                            inefficiency = "start-mining-wait-no-reset";
#if LOG_INEFFICIENCIES
                            CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                             << ": worker waited too many frames to start mining (no reset)";
#endif
                        }
                        else if (currentFrames > (12 - orderTimerResetBeforeArrival))
                        {
                            // If orderTimerResetBeforeArrival is between 8-11, expect maximum wait to be 12-orderTimerResetBeforeArrival
                            // Otherwise maximum wait can be up to 7 because of order process timer reset

                            inefficiency = "start-mining-wait-reset";
#if LOG_INEFFICIENCIES
                            CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                             << ": worker waited too many frames to start mining (reset)";
#endif
                        }
                    }
                    break;
                }

                case 5:
                {
                    // Single worker taking long path to depot or waiting too long to leave the depot
                    if (currentFrames > 5)
                    {
                        inefficiency = "leave-depot";
#if LOG_INEFFICIENCIES
                        CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                         << ": worker waited too many frames to leave depot";
#endif
                    }
                    break;
                }

                case 11:
                {
                    // TODO: Check if worker should have already been at minerals
                    break;
                }
                case 12:
                case 13:
                {
                    bool extraFrame = (currentStatus == 13);
                    int miningStart = extraData;

                    int resetFrameAfterMiningStart = OrderProcessTimer::framesToNextReset(miningStart + 1);
                    int miningEnd = currentFrame - currentFrames - (previousStatus == 11 ? previousFrames : 0) + 1;

                    int expectedWaitingFrames;

                    if (resetFrameAfterMiningStart > 82 || (resetFrameAfterMiningStart == 82 && !extraFrame))
                    {
                        // There hasn't been an order timer reset while the worker was mining, so any delay is an inefficiency
                        // The second condition is for when the order timer reset occurred exactly on the frame the mining completed; in this case we
                        // can take over immediately if the mining worker's orders are processed first
                        expectedWaitingFrames = (extraFrame ? 2 : 1);
                        inefficiency = "takeover-no-reset";
                    }
                    else if (resetFrameAfterMiningStart == 82 || (resetFrameAfterMiningStart == 81 && !extraFrame))
                    {
                        // This is a special case where the mining worker is not affected by an order timer reset, so its timing is predictable,
                        // but the taking over worker should start mining on the exact frame of an order timer reset
                        // Here we can't avoid waiting for the reset order timer to expire again and incur an additional 7 frame delay in the worst
                        // case
                        expectedWaitingFrames = 7 + (extraFrame ? 2 : 1);
                        inefficiency = "takeover-reset-on-takeover-frame";
                    }
                    else if (resetFrameAfterMiningStart < 73)
                    {
                        // There has been an order timer reset before the worker taking over needed to send its final command to mine
                        // Here we time the worker taking over to start mining at the worst-case frame the mining worker finishes
                        // This corresponds to 2 frames later than the usual timing, as the order timer is at 6 when the mining timer expires
                        // without an order timer reset
                        expectedWaitingFrames = (miningStart + 84) - miningEnd + (extraFrame ? 1 : 0);
                        inefficiency = "takeover-reset-mid-mining";
                    }
                    else
                    {
                        // There is an order timer reset around the end of mining, meaning any commands we try to send to the worker taking over
                        // to optimize the timing would be reset and therefore not be predictable
                        // We therefore send the command to the taking over worker to take effect on the reset frame, negating the effects of the
                        // reset but incurring extra waiting time (but not as much as having the worker try to switch patches)
                        expectedWaitingFrames = (miningStart + resetFrameAfterMiningStart + 12) - miningEnd;
                        inefficiency = "takeover-reset-end-mining";
                    }

                    // If the worker was still moving towards the patch at the expected takeover frame, use the single-worker logic instead
                    // In this case we expect the worker to have issued a command so it starts mining immediately at arrival, taking order
                    // resets into consideration
                    if (previousStatus == 11 && (currentFrame - miningEnd - (extraFrame ? 1 : 0)) >= currentFrames)
                    {
                        // TODO: Implement check for arrival frame
                        inefficiency.clear();
                    }

                    // Clear inefficiency if we are within the tolerance
                    if ((currentFrames + (previousStatus == 11 ? previousFrames : 0)) <= expectedWaitingFrames)
                    {
                        inefficiency.clear();
                    }

                    if (!inefficiency.empty())
                    {
#if LOG_TWOPATCH_TAKEOVER_ERRORS
                        Log::Get() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                   << ": waited " << (currentFrames + (previousStatus == 11 ? previousFrames : 0))
                                   << "; expected=" << expectedWaitingFrames
                                   << "; current=" << currentStatus << ":" << currentFrames
                                   << "; previous=" << previousStatus << ":" << previousFrames
                                   << "; miningStart=" << miningStart
                                   << "; miningEnd=" << miningEnd
                                   << "; resetFrameAfterMiningStart=" << resetFrameAfterMiningStart
                                   << "; extraFrame=" << extraFrame
                                   << "; " << inefficiency;
#endif
#if LOG_INEFFICIENCIES
                        CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                         << ": worker waited too many frames to take over mining (" << inefficiency << ")";
#endif
                    }
#if LOG_TWOPATCH_TAKEOVER
                    else
                    {
                        Log::Get() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                   << ": OK"
                                   << "; current=" << currentStatus << ":" << currentFrames
                                   << "; previous=" << previousStatus << ":" << previousFrames
                                   << "; miningStart=" << miningStart
                                   << "; miningEnd=" << miningEnd
                                   << "; resetFrameAfterMiningStart=" << resetFrameAfterMiningStart
                                   << "; extraFrame=" << extraFrame;
                    }
#endif
                    break;
                }
            }

            if (!inefficiency.empty())
            {
                patchToAlert[patch] = std::make_pair(currentFrame, inefficiency);
            }
        }

#if DRAW_INEFFICIENCIES
        for (auto it = patchToAlert.begin(); it != patchToAlert.end(); )
        {
            if ((currentFrame - it->second.first) > 24)
            {
                it = patchToAlert.erase(it);
                continue;
            }

            CherryVis::drawCircle(it->first->center.x, it->first->center.y, 32, CherryVis::DrawColor::Red);
            CherryVis::drawText(it->first->center.x, it->first->center.y, it->second.second);
            it++;
        }
#endif

        if (currentFrame % 1000 == 0)
        {
            auto efficiency = getEfficiency(currentFrame - 1000, currentFrame);
            Log::Get() << "Mining efficiency over past 1000 frames: " << efficiency;
        }
#endif
    }

    std::map<Resource, Efficiency> getEfficiencyByPatch(int fromFrame, int toFrame)
    {
        std::map<Resource, Efficiency> result;

#if TRACK_MINING_EFFICIENCY
        for (auto &[patch, miningStatus] : resourceToMiningStatus)
        {
            PatchData sgl, dbl;
            addSinglePatchData(sgl, patch, miningStatus, fromFrame, toFrame);
            addDoublePatchData(dbl, patch, miningStatus, fromFrame, toFrame);

            unsigned int gatherCollisions = 0;
            unsigned int gatherNonCollisions = 0;
            unsigned int returnCollisions = 0;
            unsigned int returnNonCollisions = 0;
            unsigned int facingPatch = 0;
            unsigned int notFacingPatch = 0;
            addObservationMapData(gatherCollisions, gatherNonCollisions, resourceToGatherCollisionObservations, patch, fromFrame, toFrame);
            addObservationMapData(returnCollisions, returnNonCollisions, resourceToReturnCollisionObservations, patch, fromFrame, toFrame);
            addObservationMapData(facingPatch, notFacingPatch, resourceToFacingPatchObservations, patch, fromFrame, toFrame);

            result[patch] = computeEfficiency(
                    sgl,
                    dbl,
                    computeObservationRate(gatherCollisions, gatherNonCollisions),
                    computeObservationRate(returnCollisions, returnNonCollisions),
                    computeObservationRate(facingPatch, notFacingPatch));
        }
#endif

        return result;
    }

    Efficiency getEfficiency(int fromFrame, int toFrame)
    {
#if TRACK_MINING_EFFICIENCY
        PatchData sgl, dbl;
        unsigned int gatherCollisions = 0;
        unsigned int gatherNonCollisions = 0;
        unsigned int returnCollisions = 0;
        unsigned int returnNonCollisions = 0;
        unsigned int facingPatch = 0;
        unsigned int notFacingPatch = 0;

        for (auto &[patch, miningStatus] : resourceToMiningStatus)
        {
            addSinglePatchData(sgl, patch, miningStatus, fromFrame, toFrame);
            addDoublePatchData(dbl, patch, miningStatus, fromFrame, toFrame);

            addObservationMapData(gatherCollisions, gatherNonCollisions, resourceToGatherCollisionObservations, patch, fromFrame, toFrame);
            addObservationMapData(returnCollisions, returnNonCollisions, resourceToReturnCollisionObservations, patch, fromFrame, toFrame);
            addObservationMapData(facingPatch, notFacingPatch, resourceToFacingPatchObservations, patch, fromFrame, toFrame);
        }

        return computeEfficiency(
                sgl,
                dbl,
                computeObservationRate(gatherCollisions, gatherNonCollisions),
                computeObservationRate(returnCollisions, returnNonCollisions),
                computeObservationRate(facingPatch, notFacingPatch));
#else
        return Efficiency{0.0,0.0,0.0,0.0,0.0,0.0};
#endif
    }

    int getFiftiethMineralFrame()
    {
        return fiftiethMineralFrame;
    }

    std::vector<int> &getThousandMineralFrames()
    {
        return thousandMineralFrames;
    }

    void addRotationTimesToResourceObservations()
    {
        for (auto &[patch, miningStatus] : resourceToMiningStatus)
        {
            PatchData sgl, dbl;
            addSinglePatchData(sgl, patch, miningStatus, -1, -1, true);
            addDoublePatchData(dbl, patch, miningStatus, -1, -1, true);

            auto &observations = WorkerMiningOptimization::resourceObservationsFor(patch);
            for (const auto &rotationTime : sgl.allRotationTimes)
            {
                observations.singleWorkerRotations.addObservation(rotationTime);
            }
            for (const auto &rotationTime : dbl.allRotationTimes)
            {
                observations.doubleWorkerRotations.addObservation(rotationTime);
            }
        }
    }

    void writeGameEndInstrumentation()
    {
#if TRACK_MINING_EFFICIENCY
        {
            auto miningEfficiency = WorkerMiningInstrumentation::getEfficiency();
            Log::Get() << "Mining efficiency over entire game: " << miningEfficiency;
            CherryVis::log() << "Mining efficiency over entire game: " << miningEfficiency;
            Log::Get() << "50th mineral frame: " << WorkerMiningInstrumentation::getFiftiethMineralFrame();
#if TRACK_MINING_EFFICIENCY_VERBOSE
            Log::Get() << "Thousand mineral frames: " << LogFormattingUtil::formatVectorlike(WorkerMiningInstrumentation::getThousandMineralFrames());
#endif
        }
#endif
#if TRACK_RESOURCE_FORECAST_ACCURACY
        {
            std::array<int, GATHER_FORECAST_FRAMES> totalPredictionCounts = {0};
            std::array<double, GATHER_FORECAST_FRAMES> totalTotalFramePredictionError = {0.0};
            std::array<int, GATHER_FORECAST_FRAMES> totalCountOfFalsePositives = {0};
            std::array<int, GATHER_FORECAST_FRAMES> totalOtherPatchesSaturated = {0};
            std::array<int, GATHER_FORECAST_FRAMES> totalOtherPatchesSaturatedCount = {0};
            for (auto &[patch, _] : resourceToFramePredictionCounts)
            {
                auto &framePredictionCounts = resourceToFramePredictionCounts[patch];
                auto &totalFramePredictionError = resourceToTotalFramePredictionError[patch];
                auto &countOfFalsePositives = resourceToCountOfFalsePositives[patch];
                auto &otherPatchesSaturated = resourceToOtherPatchesSaturated[patch];
                auto &otherPatchesSaturatedCount = resourceToOtherPatchesSaturatedCount[patch];

                std::ostringstream errors, falsePositives, otherPatches;
                errors << std::fixed << std::setprecision(2) << patch->tile << " total error: [";
                falsePositives << std::fixed << std::setprecision(3) << patch->tile << " false positive rate: [";
                otherPatches << std::fixed << std::setprecision(3) << patch->tile << " other patches saturated: [";
                std::string sep;
                for (int i = 0; i < GATHER_FORECAST_FRAMES; i++)
                {
                    errors << sep << totalFramePredictionError[i];
                    falsePositives << sep << ((double)countOfFalsePositives[i] * 100.0 / (double)framePredictionCounts[i]);
                    otherPatches << sep << ((double)otherPatchesSaturated[i] * 100.0 / (double)otherPatchesSaturatedCount[i]);
                    sep = ", ";
                }
                Log::Get() << errors.str() << "]";
                Log::Get() << falsePositives.str() << "]";
                Log::Get() << otherPatches.str() << "]";

                // Add to the totals
                std::transform(totalPredictionCounts.begin(),
                               totalPredictionCounts.end(),
                               framePredictionCounts.begin(),
                               totalPredictionCounts.begin(),
                               std::plus<>{});
                std::transform(totalTotalFramePredictionError.begin(),
                               totalTotalFramePredictionError.end(),
                               totalFramePredictionError.begin(),
                               totalTotalFramePredictionError.begin(),
                               std::plus<>{});
                std::transform(totalCountOfFalsePositives.begin(),
                               totalCountOfFalsePositives.end(),
                               countOfFalsePositives.begin(),
                               totalCountOfFalsePositives.begin(),
                               std::plus<>{});
                std::transform(totalOtherPatchesSaturated.begin(),
                               totalOtherPatchesSaturated.end(),
                               otherPatchesSaturated.begin(),
                               totalOtherPatchesSaturated.begin(),
                               std::plus<>{});
                std::transform(totalOtherPatchesSaturatedCount.begin(),
                               totalOtherPatchesSaturatedCount.end(),
                               otherPatchesSaturatedCount.begin(),
                               totalOtherPatchesSaturatedCount.begin(),
                               std::plus<>{});
            }

            // Output the totals
            {
                std::ostringstream errors, falsePositives, otherPatches;
                errors << std::fixed << std::setprecision(2) << " overall total error: [";
                falsePositives << std::fixed << std::setprecision(3) << " overall false positive rate: [";
                otherPatches << std::fixed << std::setprecision(3) << " overall other patches saturated: [";
                std::string sep;
                for (int i = 0; i < GATHER_FORECAST_FRAMES; i++)
                {
                    errors << sep << totalTotalFramePredictionError[i];
                    falsePositives << sep << ((double)totalCountOfFalsePositives[i] * 100.0 / (double)totalPredictionCounts[i]);
                    otherPatches << sep << ((double)totalOtherPatchesSaturated[i] * 100.0 / (double)totalOtherPatchesSaturatedCount[i]);
                    sep = ", ";
                }
                Log::Get() << errors.str() << "]";
                Log::Get() << falsePositives.str() << "]";
                Log::Get() << otherPatches.str() << "]";
            }
        }
#endif
    }
}
