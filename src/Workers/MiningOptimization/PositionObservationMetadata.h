#pragma once

#include "PositionAndVelocity.h"
#include <map>

namespace WorkerMiningOptimization
{
    // This is the structure we use to track observed positions and our track record using them
    struct PositionObservationMetadata
    {
    public:
        PositionAndVelocity pos;
        int deltaToNormalPathOptimalPosition;
        int bestDelta = 100;
        int bestFollowingPositionDelta = 100;
        bool followingHasUntriedPosition = false;

        std::map<int, int> noResendData;
        std::map<PositionAndVelocity, std::map<int, int>> resendPositionToData;

        void addObservation(const std::shared_ptr<PositionAndVelocity> &secondResendPosition, int delta)
        {
            if (!secondResendPosition)
            {
                noResendData[delta]++;
            }
            else
            {
                resendPositionToData[*secondResendPosition][delta]++;
            }

            // Only track best deltas that get the worker to the patch on time
            if (delta >= 0 && delta < bestDelta) bestDelta = delta;
        }

        [[nodiscard]] bool hasUntriedPosition() const
        {
            if (noResendData.empty()) return true;

            for (const auto &[_, observations] : resendPositionToData)
            {
                if (observations.empty()) return true;
            }

            return false;
        }
    };

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata);
}
