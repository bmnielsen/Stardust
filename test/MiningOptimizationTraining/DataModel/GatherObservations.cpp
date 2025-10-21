
#include "GatherObservations.h"

namespace MiningOptimizationTraining
{
    std::ostream &operator<<(std::ostream &os, const GatherObservations &gatherObservations)
    {
        os << gatherObservations.pos;
        return os;
    }
}
