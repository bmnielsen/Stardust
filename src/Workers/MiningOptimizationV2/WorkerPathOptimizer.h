#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "DataModel/MapData.h"

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
            StartedAtInitialSpawnPosition   = 1 << 3
        };

        // The last frame this worker was optimized
        int lastProcessedFrame;

        // The expected frames the worker could arrive at the target with their occurrence rates
        std::vector<std::pair<int, int>> expectedArrivalFrameAndOccurrenceRate;

        // The frame at which the worker is expected to lock to the patch
        // If the worker is not expected to lock to the patch, this will be -1
        // Only applicable when taking over from another worker
        int expectedPatchLockFrame;

        // Same as the above, but the actual frame after patch lock has occurred
        int actualPatchLockFrame;

        // The frame this worker is expected to start mining
        // Not relevant if the worker has an expected patch lock frame, since the mining start frame then depends on the
        // worker being taken over from
        // If the expected mining start frame is unknown, this will be -1
        // Only applicable when taking over from another worker
        int expectedMiningStartFrame;

        WorkerPathOptimizer(const std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>> &pathData,
                            const std::vector<std::pair<int8_t, int8_t>> &positionDeltas,
                            const unsigned int minimumNextPathLength,
                            MyWorker worker,
                            MyUnit depot,
                            Resource resource)
                : lastProcessedFrame(-2)
                , expectedPatchLockFrame(-1)
                , actualPatchLockFrame(-1)
                , expectedMiningStartFrame(-1)
                , pathData(pathData)
                , positionDeltas(positionDeltas)
                , minimumNextPathLength(minimumNextPathLength)
                , worker(std::move(worker))
                , depot(std::move(depot))
                , resource(std::move(resource))
                , statusFlags(0)
                , pathPlanned(false)
                , previousNodeNextPositions(nullptr)
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            expectedArrivalFrameAndOccurrenceRate.clear();
            expectedPatchLockFrame = -1;
            actualPatchLockFrame = -1;
            expectedMiningStartFrame = -1;
            statusFlags = 0;
            pathPlanned = false;
            plannedResendFrames.clear();
            executedResendFrames.clear();
            startPosition.reset();
            pathBeingFollowed.reset();
            expectedPath.clear();
            previousPosition.reset();
            previousNodeNextPositions = nullptr;
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

        const std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>> &pathData;
        const std::vector<std::pair<int8_t, int8_t>> &positionDeltas;
        const unsigned int minimumNextPathLength;

        MyWorker worker;
        MyUnit depot;
        Resource resource;

        // Status flags
        unsigned int statusFlags;

        // Whether we have planned a path
        bool pathPlanned;

        // Frames on which we plan to issue a resend
        std::set<int> plannedResendFrames;

        // Frames on which we have issued a resend
        std::set<int> executedResendFrames;

#if IS_OPENBW
        // The start position of the current path
        std::unique_ptr<BWAPI::ExactPosition> startPosition;
#endif

        // The path being followed, if there is one
        // As we store the paths in serialized form to save on memory, we keep the deserialized path here while we are using it.
        std::unique_ptr<Path<ObservationType>> pathBeingFollowed;

        // The expected path nodes the worker will visit
        // For paths with resends this includes the path up until the node where the last resend takes effect
        // For paths without resends this includes the path up to arrival
        std::deque<PathNode<ObservationType>*> expectedPath;

        // The worker's position on the previous frame
        std::unique_ptr<PositionAndVelocity> previousPosition;

        // The next positions from the previous path node in the path being followed
        // Used if we lose the expected path and need to back up to see if we are following a different known branch
        std::vector<std::pair<PathNode<ObservationType>, uint8_t>>* previousNodeNextPositions;

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
