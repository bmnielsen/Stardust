#pragma once

#include "../MiningOptimizationConfiguration.h"
#include "PositionAndVelocity.h"
#include "Path.h"

#include <cstdint>

namespace MiningOptimization
{
    /*
     * Stores the arrival data we need to track for return paths.
     *
     * Arrival delay: the number of frames to arrival at the depot
     * Collision: whether the worker collides with the depot after delivery when delivery does not happen on the first frame
     * Exit speed: the exit speed from the depot when delivery happens on the first frame
     * Facing target: whether the worker is facing the depot on arrival or needs to path to turn to the correct heading
     * Next path length (currently disabled): average length of the gather path from the end position of this arrival
     *
     * The booleans are packed in with the arrival delay and exit speed to save on data.
     */
    struct ReturnArrivalData
    {
        uint8_t packedArrivalDelayAndCollision = UINT8_MAX;
        uint8_t packedExitSpeedAndFacingDepot = UINT8_MAX;

        // The number of frames to arrival at the target
        [[nodiscard]] unsigned int arrivalDelay() const
        {
            // Delay is stored in the upper 7 bits, so shift one right and return
            return packedArrivalDelayAndCollision >> 1;
        }

        // Whether there is collision with delivery after arrival
        [[nodiscard]] bool collision() const
        {
            // Lowest bit is set if the worker is there is collision
            return (packedArrivalDelayAndCollision & 0b00000001) == 0b00000001;
        }

        // The exit speed of the worker from the depot back towards the patch when there was delivery at arrival
        [[nodiscard]] unsigned int exitSpeed() const
        {
            // Exit speed is stored in the upper 7 bits, so shift one right and return
            return packedExitSpeedAndFacingDepot >> 1;
        }

        // Whether the worker is facing the depot at arrival
        [[nodiscard]] bool facingTarget() const
        {
            // Lowest bit is set if the worker is facing the depot
            return (packedExitSpeedAndFacingDepot & 0b00000001) == 0b00000001;
        }

#if USE_NEXT_PATH_LENGTHS
        uint8_t nextPathLengthDelta = UINT8_MAX;

        // The length of the gather path from the end position of this arrival
        [[nodiscard]] unsigned int nextPathLength(uint8_t minimumNextPathLength) const
        {
            // Next path length is stored in the upper 6 bits, so shift two right and add the minimum value
            return (unsigned int)nextPathLengthDelta + minimumNextPathLength;
        }

        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathLengthDelta) == std::tie(other.packed, other.nextPathLengthDelta);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packed, nextPathLengthDelta) < std::tie(other.packed, other.nextPathLengthDelta);
        }
#else
        bool operator==(const ReturnArrivalData &other) const
        {
            return std::tie(packedArrivalDelayAndCollision, packedExitSpeedAndFacingDepot)
                == std::tie(other.packedArrivalDelayAndCollision, other.packedExitSpeedAndFacingDepot);
        }

        bool operator<(const ReturnArrivalData &other) const
        {
            return std::tie(packedArrivalDelayAndCollision, packedExitSpeedAndFacingDepot)
                < std::tie(other.packedArrivalDelayAndCollision, other.packedExitSpeedAndFacingDepot);
        }
#endif

        // Adds the delay after the return to the given map
        // If the order process timer at arrival is 0, the delay is allowed to be negative if speed is kept
        // Otherwise only collisions are considered
        void addDelayAfterAction(std::map<int, double> &delaysWithProbabilities,
                                 int orderProcessTimerAtArrival,
                                 int actionFrame,
                                 double baseProbability) const;

        friend std::ostream& operator<< (std::ostream& os, const ReturnArrivalData& data)
        {
            os << data.arrivalDelay() << "/" << data.exitSpeed();
            if (data.collision()) os << "[c]";
            if (!data.facingTarget()) os << "[!fd]";
            return os;
        }
    };

    typedef Path<ReturnArrivalData> ReturnPath;
    typedef PathNode<ReturnArrivalData> ReturnPathNode;
}
