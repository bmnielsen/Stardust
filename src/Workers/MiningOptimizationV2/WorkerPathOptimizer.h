#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "DataModel/MapData.h"
#include "DataModel/DeserializedPathCache.h"
#include "Solver/Solver.h"

#include "DebugFlag_MiningOptimization.h"

#if OUTPUT_STATISTICS
#include "PathStatistics.h"
#endif

namespace MiningOptimization
{
    template <typename ObservationType>
    class WorkerPathOptimizer
    {
    public:
        enum class StatusFlags : unsigned int
        {
            CapturedPath                    = 1 << 0,
            LostPath                        = 1 << 1,
            LostPathWithAssumedResult       = 1 << 2,
            StartedAtPreviousPathEnd        = 1 << 3,
            StartedAtInitialSpawnPosition   = 1 << 4,
            ReachedTenDistance              = 1 << 5,
            IssuedTakeoverResend            = 1 << 6,
            SwitchedPatch                   = 1 << 7,
        };

        MyWorker worker;
        MyUnit depot;
        Resource resource;

        // The actual frame after patch lock has occurred, or -1 if the worker hasn't patch locked
        int actualPatchLockFrame;

        WorkerPathOptimizer(const MapData &mapData,
                            PATH_CACHE_IMPLEMENTATION<ObservationType> &pathCache,
                            MyWorker worker,
                            MyUnit depot,
                            Resource resource)
                : worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , actualPatchLockFrame(-1)
                , mapData(mapData)
                , pathCache(pathCache)
                , patchTile(TilePosition::fromBWAPI(this->resource->tile))
                , lastProcessedFrame(-2)
                , statusFlags(0)
                , expectedTakeoverFrame(-1)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            actualPatchLockFrame = -1;
            statusFlags = 0;
            executedResendFrames.clear();
#if IS_OPENBW
            startPosition.reset();
#endif
            resetPath();
            expectedTakeoverFrame = -1;
            takeoverResendFrames.clear();
        }

        bool matches(const MyUnit &_depot, const Resource &_resource)
        {
            return (depot == _depot) && (resource == _resource);
        }

        // Updates the path being followed by the worker
        // Returns true if the worker is on a valid path, false otherwise
        bool updatePath();

        // Issues any orders needed to do path optimization
        void issueOrders();

        // Sets a status flag
        void setFlag(const StatusFlags flag)
        {
            statusFlags |= to_underlying(flag);
        }

        // Unsets a status flag
        void unsetFlag(const StatusFlags flag)
        {
            statusFlags &= ~to_underlying(flag);
        }

        // Checks if a status flag is set
        [[nodiscard]] bool hasFlag(const StatusFlags flag) const
        {
            return (statusFlags & to_underlying(flag)) != 0;
        }

#if OUTPUT_STATISTICS
        void updateStatistics(PathStatistics &pathStatistics);
#endif

        /*
         * These methods are where the logic differs between gather and return paths. They are implemented in their own files for each
         * template specialization.
         */

        // Gets the set of takeover action frames that can be achieved with high probability (assuming that we don't lose our path)
        std::set<int> takeoverActionFrames(int latestTakeoverFrame);

        // Tells the optimizer to plan for the given takeover frame
        void useTakeoverFrame(int takeoverFrame);

    private:
        /* References to the map mining optimization data relevant for this worker */

        const MapData &mapData;
        PATH_CACHE_IMPLEMENTATION<ObservationType> &pathCache;

        TilePosition patchTile;

        // The last frame this worker was optimized
        int lastProcessedFrame;

        // Status flags
        unsigned int statusFlags;

        // Frames on which we have issued a resend
        std::set<int> executedResendFrames;

#if IS_OPENBW
        // The start position of the current path
        std::unique_ptr<BWAPI::ExactPosition> startPosition;
#endif

        // The expected path tree the worker will visit, returned from the solver
        std::unique_ptr<SolverResult<ObservationType>> expectedPath;

        // The expected takeover frame we are currently planning to reach
        int expectedTakeoverFrame;

        // The resends needed to achieve the current expected takeover frame
        std::set<int> takeoverResendFrames;

        void resetPath()
        {
            expectedPath.reset();
        }

        /*
         * These methods are where the logic differs between gather and return paths. They are implemented in their own files for each
         * template specialization.
         */

        // Returns whether the optimization is complete, i.e. a gathering worker is transitioning to mining or a returning worker has delivered
        // its cargo.
        bool isComplete();

        // Sets the status flags related to the path start position
        void setStartOfPathFlags();

        // Called at the start of the optimize method. Should return true if path optimization should be skipped, for example if the worker
        // has already completed its pathing.
        bool skipPathOptimization();

        // Resends the relevant command (gather or return), returning whether it succeeded
        // If the command didn't succeed, BWAPI's getLastError will reveal the reason for this
        bool issueResend();
    };
}
