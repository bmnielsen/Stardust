#include "ReturnPositionObservations.h"

#include "WorkerMiningOptimization.h"
#include "OrderProcessTimer.h"

namespace WorkerMiningOptimization
{
    uint8_t ReturnArrivalObservations::mostCommonArrivalDelay() const
    {
        if (arrivalDelayAndOccurrenceRate.empty()) return UINT8_MAX;
        if (arrivalDelayAndOccurrenceRate.size() == 1) return arrivalDelayAndOccurrenceRate.begin()->first;

        uint8_t best = 0;
        uint8_t bestRate = 0;
        for (const auto &[arrivalDelay, rate] : arrivalDelayAndOccurrenceRate)
        {
            if (rate > bestRate)
            {
                best = arrivalDelay;
                bestRate = rate;
            }
        }

        return best;
    }

    uint8_t ReturnArrivalObservations::largestArrivalDelay() const
    {
        if (arrivalDelayAndOccurrenceRate.empty()) return UINT8_MAX;
        if (arrivalDelayAndOccurrenceRate.size() == 1) return arrivalDelayAndOccurrenceRate.begin()->first;

        uint8_t best = 0;
        for (const auto &[delay, _] : arrivalDelayAndOccurrenceRate)
        {
            if (delay > best)
            {
                best = delay;
            }
        }

        return best;
    }

    double ReturnArrivalObservations::expectedDeliveryDelay(int commandFrame) const
    {
        if (arrivalDelayAndOccurrenceRate.empty()) return 100.0;

        if (arrivalDelayAndOccurrenceRate.size() == 1)
        {
            return deliveryDelayForArrival(arrivalDelayAndOccurrenceRate.begin()->first,
                                           commandFrame + arrivalDelayAndOccurrenceRate.begin()->first,
                                           0,
                                           commandFrame + BWAPI::Broodwar->getLatencyFrames());
        }

        double totalDelay = 0.0;
        for (const auto &[arrivalDelay, rate] : arrivalDelayAndOccurrenceRate)
        {
            totalDelay += deliveryDelayForArrival(arrivalDelay,
                                                  commandFrame + arrivalDelay,
                                                  0,
                                                  commandFrame + BWAPI::Broodwar->getLatencyFrames()) * ((double)rate/255.0);
        }

        return totalDelay;
    }

    double ReturnArrivalObservations::expectedNoResendDeliveryDelay(const MyWorker &worker) const
    {
        if (arrivalDelayAndOccurrenceRate.empty()) return 100.0;

        if (arrivalDelayAndOccurrenceRate.size() == 1)
        {
            return deliveryDelayForArrival(arrivalDelayAndOccurrenceRate.begin()->first,
                                           BWAPI::Broodwar->getFrameCount() + arrivalDelayAndOccurrenceRate.begin()->first,
                                           worker->orderProcessTimer,
                                           BWAPI::Broodwar->getFrameCount());
        }

        double totalDelay = 0.0;
        for (const auto &[arrivalDelay, rate] : arrivalDelayAndOccurrenceRate)
        {
            totalDelay += deliveryDelayForArrival(arrivalDelay,
                                                  BWAPI::Broodwar->getFrameCount() + arrivalDelay,
                                                  worker->orderProcessTimer,
                                                  BWAPI::Broodwar->getFrameCount()) * ((double)rate/255.0);
        }

        return totalDelay;
    }

    double ReturnArrivalObservations::deliveryDelayForArrival(uint8_t arrivalDelay,
                                                              int arrivalFrame,
                                                              int knownOrderProcessTimer,
                                                              int knownOrderProcessTimerFrame) const
    {
        // Compute the order process timer at arrival
        // This takes order process timer resets into account
        int orderProcessTimerAtArrival = OrderProcessTimer::unitOrderProcessTimerAtDelta(
                knownOrderProcessTimerFrame, knownOrderProcessTimer, arrivalFrame - knownOrderProcessTimerFrame - 1);

        // If we don't know what the order process timer will be at arrival, compute an average delay considering the different exit timings
        // depending on whether the delivery happens at arrival or not
        if (orderProcessTimerAtArrival == -1)
        {
            // If the arrival frame is a reset frame, this changes the math slightly as there are only 8 possible reset values
            double delayAfterArrival;
            if (OrderProcessTimer::isResetFrame(arrivalFrame))
            {
                delayAfterArrival =
                        (deliveryAtArrivalSpeeds.expectedDeltaToNormal() + 28.0 + (7.0 * deliveryAfterArrivalSpeeds.expectedDeltaToNormal())) / 8.0;
            }
            else
            {
                delayAfterArrival =
                        (deliveryAtArrivalSpeeds.expectedDeltaToNormal() + 36.0 + (8.0 * deliveryAfterArrivalSpeeds.expectedDeltaToNormal())) / 9.0;
            }

            return (double)arrivalDelay + delayAfterArrival;
        }

        // Compute the expected delivery frame if no order process timer reset occurs
        int deliveryFrame = arrivalFrame + orderProcessTimerAtArrival;

        // Handle the case where the order process timer resets between arrival and expected delivery
        int nextResetFrame = OrderProcessTimer::nextResetFrame(arrivalFrame);
        if (nextResetFrame <= deliveryFrame)
        {
            // Average will be 3.5 frames of delay after the reset
            return (double)(arrivalDelay + (nextResetFrame - arrivalFrame)) + 3.5 + deliveryAfterArrivalSpeeds.expectedDeltaToNormal();
        }

        // No order process timer reset affects the timing, so just return the appropriate value depending on whether we deliver on the arrival frame
        // or not
        if (deliveryFrame == arrivalFrame)
        {
            return (double)arrivalDelay + deliveryAtArrivalSpeeds.expectedDeltaToNormal();
        }
        return (double)(arrivalDelay + deliveryFrame - arrivalFrame) + deliveryAfterArrivalSpeeds.expectedDeltaToNormal();
    }

    bool ReturnPositionObservations::afterExplorationHorizon() const
    {
        int referenceFrame = 8 + BWAPI::Broodwar->getLatencyFrames();
        for (const auto &[arrivalDelay, _] : noResendArrivalObservations.arrivalDelayAndOccurrenceRate)
        {
            if (arrivalDelay < (referenceFrame - RETURN_EXPLORE_AFTER))
            {
                return true;
            }
        }

        return false;
    }

    bool ReturnPositionObservations::suitableForExploration() const
    {
        if (!resendArrivalObservations.empty() && !resendArrivalObservations.shouldExploreDeliverySpeeds()) return false; // Have already explored

        // Don't explore if any of the observed arrival delays are outside our exploration horizon
        int referenceFrame = 8 + BWAPI::Broodwar->getLatencyFrames();
        for (const auto &[arrivalDelay, _] : noResendArrivalObservations.arrivalDelayAndOccurrenceRate)
        {
            if (arrivalDelay > (referenceFrame + RETURN_EXPLORE_BEFORE) || arrivalDelay < (referenceFrame - RETURN_EXPLORE_AFTER))
            {
                return false;
            }
        }

        return true;
    }

    ReturnPositionObservations* ReturnPositionObservations::nextPositionIfExists(const PositionAndVelocity &nextPos)
    {
        for (auto &candidate : nextPositions)
        {
            if (candidate.pos == nextPos)
            {
                return &candidate;
            }
        }
        return nullptr;
    }

    std::ostream &operator<<(std::ostream &os, const ReturnPositionObservations &optimalGatherPositionMetadata)
    {
        os << optimalGatherPositionMetadata.pos;
        return os;
    }
}
