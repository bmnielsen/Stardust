#pragma once

#include "TilePosition.h"
#include "PositionAndVelocity.h"
#include "OrderProcessTimer.h"
#include "Resource.h"
#include "OccurrencesAndCollisions.h"
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
        uint32_t collision;

        // Worker left the depot at normal low speed (approx. 30% speed 8 frames after delivery)
        uint32_t lowExitSpeed;

        // Worker left the depot at medium speed (50-80% speed 8 frames after delivery)
        uint32_t mediumExitSpeed;

        // Worker left the depot at high speed (80%+ speed 8 frames after delivery)
        uint32_t highExitSpeed;

        // The above four aggregated into an expected frame delay (which can be from -4 to 9, which we scale to fit into 8 bits)
        uint8_t frameDelay;

        void addObservation(ReturnSpeedObservation observation)
        {
            if ((collision + lowExitSpeed + mediumExitSpeed + highExitSpeed) == UINT32_MAX) return;
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

            // Using the following logic:
            // - Low exit speed is the norm, so does not affect the result
            // - High exit speed saves 4 frames
            // - Medium exit speed saves 2 frames
            // - Collisions cost an extra order process timer cycle
            uint32_t total = collision + lowExitSpeed + mediumExitSpeed + highExitSpeed;
            auto expectedDelay = (double)(((int)collision * 9) - ((int)mediumExitSpeed * 2) - ((int)highExitSpeed * 4)) / (double)total;

            // The range of expectedDelay is -4 to 9, so scale this to fill an 8-bit unsigned integer
            frameDelay = (uint8_t)std::round((expectedDelay + 4.0) * 19.615);
        }

        [[nodiscard]] double expectedDeltaToNormal() const
        {
            return ((double)frameDelay/19.615) - 4.0;
        }

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
        std::unordered_map<uint8_t, uint32_t> arrivalDelayAndOccurrences;
        std::unordered_map<uint8_t, uint8_t> arrivalDelayAndOccurrenceRate;
        ReturnSpeedOccurrences deliveryAfterArrivalSpeeds = {0, 0, 0, 0, 0};
        ReturnSpeedOccurrences deliveryAtArrivalSpeeds = {0, 0, 0, 0, 0};

        void add(unsigned int arrivalDelay)
        {
            if (arrivalDelay > UINT8_MAX)
            {
                Log::Get() << "ERROR: arrivalDelay " << arrivalDelay << " outside normal bounds";
                return;
            }

            if (atOccurrenceCap(arrivalDelayAndOccurrences)) return;
            arrivalDelayAndOccurrences[arrivalDelay]++;
            arrivalDelayAndOccurrenceRate = computeOccurrenceRateMap(arrivalDelayAndOccurrences);
        }

        [[nodiscard]] bool empty() const
        {
            return arrivalDelayAndOccurrenceRate.empty();
        }

        [[nodiscard]] uint8_t mostCommonArrivalDelay() const;

        [[nodiscard]] uint8_t largestArrivalDelay() const;

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
                uint8_t arrivalDelay, int arrivalFrame, int knownOrderProcessTimer, int knownOrderProcessTimerFrame) const;
    };

    // This is the structure we use to track observed positions and our track record using them
    struct ReturnPositionObservations
    {
    public:
        // The position
        PositionAndVelocity pos;

        // How often this position has occurred in its path
        // For root nodes, how often it has been observed
        uint32_t occurrences = 1;

        // The occurence rate of this position compared to its siblings as a fraction of 255
        uint8_t occurrenceRate = 0;

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

        ReturnPositionObservations(PositionAndVelocity pos, unsigned int arrivalDelay)
                : pos(pos)
        {
            noResendArrivalObservations.add(arrivalDelay);
        }

        [[nodiscard]] bool usableForPathPlanning() const;

        // Checks if any of the observed arrival delays are after our exploration horizon
        [[nodiscard]] bool afterExplorationHorizon() const;

        [[nodiscard]] bool suitableForExploration() const;

        ReturnPositionObservations* nextPositionIfExists(const PositionAndVelocity &nextPos);
    };

    std::ostream &operator<<(std::ostream &os, const ReturnArrivalObservations &returnArrivalObservations);
    std::ostream &operator<<(std::ostream &os, const ReturnPositionObservations &returnPositionObservations);
}
