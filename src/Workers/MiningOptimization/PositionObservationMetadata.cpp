#include "PositionObservationMetadata.h"

namespace WorkerMiningOptimization
{
    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos
           << " (d=" << optimalGatherPositionMetadata.deltaToNormalPathOptimalPosition
           << " b=" << optimalGatherPositionMetadata.bestDelta
           << " fb=" << optimalGatherPositionMetadata.bestFollowingPositionDelta
           << " fut=" << optimalGatherPositionMetadata.followingHasUntriedPosition
           << ")";

        return os;
    }
}
