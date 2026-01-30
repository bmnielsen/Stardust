#include "GatherArrivalData.h"

namespace MiningOptimizationTraining
{
    std::pair<int, int> GatherArrivalData::computeActionFrame(std::optional<int> lastResendFrame,
                                                              std::optional<int> orderProcessTimerResetFrame,
                                                              int pathStartFrame) const
    {
        // Compute the arrival frame, using the start frame as either the resend or the path start frame if no resend occurred
        int arrivalFrame = ((lastResendFrame.has_value()) ? *lastResendFrame : pathStartFrame) + (int)arrivalDelay();

        // Compute the order process timer value at the start of the frame where the worker is at this node (i.e. where this arrival delay is
        // measured from)
        // If the node is a resend node, the value will be 0 on the next two frames, so we set it to 11 to make the math work
        // If the node is the start of the path, the value will be 0 on the next frame, so we set it to 10 to make the math work
        int orderProcessTimerAtNode = (lastResendFrame.has_value()) ? 11 : 10;

        // Compute the order process timer value at arrival ignoring resets
        int orderProcessTimerAtArrival = orderProcessTimerAtNode - (int)arrivalDelay();
        while (orderProcessTimerAtArrival < 0)
        {
            orderProcessTimerAtArrival += 9;
        }

        // Compute the action frame ignoring resets
        int actionFrame = arrivalFrame + orderProcessTimerAtArrival + 1;

        // If there is no order process timer reset affecting the result, return now
        if (!orderProcessTimerResetFrame.has_value() || (*orderProcessTimerResetFrame > actionFrame)
            || (lastResendFrame.has_value() && (*orderProcessTimerResetFrame <= *lastResendFrame)))
        {
            return std::make_pair(actionFrame + (!facingTarget() ? 9 : 0), (collision() ? 9 : 0));
        }

        // For simplicity we just assume the action frame on average will be 4 frames after the earliest it can be, since we don't need this to
        // be super accurate for training
        return std::make_pair(std::max(arrivalFrame, *orderProcessTimerResetFrame) + 4 + (!facingTarget() ? 9 : 0), (collision() ? 9 : 0));
    }
}
