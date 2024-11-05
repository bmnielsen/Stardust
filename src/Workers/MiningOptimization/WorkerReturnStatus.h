#pragma once

#include "DebugFlag_WorkerMiningOptimization.h"

#include "MyWorker.h"
#include "Resource.h"
#include "PositionAndVelocity.h"
#include "Geo.h"

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

        // Position at which the return command was resent
        std::shared_ptr<const PositionAndVelocity> resentPosition;

        WorkerReturnStatus(MyWorker worker, MyUnit depot, Resource resource)
                : worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , lastProcessedFrame(-2)
                , pathStartsAtPatch(false)
                , resendPlanned(false)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            positionHistory.clear();
            pathStartsAtPatch = false;
            resendPlanned = false;
            plannedResendPosition = nullptr;
            expectedPath.clear();
            resentPosition = nullptr;
        }

        [[nodiscard]] bool validForObservations() const
        {
            if (positionHistory.empty()) return false;
            if (positionHistory.size() > 60) return false; // usually means distance mining
            return pathStartsAtPatch;
        }

        std::shared_ptr<PositionAndVelocity> appendCurrentPosition()
        {
            lastProcessedFrame = currentFrame;

            // First append sets whether the path starts at the patch
            if (positionHistory.empty())
            {
                pathStartsAtPatch = (resource->getDistance(worker) == 0);

                auto currentPosition = std::make_shared<PositionAndVelocity>(worker, nullptr);
                positionHistory.emplace_back(currentPosition);
                return currentPosition;
            }

            // If the path starts at the patch, include hashes of the previous positions
            // This helps us detect when the worker reaches the same position via a different path, indicating different subpixel positioning
            auto currentPosition = std::make_shared<PositionAndVelocity>(
                    worker,
                    pathStartsAtPatch ? positionHistory.rbegin()->get() : nullptr);
            positionHistory.emplace_back(currentPosition);
            return currentPosition;
        }

        void sendReturnCommand(const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (!worker->returnCargo())
            {
#if OPTIMALPOSITIONS_DEBUG
                Log::Get() << "Failed to send return command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                           << BWAPI::Broodwar->getLastError();
                CherryVis::log(worker->id) << "Failed to send return command; last error " << BWAPI::Broodwar->getLastError();
                CherryVis::log(resource->id) << "Failed to send return command; last error " << BWAPI::Broodwar->getLastError();
#endif
                return;
            }

            resentPosition = currentPosition;
        }
    };
}
