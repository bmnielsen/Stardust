#include "ReturnArrivalData.h"

namespace MiningOptimizationTraining
{
    int ReturnArrivalData::delayAfterAction(int orderProcessTimerAtArrival) const
    {
        if (orderProcessTimerAtArrival == 0)
        {
            switch (exitSpeed())
            {
                case ReturnExitSpeed::Collision:
                    return 9;
                case ReturnExitSpeed::Low:
                    return 0;
                case ReturnExitSpeed::Medium:
                    return -2;
                case ReturnExitSpeed::High:
                    return -4;
            }
        }

        return (collision() ? 9 : 0);
    }

    std::pair<int, int> ReturnArrivalData::computeActionFrame(int pathStartFrame,
                                                              std::optional<int> lastResendFrame,
                                                              std::optional<int> orderProcessTimerResetFrame) const
    {
        // Compute the arrival frame, using the start frame as either the resend or the path start frame if no resend occurred
        int arrivalFrame = ((lastResendFrame.has_value()) ? *lastResendFrame : pathStartFrame) + (int)arrivalDelay();

        // Compute the order process timer value at the start of the frame where the worker is at this node (i.e. where this arrival delay is
        // measured from)
        // If the node is a resend node, the value will be 0 on the next frame, so we set it to 10 to make the math work
        // If the node is the start of the path, the value is 0 since the worker just gained minerals
        int orderProcessTimerAtNode = (lastResendFrame.has_value()) ? 10 : 0;

        // Compute the order process timer value at arrival ignoring resets
        int orderProcessTimerAtArrival = orderProcessTimerAtNode - (int)arrivalDelay();
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

    ReturnArrivalData ReturnArrivalData::create(unsigned int arrivalDelay,
                                                ReturnExitSpeed exitSpeed,
                                                bool collision,
                                                PositionAndVelocity nextPathStartPositionDeliveryAtArrival,
                                                PositionAndVelocity nextPathStartPositionDeliveryAfterArrival)
    {
        // Arrival delay values outside the range of 14 bits are clamped
        // This is fine since such long arrival delays would never be useful for optimization anyway
        arrivalDelay = std::min(UINT13_MAX, arrivalDelay);

        // Shift to the left to make room for the exit speed and collision
        uint16_t packed = (uint16_t)arrivalDelay << 3;

        // Add the exit speed
        packed += (uint16_t)exitSpeed;

        // Add the collision
        if (collision) packed |= 0b0000000000000100;

        return ReturnArrivalData{packed,
                                 std::move(nextPathStartPositionDeliveryAtArrival),
                                 std::move(nextPathStartPositionDeliveryAfterArrival)};
    }

    ReturnArrivalData ReturnArrivalData::createFromSimulatedPaths(
            const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAtArrival,
            const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAfterArrival)
    {
        // The return exit speed is bucketed based on the squared speed
        // The max speed of a worker is 5 pixels per frame, which corresponds to 5*256=1280 subpixels per frame or 1638400 squared
        ReturnExitSpeed returnExitSpeed;
        if (simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath > 1048576) // 80% of top speed
        {
            returnExitSpeed = ReturnExitSpeed::High;
        }
        else if (simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath > 409600) // 50% of top speed
        {
            returnExitSpeed = ReturnExitSpeed::Medium;
        }
        else if (simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath == 0)
        {
            returnExitSpeed = ReturnExitSpeed::Collision;
        }
        else
        {
            returnExitSpeed = ReturnExitSpeed::Low;
        }

        bool collision = (simulatedPathWithActionAfterArrival.squaredSpeedEightFramesAlongNextPath == 0);

        return create(simulatedPathWithActionAtArrival.positions.size(),
                      returnExitSpeed,
                      collision,
                      PositionAndVelocity{simulatedPathWithActionAtArrival.nextPathStartPosition},
                      PositionAndVelocity{simulatedPathWithActionAfterArrival.nextPathStartPosition});
    }

    std::set<std::tuple<int, int, BWAPI::ExactPosition, bool>> InitialWorkerReturnArrivalData::computePathResult(
            int pathStartFrame,
            bool pathStartsWithGatherCommand,
            std::optional<int> lastResendFrame,
            const std::set<int> &orderProcessTimerResetValues) const
    {
        // The reference frame (where we know the order process timer value) is either the path start or the last resend
        int referenceFrame = (lastResendFrame.has_value()) ? *lastResendFrame : pathStartFrame;

        // The arrival frame adds the delay from here
        int arrivalFrame = referenceFrame + arrivalDelay;

        // Set the reference frame and order process timer to where we know the order process timer value
        // The order process timer value here is the value at the start of the frame
        int initialOrderProcessTimer;
        if (lastResendFrame)
        {
            // When we have a resend, the order process timer stays at 0 for an extra frame, so we fast-forward a couple of frames
            referenceFrame += 2;
            initialOrderProcessTimer = 8;
        }
        else
        {
            // At path start the value is 0 because we just gained minerals
            initialOrderProcessTimer = 0;
        }

        // Run the order process timer cycle for each reset value until action and record the results
        std::set<std::tuple<int, int, BWAPI::ExactPosition, bool>> results;
        for (auto resetValue : orderProcessTimerResetValues)
        {
            int frame = referenceFrame;
            int orderProcessTimer = initialOrderProcessTimer;
            while (true)
            {
                if (frame == 158)
                {
                    orderProcessTimer = resetValue;
                }

                // Delivery at arrival
                if (orderProcessTimer == 0 && frame == arrivalFrame)
                {
                    int delay;
                    switch (exitSpeedDeliveryAtArrival)
                    {
                        case ReturnExitSpeed::Collision:
                            delay = 9;
                            break;
                        case ReturnExitSpeed::Low:
                            delay = 0;
                            break;
                        case ReturnExitSpeed::Medium:
                            delay = -2;
                            break;
                        case ReturnExitSpeed::High:
                            delay = -4;
                            break;
                    }
                    results.emplace(frame, delay, nextPathStartPositionDeliveryAtArrival, true);
                    break;
                }

                // Delivery after arrival
                if (orderProcessTimer == 0 && frame > arrivalFrame)
                {
                    results.emplace(frame, collisionDeliveryAfterArrival, nextPathStartPositionDeliveryAfterArrival, false);
                    break;
                }

                orderProcessTimer--;
                if (orderProcessTimer < 0) orderProcessTimer = 8;

                frame++;
            }
        }

        return results;
    }

    InitialWorkerReturnArrivalData InitialWorkerReturnArrivalData::createFromSimulatedPath(
            const BWAPI::SimulateGatherPathResult &simulatedPathDeliveryAtArrival,
            const BWAPI::SimulateGatherPathResult &simulatedPathDeliveryAfterArrival)
    {
        if (simulatedPathDeliveryAtArrival.positions.empty()) return {};

        // The return exit speed is bucketed based on the squared speed
        // The max speed of a worker is 5 pixels per frame, which corresponds to 5*256=1280 subpixels per frame or 1638400 squared
        ReturnExitSpeed returnExitSpeed;
        if (simulatedPathDeliveryAtArrival.squaredSpeedEightFramesAlongNextPath > 1048576) // 80% of top speed
        {
            returnExitSpeed = ReturnExitSpeed::High;
        }
        else if (simulatedPathDeliveryAtArrival.squaredSpeedEightFramesAlongNextPath > 409600) // 50% of top speed
        {
            returnExitSpeed = ReturnExitSpeed::Medium;
        }
        else if (simulatedPathDeliveryAtArrival.squaredSpeedEightFramesAlongNextPath == 0)
        {
            returnExitSpeed = ReturnExitSpeed::Collision;
        }
        else
        {
            returnExitSpeed = ReturnExitSpeed::Low;
        }

        bool collision = (simulatedPathDeliveryAfterArrival.squaredSpeedEightFramesAlongNextPath == 0);

        return {
                (uint16_t)simulatedPathDeliveryAtArrival.positions.size(),
                returnExitSpeed,
                collision,
                simulatedPathDeliveryAtArrival.nextPathStartPosition,
                simulatedPathDeliveryAfterArrival.nextPathStartPosition
        };
    }

    std::ostream &operator<<(std::ostream &os, const ReturnExitSpeed &exitSpeed)
    {
        switch (exitSpeed)
        {
            case ReturnExitSpeed::Collision:
                os << "Collision";
                break;
            case ReturnExitSpeed::Low:
                os << "Low";
                break;
            case ReturnExitSpeed::Medium:
                os << "Medium";
                break;
            case ReturnExitSpeed::High:
                os << "High";
                break;
        }
        return os;
    }
}
