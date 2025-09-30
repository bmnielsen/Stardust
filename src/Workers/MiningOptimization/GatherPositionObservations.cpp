#include "GatherPositionObservations.h"

#include "WorkerMiningOptimization.h"

namespace
{
    // Packs the arrival delay (already verified to be between -64 and 63 inclusive) and the facing patch boolean into one 8-bit value.
    // The highest 7 bits are used to store the arrival delay and the lowest is 1 if not facing patch.
    int8_t packArrivalDelayAndFacingPatch(int arrivalDelay, bool facingPatch)
    {
        int8_t result = (int8_t)arrivalDelay * 2;
        if (!facingPatch)
        {
            result |= 0b00000001;
        }
        return result;
    }
}

namespace WorkerMiningOptimization
{
    bool GatherResendArrivalObservations::addArrival(int arrivalDelta, bool facingPatch)
    {
        if (arrivalDelta > 63 || arrivalDelta < -64)
        {
            Log::Get() << "ERROR: Arrival delta " << arrivalDelta << " out of bounds";
            return false;
        }

        bool result = empty();

        auto addArrival = [&]()
        {
            if (atOccurrenceCap(packedArrivalDelayAndFacingPatchToOccurrences)) return;
            packedArrivalDelayAndFacingPatchToOccurrences[packArrivalDelayAndFacingPatch(arrivalDelta, facingPatch)]++;
        };

        // If there is only one observation before this one, replace it
        // This is to handle the situation where we have made a provisional observation in two-worker mining where we know
        // the worker didn't arrive on time, but not what its exact arrival would have been without resending a command
        if (packedArrivalDelayAndFacingPatchToOccurrences.size() == 1 && packedArrivalDelayAndFacingPatchToOccurrences.begin()->second == 1)
        {
            packedArrivalDelayAndFacingPatchToOccurrences.clear();
            addArrival();
        }
        addArrival();
        packedArrivalDelayAndFacingPatchToOccurrenceRate = computeOccurrenceRateMap(packedArrivalDelayAndFacingPatchToOccurrences);
        return result;
    }

    int8_t GatherResendArrivalObservations::mostCommonPackedArrivalDelayAndFacingPatch() const
    {
        if (packedArrivalDelayAndFacingPatchToOccurrenceRate.empty()) return packArrivalDelayAndFacingPatch(63, false);
        if (packedArrivalDelayAndFacingPatchToOccurrenceRate.size() == 1)
        {
            return packedArrivalDelayAndFacingPatchToOccurrenceRate.begin()->first;
        }

        int8_t best = -1;
        uint8_t bestRate = 0;
        for (const auto &[packedArrivalDelayAndFacingPatch, occurrenceRate]
                : packedArrivalDelayAndFacingPatchToOccurrenceRate)
        {
            if (occurrenceRate > bestRate)
            {
                best = packedArrivalDelayAndFacingPatch;
                bestRate = occurrenceRate;
            }
        }

        return best;
    }

    double GatherResendArrivalObservations::expectedMiningDelay(int commandFrame) const
    {
        if (packedArrivalDelayAndFacingPatchToOccurrenceRate.empty()) return 100.0;

        if (packedArrivalDelayAndFacingPatchToOccurrenceRate.size() == 1)
        {
            return packedArrivalDelayAndFacingPatchToMiningDelay(packedArrivalDelayAndFacingPatchToOccurrenceRate.begin()->first, commandFrame);
        }

        // If the most common arrival delay is positive, return it
        auto mostCommon = mostCommonPackedArrivalDelayAndFacingPatch();
        if (unpackArrivalDelay(mostCommon) > 0)
        {
            return packedArrivalDelayAndFacingPatchToMiningDelay(mostCommon, commandFrame);
        }

        double totalMiningDelay = 0.0;
        for (const auto &[packedArrivalDelayAndFacingPatch, occurrenceRate]
            : packedArrivalDelayAndFacingPatchToOccurrenceRate)
        {
            totalMiningDelay +=
                    (packedArrivalDelayAndFacingPatchToMiningDelay(packedArrivalDelayAndFacingPatch, commandFrame)
                        * ((double)occurrenceRate / 255.0));
        }

        return totalMiningDelay;
    }

    double GatherResendArrivalObservations::packedArrivalDelayAndFacingPatchToMiningDelay(int8_t packedArrivalDelayAndFacingPatch, int commandFrame)
    {
        int arrivalDelay = unpackArrivalDelay(packedArrivalDelayAndFacingPatch);

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
                int unstableUpperThreshold,
                auto *valueConverter)
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
                auto convertedValue = valueConverter ? (*valueConverter)(val) : (int)val;
                if (convertedValue < lowerThreshold) return true;
                if (convertedValue > upperThreshold) return true;
            }

            return false;
        };

        // If we haven't observed the "normal" path, try to use resend arrivals
        if (deltaToBenchmarkAndOccurrenceRate.empty())
        {
            // If there are no resend arrivals, we allow the position to be used
            if (noSecondResendArrivalObservations.packedArrivalDelayAndFacingPatchToOccurrenceRate.empty())
            {
                return true;
            }

            auto packedArrivalExtractor = [](int8_t packedArrivalDelayAndFacingPatch) -> int
            {
                return GatherResendArrivalObservations::unpackArrivalDelay(packedArrivalDelayAndFacingPatch);
            };

            return !exceedsThreshold(noSecondResendArrivalObservations.packedArrivalDelayAndFacingPatchToOccurrenceRate,
                                     0,
                                     BWAPI::Broodwar->getLatencyFrames() + 11 + GATHER_EXPLORE_BEFORE,
                                     0,
                                     BWAPI::Broodwar->getLatencyFrames() + 11 + 5,
                                     &packedArrivalExtractor);
        }

        auto valueConverter = [](int8_t deltaToBenchmark) -> int
        {
            return (int)deltaToBenchmark;
        };

        return !exceedsThreshold(deltaToBenchmarkAndOccurrenceRate,
                                 -GATHER_EXPLORE_BEFORE,
                                 INT_MAX,
                                 -5,
                                 INT_MAX,
                                 &valueConverter);
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
