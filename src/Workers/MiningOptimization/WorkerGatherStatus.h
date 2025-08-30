#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "PositionAndVelocity.h"
#include "GatherPositionObservations.h"

namespace WorkerMiningOptimization
{
    struct WorkerGatherStatus
    {
        // The worker gathering
        MyWorker worker;

        // The depot returning to
        MyUnit depot;

        // The resource being gathered from
        Resource resource;

        // The last frame this worker was optimized
        int lastProcessedFrame;

        // The positions this worker has visited while on its way to the patch
        std::vector<std::shared_ptr<const PositionAndVelocity>> positionHistory;

        // Whether the path started at the depot
        bool pathStartsAtDepot;

        // Whether the path started at the worker's spawn position
        bool pathStartsAtSpawnPosition;

        // How many positions should be ignored before optimizing the worker
        int remainingIgnorePositions;

        // Whether the worker has left the depot
        bool hasLeftDepot;

        // Whether we have planned the resends we want to send on this path
        bool resendsPlanned;

        // Planned first resend position
        std::unique_ptr<GatherPositionObservationPtr> plannedResendPosition;

        // Planned second resend position
        std::unique_ptr<GatherPositionObservationPtr> plannedSecondResendPosition;

        // The expected node path the worker will follow
        std::deque<GatherPositionObservationPtr> expectedPath;

        // The current position of the worker in the expected path
        std::unique_ptr<GatherPositionObservationPtr> currentNode;

        // The expected frame the worker will arrive at the patch
        int expectedArrivalFrame;

        // Positions at which the gather command was resent
        std::vector<std::shared_ptr<const PositionAndVelocity>> resentPositions;

        // Frames at which we have send gather commands
        std::set<int> resentFrames;

        // Mode to use for takeover, current allowed values: 0=use normal approach optimization, 1=use takeover optimization, 2=at patch
        int takeoverState;

        // The frame where we want to take over mining from another worker
        int takeoverFrame;

        // Tracks the frame when the worker passed a position LF+1 before reaching 10 distance from the patch
        int passed10DistancePosition;

        // Whether the worker passed a previously-unregistered 10-distance position
        bool passedUnregistered10DistancePosition;

        // Whether the worker switched patches while trying to mine
        bool switchedPatches;

        // Whether the worker reached the WaitForMinerals state while another worker was still mining
        // This is a similar situation to patch switching, but happens if the worker is already at the patch
        // We track observations on the first WaitForMinerals frame
        bool waitForMineralsWhileOtherStillMining;

        // Whether the worker had path data it could use on this approach
        bool hasPathData;

        // Whether the worker is expected to lock to the patch at this given frame
        // If the worker is not expected to lock to the patch, this will be -1
        int expectedPatchLockFrame;

        // The frame this worker is expected to start mining
        // Not relevant if the worker has an expected patch lock frame, since the mining start frame then depends on the
        // worker being taken over from
        // If the expected mining start frame is unknown, this will be -1
        int expectedMiningStartFrame;

        WorkerGatherStatus(MyWorker worker, MyUnit depot, Resource resource)
                : worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , lastProcessedFrame(-2)
                , pathStartsAtDepot(false)
                , pathStartsAtSpawnPosition(false)
                , remainingIgnorePositions(0)
                , hasLeftDepot(false)
                , resendsPlanned(false)
                , expectedArrivalFrame(-1)
                , takeoverState(0)
                , takeoverFrame(-1)
                , passed10DistancePosition(-1)
                , passedUnregistered10DistancePosition(false)
                , switchedPatches(false)
                , waitForMineralsWhileOtherStillMining(false)
                , hasPathData(false)
                , expectedPatchLockFrame(-1)
                , expectedMiningStartFrame(-1)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            positionHistory.clear();
            pathStartsAtDepot = false;
            pathStartsAtSpawnPosition = false;
            remainingIgnorePositions = 0;
            hasLeftDepot = false;
            resendsPlanned = false;
            expectedArrivalFrame = -1;
            plannedResendPosition = nullptr;
            plannedSecondResendPosition = nullptr;
            expectedPath.clear();
            currentNode = nullptr;
            resentPositions.clear();
            resentFrames.clear();
            takeoverState = 0;
            takeoverFrame = -1;
            passed10DistancePosition = -1;
            passedUnregistered10DistancePosition = false;
            switchedPatches = false;
            waitForMineralsWhileOtherStillMining = false;
            hasPathData = false;
            expectedPatchLockFrame = -1;
            expectedMiningStartFrame = -1;
        }

        bool ignoreThisPosition()
        {
            if (!positionHistory.empty()) return false;

            // If the path started at the worker's spawn position, ignore the first 12 positions
            // The rationale for this is that spawning workers get a random heading, so treating each spawn location separately will result in
            // an excessive number of root nodes
            if (!pathStartsAtSpawnPosition)
            {
                pathStartsAtSpawnPosition = (worker->lastCarryingResourceChange == -1) && (worker->lastPosition == worker->spawnPosition);
                if (!pathStartsAtSpawnPosition) return false;

                remainingIgnorePositions = 12;
            }

            if (remainingIgnorePositions == 0) return false;
            remainingIgnorePositions--;
            return true;
        }

        std::shared_ptr<PositionAndVelocity> appendCurrentPosition();

        bool sendGatherCommand(BWAPI::Unit resourceBwapiUnit, const std::shared_ptr<PositionAndVelocity> &currentPosition);

        [[nodiscard]] std::shared_ptr<const PositionAndVelocity> resentPosition() const
        {
            if (resentPositions.empty()) return nullptr;
            return *resentPositions.begin();
        }

        [[nodiscard]] int plannedResendCount() const
        {
            int result = 0;
            if (plannedResendPosition) result++;
            if (plannedSecondResendPosition) result++;
            return result;
        }

        [[nodiscard]] int lastResendFrame() const
        {
            if (resentFrames.empty()) return -1;
            return *resentFrames.rbegin();
        }
    };
}
