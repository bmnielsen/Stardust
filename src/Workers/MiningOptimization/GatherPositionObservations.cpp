#include "GatherPositionObservations.h"

#include "WorkerMiningOptimization.h"

namespace WorkerMiningOptimization
{
    bool GatherResendArrivalObservations::addArrival(int arrivalDelta)
    {
        if (arrivalDelta > INT8_MAX || arrivalDelta < INT8_MIN)
        {
            Log::Get() << "ERROR: Arrival delta " << arrivalDelta << " out of bounds";
            return false;
        }

        bool result = empty();

        auto addArrival = [&]()
        {
            if (atOccurrenceCap(arrivalDelayAndOccurrences)) return;
            arrivalDelayAndOccurrences[(int8_t)arrivalDelta]++;
        };

        // If there is only one observation before this one, replace it
        // This is to handle the situation where we have made a provisional observation in two-worker mining where we know
        // the worker didn't arrive on time, but not what its exact arrival would have been without resending a command
        if (arrivalDelayAndOccurrences.size() == 1 && arrivalDelayAndOccurrences.begin()->second == 1)
        {
            arrivalDelayAndOccurrences.clear();
            addArrival();
        }
        addArrival();
        arrivalDelayAndOccurrenceRate = computeOccurrenceRateMap(arrivalDelayAndOccurrences);
        return result;
    }

    int8_t GatherResendArrivalObservations::mostCommonArrivalDelay() const
    {
        if (arrivalDelayAndOccurrenceRate.empty()) return INT8_MAX;
        if (arrivalDelayAndOccurrenceRate.size() == 1) return arrivalDelayAndOccurrenceRate.begin()->first;

        int8_t best = -1;
        uint8_t bestRate = 0;
        for (const auto &[arrivalDelay, occurrenceRate] : arrivalDelayAndOccurrenceRate)
        {
            if (occurrenceRate > bestRate)
            {
                best = arrivalDelay;
                bestRate = occurrenceRate;
            }
        }

        return best;
    }

    double GatherResendArrivalObservations::expectedMiningDelay(int commandFrame) const
    {
        if (arrivalDelayAndOccurrenceRate.empty()) return 100.0;

        if (arrivalDelayAndOccurrenceRate.size() == 1) return arrivalDelayToMiningDelay(arrivalDelayAndOccurrenceRate.begin()->first, commandFrame);

        // If the most common arrival delay is positive, return it
        auto mostCommon = mostCommonArrivalDelay();
        if (mostCommon > 0) return arrivalDelayToMiningDelay(mostCommon, commandFrame);

        double totalMiningDelay = 0.0;
        for (const auto &[arrivalDelay, occurrenceRate] : arrivalDelayAndOccurrenceRate)
        {
            totalMiningDelay += (arrivalDelayToMiningDelay(arrivalDelay, commandFrame) * ((double)occurrenceRate / 255.0));
        }

        return totalMiningDelay;
    }

    double GatherResendArrivalObservations::arrivalDelayToMiningDelay(int arrivalDelay, int commandFrame)
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
    }

    double GatherPositionObservations::averageDeltaToBenchmark() const
    {
        if (deltaToBenchmarkAndOccurrenceRate.empty()) return 100;
        if (deltaToBenchmarkAndOccurrenceRate.size() == 1) return deltaToBenchmarkAndOccurrenceRate.begin()->first;

        double accumulator = 0.0;
        for (const auto &[delta, rate] : deltaToBenchmarkAndOccurrenceRate)
        {
            accumulator += (double)delta * ((double)rate / 255.0);
        }

        return accumulator;
    }

    int GatherPositionObservations::probableDeltaToBenchmark() const
    {
        if (deltaToBenchmarkAndOccurrenceRate.empty()) return 100;
        if (deltaToBenchmarkAndOccurrenceRate.size() == 1) return deltaToBenchmarkAndOccurrenceRate.begin()->first;

        int8_t best = 100;
        uint8_t bestRate = 0;
        for (const auto &[delta, rate] : deltaToBenchmarkAndOccurrenceRate)
        {
            if (rate > bestRate)
            {
                best = delta;
                bestRate = rate;
            }
        }

        return best;
    }

    int GatherPositionObservations::largestDeltaToBenchmark() const
    {
        if (deltaToBenchmarkAndOccurrenceRate.empty()) return 100;
        if (deltaToBenchmarkAndOccurrenceRate.size() == 1) return deltaToBenchmarkAndOccurrenceRate.begin()->first;

        int8_t best = INT8_MIN;
        for (const auto &[delta, _] : deltaToBenchmarkAndOccurrenceRate)
        {
            if (delta > best)
            {
                best = delta;
            }
        }

        return best;
    }

    bool GatherPositionObservations::usableForPathPlanning() const
    {
        auto exceedsThreshold = []<typename T>(
                const std::unordered_map<int8_t, T> &map,
                int stableLowerThreshold,
                int stableUpperThreshold,
                int unstableLowerThreshold,
                int unstableUpperThreshold)
        {
            int lowerThreshold, upperThreshold;
            if (map.size() == 1)
            {
                lowerThreshold = stableLowerThreshold;
                upperThreshold = stableUpperThreshold;
            }
            else
            {
                lowerThreshold = unstableLowerThreshold;
                upperThreshold = unstableUpperThreshold;
            }

            for (const auto &[val, _] : map)
            {
                if ((int)val < lowerThreshold) return true;
                if ((int)val > upperThreshold) return true;
            }

            return false;
        };

        // If we haven't observed the "normal" path, try to use resend arrivals
        if (deltaToBenchmarkAndOccurrenceRate.empty())
        {
            // If there are no resend arrivals, we allow the position to be used
            if (noSecondResendArrivalObservations.arrivalDelayAndOccurrenceRate.empty())
            {
                return true;
            }

            return !exceedsThreshold(noSecondResendArrivalObservations.arrivalDelayAndOccurrenceRate,
                                     0,
                                     BWAPI::Broodwar->getLatencyFrames() + 11 + GATHER_EXPLORE_BEFORE,
                                     0,
                                     BWAPI::Broodwar->getLatencyFrames() + 11 + 5);
        }

        return !exceedsThreshold(deltaToBenchmarkAndOccurrenceRate,
                                 -GATHER_EXPLORE_BEFORE,
                                 INT_MAX,
                                 -5,
                                 INT_MAX);
    }

    std::ostream &operator<<(std::ostream &os, const GatherPositionObservationPtr &ptr)
    {
        if (ptr.pos)
        {
            os << ptr.pos->pos;
        }
        else
        {
            os << ptr.secondResendPos->pos;
        }
        return os;
    }

    std::ostream &operator<<(std::ostream &os, const GatherPositionObservations &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos
           << " (d=" << optimalGatherPositionMetadata.probableDeltaToBenchmark() << ")";

        return os;
    }
}
