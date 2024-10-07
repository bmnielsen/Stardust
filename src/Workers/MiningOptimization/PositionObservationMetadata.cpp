#include "PositionObservationMetadata.h"

namespace WorkerMiningOptimization
{
    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos
           << " (d=" << optimalGatherPositionMetadata.deltaToNormalPathOptimalPosition
           << " try=" << optimalGatherPositionMetadata.hasPositionToTry
           << ")";

        return os;
    }
}
