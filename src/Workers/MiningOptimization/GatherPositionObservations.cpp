#include "GatherPositionObservations.h"

namespace WorkerMiningOptimization
{
    int8_t GatherResendArrivalObservations::mostCommonArrivalDelay() const
    {
        if (arrivalDelayAndOccurrences.empty()) return INT8_MAX;
        if (arrivalDelayAndOccurrences.size() == 1) return arrivalDelayAndOccurrences.begin()->first;

        int8_t best = -1;
        uint16_t bestCount = 0;
        for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
        {
            if (occurrences > bestCount)
            {
                best = arrivalDelay;
                bestCount = occurrences;
            }
        }

        return best;
    }

    double GatherResendArrivalObservations::expectedMiningDelay(int commandFrame) const
    {
        if (arrivalDelayAndOccurrences.empty()) return 100.0;

        auto arrivalDelayToMiningDelay = [&](int arrivalDelay)
        {
            // Compute the delay between the gather command kicking in and mining starting
            // If the worker arrives at the patch on time, the delay is 0
            // If not, the delay will correspond to how long it takes the worker's order process timer to reach 0 again
            int miningDelay;
            if (arrivalDelay <= 0)
            {
                miningDelay = 0;
            }
            else
            {
                miningDelay = arrivalDelay;
                if (miningDelay % 9 != 0) miningDelay += (9 - miningDelay % 9);
            }

            // Check for order process timer resets that will affect start of mining
            int framesToNextReset = OrderProcessTimer::framesToNextReset(commandFrame + BWAPI::Broodwar->getLatencyFrames() + 1);
            if (framesToNextReset < (11 + arrivalDelay))
            {
                // A reset will happen before the worker arrives at the patch
                // On average we will need to wait 4 frames after arrival before mining
                return arrivalDelay + 4.0;
            }
            if (framesToNextReset < (11 + miningDelay))
            {
                // A reset will happen after the worker arrives at the patch, but before it can start mining
                // On average we will need to wait 3.5 frames after the reset
                return framesToNextReset - 11 + 3.5;
            }

            // No reset, return the computed mining delay
            return (double)miningDelay;
        };

        if (arrivalDelayAndOccurrences.size() == 1) return arrivalDelayToMiningDelay(arrivalDelayAndOccurrences.begin()->first);

        // If the most common arrival delay is positive, return it
        auto mostCommon = mostCommonArrivalDelay();
        if (mostCommon > 0) return arrivalDelayToMiningDelay(mostCommon);

        double totalMiningDelay = 0.0;
        uint32_t totalOccurrences = 0;
        for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
        {
            totalMiningDelay += (arrivalDelayToMiningDelay(arrivalDelay) * occurrences);
            totalOccurrences += occurrences;
        }

        return totalMiningDelay / (double)totalOccurrences;
    }

    double GatherPositionObservations::averageDeltaToBenchmark() const
    {
        if (deltaToBenchmarkAndOccurrences.empty()) return 100;
        if (deltaToBenchmarkAndOccurrences.size() == 1) return deltaToBenchmarkAndOccurrences.begin()->first;

        int accumulator = 0;
        uint16_t total = 0;
        for (const auto &[delta, occurrences] : deltaToBenchmarkAndOccurrences)
        {
            accumulator += (int)delta * (int)occurrences;
            total += occurrences;
        }

        if (total == 0) return 100;

        return (double)accumulator / (double)total;
    }

    int GatherPositionObservations::probableDeltaToBenchmark() const
    {
        if (deltaToBenchmarkAndOccurrences.empty()) return 100;
        if (deltaToBenchmarkAndOccurrences.size() == 1) return deltaToBenchmarkAndOccurrences.begin()->first;

        int8_t best = 100;
        uint16_t bestOccurrences = 0;
        for (const auto &[delta, occurrences] : deltaToBenchmarkAndOccurrences)
        {
            if (occurrences > bestOccurrences)
            {
                best = delta;
                bestOccurrences = occurrences;
            }
        }

        return best;
    }

    int GatherPositionObservations::largestDeltaToBenchmark() const
    {
        if (deltaToBenchmarkAndOccurrences.empty()) return 100;
        if (deltaToBenchmarkAndOccurrences.size() == 1) return deltaToBenchmarkAndOccurrences.begin()->first;

        int8_t best = INT8_MIN;
        for (const auto &[delta, occurrences] : deltaToBenchmarkAndOccurrences)
        {
            if (delta > best)
            {
                best = delta;
            }
        }

        return best;
    }

    bool GatherPositionObservations::addArrivalObservation(SecondResendGatherPositionObservations *secondResendPositionData, int arrivalDelta)
    {
        if (arrivalDelta > INT8_MAX || arrivalDelta < INT8_MIN)
        {
            Log::Get() << "ERROR: Arrival delta " << arrivalDelta << " out of bounds";
            return false;
        }

        auto &observations = secondResendPositionData ? secondResendPositionData->arrivalObservations : noSecondResendArrivalObservations;
        bool result = observations.empty();
        observations.addArrival((int8_t)arrivalDelta);
        return result;
    }

    std::ostream &operator<<(std::ostream &os, const GatherPositionObservations &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos
           << " (d=" << optimalGatherPositionMetadata.probableDeltaToBenchmark() << ")";

        return os;
    }
}
