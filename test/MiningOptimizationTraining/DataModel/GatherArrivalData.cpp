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

        // Find the index of the first position that is 10 distance from the patch
        int i = 0;
        for (auto it = simulatedPathWithActionAtArrival.positions.rbegin();
             it != simulatedPathWithActionAtArrival.positions.rend();
             it++)
        {
            auto dist = Geo::EdgeToEdgeDistance(BWAPI::UnitTypes::Protoss_Probe,
                                                it->pos(),
                                                BWAPI::UnitTypes::Resource_Mineral_Field,
                                                patch->getPosition());
            if (dist > 10) break;
            i++;
        }

        return create(simulatedPathWithActionAtArrival.positions.size(),
                      facingTarget,
                      simulatedPathWithActionAtArrival.squaredSpeedEightFramesAlongNextPath == 0
                      || simulatedPathWithActionAfterArrival.squaredSpeedEightFramesAlongNextPath == 0,
                      (uint8_t)std::min(i, 255),
                      PositionAndVelocity{simulatedPathWithActionAtArrival.nextPathStartPosition});
    }

    std::vector<InitialWorkerComputePathResult> InitialWorkerGatherArrivalData::computePathResult(
            int pathStartFrame,
            bool pathStartsWithGatherCommand,
            std::optional<int> lastResendFrame,
            const std::set<int> &orderProcessTimerResetValues) const
    {
        // The reference frame (where we know the order process timer value) is either the path start or the last resend
        int referenceFrame = (lastResendFrame.has_value()) ? *lastResendFrame : pathStartFrame;

        // The arrival frame adds the delay from here
        int arrivalFrame = referenceFrame + arrivalDelay;

        // Adjust the reference frame to where we know the order process timer value
        // The order process timer value here is the value at the start of the frame
        // It is in reality in the range 0-8, but always starts with an extra frame at 0 so using 10 makes the math work
        int initialOrderProcessTimer = 10;
        if (lastResendFrame)
        {
            // When we have a resend, the order process timer stays at 0 for two frames, so we increment the reference frame
            referenceFrame++;
        }
        else if (pathStartsWithGatherCommand)
        {
            // When there is no resend, but the path started with a gather command, we adjust the reference frame to account for the gather command
            // and latency
            referenceFrame += BWAPI::Broodwar->getLatencyFrames() + 1;
        }

        // Run the order process timer cycle for each reset value until action and record the results
        std::vector<InitialWorkerComputePathResult> results;
        for (auto resetValue : orderProcessTimerResetValues)
        {
            int frame = referenceFrame;
            int orderProcessTimer = initialOrderProcessTimer;
            bool orderProcessTimerResets = false;
            while (true)
            {
                if (frame == 158 && frame > referenceFrame)
                {
                    orderProcessTimer = resetValue;
                    orderProcessTimerResets = true;
                }

                if (orderProcessTimer == 0 && frame >= arrivalFrame)
                {
                    results.emplace_back(frame,
                                         true,
                                         collision ? 9 : 0,
                                         nextPathStartPosition,
                                         orderProcessTimerResets ? (std::optional<int>)resetValue : std::nullopt);
                    break;
                }

                orderProcessTimer--;
                if (orderProcessTimer < 0) orderProcessTimer = 8;

                frame++;
            }
            if (!orderProcessTimerResets) return results;
        }

        return results;
    }

    InitialWorkerGatherArrivalData InitialWorkerGatherArrivalData::createFromSimulatedPath(const BWAPI::SimulateGatherPathResult &simulatedPath,
                                                                                           BWAPI::Unit patch)
    {
        if (simulatedPath.positions.empty()) return {};

        return {
                (uint16_t)simulatedPath.positions.size(),
                isFacingPatch(simulatedPath.actionPosition, patch),
                simulatedPath.squaredSpeedEightFramesAlongNextPath == 0,
                simulatedPath.nextPathStartPosition
        };
    }
}
