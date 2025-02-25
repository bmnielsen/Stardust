#include "OccurrencesAndCollisions.h"

namespace WorkerMiningOptimization
{
    uint8_t computeCollisionRate(uint32_t collisions, uint32_t nonCollisions)
    {
        return (uint8_t)(std::round(255.0 * ((double)collisions/(double)(collisions + nonCollisions))));
    }
}
