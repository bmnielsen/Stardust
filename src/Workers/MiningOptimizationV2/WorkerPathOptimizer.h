#pragma once

#include "MyWorker.h"
#include "Resource.h"
#include "DataModel/MapData.h"

#define VERBOSE_MINING_LOGGING true

namespace MiningOptimization
{
    template <typename ObservationType>
    class WorkerPathOptimizer
    {
    public:
        // The last frame this worker was optimized
        int lastProcessedFrame;

        // The path being followed, if there is one
        // As we store the paths in serialized form to save on memory, we keep the deserialized path here while we are using it.
        std::unique_ptr<Path<ObservationType>> pathBeingFollowed;

        // The expected path nodes the worker will visit
        // For paths with resends this includes the path up until the node where the last resend takes effect
        // For paths without resends this includes the path up to arrival
        std::deque<PathNode<ObservationType>*> expectedPath;

        // Frames on which we plan to issue a resend
        std::set<int> plannedResendFrames;

        // Frames on which we have issued a resend
        std::set<int> executedResendFrames;

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
        {}

        void reset()
        {
            lastProcessedFrame = -2;
            pathBeingFollowed.reset();
            expectedPath.clear();
            plannedResendFrames.clear();
            executedResendFrames.clear();
            expectedArrivalFrameAndOccurrenceRate.clear();
            expectedPatchLockFrame = -1;
            actualPatchLockFrame = -1;
            expectedMiningStartFrame = -1;
        }

        bool matches(const MyUnit &_depot, const Resource &_resource)
        {
            return (depot == _depot) && (resource == _resource);
        }

        // Runs the optimization
        // Called from Workers each frame during approach to the patch or depot
        void optimize();

    private:
        const std::unordered_map<PositionAndVelocity, SerializedPath<ObservationType>> &pathData;
        const std::vector<std::pair<int8_t, int8_t>> &positionDeltas;
        const unsigned int minimumNextPathLength;

        MyWorker worker;
        MyUnit depot;
        Resource resource;

        /*
         * These methods are where the logic differs between gather and return paths. They are implemented in their own files for each
         * template specialization.
         */

        // Called at the start of the optimize method. Should return true if path optimization should be skipped, for example if the worker
        // has already completed its pathing.
        bool skipPathOptimization();

        bool issueResend();
    };
}
