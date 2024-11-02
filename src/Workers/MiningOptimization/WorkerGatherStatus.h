#pragma once

#include "DebugFlag_WorkerMiningOptimization.h"

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

        // The expected path the worker will follow
        std::deque<PositionAndVelocity> expectedPath;

        // Positions at which the gather command was resent
        std::vector<std::shared_ptr<const PositionAndVelocity>> resentPositions;

        // Frames at which we have send gather commands
        std::set<int> resentFrames;

        // Used to mark that the worker should have the gather command resent on this specific frame
        int resendCommandOnFrame;

        // Mode to use for takeover, current allowed values: 0=use normal approach optimization, 1=use takeover optimization, 2=at patch
        int takeoverState;

        // The frame where we want to take over mining from another worker
        int takeoverFrame;

        // Tracks the frame when the worker passed a position LF+1 before reaching 10 distance from the patch
        int passed10DistancePosition;

        // Whether the worker switched patches while trying to mine
        bool switchedPatches;

        WorkerGatherStatus(MyWorker worker, MyUnit depot, Resource resource)
                : worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , lastProcessedFrame(-2)
                , resendsPlanned(false)
                , resendCommandOnFrame(-2)
                , takeoverState(0)
                , takeoverFrame(-1)
                , passed10DistancePosition(-1)
                , switchedPatches(false)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            positionHistory.clear();
            resendsPlanned = false;
            plannedResendPosition = nullptr;
            plannedSecondResendPosition = nullptr;
            expectedPath.clear();
            resentPositions.clear();
            resentFrames.clear();
            resendCommandOnFrame = -2;
            takeoverState = 0;
            takeoverFrame = -1;
            passed10DistancePosition = -1;
            switchedPatches = false;
        }

        [[nodiscard]] bool resentOnSchedule() const
        {
            return resendCommandOnFrame != -2;
        }

        [[nodiscard]] bool pathStartsAtDepot() const
        {
            if (positionHistory.empty()) return false;
            if (positionHistory.size() > 60) return false; // usually means distance mining

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

        void sendGatherCommand(BWAPI::Unit resourceBwapiUnit, const std::shared_ptr<PositionAndVelocity> &currentPosition)
        {
            if (!worker->gather(resourceBwapiUnit))
            {
#if OPTIMALPOSITIONS_DEBUG
                    Log::Get() << "Failed to send gather command for " << worker->id << " @ " << worker->getTilePosition() << ": "
                               << BWAPI::Broodwar->getLastError();
                    CherryVis::log(worker->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
                    CherryVis::log(resource->id) << "Failed to send gather command; last error " << BWAPI::Broodwar->getLastError();
#endif
                return;
            }

            resentPositions.push_back(currentPosition);
            resentFrames.insert(currentFrame);
        }

        const PositionAndVelocity *resentPosition() const
        {
            if (resentPositions.empty()) return nullptr;
            return resentPositions.begin()->get();
        }
    };
}
