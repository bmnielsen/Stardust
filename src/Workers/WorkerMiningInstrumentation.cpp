#include "WorkerMiningInstrumentation.h"

#include <ranges>

#include "Workers.h"
#include "Map.h"
#include "OrderProcessTimer.h"
#include "MiningOptimization/WorkerMiningOptimization.h"

#define TRACK_MINING_EFFICIENCY true

#if INSTRUMENTATION_ENABLED
#define LOG_WORKER_ORDERS false
#define LOG_PATCH_STATUS false
#define LOG_TWOPATCH_TAKEOVER false
#define LOG_TWOPATCH_TAKEOVER_ERRORS false
#endif

namespace WorkerMiningInstrumentation
{
    namespace
    {
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
        // 12 = two workers assigned, one worker is returning cargo, other worker is waiting to mine, worker returning cargo has orders processed first
        // 13 = same as 12, but where worker returning cargo might not have had its orders processed first
        std::map<Resource, std::vector<std::tuple<int, int, int>>> resourceToMiningStatus;

        // All collision observations for each patch
        std::map<Resource, std::vector<std::pair<int, bool>>> resourceToCollisionObservations;

        // Map storing alerts for each patch, so we can show them for a while
        std::map<Resource, std::pair<int, std::string>> patchToAlert;

        // Indirection to the Workers::mineralsAndAssignedWorkers function allowing it to be overridden by tests
        std::function<std::map<Resource, std::set<MyWorker>> &()> getMineralsAndAssignedWorkers = Workers::mineralsAndAssignedWorkers;

        // Frame when we gathered the 50th mineral
        int fiftiethMineralFrame;

        // Frames when we reached each 1000 of minerals gathered
        std::vector<int> thousandMineralFrames;

        struct PatchData
        {
            unsigned int framesMined = 0;
            unsigned int framesNotMined = 0;
            unsigned int totalRotationFrames = 0;
            unsigned int rotationCount = 0;
        };

