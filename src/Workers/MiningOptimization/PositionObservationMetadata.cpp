#include "PositionObservationMetadata.h"

namespace WorkerMiningOptimization
{
    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos
           << " (o=" << optimalGatherPositionMetadata.observations
           << " s=" << optimalGatherPositionMetadata.successes
           << " f=" << optimalGatherPositionMetadata.failures
           << ")";

        return os;
    }
}