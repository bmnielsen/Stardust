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

        if (observationCount >= 10)
        {
            auto v = ((int)value - (int)average) * ((int)value - (int)average);
            if (v > (UINT64_MAX - varianceAccumulator))
            {
                Log::Get() << "ERROR: Adding observation would overflow variance accumulator";
                return;
            }
            varianceAccumulator += v;
        }

        observationCount++;
        accumulator += value;

        auto avg = (uint64_t)std::round((double)accumulator / (double)observationCount);
        if (avg > UINT16_MAX)
        {
            Log::Get() << "ERROR: Average of " << avg << " would overflow 16-bit unsigned int";
            return;
        }
        average = (uint16_t)avg;

        auto varianceAvg = (uint64_t)std::round((double)varianceAccumulator / (double)(observationCount - 10));
        if (varianceAvg > UINT16_MAX)
        {
            Log::Get() << "ERROR: Variance of " << varianceAvg << " would overflow 16-bit unsigned int";
            return;
        }
        variance = (uint16_t)varianceAvg;
    }

    ResourceObservation &ResourceObservations::startingWorkerObservationsFor(int startingWorkerIndex)
    {
        if (startingWorkerObservations.empty())
        {
            startingWorkerObservations.reserve(4);
            for (int i = 0; i < 4; i++)
            {
                startingWorkerObservations.emplace_back();
            }
        }

        return startingWorkerObservations[startingWorkerIndex];
    }
}
