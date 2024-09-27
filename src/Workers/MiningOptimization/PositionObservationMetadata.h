#pragma once

#include "PositionAndVelocity.h"
#include <map>

namespace WorkerMiningOptimization
{
    // This is the structure we use to track observed positions and our track record using them
    struct PositionObservationMetadata
    {
    public:
        PositionAndVelocity pos;

        // How many times we have observed this position with no resends potentially disturbing the path
        unsigned int observations = 0;

        // How many times this position was used successfully
        unsigned int successes = 0;

        // How many times this position was used unsuccessfully
        unsigned int failures = 0;

        // Additional position metadata gathered from the failure cases
        std::map<PositionAndVelocity, std::map<int, PositionObservationMetadata>> failurePositionMetadata;

        void trackObservation()
        {
            if (atObservationCap()) return;
            observations++;
        }

        void trackSuccess()
        {
            if (atObservationCap()) return;
            successes++;
        }

        void trackFailure()
        {
            if (atObservationCap()) return;
            failures++;
        }

        [[nodiscard]] bool atObservationCap() const
        {
            // Set a cap on how many observations we track to reduce computation time
            // Beyond a certain point additional observations are not going to have any impact anyway
            return (observations + successes + failures) >= 1000;
        }
    };

    std::ostream &operator<<(std::ostream &os, const PositionObservationMetadata &optimalGatherPositionMetadata);
}