        void addPatchData(PatchData &patchData,
                          const Resource &patch,
                          const std::vector<std::tuple<int, int, int>> &miningStatus,
                          int miningState,
                          const std::set<int> &nonMiningStates,
                          int fromFrame,
                          int toFrame)
        {
            bool recording = false;
            int lastStatus = -1;
            int currentPeriodMined = 0;
            int currentPeriodNotMined = 0;
            int currentPeriodStartFrame = 0;
            int lastFrame = -1;
            for (auto &[status, frame, _] : miningStatus)
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

                if (status == miningState && nonMiningStates.contains(lastStatus))
                {
                    // This is the transition from not mining to mining, which we use as our start of a measurement period

                    // If we were recording a period, flush it now
                    if (recording)
                    {
                        patchData.framesMined += currentPeriodMined;
                        patchData.framesNotMined += currentPeriodNotMined;
                        patchData.totalRotationFrames += (frame - currentPeriodStartFrame);
                        patchData.rotationCount++;
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

        void addCollisionData(unsigned long &collisions, unsigned long &nonCollisions, const Resource &patch, int fromFrame, int toFrame)
        {
            for (const auto &[frame, collision] : resourceToCollisionObservations[patch])
            {
                if (fromFrame != -1 && frame < fromFrame) continue;
                if (toFrame != -1 && frame >= toFrame) continue;

                (collision ? collisions : nonCollisions)++;
            }
        }

        Efficiency computeEfficiency(const PatchData &sgl, const PatchData &dbl, unsigned long collisions, unsigned long nonCollisions)
        {
            auto computeMiningPercentage = [](const PatchData &pd)
            {
                if (pd.framesMined == 0 && pd.framesNotMined == 0) return 0.0;

                return 100.0 * (double)pd.framesMined / (double)(pd.framesMined + pd.framesNotMined);
            };

            unsigned long collisionObservations = collisions + nonCollisions;

            return Efficiency{
                    (sgl.rotationCount == 0) ? 0.0 : ((double)sgl.totalRotationFrames / (double)sgl.rotationCount),
                    computeMiningPercentage(sgl),
                    (dbl.rotationCount == 0) ? 0.0 : ((double)dbl.totalRotationFrames / (double)dbl.rotationCount),
                    computeMiningPercentage(dbl),
                    (collisionObservations == 0) ? +.0 : ((double)collisions / (double)collisionObservations),
            };
        }
    #endif
    }

    void initialize(const std::function<std::map<Resource, std::set<MyWorker>> &()> &getMineralsAndAssignedWorkersOverride)
    {
#if TRACK_MINING_EFFICIENCY
        resourceToMiningStatus.clear();
        patchToAlert.clear();
        if (getMineralsAndAssignedWorkersOverride)
        {
            getMineralsAndAssignedWorkers = getMineralsAndAssignedWorkersOverride;
        }
        fiftiethMineralFrame = -1;
        thousandMineralFrames.clear();
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

        if ((BWAPI::Broodwar->self()->gatheredMinerals() - 50) >= (1000 * (thousandMineralFrames.size() + 1)))
        {
            thousandMineralFrames.push_back(currentFrame);
            if (!WorkerMiningOptimization::isExploring())
            {
                Log::Get() << "Gathered " << (1000 * thousandMineralFrames.size()) << "th mineral";
            }
        }

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
            if (!closestBase || !closestBase->resourceDepot || !closestBase->resourceDepot->exists()) continue;
            auto &depot = closestBase->resourceDepot;

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
                
                if (workerA->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals ||
                    workerB->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    status = 10;
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
                    status = -1;
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
                        int orderTimerResetBeforeArrival = OrderProcessTimer::framesToPreviousReset(currentFrame - currentFrames);
                        if (orderTimerResetBeforeArrival > 11)
                        {
                            inefficiency = "start-mining-wait-no-reset";
                            CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                             << ": worker waited too many frames to start mining (no reset)";
                        }
                        else if (currentFrames > (12 - orderTimerResetBeforeArrival))
                        {
                            // If orderTimerResetBeforeArrival is between 8-11, expect maximum wait to be 12-orderTimerResetBeforeArrival
                            // Otherwise maximum wait can be up to 7 because of order process timer reset

                            inefficiency = "start-mining-wait-reset";
                            CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                             << ": worker waited too many frames to start mining (reset)";
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
                        CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                         << ": worker waited too many frames to leave depot";
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

                    int resetFrameAfterMiningStart = OrderProcessTimer::framesToNextReset(miningStart);
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
                        CherryVis::log() << "Patch @ " << BWAPI::WalkPosition(patch->center)
                                         << ": worker waited too many frames to take over mining (" << inefficiency << ")";
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

        if (currentFrame % 1000 == 0)
        {
            auto efficiency = getEfficiency(currentFrame - 1000, currentFrame);
            Log::Get() << "Mining efficiency over past 1000 frames: " << efficiency;
        }
#endif
    }

    void trackCollisionObservation(const Resource &patch, bool collision)
    {
        resourceToCollisionObservations[patch].emplace_back(currentFrame, collision);
    }

    std::map<Resource, Efficiency> getEfficiencyByPatch(int fromFrame, int toFrame)
    {
        std::map<Resource, Efficiency> result;

#if TRACK_MINING_EFFICIENCY
        for (auto &[patch, miningStatus] : resourceToMiningStatus)
        {
            PatchData sgl, dbl;
            unsigned long collisions = 0;
            unsigned long nonCollisions = 0;

            addPatchData(sgl, patch, miningStatus, 2, {0, 1, 3, 4, 5}, fromFrame, toFrame);
            addPatchData(dbl, patch, miningStatus, 10, {11, 12, 13}, fromFrame, toFrame);
            addCollisionData(collisions, nonCollisions, patch, fromFrame, toFrame);

            result[patch] = computeEfficiency(sgl, dbl, collisions, nonCollisions);
        }
#endif

        return result;
    }

    Efficiency getEfficiency(int fromFrame, int toFrame)
    {
#if TRACK_MINING_EFFICIENCY
        PatchData sgl, dbl;
        unsigned long collisions = 0;
        unsigned long nonCollisions = 0;

        for (auto &[patch, miningStatus] : resourceToMiningStatus)
        {
            addPatchData(sgl, patch, miningStatus, 2, {0, 1, 3, 4, 5}, fromFrame, toFrame);
            addPatchData(dbl, patch, miningStatus, 10, {11, 12, 13}, fromFrame, toFrame);
            addCollisionData(collisions, nonCollisions, patch, fromFrame, toFrame);
        }

        return computeEfficiency(sgl, dbl, collisions, nonCollisions);
#else
        return Efficiency{0.0,0.0,0.0,0.0};
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
}