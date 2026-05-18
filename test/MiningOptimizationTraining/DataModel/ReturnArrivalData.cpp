#include "ReturnArrivalData.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        uint8_t convertSquaredSpeed(uint64_t squaredSpeed)
        {
            if (squaredSpeed >= 1638400) return 128;
            return (uint8_t)std::round(std::sqrt((double)squaredSpeed) / 10.0);
        }
    }

    int returnExitSpeedToDelay(uint8_t exitSpeed)
    {
        // The exit speed is stored as a 7-bit integer reflecting the worker's speed as a percentage of maximum
        // If it is 0, the worker collided with the depot after delivery
        // Otherwise we bucket the delay numbers based on what percentage of speed is preserved

        if (exitSpeed == 0) return 9;

        if (exitSpeed >= 102) return -4; // Approx. 80% of speed
        if (exitSpeed >= 64) return -2; // 50% of max speed
        return 0;
    }

    int ReturnArrivalData::delayAfterAction(int orderProcessTimerAtArrival) const
    {
        if (orderProcessTimerAtArrival == 0)
        {
            return returnExitSpeedToDelay(exitSpeedDeliveryAtArrival);
        }

        return (collisionDeliveryAfterArrival ? 9 : 0);
    }

    std::pair<int, int> ReturnArrivalData::computeActionFrame(int pathStartFrame,
                                                              std::optional<int> lastResendFrame,
                                                              std::optional<int> orderProcessTimerResetFrame) const
    {
        // Compute the arrival frame, using the start frame as either the resend or the path start frame if no resend occurred
        int arrivalFrame = ((lastResendFrame.has_value()) ? *lastResendFrame : pathStartFrame) + (int)arrivalDelay;

        // Compute the order process timer value at the start of the frame where the worker is at this node (i.e. where this arrival delay is
        // measured from)
        // If the node is a resend node, the value will be 0 on the next frame, so we set it to 10 to make the math work
        // If the node is the start of the path, the value is 0 since the worker just gained minerals
        int orderProcessTimerAtNode = (lastResendFrame.has_value()) ? 10 : 0;

        // Compute the order process timer value at arrival ignoring resets
        int orderProcessTimerAtArrival = orderProcessTimerAtNode - (int)arrivalDelay;
        while (orderProcessTimerAtArrival < 0)
        {
            orderProcessTimerAtArrival += 9;
        }

        // Compute the action frame ignoring resets
        int actionFrame = arrivalFrame + orderProcessTimerAtArrival;

        // If there is no order process timer reset affecting the result, return now
        if (!orderProcessTimerResetFrame.has_value() || (*orderProcessTimerResetFrame > actionFrame)
            || (lastResendFrame.has_value() && (*orderProcessTimerResetFrame <= *lastResendFrame)))
        {
            return std::make_pair(actionFrame, delayAfterAction(orderProcessTimerAtArrival));
        }

        // For simplicity we just assume the action frame on average will be 4 frames after the earliest it can be, since we don't need this to
        // be super accurate for training, and that the delay will also be the average of 8 possible values
        int averageDelay = (int)std::round((double)(delayAfterAction(0) + delayAfterAction(1) * 7) / 8.0);
        return std::make_pair(std::max(arrivalFrame, *orderProcessTimerResetFrame) + 4, averageDelay);
    }

    ReturnArrivalData ReturnArrivalData::createFromSimulatedPaths(
            const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAtArrival,
            const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAfterArrival)
    {
        return ReturnArrivalData{
            (uint8_t)std::min(UINT7_MAX, (unsigned int)simulatedPathWithActionAtArrival.positions.size()),
            (simulatedPathWithActionAfterArrival.squaredSpeedEightFramesAlongNextPath == 0),
            convertSquaredSpeed(simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath),
            (simulatedPathWithActionAtArrival.actionFrame == simulatedPathWithActionAtArrival.arrivalFrame),
            PositionAndVelocity{simulatedPathWithActionAtArrival.nextPathStartPosition},
            PositionAndVelocity{simulatedPathWithActionAfterArrival.nextPathStartPosition}
        };
    }

    InitialWorkerReturnArrivalData InitialWorkerReturnArrivalData::createFromSimulatedPath(
            const BWAPI::SimulateGatherPathResult &simulatedPathDeliveryAtArrival,
            const BWAPI::SimulateGatherPathResult &simulatedPathDeliveryAfterArrival)
    {
        if (simulatedPathDeliveryAtArrival.positions.empty()) return {};

        return InitialWorkerReturnArrivalData{
            (uint8_t)std::min(UINT7_MAX, (unsigned int)simulatedPathDeliveryAtArrival.positions.size()),
            (simulatedPathDeliveryAfterArrival.squaredSpeedEightFramesAlongNextPath == 0),
            convertSquaredSpeed(simulatedPathDeliveryAtArrival.squaredSpeedEightFramesAlongNextPath),
            (simulatedPathDeliveryAtArrival.actionFrame == simulatedPathDeliveryAtArrival.arrivalFrame),
            simulatedPathDeliveryAtArrival.nextPathStartPosition,
            simulatedPathDeliveryAfterArrival.nextPathStartPosition
        };
    }
}
