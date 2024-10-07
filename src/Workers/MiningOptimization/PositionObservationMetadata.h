#pragma once

#include "PositionAndVelocity.h"
#include <map>

namespace WorkerMiningOptimization
{
    struct ResendPositionObservations
    {
        std::map<int, int> data;

        void add(int arrivalDelta)
        {
            data[arrivalDelta]++;
        }

        [[nodiscard]] bool empty() const
        {
            return data.empty();
        }

        [[nodiscard]] int mostCommonArrivalDelay() const
        {
            int best = -1;
            int bestCount = 0;
            for (const auto &[arrivalDelay, occurrences] : data)
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
            return data.contains(arrivalDelay);
        }
    };

    struct SecondResendPositionObservationMetadata
    {
        PositionAndVelocity pos;
        std::map<PositionAndVelocity, int> next;
        int deltaToFirstResend;
        ResendPositionObservations observations;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct PositionObservationMetadata
    {
    public:
        uint32_t pathHash;
        PositionAndVelocity pos;
        std::map<PositionAndVelocity, int> next;

        int deltaToNormalPathOptimalPosition;

        bool hasPositionToTry;

        ResendPositionObservations noResendObservations;
        std::map<PositionAndVelocity, SecondResendPositionObservationMetadata> secondResendMetadata;

        void addObservation(const std::shared_ptr<const PositionAndVelocity> &secondResendPosition, int arrivalDelta)
        {
            if (secondResendPosition)
            {
                secondResendMetadata[*secondResendPosition].observations.add(arrivalDelta);
            }
            else
            {
                noResendObservations.add(arrivalDelta);
            }

            // Clear the "have a position to try" flag if we no longer have anything to try
            if (hasPositionToTry && !hasUntriedPosition())
            {
                hasPositionToTry = false;
            }
        }

        [[nodiscard]] bool hasUntriedPosition() const
        {
            if (noResendObservations.empty()) return true;

            std::set<int> secondResendDeltasExplored;
            std::set<int> secondResendDeltasUnexplored;
            for (const auto &metadata : secondResendMetadata)
            {
                if (metadata.second.deltaToFirstResend == BWAPI::Broodwar->getLatencyFrames()) continue;

                if (metadata.second.observations.empty())
                {
                    secondResendDeltasUnexplored.emplace(metadata.second.deltaToFirstResend);
                }
                else
                {
                    secondResendDeltasExplored.emplace(metadata.second.deltaToFirstResend);
                }
            }
            if (secondResendDeltasUnexplored.empty()) return false;

            for (int delta : secondResendDeltasExplored) secondResendDeltasUnexplored.erase(delta);
            return !secondResendDeltasUnexplored.empty();
        }

        std::vector<const SecondResendPositionObservationMetadata*> expectedPathAfterResend() const
        {
            std::vector<const SecondResendPositionObservationMetadata*> result;

            std::map<int, std::vector<const SecondResendPositionObservationMetadata*>> secondResendByDelta;
            for (auto &[secondResendPos, secondResendData] : secondResendMetadata)
            {
                secondResendByDelta[secondResendData.deltaToFirstResend].push_back(&secondResendData);
            }

            const std::map<PositionAndVelocity, int> *nextOccurrences = &next;
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
    };

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata);
}
