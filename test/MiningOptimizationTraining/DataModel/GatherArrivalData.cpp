#include "GatherArrivalData.h"

namespace MiningOptimizationTraining
{
    namespace
    {
        // Computes whether a worker at the given exact position is facing the patch (so it can start mining without waiting to turn)
        bool isFacingPatch(const BWAPI::ExactPosition &position, BWAPI::Unit patch)
        {
            auto vectorToPatch = patch->getPosition() - position.pos();
            auto angleDiff = Geo::BWAngleDiff(position.heading, Geo::BWDirection(vectorToPatch));
            return (angleDiff <= 2 * BWAPI::UnitTypes::Protoss_Probe.turnRadius());
        }
    }

    std::pair<int, int> GatherArrivalData::computeActionFrame(int pathStartFrame,
                                                              std::optional<int> lastResendFrame,
                                                              std::optional<int> orderProcessTimerResetFrame) const
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

    GatherArrivalData GatherArrivalData::create(unsigned int arrivalDelay,
                                                bool facingTarget,
                                                bool collision,
                                                uint8_t tenDistanceDelta,
                                                const PositionAndVelocity &nextPathStartPosition)
    {
        // Arrival delay values outside the range of 14 bits are clamped
        // This is fine since such long arrival delays would never be useful for optimization anyway
        arrivalDelay = std::min(UINT14_MAX, arrivalDelay);

        // Shift to the left to make room for the boolean bits
        uint16_t packed = (uint16_t)arrivalDelay << 2;

        // We assume we are usually facing the target, so only set the lowest bit if this isn't the case
        if (!facingTarget) packed |= 0b00000001;

        // We set the second-lowest bit if there is a collision
        if (collision) packed |= 0b00000010;

        return GatherArrivalData{packed, tenDistanceDelta, nextPathStartPosition};
    }

    // Populates the members of the struct, except arrivalDelay, from simulated path data
    GatherArrivalData GatherArrivalData::createFromSimulatedPaths(
            const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAtArrival,
            const BWAPI::SimulateGatherPathResult &simulatedPathWithActionAfterArrival,
            BWAPI::Unit patch)
    {
        if (simulatedPathWithActionAtArrival.positions.empty()) return {};

        // The worker is "facing target" if it can turn to face the patch in two frames
        // We check both paths to capture cases where the worker turns while waiting to perform its action
        // This does mean that in some cases we could use a path as long as we are sure the action will occur at arrival, but
        // such paths are rare so we don't want to bother investing the extra data storage
        bool facingTarget = isFacingPatch(simulatedPathWithActionAtArrival.actionPosition, patch)
                            && isFacingPatch(simulatedPathWithActionAfterArrival.actionPosition, patch);

        // We expect the action frame for action at arrival to equal the arrival frame
        // If it doesn't, it means the worker arrived with its pathing messed up, which is equivalent in effect to not facing the patch
        bool atMoveTarget = true;
        if (simulatedPathWithActionAtArrival.actionFrame != simulatedPathWithActionAtArrival.arrivalFrame)
        {
            atMoveTarget = false;
        }

        // Find the index of the first position that is 10 distance from the patch
        uint8_t tenDistanceDelta = (UINT8_MAX - 1);
        int i = 0;
        for (auto it = simulatedPathWithActionAtArrival.positions.rbegin();
             it != simulatedPathWithActionAtArrival.positions.rend();
             it++)
        {
            auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                it->pos(),
                                                BWAPI::UnitTypes::Resource_Mineral_Field,
                                                patch->getPosition());
            if (dist > 10)
            {
                tenDistanceDelta = (uint8_t)std::min(i, (UINT8_MAX - 2));
                break;
            }
            i++;
        }

        return create(simulatedPathWithActionAtArrival.positions.size(),
                      facingTarget && atMoveTarget,
                      simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath == 0
                      || simulatedPathWithActionAfterArrival.squaredSpeedEightFramesAlongNextPath == 0,
                      tenDistanceDelta,
                      PositionAndVelocity{simulatedPathWithActionAtArrival.nextPathStartPosition});
    }

    InitialWorkerGatherArrivalData InitialWorkerGatherArrivalData::createFromSimulatedPath(const BWAPI::SimulateGatherPathResult &simulatedPath,
                                                                                           BWAPI::Unit patch)
    {
        if (simulatedPath.positions.empty()) return {};

        // For gather, we always simulate with action at arrival, so we expect the action frame to equal the arrival frame
        // If it doesn't, it means the worker arrived with its pathing messed up, which is equivalent in effect to not facing the patch
        bool atMoveTarget = true;
        if (simulatedPath.actionFrame != simulatedPath.arrivalFrame)
        {
            atMoveTarget = false;
        }

        return {
                (uint16_t)simulatedPath.positions.size(),
                isFacingPatch(simulatedPath.actionPosition, patch) && atMoveTarget,
                simulatedPath.squaredSpeedEightFramesAlongNextPath == 0,
                simulatedPath.nextPathStartPosition
        };
    }
}
