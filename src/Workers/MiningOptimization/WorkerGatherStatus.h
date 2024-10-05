#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "PositionAndVelocity.h"
#include "Geo.h"

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

        // Whether we have planned the resends we want to send on this path
        bool resendsPlanned;

        // Planned first resend position
        std::shared_ptr<const PositionAndVelocity> plannedResendPosition;

        // Planned second resend position
        std::shared_ptr<const PositionAndVelocity> plannedSecondResendPosition;

        // The position at which the gather command was resent, or nullptr if it hasn't been resent
        std::shared_ptr<const PositionAndVelocity> resentPosition;

        // The position at which the gather command was resent again, or nullptr if it hasn't been resent
        std::shared_ptr<const PositionAndVelocity> secondResentPosition;

        // Used to mark that the worker should have the gather command resent on this specific frame
        int resendCommandOnFrame;

        // Tracks whether the worker has passed the position LF+1 before reaching 10 distance from the patch
        std::shared_ptr<const PositionAndVelocity> passed10DistancePosition;

        // Mode to use for takeover, current allowed values: 0=use normal approach optimization, 1=use takeover optimization, 2=at patch
        int takeoverMode;

        WorkerGatherStatus(MyWorker worker, MyUnit depot, Resource resource)
                : worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , lastProcessedFrame(-2)
                , resendsPlanned(false)
                , resendCommandOnFrame(-2)
                , takeoverMode(0)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            positionHistory.clear();
            resendsPlanned = false;
            plannedResendPosition = nullptr;
            plannedSecondResendPosition = nullptr;
            resentPosition = nullptr;
            secondResentPosition = nullptr;
            resendCommandOnFrame = -2;
            passed10DistancePosition = nullptr;
            takeoverMode = 0;
        }

        [[nodiscard]] bool resentOnSchedule() const
        {
            return resendCommandOnFrame != -2;
        }

        [[nodiscard]] bool pathStartsAtDepot() const
        {
            if (positionHistory.empty()) return false;

            return Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe, (*positionHistory.begin())->pos(), depot->type, depot->lastPosition) == 0;
        }

        std::shared_ptr<PositionAndVelocity> appendCurrentPosition()
        {
            // If the path starts at the depot, include hashes of the previous positions
            // This helps us detect when the worker takes a slightly different path
            // We can't use it when the path starts elsewhere though, as the worker could have been anywhere and we will not get enough data
            auto currentPosition = std::make_shared<PositionAndVelocity>(
                    worker,
                    (!positionHistory.empty() && pathStartsAtDepot()) ? positionHistory.rbegin()->get() : nullptr);
            positionHistory.emplace_back(currentPosition);
            lastProcessedFrame = currentFrame;

            return currentPosition;
        }
    };
}
