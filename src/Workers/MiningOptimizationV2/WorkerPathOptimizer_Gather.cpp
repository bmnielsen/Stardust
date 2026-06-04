#include "WorkerPathOptimizer.h"

#include "Workers.h"
#include "OrderProcessTimer.h"
#include "LogFormattingUtil.h"

#include "DebugFlag_MiningOptimization.h"

namespace MiningOptimization
{
    namespace
    {
        void takeoverActionFrameSetIntersection(std::set<int> &current, const std::set<int> &intersector)
        {
            // Just assign if this is the first intersect
            if (current.empty())
            {
                current = intersector;
                return;
            }

            // Extend the current set if needed
            if (*intersector.rbegin() > *current.rbegin())
            {
                for (int frame = (*current.rbegin() + 1); frame <= *intersector.rbegin(); frame++)
                {
                    current.insert(frame);
                }
            }

            // Remove items from the current set that aren't in the new one
            for (auto it = current.begin(); it != current.end(); )
            {
                if (intersector.contains(*it))
                {
                    ++it;
                }
                else
                {
                    it = current.erase(it);
                }
            }
        }
    }

    template <>
    std::set<int> WorkerPathOptimizer<GatherArrivalData>::takeoverActionFrames()
    {
        // TODO: Handle case where we don't have a path but are close enough to the patch to be able to make some guarantees about possible frames
        if (!expectedPath) return {};

        // Compute this on each solver result leaf node and arregate up at each step up the tree
        // To compute it at a leaf node, we have the last resend and the arrival data including when resends always arrive, so we can run a
        // simulation with all potential later resend timings and gather the frames that fall out of that
        // We only consider frames that are 100% achievable by all possible arrival values
        // We don't consider solutions that require knowing in advance which path branch we end up on - TODO should we consider equivalent branches?
        // Order process timer resets within 10-distance should be captured here
        // Store on the solver result how each frame will be achieved as well, so we don't have to recompute it later
        // Once the data has been added to the solver result, we don't need to recompute it

        if (expectedPath->takeoverActionFrames)
        {
            return *expectedPath->takeoverActionFrames;
        }

        std::function<void(SolverResult<GatherArrivalData> &, std::set<int>, int)> processBranch;
        processBranch = [&](SolverResult<GatherArrivalData> &branch, std::set<int> resendFrames, int branchStartFrame)
        {
            resendFrames.insert(branch.resendFramesOnThisBranch.begin(), branch.resendFramesOnThisBranch.end());

            if (branch.nextBranches.empty())
            {
                auto processArrivalData = [&](const SolverArrivalData &arrivalData)
                {
                    std::set<int> result;

                    // Find the first frame a resend can take effect that is guaranteed to reach the patch on time
                    // This is either the resend always arrives frame, or the frame LF from the start of the branch if this is higher
                    int startFrame = std::max(arrivalData.resendAlwaysArrivesFrame, branchStartFrame + BWAPI::Broodwar->getLatencyFrames());

                    // Get the worker's order process timer values at this frame
                    auto orderProcessTimer = OrderProcessTimer::atStartOfFrameAtDelta(
                        currentFrame,
                        worker->possibleOrderProcessTimerValues,
                        resendFrames,
                        {},
                        startFrame - currentFrame);


                    // Initialize with the state at the start of branch or last resend frame



                    return result;
                };

                std::set<int> takeoverActionFrames;
                for (const auto &[arrivalData, _] : branch.arrivalDataWithProbabilities)
                {
                    takeoverActionFrameSetIntersection(takeoverActionFrames, processArrivalData(arrivalData));
                }

                // TODO: Leaf node logic that actually does the simulation

                branch.takeoverActionFrames = {};
                return;
            }

            // This is an intermediate branch, so compute the results from all of the next branches and aggregate them here
            // The aggregation is a set intersection, but it needs to take into account that each set implicitly contains all values after
            // the highest one

            std::set<int> takeoverActionFrames;
            for (auto &nextBranch : branch.nextBranches)
            {
                processBranch(nextBranch, resendFrames, branchStartFrame + (int)branch.pathToNextBranch.size());
                takeoverActionFrameSetIntersection(takeoverActionFrames, *nextBranch.takeoverActionFrames);
            }
            branch.takeoverActionFrames = takeoverActionFrames;
        };

        processBranch(*expectedPath, {}, currentFrame + 1);
        return *expectedPath->takeoverActionFrames;
    }

    template <>
    bool WorkerPathOptimizer<GatherArrivalData>::isComplete()
    {
        return worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals
            || worker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals;
    }

    template <>
    void WorkerPathOptimizer<GatherArrivalData>::setStartOfPathFlags()
    {
        // Started at the end of last path if the worker just delivered minerals
        if (!worker->carryingResource && worker->lastCarryingResourceChange == currentFrame)
        {
            setFlag(StatusFlags::StartedAtPreviousPathEnd);
        }

        // Is at the initial spawn position if this is the start of the game
        if (currentFrame <= 2 && worker->lastPosition == worker->spawnPosition)
        {
            setFlag(StatusFlags::StartedAtInitialSpawnPosition);
        }
    }

