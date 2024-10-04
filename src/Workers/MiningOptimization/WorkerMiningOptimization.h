#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "PositionObservationMetadata.h"
#include "WorkerGatherStatus.h"

#if INSTRUMENTATION_ENABLED
#define TAKEOVER_DEBUG true
#define OPTIMALPOSITIONS_DEBUG true
#endif

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
                                        std::map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositions,
                                        std::map<PositionAndVelocity, PositionObservationMetadata> &takeoverResendPositions);

    WorkerGatherStatus &gatherStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource);
    std::map<PositionAndVelocity, PositionObservationMetadata> &optimalGatherPositionsFor(const Resource &resource);
    std::map<PositionAndVelocity, PositionObservationMetadata> &tenDistancePositionsFor(const Resource &resource);
    std::map<PositionAndVelocity, PositionObservationMetadata> &takeoverPositionsFor(const Resource &resource);

    bool isExploring();
    void setExploring(bool exploring);
}
