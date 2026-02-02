#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "DataModel/MapData.h"
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
            StartedAtPreviousPathEnd        = 1 << 2,
            StartedAtInitialSpawnPosition   = 1 << 3,
            GatherTakeover                  = 1 << 4,
            SwitchedPatch                   = 1 << 5,
        };

        // The last frame this worker was optimized
        int lastProcessedFrame;

        // The actual frame after patch lock has occurred, or -1 if the worker hasn't patch locked
        int actualPatchLockFrame;

        WorkerPathOptimizer(const MapData &mapData,
                            const std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>> &pathData,
                            MyWorker worker,
                            MyUnit depot,
                            Resource resource)
                : lastProcessedFrame(-2)
                , actualPatchLockFrame(-1)
                , mapData(mapData)
                , pathData(pathData)
                , worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , statusFlags(0)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            actualPatchLockFrame = -1;
            statusFlags = 0;
            executedResendFrames.clear();
#if IS_OPENBW
            previousPathStartPosition = std::move(startPosition);
            startPosition.reset();
            positions.clear();
#endif
            pathBeingFollowed.reset();
            expectedPath.reset();
            takeoverFrames.clear();
        }

        bool matches(const MyUnit &_depot, const Resource &_resource)
        {
            return (depot == _depot) && (resource == _resource);
        }

        // Runs the optimization
        // Called from Workers each frame during approach to the patch or depot
        void optimize();

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

    private:
        /* References to the map mining optimization data relevant for this worker */

        const MapData &mapData;
        const std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>> &pathData;

        MyWorker worker;
        MyUnit depot;
        Resource resource;

        // Status flags
        unsigned int statusFlags;

        // Frames on which we have issued a resend
        std::set<int> executedResendFrames;

#if IS_OPENBW
        // The start position of the current path
        std::unique_ptr<BWAPI::ExactPosition> startPosition;

        // The start position of the previous path
        std::unique_ptr<BWAPI::ExactPosition> previousPathStartPosition;

        std::vector<BWAPI::ExactPosition> positions;
#endif

        // The path being followed, if there is one
        // As we store the paths in serialized form to save on memory, we keep the deserialized path here while we are using it.
        std::unique_ptr<Path<ObservationType>> pathBeingFollowed;

        // The expected path tree the worker will visit, returned from the solver
        std::unique_ptr<SolverResult<ObservationType>> expectedPath;

        // The potential takeover frames from another worker, with the probability of the patch being free at each frame
        // Empty if there is not another worker assigned to the patch
        std::map<int, double> takeoverFrames;

        void resetPath()
        {
            pathBeingFollowed.reset();
            expectedPath.reset();
        }

        // Captures and follows a path, if possible
        void updatePath();

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

        // Initializes gather takeover from another worker, if applicable.
        void initializeGatherTakeover();

        // Resends the relevant command (gather or return), returning whether it succeeded
        // If the command didn't succeed, BWAPI's getLastError will reveal the reason for this
        bool issueResend();
    };
}
