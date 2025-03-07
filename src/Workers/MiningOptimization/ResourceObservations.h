#pragma once

#include "TilePosition.h"

namespace WorkerMiningOptimization
{
    struct ResourceObservation
    {
        uint32_t observationCount = 0;

        uint64_t accumulator = 0;

        uint16_t average = 0;

        void addObservation(uint64_t value);
    };

    struct ResourceObservations
    {
        TilePosition tile;

        // Tracks the rotation time when a single worker is assigned to this patch
        // Measured as the time from the worker delivering one cargo to the same worker delivering its next cargo
        ResourceObservation singleWorkerRotations;

        // Tracks the rotation time when two workers are assigned to this patch
        // Measured as the time from the first worker delivering its cargo to the other worker delivering its cargo
        ResourceObservation doubleWorkerRotations;

        // For starting workers, we measure the frame on which each worker delivers its second set of minerals from this patch
        // The vector will either have 4 entries or be empty (for resources not at a starting position)
        // The workers in the vector are in the same order as in MapSpecificOverride::startingWorkerPositions
        std::vector<ResourceObservation> startingWorkerObservations;

        ResourceObservation &startingWorkerObservationsFor(int startingWorkerIndex);
    };
}
