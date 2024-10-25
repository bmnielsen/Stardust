#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "PositionObservationMetadata.h"
#include "WorkerGatherStatus.h"

#define EXPLORE_BEFORE 12
#define EXPLORE_AFTER 5
#define EXPLORE_SECOND_RESEND_POSITIONS 2 // This is in addition to EXPLORE_AFTER

namespace WorkerMiningOptimization
{
    void initialize();

    void flushObservations();

    void write();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    void flushStartOfMiningObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses);
    void handleStartOfMiningPatchSwitch(WorkerGatherStatus &workerStatus,
                                        const Resource &resource,
                                        std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                        std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions);

    WorkerGatherStatus &gatherStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource);
    std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositionsFor(const Resource &resource);
    std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositionsFor(const Resource &resource);
    std::unordered_map<PositionAndVelocity, PositionObservationMetadata> &takeoverPositionsFor(const Resource &resource);

    bool isExploring();
    void setExploring(bool exploring);
}
