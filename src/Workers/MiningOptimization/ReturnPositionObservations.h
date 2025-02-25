#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include "Occurrences.h"
#include <map>

namespace WorkerMiningOptimization
{
    struct ReturnSpeedOccurrences
    {
        enum class ReturnSpeedObservation
        {
            Collision,
            LowExitSpeed,
            MediumExitSpeed,
            HighExitSpeed
        };

        // Worker collided with the depot
        COLLISION_TYPE collision;

        // Worker left the depot at normal low speed (approx. 30% speed 8 frames after delivery)
        COLLISION_TYPE lowExitSpeed;

        // Worker left the depot at medium speed (50-80% speed 8 frames after delivery)
        COLLISION_TYPE mediumExitSpeed;

        // Worker left the depot at high speed (80%+ speed 8 frames after delivery)
        COLLISION_TYPE highExitSpeed;

        void addObservation(ReturnSpeedObservation observation)
        {
            if ((collision + lowExitSpeed + mediumExitSpeed + highExitSpeed) == UINT16_MAX) return;
            if (observation == ReturnSpeedObservation::Collision)
            {
                collision++;
            }
            else if (observation == ReturnSpeedObservation::LowExitSpeed)
            {
                lowExitSpeed++;
            }
            else if (observation == ReturnSpeedObservation::MediumExitSpeed)
            {
                mediumExitSpeed++;
            }
            else
            {
                highExitSpeed++;
            }
        }

        [[nodiscard]] double expectedDeltaToNormal() const;

        [[nodiscard]] bool disagreement() const
        {
            // There is disagreement if the category with most occurrences is less than 75% of the total
            long maxOccurrences = std::max({collision, lowExitSpeed, mediumExitSpeed, highExitSpeed});
            long total = collision + lowExitSpeed + mediumExitSpeed + highExitSpeed;

            return (maxOccurrences * 4) < (total * 3);
        }
    };

    struct ReturnArrivalObservations
    {
        std::unordered_map<uint16_t, OCCURRENCE_TYPE> arrivalDelayAndOccurrences;
        ReturnSpeedOccurrences deliveryAfterArrivalSpeeds = {0, 0, 0, 0};
        ReturnSpeedOccurrences deliveryAtArrivalSpeeds = {0, 0, 0, 0};

        void add(uint16_t arrivalDelay)
        {
            if (atOccurrenceCap(arrivalDelayAndOccurrences)) return;
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

    private:
        [[nodiscard]] double deliveryDelayForArrival(
                uint16_t arrivalDelay, int arrivalFrame, int knownOrderProcessTimer, int knownOrderProcessTimerFrame) const;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct ReturnPositionObservations
    {
    public:
        // The position
        PositionAndVelocity pos;

        // How often this position has occurred in its path
        // For root nodes, how often it has been observed
        OCCURRENCE_TYPE occurrences = 1;

        // All next positions seen from this position
        // Will be empty on leaf nodes
        std::vector<ReturnPositionObservations> nextPositions;

        // Observations for when no resend was sent here
        ReturnArrivalObservations noResendArrivalObservations;

        // Observations for when a resend was sent here
        ReturnArrivalObservations resendArrivalObservations;

        ReturnPositionObservations() = default;

        explicit ReturnPositionObservations(PositionAndVelocity pos)
                : pos(pos)
        {}

        ReturnPositionObservations(PositionAndVelocity pos, uint16_t arrival)
                : pos(pos)
                , noResendArrivalObservations(ReturnArrivalObservations{{{arrival, 1}}})
        {}

        // Checks if any of the observed arrival delays are after our exploration horizon
        [[nodiscard]] bool afterExplorationHorizon() const;

        [[nodiscard]] bool suitableForExploration() const;

        ReturnPositionObservations* nextPositionIfExists(const PositionAndVelocity &nextPos);
    };

    std::ostream &operator<<(std::ostream &os, const ReturnPositionObservations &returnPositionObservations);
}
