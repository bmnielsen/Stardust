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
        int deltaToFirstResend;
        ResendPositionObservations observations;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct PositionObservationMetadata
    {
    public:
        PositionAndVelocity pos;
        std::shared_ptr<const PositionAndVelocity> next;
        int deltaToNormalPathOptimalPosition;
        int bestDelta = 100;
        int bestFollowingPositionDelta = 100;
        bool hasPositionToTry = false;
        bool followingHasPositionToTry = false;

        ResendPositionObservations noResendObservations;
        std::vector<SecondResendPositionObservationMetadata> secondResendMetadata;

        [[nodiscard]] SecondResendPositionObservationMetadata* secondResendMetadataFor(const PositionAndVelocity &secondResendPosition)
        {
            for (auto &candidate : secondResendMetadata)
            {
                if (candidate.pos.equals(secondResendPosition)) return &candidate;
            }

            return nullptr;
        }

        [[nodiscard]] const SecondResendPositionObservationMetadata* optimalSecondResendPositionMetadata() const
        {
            if (bestDelta == 100) return nullptr;

            if (bestDelta > deltaToNormalPathOptimalPosition)
            {
                for (const auto &candidate : secondResendMetadata)
                {
                    if (candidate.observations.hasArrivalDelay(bestDelta - deltaToNormalPathOptimalPosition - candidate.deltaToFirstResend))
                    {
                        return &candidate;
                    }
                }
            }

            return nullptr;
        }

        void addObservation(const std::shared_ptr<const PositionAndVelocity> &secondResendPosition, int arrivalDelta)
        {
            int deltaFromNormalPath = 100;
            if (!secondResendPosition)
            {
                noResendObservations.add(arrivalDelta);
                if (arrivalDelta >= 0)
                {
                    deltaFromNormalPath = deltaToNormalPathOptimalPosition + arrivalDelta;
                }
            }
            else
            {
                auto metadata = secondResendMetadataFor(*secondResendPosition);
                if (metadata)
                {
                    metadata->observations.add(arrivalDelta);
                    if (arrivalDelta >= 0)
                    {
                        deltaFromNormalPath = deltaToNormalPathOptimalPosition + metadata->deltaToFirstResend + arrivalDelta;
                    }
                }
            }

            if (deltaFromNormalPath < bestDelta) bestDelta = deltaFromNormalPath;
        }

        void updateState()
        {
            // Purge second resend metadata that is not needed
            int maxRelevantSecondResendDelta = std::min(bestDelta, bestFollowingPositionDelta) - deltaToNormalPathOptimalPosition;
            for (auto it = secondResendMetadata.begin(); it != secondResendMetadata.end(); it++)
            {
                if (it->deltaToFirstResend > maxRelevantSecondResendDelta)
                {
                    secondResendMetadata.erase(it, secondResendMetadata.end());
                    break;
                }
            }

            // Clear the "have a position to try" flag if we no longer have anything to try
            if (hasPositionToTry && !hasUntriedPosition())
            {
                hasPositionToTry = false;
            }
        }

    private:
        [[nodiscard]] bool hasUntriedPosition() const
        {
            if (noResendObservations.empty()) return true;

            for (const auto &metadata : secondResendMetadata)
            {
                if (metadata.observations.empty()) return true;
            }

            return false;
        }
    };

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata);
}
