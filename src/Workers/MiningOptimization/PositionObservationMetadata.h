#pragma once

#include "PositionAndVelocity.h"
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

        [[nodiscard]] bool hasArrivalDelay(int arrivalDelay) const
        {
            return arrivalDelayAndOccurrences.contains(arrivalDelay);
        }

        [[nodiscard]] double expectedArrivalDelay() const
        {
            if (arrivalDelayAndOccurrences.empty()) return 100.0;
            if (arrivalDelayAndOccurrences.size() == 1) return arrivalDelayAndOccurrences.begin()->first;

            // If the most common arrival delay is negative, return it
            auto mostCommon = mostCommonArrivalDelay();
            if (mostCommon < 0) return mostCommon;

            double totalArrivalDelay = 0.0;
            int totalOccurrences = 0;
            for (const auto &[arrivalDelay, occurrences] : arrivalDelayAndOccurrences)
            {
                // If the arrival delay is negative, this means we don't reach the patch on time, so we penalize this heavily
                totalArrivalDelay += arrivalDelay + ((arrivalDelay < 0) ? (BWAPI::Broodwar->getLatencyFrames() + 11) : 0);
                totalOccurrences += occurrences;
            }

            return totalArrivalDelay / (double)totalOccurrences;
        }
    };

    struct SecondResendPositionObservationMetadata
    {
        PositionAndVelocity pos;
        std::unordered_map<PositionAndVelocity, int> next;
        int deltaToFirstResend;
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
            observations.add(arrivalDelta);
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
