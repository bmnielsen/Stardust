#include "PositionObservationMetadata.h"

namespace WorkerMiningOptimization
{
    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos
           << " (d=" << optimalGatherPositionMetadata.deltaToNormalPathOptimalPosition
           << " pb=" << optimalGatherPositionMetadata.bestPreviousPositionDelta
           << " b=" << optimalGatherPositionMetadata.bestDelta
           << " fb=" << optimalGatherPositionMetadata.bestFollowingPositionDelta
           << " try=" << optimalGatherPositionMetadata.hasPositionToTry
           << " fut=" << optimalGatherPositionMetadata.followingHasPositionToTry
           << ")";

        return os;
    }
}
