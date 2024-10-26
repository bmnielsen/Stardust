#pragma once

#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
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

        [[nodiscard]] int mostCommonArrivalDelay() const;

        [[nodiscard]] double expectedMiningDelay(bool otherWorkerAssigned, int commandFrame) const;
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

        bool addObservation(SecondResendPositionObservationMetadata* secondResendPositionData, int arrivalDelta);

        [[nodiscard]] std::vector<const SecondResendPositionObservationMetadata*> expectedPathAfterResend() const;

        [[nodiscard]] SecondResendPositionObservationMetadata* secondResendMetadataFor(const PositionAndVelocity *secondResendPosition)
        {
            if (!secondResendPosition) return nullptr;

            auto secondResendDataIt = secondResendMetadata.find(*secondResendPosition);
            return (secondResendDataIt == secondResendMetadata.end()) ? nullptr : &secondResendDataIt->second;
        }

        static void outputDataFileHeaderRow(std::ofstream &file);
        void outputToDataFile(std::ofstream &file, const Resource &resource) const;
        static bool parseFromDataFile(
                const std::vector<std::string> &line,
                std::map<Resource, std::unordered_map<PositionAndVelocity, PositionObservationMetadata>> &map,
                int lineNumber);
    };

    // Stripped-down version used for takeover resends
    struct PositionObservationMetadataForTakeoverResends
    {
    public:
        PositionAndVelocity pos;

        std::map<int, int> noSecondResendArrivalDelayAndOccurrences;
        std::unordered_map<PositionAndVelocity, std::map<int, int>> secondResendArrivalDelayAndOccurrences;

        void addObservation(const std::shared_ptr<const PositionAndVelocity> &secondResendPosition, int arrivalDelta)
        {
            auto &observations =
                    secondResendPosition
                    ? secondResendArrivalDelayAndOccurrences[*secondResendPosition]
                    : noSecondResendArrivalDelayAndOccurrences;
            observations[arrivalDelta]++;
        }

        [[nodiscard]] std::map<int, int>* secondResendObservationsFor(const PositionAndVelocity *secondResendPosition)
        {
            if (!secondResendPosition) return nullptr;

            auto secondResendDataIt = secondResendArrivalDelayAndOccurrences.find(*secondResendPosition);
            return (secondResendDataIt == secondResendArrivalDelayAndOccurrences.end()) ? nullptr : &secondResendDataIt->second;
        }

        static void outputDataFileHeaderRow(std::ofstream &file);
        void outputToDataFile(std::ofstream &file, const Resource &resource) const;
        static bool parseFromDataFile(
                const std::vector<std::string> &line,
                std::map<Resource, std::unordered_map<PositionAndVelocity, PositionObservationMetadataForTakeoverResends>> &map,
                int lineNumber);
    };

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata);
    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadataForTakeoverResends &optimalGatherPositionMetadata);
}
