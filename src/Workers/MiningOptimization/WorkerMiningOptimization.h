#pragma once

#include "Common.h"
#include "MyWorker.h"
#include "Resource.h"
#include "GatherPositionObservations.h"
#include "WorkerGatherStatus.h"
#include "ReturnPositionObservations.h"
#include "WorkerReturnStatus.h"

#define ENABLE_GATHER_OPTIMIZATION true
#define ENABLE_RETURN_OPTIMIZATION true
#define ENABLE_PATH_BASED_TAKEOVER true

// In our data files we track the full paths between depot and patch, but we only explore positions within the bounds defined here
#define GATHER_EXPLORE_BEFORE 12
#define GATHER_EXPLORE_AFTER 5

// Return bounds are relative to the position LF+8 frames before arrival in the normal path, which is often the optimal position to resend from
#define RETURN_EXPLORE_BEFORE 15
#define RETURN_EXPLORE_AFTER 5

namespace WorkerMiningOptimization
{
    void initialize();

    void flushObservations();

    void write();

    // Optimizes the start of mining
    void optimizeStartOfMining(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    void planGatherResendsSingle(WorkerGatherStatus &workerStatus,
                                 const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                 const std::shared_ptr<PositionAndVelocity> &currentPosition);

    void validatePlannedGatherPathSingle(WorkerGatherStatus &workerStatus,
                                         const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition);

    void planGatherResendsDouble(WorkerGatherStatus &workerStatus,
                                 const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                 const std::shared_ptr<PositionAndVelocity> &currentPosition);

    void validatePlannedGatherPathDouble(WorkerGatherStatus &workerStatus,
                                         const std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalPositions,
                                         const std::shared_ptr<PositionAndVelocity> &currentPosition);

    void flushGatherObservations(std::map<MyWorker, WorkerGatherStatus> &workerGatherStatuses);

    void handleGatherPatchSwitch(WorkerGatherStatus &workerStatus);

    WorkerGatherStatus &gatherStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    WorkerGatherStatus *gatherStatusFor(const MyWorker &worker);

    std::unordered_map<PositionAndVelocity, GatherPositionObservations> &optimalGatherPositionsFor(const Resource &resource);

    std::unordered_set<PositionAndVelocity> &tenDistancePositionsFor(const Resource &resource);

    // Optimizes returning a resource
    void optimizeReturnOfResource(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    void flushReturnObservations(std::map<MyWorker, WorkerReturnStatus> &workerReturnStatuses);

    WorkerReturnStatus &returnStatusFor(const MyWorker &worker, const MyUnit &depot, const Resource &resource);

    std::unordered_map<PositionAndVelocity, ReturnPositionObservations> &optimalReturnPositionsFor(const Resource &resource);

    bool isExploring();

    void setExploring(bool exploring);
}
