#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include <bitsery/ext/std_map.h>
#include <map>

namespace WorkerMiningOptimization
{
    struct ReturnSpeedOccurrences
    {
        // Worker collided with the depot
        uint32_t collision;

        // Worker left the depot at normal low speed (approx. 30% speed 8 frames after delivery)
        uint32_t lowExitSpeed;

        // Worker left the depot at medium speed (50-80% speed 8 frames after delivery)
        uint32_t mediumExitSpeed;

        // Worker left the depot at high speed (80%+ speed 8 frames after delivery)
        uint32_t highExitSpeed;

        [[nodiscard]] double expectedDeltaToNormal() const;

        [[nodiscard]] bool disagreement() const
        {
            // There is disagreement if the category with most occurrences is less than 75% of the total
            auto maxOccurrences = std::max({collision, lowExitSpeed, mediumExitSpeed, highExitSpeed});
            auto total = collision + lowExitSpeed + mediumExitSpeed + highExitSpeed;

            return (maxOccurrences * 4) < (total * 3);
        }

        template <typename S>
        void serialize(S& s)
        {
            s.value4b(collision);
            s.value4b(lowExitSpeed);
            s.value4b(mediumExitSpeed);
            s.value4b(highExitSpeed);
        }
    };

    struct ReturnArrivalObservations
    {
        std::unordered_map<uint16_t, uint32_t> arrivalDelayAndOccurrences;
        ReturnSpeedOccurrences deliveryAfterArrivalSpeeds = {0, 0, 0, 0};
        ReturnSpeedOccurrences deliveryAtArrivalSpeeds = {0, 0, 0, 0};

        void add(uint16_t arrivalDelay)
        {
            arrivalDelayAndOccurrences[arrivalDelay]++;
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrences.empty();
        }

        [[nodiscard]] uint16_t mostCommonArrivalDelay() const;

        [[nodiscard]] uint16_t largestArrivalDelay() const;

        // Computes the expected number of frames from resending here to delivery
        [[nodiscard]] double expectedDeliveryDelay(int commandFrame) const;

        // Compute the expected number of frames to delivery if the given worker
        [[nodiscard]] double expectedNoResendDeliveryDelay(const MyWorker &worker) const;

        // Whether we need to explore delivery speeds for these observations
        [[nodiscard]] bool shouldExploreDeliverySpeeds() const
        {
            uint32_t total = deliveryAfterArrivalSpeeds.collision
                             + deliveryAfterArrivalSpeeds.lowExitSpeed
                             + deliveryAfterArrivalSpeeds.mediumExitSpeed
                             + deliveryAfterArrivalSpeeds.highExitSpeed
                             + deliveryAtArrivalSpeeds.collision
                             + deliveryAtArrivalSpeeds.lowExitSpeed
                             + deliveryAtArrivalSpeeds.mediumExitSpeed
                             + deliveryAtArrivalSpeeds.highExitSpeed;

            // Always explore until 2 observations and stop exploring after 5
            if (total < 2) return true;
            if (total >= 5) return false;

            // In the in-between period, explore if there is disagreement
            return deliveryAfterArrivalSpeeds.disagreement() || deliveryAtArrivalSpeeds.disagreement();
        }

        template <typename S>
        void serialize(S& s)
        {
            s.ext(arrivalDelayAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, uint16_t& key, uint32_t& value) {
                s.value2b(key);
                s.value4b(value);
            });
            s.object(deliveryAfterArrivalSpeeds);
            s.object(deliveryAtArrivalSpeeds);
        }

    private:
        [[nodiscard]] double deliveryDelayForArrival(
                uint16_t arrivalDelay, int arrivalFrame, int knownOrderProcessTimer, int knownOrderProcessTimerFrame) const;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct ReturnPositionObservations
    {
    public:
        // Hash of the first position along this path after leaving the patch, only used for debugging
        // Value of 0 indicates the path didn't start at the patch
        uint32_t pathHash;

        // The position
        PositionAndVelocity pos;

        // All next positions seen from this position, with their count of observations
        // May be empty for the last position we consider in a path
        std::unordered_map<PositionAndVelocity, uint32_t> nextPositionAndOccurrences;

        // Observations for when no resend was sent here
        ReturnArrivalObservations noResendArrivalObservations;

        // Observations for when a resend was sent here
        ReturnArrivalObservations resendArrivalObservations;

        ReturnPositionObservations(){}

        ReturnPositionObservations(uint32_t pathHash, PositionAndVelocity pos)
                : pathHash(pathHash)
                , pos(pos)
        {}

        ReturnPositionObservations(uint32_t pathHash, PositionAndVelocity pos, uint16_t arrival)
                : pathHash(pathHash)
                , pos(pos)
                , noResendArrivalObservations(ReturnArrivalObservations{{{arrival, 1}}})
        {}

        ReturnPositionObservations(
                uint32_t pathHash,
                PositionAndVelocity pos,
                std::unordered_map<PositionAndVelocity, uint32_t> &&nextPositionAndOccurrences,
                ReturnArrivalObservations &&noResendArrivalObservations,
                ReturnArrivalObservations &&resendArrivalObservations)
                : pathHash(pathHash)
                , pos(pos)
                , nextPositionAndOccurrences(nextPositionAndOccurrences)
                , noResendArrivalObservations(std::move(noResendArrivalObservations))
                , resendArrivalObservations(std::move(resendArrivalObservations))
        {}

        // Checks if any of the observed arrival delays are after our exploration horizon
        [[nodiscard]] bool afterExplorationHorizon() const;

        [[nodiscard]] bool suitableForExploration() const;

        static void outputDataFileHeaderRow(std::ofstream &file);

        void outputToDataFile(std::ofstream &file, const TilePosition &resourceTile) const;

        static bool parseFromDataFile(
                const std::vector<std::string> &line,
                std::map<TilePosition, std::unordered_map<PositionAndVelocity, ReturnPositionObservations>> &map,
                int lineNumber);

        template <typename S>
        void serialize(S& s) {
            s.value4b(pathHash);
            s.object(pos);
            s.ext(nextPositionAndOccurrences, bitsery::ext::StdMap{ INT_MAX }, [](S& s, PositionAndVelocity& key, uint32_t& value) {
                s.object(key);
                s.value4b(value);
            });
            s.object(noResendArrivalObservations);
            s.object(resendArrivalObservations);
        }
    };

    std::ostream &operator<<(std::ostream &os, const ReturnPositionObservations &returnPositionObservations);
}
