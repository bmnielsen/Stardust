#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "PositionAndVelocity.h"

namespace WorkerMiningOptimization
{
    struct WorkerReturnStatus
    {
        // The worker gathering
        MyWorker worker;

        // The depot returning to
        MyUnit depot;

        // The resource being gathered from
        Resource resource;

        // The last frame this worker was optimized
        int lastProcessedFrame;

        // The positions this worker has visited while on its way to the depot
        std::vector<std::shared_ptr<const PositionAndVelocity>> positionHistory;

        // Whether the path started at the patch
        bool pathStartsAtPatch;

        // Whether we have planned the resend we want to send on this path
        bool resendPlanned;

        // Planned resend position
        std::shared_ptr<const PositionAndVelocity> plannedResendPosition;

        // The expected path the worker will follow
        std::deque<PositionAndVelocity> expectedPath;

        // Whether the planned resend position is being tried for exploratory purposes
        bool plannedResendIsForExploration;

        // The expected delay in frames between the resend and delivery of resource
        double expectedDelayAfterResend;

        // Position at which the return command was resent
        std::shared_ptr<const PositionAndVelocity> resentPosition;

        WorkerReturnStatus(MyWorker worker, MyUnit depot, Resource resource)
                : worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , lastProcessedFrame(-2)
                , pathStartsAtPatch(false)
                , resendPlanned(false)
                , plannedResendIsForExploration(false)
                , expectedDelayAfterResend(100.0)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            positionHistory.clear();
            pathStartsAtPatch = false;
            resendPlanned = false;
            plannedResendPosition = nullptr;
            expectedPath.clear();
            plannedResendIsForExploration = false;
            expectedDelayAfterResend = 100.0;
            resentPosition = nullptr;
        }

        [[nodiscard]] bool validForObservations() const
        {
            if (positionHistory.empty()) return false;
            if (positionHistory.size() > 60) return false; // usually means distance mining
            return pathStartsAtPatch;
        }

        std::shared_ptr<PositionAndVelocity> appendCurrentPosition();

        void sendReturnCommand(const std::shared_ptr<PositionAndVelocity> &currentPosition);
    };
}
