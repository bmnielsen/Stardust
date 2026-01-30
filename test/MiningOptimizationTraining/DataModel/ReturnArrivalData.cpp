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
