#pragma once

#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include <map>

namespace WorkerMiningOptimization
{
    struct ResendPositionObservations
    {
        std::map<int, int> arrivalDelayAndOccurrences;
        int collisions = 0;
        int nonCollisions = 0;

        void add(int arrivalDelay)
        {
            arrivalDelayAndOccurrences[arrivalDelay]++;
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrences.empty();
        }

        [[nodiscard]] int mostCommonArrivalDelay() const
        {
            if (arrivalDelayAndOccurrences.empty()) return INT_MAX;
            if (arrivalDelayAndOccurrences.size() == 1) return arrivalDelayAndOccurrences.begin()->first;

            int best = -1;
            int bestCount = 0;
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

        [[nodiscard]] double expectedMiningDelay(bool otherWorkerAssigned, int commandFrame) const
        {
            if (arrivalDelayAndOccurrences.empty()) return 100.0;

            auto arrivalDelayToMiningDelay = [&](int arrivalDelay)
            {
                // If the worker doesn't arrive at the patch on time, and another worker is assigned to the patch, it is likely that
                // our worker will try to switch patches when its order kicks in, resulting in a full command cycle of extra delay
                if (arrivalDelay > 0 && otherWorkerAssigned)
                {
                    return (double)(arrivalDelay + 11 + BWAPI::Broodwar->getLatencyFrames());
                }

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
            int totalOccurrences = 0;
            for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
            {
                totalMiningDelay += arrivalDelayToMiningDelay(arrivalDelay);
                totalOccurrences += occurrences;
            }

            return totalMiningDelay / (double)totalOccurrences;
        }
    };

    struct SecondResendPositionObservationMetadata
    {
        PositionAndVelocity pos;
        std::unordered_map<PositionAndVelocity, int> next;
        int deltaToFirstResend = 0;
        ResendPositionObservations observations;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct PositionObservationMetadata
    {
    public:
        uint32_t pathHash;
        PositionAndVelocity pos;
        std::unordered_map<PositionAndVelocity, int> next;

        int deltaToNormalPathOptimalPosition;

        ResendPositionObservations noSecondResendObservations;
        std::unordered_map<PositionAndVelocity, SecondResendPositionObservationMetadata> secondResendMetadata;

        int noResendCollisions = 0;
        int noResendNonCollisions = 0;

        bool addObservation(const std::shared_ptr<const PositionAndVelocity> &secondResendPosition, int arrivalDelta)
        {
            auto &observations = secondResendPosition ? secondResendMetadata[*secondResendPosition].observations : noSecondResendObservations;
            bool result = observations.empty();
            observations.add(-arrivalDelta);
            return result;
        }

        [[nodiscard]] std::vector<const SecondResendPositionObservationMetadata*> expectedPathAfterResend() const
        {
            std::vector<const SecondResendPositionObservationMetadata*> result;

            std::map<int, std::vector<const SecondResendPositionObservationMetadata*>> secondResendByDelta;
            for (auto &[secondResendPos, secondResendData] : secondResendMetadata)
            {
                secondResendByDelta[secondResendData.deltaToFirstResend].push_back(&secondResendData);
            }

            const std::unordered_map<PositionAndVelocity, int> *nextOccurrences = &next;
            int delta = 1;
            while (true)
            {
                int bestOccurrences = 0;
                const SecondResendPositionObservationMetadata* bestMetadata = nullptr;
                for (const auto &secondResendData : secondResendByDelta[delta])
                {
                    auto occurrencesIt = nextOccurrences->find(secondResendData->pos);
                    if (occurrencesIt != nextOccurrences->end() && occurrencesIt->second > bestOccurrences)
                    {
                        bestOccurrences = occurrencesIt->second;
                        bestMetadata = secondResendData;
                    }
                }

                if (!bestMetadata) break;

                result.push_back(bestMetadata);
                delta++;
                nextOccurrences = &bestMetadata->next;
            }

            return result;
        }

        [[nodiscard]] SecondResendPositionObservationMetadata* secondResendMetadataFor(const PositionAndVelocity *secondResendPosition)
        {
            if (!secondResendPosition) return nullptr;

            auto secondResendDataIt = secondResendMetadata.find(*secondResendPosition);
            return (secondResendDataIt == secondResendMetadata.end()) ? nullptr : &secondResendDataIt->second;
        }
    };

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata);
}
