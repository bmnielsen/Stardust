#include "ResourceObservations.h"

#include "Common.h"

namespace WorkerMiningOptimization
{
    void ResourceObservation::addObservation(uint64_t value)
    {
        if (value > (UINT64_MAX - accumulator))
        {
            Log::Get() << "ERROR: Adding observation would overflow accumulator";
            return;
        }
        if (observationCount == UINT32_MAX) return;

        observationCount++;
        accumulator += value;

        auto avg = (uint64_t)std::round((double)accumulator / (double)observationCount);
        if (avg > UINT16_MAX)
        {
            Log::Get() << "ERROR: Average of " << avg << " would overflow 16-bit unsigned int";
            return;
        }
        average = (uint16_t)avg;
    }
}