    template <>
    bool WorkerPathOptimizer<GatherArrivalData>::skipPathOptimization()
    {
        // Ensure the resource is visible
        auto resourceBwapiUnit = resource->getBwapiUnitIfVisible();
        if (!resourceBwapiUnit)
        {
            CherryVis::log(worker->id) << "mineral field unit not visible";
            return true;
        }

        // Ensure the worker is targeting the correct resource
        // Our logic ensures mineral locking automatically except in some specific cases:
        // - worker has been released from combat, which can leave it with a gather order to a random patch used for kiting
        // - workers have been avoiding a no-go area and returning to mining as a group, so the timing gets messed up
        // - both workers reach the patch at approximately the same time after one or both are (re)assigned
        // - we get a diversion from our observed path and are unlucky with the order timer
        // - we mispredict a patch locking
        if (worker->bwapiUnit->getOrderTarget() && worker->bwapiUnit->getOrderTarget()->getResources()
            && worker->bwapiUnit->getOrderTarget() != resourceBwapiUnit
            && worker->lastCommandFrame < (currentFrame - BWAPI::Broodwar->getLatencyFrames()))
        {
#if LOGGING_ENABLED
            CherryVis::log(worker->id) << "targeting different patch; resending order";
#endif
            // There could be a Unit_Busy failure here, but we will pick up next frame that the command hasn't been issued
            if (worker->gather(resourceBwapiUnit))
            {
                executedResendFrames.insert(currentFrame);
            }
            setFlag(StatusFlags::SwitchedPatch);
            resetPath();
            return true;
        }

        // If the worker is transitioning to mine, nothing more is needed
        if (worker->bwapiUnit->getOrder() == BWAPI::Orders::WaitForMinerals)
        {
            // If another worker is mining the patch, ensure we have marked patch locking
            if (actualPatchLockFrame == -1)
            {
                auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
                if (otherWorker && otherWorker->bwapiUnit->getOrder() == BWAPI::Orders::MiningMinerals)
                {
                    actualPatchLockFrame = currentFrame;

#if VERBOSE_PATH_LOGGING
                    CherryVis::log(worker->id) << "Patch locked";
#endif
                }
            }

            return true;
        }

        return false;
    }

    template <>
    void WorkerPathOptimizer<GatherArrivalData>::initializeGatherTakeover()
    {
        auto otherWorker = Workers::getOtherWorkerMining(resource, worker);
        if (!otherWorker)
        {
            // There might have been another worker earlier, if so clear all of our state
            // TODO: Replan if possible
            if (hasFlag(StatusFlags::GatherTakeover))
            {
                reset();
            }
            return;
        }

        setFlag(StatusFlags::GatherTakeover);

        // If the other worker is not mining, clear the takeover frames, as the patch is now available
        if (otherWorker->bwapiUnit->getOrder() != BWAPI::Orders::MiningMinerals || otherWorker->lastStartedMining == -1)
        {
            takeoverFrames.clear();
            return;
        }

        // Compute the takeover frame probabilities, i.e. the probability at any given frame that we will be able to take over mining from the other
        // worker

        // Nothing is needed if they have already been computed
        if (!takeoverFrames.empty()) return;

        // Take into account the relative order process index of the workers: if this worker's orders are processed before the other worker's,
        // we need an extra frame before the patch is available
        int extraFrame = (worker->orderProcessIndex <= otherWorker->orderProcessIndex) ? 1 : 0;

        // With no order timer resets, mining will end 81 frames after it starts
        int miningEndFrame = otherWorker->lastStartedMining + 81;

        // Check for an order process timer reset before mining end
        int previousOrderTimerReset = OrderProcessTimer::previousResetFrame(miningEndFrame);
        if (previousOrderTimerReset < otherWorker->lastStartedMining)
        {
            // There was no reset, so the patch will be available at the end frame computed earlier
            takeoverFrames[miningEndFrame + extraFrame] = 1.0;

#if VERBOSE_TAKEOVER_LOGGING
            CherryVis::log(otherWorker->id) << "Extra frame: " << extraFrame
                                            << "\nTakeover frame: " << (miningEndFrame + extraFrame);
#endif
            return;
        }

        // There was an order process timer reset, so mining could end at the latest of the following frames:
        // - Mining timer expiry
        // - Order process timer reset
        // - Next frame
        int earliestMiningEndFrame = std::max({otherWorker->lastStartedMining + 75, previousOrderTimerReset, currentFrame + 1});

        // Get the possible order process timer values at the earliest end frame
        std::multiset<int> orderProcessTimerValuesAtEarliestMiningEndFrame;
        if (OrderProcessTimer::isResetFrame(currentFrame + 1))
        {
            orderProcessTimerValuesAtEarliestMiningEndFrame = {0, 1, 2, 3, 4, 5, 6, 7};
        }
        else
        {
            orderProcessTimerValuesAtEarliestMiningEndFrame =
                    OrderProcessTimer::atStartOfFrameAtDelta(currentFrame + 1,
                                                             otherWorker->possibleOrderProcessTimerValues,
                                                             {},
                                                             {},
                                                             (earliestMiningEndFrame - (currentFrame + 1)));
        }

        // Now generate the probabilities of each end frame
        for (auto orderProcessTimerValue : orderProcessTimerValuesAtEarliestMiningEndFrame)
        {
            takeoverFrames[earliestMiningEndFrame + orderProcessTimerValue + extraFrame]
                = 1.0 / (double)orderProcessTimerValuesAtEarliestMiningEndFrame.size();
        }

#if VERBOSE_TAKEOVER_LOGGING
        CherryVis::log(otherWorker->id) << "Extra frame: " << extraFrame
                                        << "\nTakeover frames: " << LogFormattingUtil::formatProbabilityMap(takeoverFrames, INT_MAX);
#endif
    }

    template <>
    bool WorkerPathOptimizer<GatherArrivalData>::issueResend()
    {
        auto bwapiUnit = resource->getBwapiUnitIfVisible();
        if (!bwapiUnit)
        {
            BWAPI::Broodwar->setLastError(BWAPI::Errors::Unit_Not_Visible);
            return false;
        }

        return worker->gather(bwapiUnit);
    }

    template class WorkerPathOptimizer<GatherArrivalData>;
}
